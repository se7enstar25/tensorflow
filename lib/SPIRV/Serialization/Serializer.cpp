//===- Serializer.cpp - MLIR SPIR-V Serialization -------------------------===//
//
// Copyright 2019 The MLIR Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// =============================================================================
//
// This file defines the MLIR SPIR-V module to SPIR-V binary seralization.
//
//===----------------------------------------------------------------------===//

#include "mlir/SPIRV/Serialization.h"

#include "SPIRVBinaryUtils.h"
#include "mlir/SPIRV/SPIRVOps.h"
#include "mlir/SPIRV/SPIRVTypes.h"
#include "mlir/Support/LogicalResult.h"
#include "llvm/ADT/SmallVector.h"

using namespace mlir;

static inline uint32_t getPrefixedOpcode(uint32_t wordCount,
                                         spirv::Opcode opcode) {
  assert(((wordCount >> 16) == 0) && "word count out of range!");
  return (wordCount << 16) | static_cast<uint32_t>(opcode);
}

static inline void buildInstruction(spirv::Opcode op,
                                    ArrayRef<uint32_t> operands,
                                    SmallVectorImpl<uint32_t> &binary) {
  uint32_t wordCount = 1 + operands.size();
  binary.push_back(getPrefixedOpcode(wordCount, op));
  binary.append(operands.begin(), operands.end());
}

namespace {

/// A SPIR-V module serializer.
///
/// A SPIR-V binary module is a single linear stream of instructions; each
/// instruction is composed of 32-bit words with the layout:
///
///   | <word-count>|<opcode> |  <operand>   |  <operand>   | ... |
///   | <------ word -------> | <-- word --> | <-- word --> | ... |
///
/// For the first word, the 16 high-order bits are the word count of the
/// instruction, the 16 low-order bits are the opcode enumerant. The
/// instructions then belong to different sections, which must be laid out in
/// the particular order as specified in "2.4 Logical Layout of a Module" of
/// the SPIR-V spec.
class Serializer {
public:
  /// Creates a serializer for the given SPIR-V `module`.
  explicit Serializer(spirv::ModuleOp module) : module(module) {}

  /// Serializes the remembered SPIR-V module.
  LogicalResult serialize();

  /// Collects the final SPIR-V `binary`.
  void collect(SmallVectorImpl<uint32_t> &binary);

private:
  /// Creates SPIR-V module header in the given `header`.
  LogicalResult processHeader();

  LogicalResult processMemoryModel();

  // Method to dispatch type serialization
  LogicalResult processType(Location loc, Type type, uint32_t &typeID);

  // Methods to serialize individual types
  LogicalResult processBasicType(Location loc, Type type,
                                 spirv::Opcode &typeEnum,
                                 SmallVectorImpl<uint32_t> &operands);
  LogicalResult processFunctionType(Location loc, FunctionType type,
                                    spirv::Opcode &typeEnum,
                                    SmallVectorImpl<uint32_t> &operands);

  // Main method to dispatch operation serialization
  LogicalResult processOperation(Operation *op, uint32_t &opID);

  // Methods to serialize individual operation types
  LogicalResult processFuncOp(FuncOp op, uint32_t &funcID);

  uint32_t getNextID() { return nextID++; }

private:
  /// The SPIR-V module to be serialized.
  spirv::ModuleOp module;

  /// The next available result <id>.
  uint32_t nextID = 1;

  // The following are for different SPIR-V instruction sections. They follow
  // the logical layout of a SPIR-V module.

  SmallVector<uint32_t, spirv::kHeaderWordCount> header;
  SmallVector<uint32_t, 4> capabilities;
  SmallVector<uint32_t, 0> extensions;
  SmallVector<uint32_t, 0> extendedSets;
  SmallVector<uint32_t, 3> memoryModel;
  SmallVector<uint32_t, 0> entryPoints;
  SmallVector<uint32_t, 4> executionModes;
  // TODO(antiagainst): debug instructions
  SmallVector<uint32_t, 0> decorations;
  SmallVector<uint32_t, 0> typesGlobalValues;
  SmallVector<uint32_t, 0> functionDecls;
  SmallVector<uint32_t, 0> functionDefns;

  // Map from type used in SPIR-V module to their IDs
  DenseMap<Type, uint32_t> typeIDMap;
};
} // namespace

LogicalResult Serializer::serialize() {
  if (failed(module.verify()))
    return failure();

  // TODO(antiagainst): handle the other sections
  processMemoryModel();

  // Iterate over the module body to serialze it. Assumptions are that there is
  // only one basic block in the moduleOp
  for (auto &op : module.getBlock()) {
    uint32_t opID = 0;
    if (failed(processOperation(&op, opID))) {
      return failure();
    }
  }
  return success();
}

void Serializer::collect(SmallVectorImpl<uint32_t> &binary) {
  // The number of words in the SPIR-V module header

  auto moduleSize = header.size() + capabilities.size() + extensions.size() +
                    extendedSets.size() + memoryModel.size() +
                    entryPoints.size() + executionModes.size() +
                    decorations.size() + typesGlobalValues.size() +
                    functionDecls.size() + functionDefns.size();

  binary.clear();
  binary.reserve(moduleSize);

  processHeader();
  binary.append(header.begin(), header.end());
  binary.append(capabilities.begin(), capabilities.end());
  binary.append(extensions.begin(), extensions.end());
  binary.append(extendedSets.begin(), extendedSets.end());
  binary.append(memoryModel.begin(), memoryModel.end());
  binary.append(entryPoints.begin(), entryPoints.end());
  binary.append(executionModes.begin(), executionModes.end());
  binary.append(decorations.begin(), decorations.end());
  binary.append(typesGlobalValues.begin(), typesGlobalValues.end());
  binary.append(functionDecls.begin(), functionDecls.end());
  binary.append(functionDefns.begin(), functionDefns.end());
}

LogicalResult Serializer::processHeader() {
  // The serializer tool ID registered to the Khronos Group
  constexpr uint32_t kGeneratorNumber = 22;
  // The major and minor version number for the generated SPIR-V binary.
  // TODO(antiagainst): use target environment to select the version
  constexpr uint8_t kMajorVersion = 1;
  constexpr uint8_t kMinorVersion = 0;

  // See "2.3. Physical Layout of a SPIR-V Module and Instruction" in the SPIR-V
  // spec for the definition of the binary module header.
  //
  // The first five words of a SPIR-V module must be:
  // +-------------------------------------------------------------------------+
  // | Magic number                                                            |
  // +-------------------------------------------------------------------------+
  // | Version number (bytes: 0 | major number | minor number | 0)             |
  // +-------------------------------------------------------------------------+
  // | Generator magic number                                                  |
  // +-------------------------------------------------------------------------+
  // | Bound (all result <id>s in the module guaranteed to be less than it)    |
  // +-------------------------------------------------------------------------+
  // | 0 (reserved for instruction schema)                                     |
  // +-------------------------------------------------------------------------+
  header.push_back(spirv::kMagicNumber);
  header.push_back((kMajorVersion << 16) | (kMinorVersion << 8));
  header.push_back(kGeneratorNumber);
  header.push_back(nextID); // ID bound
  header.push_back(0);      // Schema (reserved word)
  return success();
}

LogicalResult Serializer::processMemoryModel() {
  uint32_t mm = module.getAttrOfType<IntegerAttr>("memory_model").getInt();
  uint32_t am = module.getAttrOfType<IntegerAttr>("addressing_model").getInt();

  buildInstruction(spirv::Opcode::OpMemoryModel, {am, mm}, memoryModel);
  return success();
}

LogicalResult Serializer::processType(Location loc, Type type,
                                      uint32_t &typeID) {
  auto it = typeIDMap.find(type);
  if (it != typeIDMap.end()) {
    typeID = it->second;
    return success();
  }
  typeID = getNextID();
  SmallVector<uint32_t, 4> operands;
  operands.push_back(typeID);
  auto typeEnum = spirv::Opcode::OpTypeVoid;
  if ((type.isa<FunctionType>() &&
       succeeded(processFunctionType(loc, type.cast<FunctionType>(), typeEnum,
                                     operands))) ||
      succeeded(processBasicType(loc, type, typeEnum, operands))) {
    buildInstruction(typeEnum, operands, typesGlobalValues);
    typeIDMap[type] = typeID;
    return success();
  }
  return failure();
}

LogicalResult
Serializer::processBasicType(Location loc, Type type, spirv::Opcode &typeEnum,
                             SmallVectorImpl<uint32_t> &operands) {
  if (type.isa<NoneType>()) {
    typeEnum = spirv::Opcode::OpTypeVoid;
    return success();
  }
  /// TODO(ravishankarm) : Handle other types
  return emitError(loc, "unhandled type in serialization : ") << type;
}

LogicalResult
Serializer::processFunctionType(Location loc, FunctionType type,
                                spirv::Opcode &typeEnum,
                                SmallVectorImpl<uint32_t> &operands) {
  typeEnum = spirv::Opcode::OpTypeFunction;
  assert(type.getNumResults() <= 1 &&
         "Serialization supports only a single return value");
  uint32_t resultID = 0;
  if (failed(processType(loc,
                         type.getNumResults() == 1
                             ? type.getResult(0)
                             : mlir::NoneType::get(module.getContext()),
                         resultID))) {
    return failure();
  }
  operands.push_back(resultID);
  for (auto &res : type.getInputs()) {
    uint32_t argTypeID = 0;
    if (failed(processType(loc, res, argTypeID))) {
      return failure();
    }
    operands.push_back(argTypeID);
  }
  return success();
}

LogicalResult Serializer::processOperation(Operation *op, uint32_t &opID) {
  opID = getNextID();
  if ((isa<FuncOp>(op) && succeeded(processFuncOp(cast<FuncOp>(op), opID))) ||
      isa<spirv::ModuleEndOp>(op)) {
    return success();
  }
  /// TODO(ravishankarm) : Handle other ops
  return op->emitError("unhandled operation serialization");
}

LogicalResult Serializer::processFuncOp(FuncOp op, uint32_t &funcID) {
  uint32_t typeID = 0;
  // Generate type of the function
  processType(op.getLoc(), op.getType(), typeID);
  // TODO(ravishankarm) : Process Function body
  return success();
}

LogicalResult spirv::serialize(spirv::ModuleOp module,
                               SmallVectorImpl<uint32_t> &binary) {
  Serializer serializer(module);

  if (failed(serializer.serialize()))
    return failure();

  serializer.collect(binary);
  return success();
}
