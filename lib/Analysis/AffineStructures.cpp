//===- AffineStructures.cpp - MLIR Affine Structures Class-------*- C++ -*-===//
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
// Structures for affine/polyhedral analysis of MLIR functions.
//
//===----------------------------------------------------------------------===//

#include "mlir/Analysis/AffineStructures.h"
#include "mlir/Analysis/AffineAnalysis.h"
#include "mlir/IR/AffineExprVisitor.h"
#include "mlir/IR/AffineMap.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Instructions.h"
#include "mlir/IR/IntegerSet.h"
#include "mlir/Support/MathExtras.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "affine-structures"

using namespace mlir;
using namespace llvm;

namespace {

// Affine map composition terminology:
// *) current: refers to the target map of the composition operation. It is the
//    map into which results from the 'input' map are forward substituted.
// *) input: refers to the map which is being forward substituted into the
//    'current' map.
// *) output: refers to the resulting affine map after composition.

// AffineMapCompositionUpdate encapsulates the state necessary to compose
// AffineExprs for two affine maps using AffineExprComposer (see below).
struct AffineMapCompositionUpdate {
  using PositionMap = DenseMap<unsigned, unsigned>;

  explicit AffineMapCompositionUpdate(ArrayRef<AffineExpr> inputResults)
      : inputResults(inputResults), outputNumDims(0), outputNumSymbols(0) {}

  // Map from 'curr' affine map dim position to 'output' affine map
  // dim position.
  PositionMap currDimMap;
  // Map from dim position of 'curr' affine map to index into 'inputResults'.
  PositionMap currDimToInputResultMap;
  // Map from 'curr' affine map symbol position to 'output' affine map
  // symbol position.
  PositionMap currSymbolMap;
  // Map from 'input' affine map dim position to 'output' affine map
  // dim position.
  PositionMap inputDimMap;
  // Map from 'input' affine map symbol position to 'output' affine map
  // symbol position.
  PositionMap inputSymbolMap;
  // Results of 'input' affine map.
  ArrayRef<AffineExpr> inputResults;
  // Number of dimension operands for 'output' affine map.
  unsigned outputNumDims;
  // Number of symbol operands for 'output' affine map.
  unsigned outputNumSymbols;
};

// AffineExprComposer composes two AffineExprs as specified by 'mapUpdate'.
class AffineExprComposer {
public:
  // Compose two AffineExprs using dimension and symbol position update maps,
  // as well as input map result AffineExprs specified in 'mapUpdate'.
  AffineExprComposer(const AffineMapCompositionUpdate &mapUpdate)
      : mapUpdate(mapUpdate), walkingInputMap(false) {}

  AffineExpr walk(AffineExpr expr) {
    switch (expr.getKind()) {
    case AffineExprKind::Add:
      return walkBinExpr(
          expr, [](AffineExpr lhs, AffineExpr rhs) { return lhs + rhs; });
    case AffineExprKind::Mul:
      return walkBinExpr(
          expr, [](AffineExpr lhs, AffineExpr rhs) { return lhs * rhs; });
    case AffineExprKind::Mod:
      return walkBinExpr(
          expr, [](AffineExpr lhs, AffineExpr rhs) { return lhs % rhs; });
    case AffineExprKind::FloorDiv:
      return walkBinExpr(expr, [](AffineExpr lhs, AffineExpr rhs) {
        return lhs.floorDiv(rhs);
      });
    case AffineExprKind::CeilDiv:
      return walkBinExpr(expr, [](AffineExpr lhs, AffineExpr rhs) {
        return lhs.ceilDiv(rhs);
      });
    case AffineExprKind::Constant:
      return expr;
    case AffineExprKind::DimId: {
      unsigned dimPosition = expr.cast<AffineDimExpr>().getPosition();
      if (walkingInputMap) {
        return getAffineDimExpr(mapUpdate.inputDimMap.lookup(dimPosition),
                                expr.getContext());
      }
      // Check if we are just mapping this dim to another position.
      if (mapUpdate.currDimMap.count(dimPosition) > 0) {
        assert(mapUpdate.currDimToInputResultMap.count(dimPosition) == 0);
        return getAffineDimExpr(mapUpdate.currDimMap.lookup(dimPosition),
                                expr.getContext());
      }
      // We are substituting an input map result at 'dimPositon'
      // Forward substitute currDimToInputResultMap[dimPosition] into this
      // map.
      AffineExprComposer composer(mapUpdate, /*walkingInputMap=*/true);
      unsigned inputResultIndex =
          mapUpdate.currDimToInputResultMap.lookup(dimPosition);
      assert(inputResultIndex < mapUpdate.inputResults.size());
      return composer.walk(mapUpdate.inputResults[inputResultIndex]);
    }
    case AffineExprKind::SymbolId:
      unsigned symbolPosition = expr.cast<AffineSymbolExpr>().getPosition();
      if (walkingInputMap) {
        return getAffineSymbolExpr(
            mapUpdate.inputSymbolMap.lookup(symbolPosition), expr.getContext());
      }
      return getAffineSymbolExpr(mapUpdate.currSymbolMap.lookup(symbolPosition),
                                 expr.getContext());
    }
  }

private:
  AffineExprComposer(const AffineMapCompositionUpdate &mapUpdate,
                     bool walkingInputMap)
      : mapUpdate(mapUpdate), walkingInputMap(walkingInputMap) {}

  AffineExpr walkBinExpr(AffineExpr expr,
                         std::function<AffineExpr(AffineExpr, AffineExpr)> op) {
    auto binOpExpr = expr.cast<AffineBinaryOpExpr>();
    return op(walk(binOpExpr.getLHS()), walk(binOpExpr.getRHS()));
  }

  // Map update specifies to dim and symbol postion maps, as well as the input
  // result AffineExprs to forward subustitute into the input map.
  const AffineMapCompositionUpdate &mapUpdate;
  // True if we are walking an AffineExpr in the 'input' map, false if
  // we are walking the 'input' map.
  bool walkingInputMap;
};

} // end anonymous namespace

static void
forwardSubstituteMutableAffineMap(const AffineMapCompositionUpdate &mapUpdate,
                                  MutableAffineMap *map) {
  for (unsigned i = 0, e = map->getNumResults(); i < e; i++) {
    AffineExprComposer composer(mapUpdate);
    map->setResult(i, composer.walk(map->getResult(i)));
  }
  // TODO(andydavis) Evaluate if we need to update range sizes here.
  map->setNumDims(mapUpdate.outputNumDims);
  map->setNumSymbols(mapUpdate.outputNumSymbols);
}

//===----------------------------------------------------------------------===//
// MutableAffineMap.
//===----------------------------------------------------------------------===//

MutableAffineMap::MutableAffineMap(AffineMap map)
    : numDims(map.getNumDims()), numSymbols(map.getNumSymbols()),
      // A map always has at least 1 result by construction
      context(map.getResult(0).getContext()) {
  for (auto result : map.getResults())
    results.push_back(result);
  for (auto rangeSize : map.getRangeSizes())
    results.push_back(rangeSize);
}

void MutableAffineMap::reset(AffineMap map) {
  results.clear();
  rangeSizes.clear();
  numDims = map.getNumDims();
  numSymbols = map.getNumSymbols();
  // A map always has at least 1 result by construction
  context = map.getResult(0).getContext();
  for (auto result : map.getResults())
    results.push_back(result);
  for (auto rangeSize : map.getRangeSizes())
    results.push_back(rangeSize);
}

bool MutableAffineMap::isMultipleOf(unsigned idx, int64_t factor) const {
  if (results[idx].isMultipleOf(factor))
    return true;

  // TODO(bondhugula): use simplifyAffineExpr and FlatAffineConstraints to
  // complete this (for a more powerful analysis).
  return false;
}

// Simplifies the result affine expressions of this map. The expressions have to
// be pure for the simplification implemented.
void MutableAffineMap::simplify() {
  // Simplify each of the results if possible.
  // TODO(ntv): functional-style map
  for (unsigned i = 0, e = getNumResults(); i < e; i++) {
    results[i] = simplifyAffineExpr(getResult(i), numDims, numSymbols);
  }
}

AffineMap MutableAffineMap::getAffineMap() const {
  return AffineMap::get(numDims, numSymbols, results, rangeSizes);
}

MutableIntegerSet::MutableIntegerSet(IntegerSet set, MLIRContext *context)
    : numDims(set.getNumDims()), numSymbols(set.getNumSymbols()),
      context(context) {
  // TODO(bondhugula)
}

// Universal set.
MutableIntegerSet::MutableIntegerSet(unsigned numDims, unsigned numSymbols,
                                     MLIRContext *context)
    : numDims(numDims), numSymbols(numSymbols), context(context) {}

//===----------------------------------------------------------------------===//
// AffineValueMap.
//===----------------------------------------------------------------------===//

AffineValueMap::AffineValueMap(const AffineApplyOp &op)
    : map(op.getAffineMap()) {
  for (auto *operand : op.getOperands())
    operands.push_back(const_cast<Value *>(operand));
  for (unsigned i = 0, e = op.getNumResults(); i < e; i++)
    results.push_back(const_cast<Value *>(op.getResult(i)));
}

AffineValueMap::AffineValueMap(AffineMap map, ArrayRef<Value *> operands)
    : map(map) {
  for (Value *operand : operands) {
    this->operands.push_back(operand);
  }
}

void AffineValueMap::reset(AffineMap map, ArrayRef<Value *> operands) {
  this->operands.clear();
  this->results.clear();
  this->map.reset(map);
  for (Value *operand : operands) {
    this->operands.push_back(operand);
  }
}

void AffineValueMap::forwardSubstitute(const AffineApplyOp &inputOp) {
  SmallVector<bool, 4> inputResultsToSubstitute(inputOp.getNumResults());
  for (unsigned i = 0, e = inputOp.getNumResults(); i < e; i++)
    inputResultsToSubstitute[i] = true;
  forwardSubstitute(inputOp, inputResultsToSubstitute);
}

void AffineValueMap::forwardSubstituteSingle(const AffineApplyOp &inputOp,
                                             unsigned inputResultIndex) {
  SmallVector<bool, 4> inputResultsToSubstitute(inputOp.getNumResults(), false);
  inputResultsToSubstitute[inputResultIndex] = true;
  forwardSubstitute(inputOp, inputResultsToSubstitute);
}

// Returns true and sets 'indexOfMatch' if 'valueToMatch' is found in
// 'valuesToSearch' beginning at 'indexStart'. Returns false otherwise.
static bool findIndex(Value *valueToMatch, ArrayRef<Value *> valuesToSearch,
                      unsigned indexStart, unsigned *indexOfMatch) {
  unsigned size = valuesToSearch.size();
  for (unsigned i = indexStart; i < size; ++i) {
    if (valueToMatch == valuesToSearch[i]) {
      *indexOfMatch = i;
      return true;
    }
  }
  return false;
}

// AffineValueMap forward substitution composes results from the affine map
// associated with 'inputOp', with the map it currently represents. This is
// accomplished by updating its MutableAffineMap and operand list to represent
// a new 'output' map which is the composition of the 'current' and 'input'
// maps (see "Affine map composition terminology" above for details).
//
// Affine map forward substitution is comprised of the following steps:
// *) Compute input affine map result indices used by the current map.
// *) Gather all dim and symbol positions from all AffineExpr input results
//    computed in previous step.
// *) Build output operand list:
//  *) Add curr map dim operands:
//    *) If curr dim operand is being forward substituted by result of input
//       map, store mapping from curr postion to input result index.
//    *) Else add curr dim operand to output operand list.
//  *) Add input map dim operands:
//    *) If input map dim operand is used (step 2), add to output operand
//       list (scanning current list for dups before updating mapping).
//  *) Add curr map dim symbols.
//  *) Add input map dim symbols (if used from step 2), dedup if needed.
// *) Update operands and forward substitute new dim and symbol mappings
//    into MutableAffineMap 'map'.
//
// TODO(andydavis) Move this to a function which can be shared with
// forwardSubstitute(const AffineValueMap &inputMap).
void AffineValueMap::forwardSubstitute(
    const AffineApplyOp &inputOp, ArrayRef<bool> inputResultsToSubstitute) {
  unsigned currNumDims = map.getNumDims();
  unsigned inputNumResults = inputOp.getNumResults();

  // Gather result indices from 'inputOp' used by current map.
  DenseSet<unsigned> inputResultsUsed;
  DenseMap<unsigned, unsigned> currOperandToInputResult;
  for (unsigned i = 0; i < currNumDims; ++i) {
    for (unsigned j = 0; j < inputNumResults; ++j) {
      if (!inputResultsToSubstitute[j])
        continue;
      if (operands[i] == const_cast<Value *>(inputOp.getResult(j))) {
        currOperandToInputResult[i] = j;
        inputResultsUsed.insert(j);
      }
    }
  }

  // Return if there were no uses of 'inputOp' results in 'operands'.
  if (inputResultsUsed.empty()) {
    return;
  }

  class AffineExprPositionGatherer
      : public AffineExprVisitor<AffineExprPositionGatherer> {
  public:
    unsigned numDims;
    DenseSet<unsigned> *positions;
    AffineExprPositionGatherer(unsigned numDims, DenseSet<unsigned> *positions)
        : numDims(numDims), positions(positions) {}
    void visitDimExpr(AffineDimExpr expr) {
      positions->insert(expr.getPosition());
    }
    void visitSymbolExpr(AffineSymbolExpr expr) {
      positions->insert(numDims + expr.getPosition());
    }
  };

  // Gather dim and symbol positions from 'inputOp' on which
  // 'inputResultsUsed' depend.
  AffineMap inputMap = inputOp.getAffineMap();
  unsigned inputNumDims = inputMap.getNumDims();
  DenseSet<unsigned> inputPositionsUsed;
  AffineExprPositionGatherer gatherer(inputNumDims, &inputPositionsUsed);
  for (unsigned i = 0; i < inputNumResults; ++i) {
    if (inputResultsUsed.count(i) == 0)
      continue;
    gatherer.walkPostOrder(inputMap.getResult(i));
  }

  // Build new output operands list and map update.
  SmallVector<Value *, 4> outputOperands;
  unsigned outputOperandPosition = 0;
  AffineMapCompositionUpdate mapUpdate(inputOp.getAffineMap().getResults());

  // Add dim operands from current map.
  for (unsigned i = 0; i < currNumDims; ++i) {
    if (currOperandToInputResult.count(i) > 0) {
      mapUpdate.currDimToInputResultMap[i] = currOperandToInputResult[i];
    } else {
      mapUpdate.currDimMap[i] = outputOperandPosition++;
      outputOperands.push_back(operands[i]);
    }
  }

  // Add dim operands from input map.
  for (unsigned i = 0; i < inputNumDims; ++i) {
    // Skip input dim operands that we won't use.
    if (inputPositionsUsed.count(i) == 0)
      continue;
    // Check if input operand has a dup in current operand list.
    auto *inputOperand = const_cast<Value *>(inputOp.getOperand(i));
    unsigned outputIndex;
    if (findIndex(inputOperand, outputOperands, /*indexStart=*/0,
                  &outputIndex)) {
      mapUpdate.inputDimMap[i] = outputIndex;
    } else {
      mapUpdate.inputDimMap[i] = outputOperandPosition++;
      outputOperands.push_back(inputOperand);
    }
  }

  // Done adding dimension operands, so store new output num dims.
  unsigned outputNumDims = outputOperandPosition;

  // Add symbol operands from current map.
  unsigned currNumOperands = operands.size();
  for (unsigned i = currNumDims; i < currNumOperands; ++i) {
    unsigned currSymbolPosition = i - currNumDims;
    unsigned outputSymbolPosition = outputOperandPosition - outputNumDims;
    mapUpdate.currSymbolMap[currSymbolPosition] = outputSymbolPosition;
    outputOperands.push_back(operands[i]);
    ++outputOperandPosition;
  }

  // Add symbol operands from input map.
  unsigned inputNumOperands = inputOp.getNumOperands();
  for (unsigned i = inputNumDims; i < inputNumOperands; ++i) {
    // Skip input symbol operands that we won't use.
    if (inputPositionsUsed.count(i) == 0)
      continue;
    unsigned inputSymbolPosition = i - inputNumDims;
    // Check if input operand has a dup in current operand list.
    auto *inputOperand = const_cast<Value *>(inputOp.getOperand(i));
    // Find output operand index of 'inputOperand' dup.
    unsigned outputIndex;
    // Start at index 'outputNumDims' so that only symbol operands are searched.
    if (findIndex(inputOperand, outputOperands, /*indexStart=*/outputNumDims,
                  &outputIndex)) {
      unsigned outputSymbolPosition = outputIndex - outputNumDims;
      mapUpdate.inputSymbolMap[inputSymbolPosition] = outputSymbolPosition;
    } else {
      unsigned outputSymbolPosition = outputOperandPosition - outputNumDims;
      mapUpdate.inputSymbolMap[inputSymbolPosition] = outputSymbolPosition;
      outputOperands.push_back(inputOperand);
      ++outputOperandPosition;
    }
  }

  // Set output number of dimension and symbol operands.
  mapUpdate.outputNumDims = outputNumDims;
  mapUpdate.outputNumSymbols = outputOperands.size() - outputNumDims;

  // Update 'operands' with new 'outputOperands'.
  operands.swap(outputOperands);
  // Forward substitute 'mapUpdate' into 'map'.
  forwardSubstituteMutableAffineMap(mapUpdate, &map);
}

inline bool AffineValueMap::isMultipleOf(unsigned idx, int64_t factor) const {
  return map.isMultipleOf(idx, factor);
}

/// This method uses the invariant that operands are always positionally aligned
/// with the AffineDimExpr in the underlying AffineMap.
bool AffineValueMap::isFunctionOf(unsigned idx, Value *value) const {
  unsigned index;
  if (!findIndex(value, operands, /*indexStart=*/0, &index)) {
    return false;
  }
  auto expr = const_cast<AffineValueMap *>(this)->getAffineMap().getResult(idx);
  // TODO(ntv): this is better implemented on a flattened representation.
  // At least for now it is conservative.
  return expr.isFunctionOfDim(index);
}

Value *AffineValueMap::getOperand(unsigned i) const {
  return static_cast<Value *>(operands[i]);
}

ArrayRef<Value *> AffineValueMap::getOperands() const {
  return ArrayRef<Value *>(operands);
}

AffineMap AffineValueMap::getAffineMap() const { return map.getAffineMap(); }

AffineValueMap::~AffineValueMap() {}

//===----------------------------------------------------------------------===//
// FlatAffineConstraints.
//===----------------------------------------------------------------------===//

// Copy constructor.
FlatAffineConstraints::FlatAffineConstraints(
    const FlatAffineConstraints &other) {
  numReservedCols = other.numReservedCols;
  numDims = other.getNumDimIds();
  numSymbols = other.getNumSymbolIds();
  numIds = other.getNumIds();

  auto otherIds = other.getIds();
  ids.reserve(numReservedCols);
  ids.append(otherIds.begin(), otherIds.end());

  unsigned numReservedEqualities = other.getNumReservedEqualities();
  unsigned numReservedInequalities = other.getNumReservedInequalities();

  equalities.reserve(numReservedEqualities * numReservedCols);
  inequalities.reserve(numReservedInequalities * numReservedCols);

  for (unsigned r = 0, e = other.getNumInequalities(); r < e; r++) {
    addInequality(other.getInequality(r));
  }
  for (unsigned r = 0, e = other.getNumEqualities(); r < e; r++) {
    addEquality(other.getEquality(r));
  }
}

// Clones this object.
std::unique_ptr<FlatAffineConstraints> FlatAffineConstraints::clone() const {
  return std::make_unique<FlatAffineConstraints>(*this);
}

// Construct from an IntegerSet.
FlatAffineConstraints::FlatAffineConstraints(IntegerSet set)
    : numReservedCols(set.getNumOperands() + 1),
      numIds(set.getNumDims() + set.getNumSymbols()), numDims(set.getNumDims()),
      numSymbols(set.getNumSymbols()) {
  equalities.reserve(set.getNumEqualities() * numReservedCols);
  inequalities.reserve(set.getNumInequalities() * numReservedCols);
  ids.resize(numIds, None);

  // Flatten expressions and add them to the constraint system.
  std::vector<SmallVector<int64_t, 8>> flatExprs;
  FlatAffineConstraints localVarCst;
  if (!getFlattenedAffineExprs(set, &flatExprs, &localVarCst)) {
    assert(false && "flattening unimplemented for semi-affine integer sets");
    return;
  }
  assert(flatExprs.size() == set.getNumConstraints());
  for (unsigned l = 0, e = localVarCst.getNumLocalIds(); l < e; l++) {
    addLocalId(getNumLocalIds());
  }

  for (unsigned i = 0, e = flatExprs.size(); i < e; ++i) {
    const auto &flatExpr = flatExprs[i];
    assert(flatExpr.size() == getNumCols());
    if (set.getEqFlags()[i]) {
      addEquality(flatExpr);
    } else {
      addInequality(flatExpr);
    }
  }
  // Add the other constraints involving local id's from flattening.
  append(localVarCst);
}

void FlatAffineConstraints::reset(unsigned numReservedInequalities,
                                  unsigned numReservedEqualities,
                                  unsigned newNumReservedCols,
                                  unsigned newNumDims, unsigned newNumSymbols,
                                  unsigned newNumLocals,
                                  ArrayRef<Value *> idArgs) {
  assert(newNumReservedCols >= newNumDims + newNumSymbols + newNumLocals + 1 &&
         "minimum 1 column");
  numReservedCols = newNumReservedCols;
  numDims = newNumDims;
  numSymbols = newNumSymbols;
  numIds = numDims + numSymbols + newNumLocals;
  equalities.clear();
  inequalities.clear();
  if (numReservedEqualities >= 1)
    equalities.reserve(newNumReservedCols * numReservedEqualities);
  if (numReservedInequalities >= 1)
    inequalities.reserve(newNumReservedCols * numReservedInequalities);
  ids.clear();
  if (idArgs.empty()) {
    ids.resize(numIds, None);
  } else {
    ids.reserve(idArgs.size());
    ids.append(idArgs.begin(), idArgs.end());
  }
}

void FlatAffineConstraints::reset(unsigned newNumDims, unsigned newNumSymbols,
                                  unsigned newNumLocals,
                                  ArrayRef<Value *> idArgs) {
  reset(0, 0, newNumDims + newNumSymbols + newNumLocals + 1, newNumDims,
        newNumSymbols, newNumLocals, idArgs);
}

void FlatAffineConstraints::append(const FlatAffineConstraints &other) {
  assert(other.getNumCols() == getNumCols());
  assert(other.getNumDimIds() == getNumDimIds());
  assert(other.getNumSymbolIds() == getNumSymbolIds());

  inequalities.reserve(inequalities.size() +
                       other.getNumInequalities() * numReservedCols);
  equalities.reserve(equalities.size() +
                     other.getNumEqualities() * numReservedCols);

  for (unsigned r = 0, e = other.getNumInequalities(); r < e; r++) {
    addInequality(other.getInequality(r));
  }
  for (unsigned r = 0, e = other.getNumEqualities(); r < e; r++) {
    addEquality(other.getEquality(r));
  }
}

void FlatAffineConstraints::addLocalId(unsigned pos) {
  addId(IdKind::Local, pos);
}

void FlatAffineConstraints::addDimId(unsigned pos, Value *id) {
  addId(IdKind::Dimension, pos, id);
}

void FlatAffineConstraints::addSymbolId(unsigned pos, Value *id) {
  addId(IdKind::Symbol, pos, id);
}

/// Adds a dimensional identifier. The added column is initialized to
/// zero.
void FlatAffineConstraints::addId(IdKind kind, unsigned pos, Value *id) {
  if (kind == IdKind::Dimension) {
    assert(pos <= getNumDimIds());
  } else if (kind == IdKind::Symbol) {
    assert(pos <= getNumSymbolIds());
  } else {
    assert(pos <= getNumLocalIds());
  }

  unsigned oldNumReservedCols = numReservedCols;

  // Check if a resize is necessary.
  if (getNumCols() + 1 > numReservedCols) {
    equalities.resize(getNumEqualities() * (getNumCols() + 1));
    inequalities.resize(getNumInequalities() * (getNumCols() + 1));
    numReservedCols++;
  }

  unsigned absolutePos;

  if (kind == IdKind::Dimension) {
    absolutePos = pos;
    numDims++;
  } else if (kind == IdKind::Symbol) {
    absolutePos = pos + getNumDimIds();
    numSymbols++;
  } else {
    absolutePos = pos + getNumDimIds() + getNumSymbolIds();
  }
  numIds++;

  // Note that getNumCols() now will already return the new size, which will be
  // at least one.
  int numInequalities = static_cast<int>(getNumInequalities());
  int numEqualities = static_cast<int>(getNumEqualities());
  int numCols = static_cast<int>(getNumCols());
  for (int r = numInequalities - 1; r >= 0; r--) {
    for (int c = numCols - 2; c >= 0; c--) {
      if (c < absolutePos)
        atIneq(r, c) = inequalities[r * oldNumReservedCols + c];
      else
        atIneq(r, c + 1) = inequalities[r * oldNumReservedCols + c];
    }
    atIneq(r, absolutePos) = 0;
  }

  for (int r = numEqualities - 1; r >= 0; r--) {
    for (int c = numCols - 2; c >= 0; c--) {
      // All values in column absolutePositions < absolutePos have the same
      // coordinates in the 2-d view of the coefficient buffer.
      if (c < absolutePos)
        atEq(r, c) = equalities[r * oldNumReservedCols + c];
      else
        // Those at absolutePosition >= absolutePos, get a shifted
        // absolutePosition.
        atEq(r, c + 1) = equalities[r * oldNumReservedCols + c];
    }
    // Initialize added dimension to zero.
    atEq(r, absolutePos) = 0;
  }

  // If an 'id' is provided, insert it; otherwise use None.
  if (id) {
    ids.insert(ids.begin() + absolutePos, id);
  } else {
    ids.insert(ids.begin() + absolutePos, None);
  }
  assert(ids.size() == getNumIds());
}

// This routine may add additional local variables if the flattened expression
// corresponding to the map has such variables due to the presence of
// mod's, ceildiv's, and floordiv's.
bool FlatAffineConstraints::composeMap(AffineValueMap *vMap) {
  // Assert if the map and this constraint set aren't associated with the same
  // identifiers in the same order.
  assert(vMap->getNumDims() <= getNumDimIds());
  assert(vMap->getNumSymbols() <= getNumSymbolIds());
  for (unsigned i = 0, e = vMap->getNumDims(); i < e; i++) {
    assert(ids[i].hasValue());
    assert(vMap->getOperand(i) == ids[i].getValue());
  }
  for (unsigned i = 0, e = vMap->getNumSymbols(); i < e; i++) {
    assert(ids[numDims + i].hasValue());
    assert(vMap->getOperand(vMap->getNumDims() + i) ==
           ids[numDims + i].getValue());
  }

  std::vector<SmallVector<int64_t, 8>> flatExprs;
  FlatAffineConstraints cst;
  if (!getFlattenedAffineExprs(vMap->getAffineMap(), &flatExprs, &cst)) {
    LLVM_DEBUG(llvm::dbgs()
               << "composition unimplemented for semi-affine maps");
    return false;
  }
  assert(flatExprs.size() == vMap->getNumResults());

  // Make the value map and the flat affine cst dimensions compatible.
  // A lot of this code will be refactored/cleaned up.
  // TODO(bondhugula): the next ~20 lines of code is pretty UGLY. This needs
  // to be factored out into an FlatAffineConstraints::alignAndMerge().
  for (unsigned l = 0, e = cst.getNumLocalIds(); l < e; l++) {
    addLocalId(0);
  }

  for (unsigned t = 0, e = vMap->getNumResults(); t < e; t++) {
    // TODO: Consider using a batched version to add a range of IDs.
    addDimId(0);
    cst.addDimId(0);
  }

  assert(cst.getNumDimIds() <= getNumDimIds());
  for (unsigned t = 0, e = getNumDimIds() - cst.getNumDimIds(); t < e; t++) {
    // Dimensions that are in 'this' but not in vMap/cst are added at the end.
    cst.addDimId(cst.getNumDimIds());
  }
  assert(cst.getNumSymbolIds() <= getNumSymbolIds());
  for (unsigned t = 0, e = getNumSymbolIds() - cst.getNumSymbolIds(); t < e;
       t++) {
    // Dimensions that are in 'this' but not in vMap/cst are added at the end.
    cst.addSymbolId(cst.getNumSymbolIds());
  }
  assert(cst.getNumLocalIds() <= getNumLocalIds());
  for (unsigned t = 0, e = getNumLocalIds() - cst.getNumLocalIds(); t < e;
       t++) {
    cst.addLocalId(cst.getNumLocalIds());
  }
  /// Finally, append cst to this constraint set.
  append(cst);

  // We add one equality for each result connecting the result dim of the map to
  // the other identifiers.
  // For eg: if the expression is 16*i0 + i1, and this is the r^th
  // iteration/result of the value map, we are adding the equality:
  //  d_r - 16*i0 - i1 = 0. Hence, when flattening say (i0 + 1, i0 + 8*i2), we
  //  add two equalities overall: d_0 - i0 - 1 == 0, d1 - i0 - 8*i2 == 0.
  for (unsigned r = 0, e = flatExprs.size(); r < e; r++) {
    const auto &flatExpr = flatExprs[r];
    // eqToAdd is the equality corresponding to the flattened affine expression.
    SmallVector<int64_t, 8> eqToAdd(getNumCols(), 0);
    // Set the coefficient for this result to one.
    eqToAdd[r] = 1;

    assert(flatExpr.size() >= vMap->getNumOperands() + 1);

    // Dims and symbols.
    for (unsigned i = 0, e = vMap->getNumOperands(); i < e; i++) {
      unsigned loc;
      bool ret = findId(*vMap->getOperand(i), &loc);
      assert(ret && "value map's id can't be found");
      (void)ret;
      // We need to negate 'eq[r]' since the newly added dimension is going to
      // be set to this one.
      eqToAdd[loc] = -flatExpr[i];
    }
    // Local vars common to eq and cst are at the beginning.
    int j = getNumDimIds() + getNumSymbolIds();
    int end = flatExpr.size() - 1;
    for (int i = vMap->getNumOperands(); i < end; i++, j++) {
      eqToAdd[j] = -flatExpr[i];
    }

    // Constant term.
    eqToAdd[getNumCols() - 1] = -flatExpr[flatExpr.size() - 1];

    // Add the equality connecting the result of the map to this constraint set.
    addEquality(eqToAdd);
  }

  return true;
}

// Searches for a constraint with a non-zero coefficient at 'colIdx' in
// equality (isEq=true) or inequality (isEq=false) constraints.
// Returns true and sets row found in search in 'rowIdx'.
// Returns false otherwise.
static bool
findConstraintWithNonZeroAt(const FlatAffineConstraints &constraints,
                            unsigned colIdx, bool isEq, unsigned *rowIdx) {
  auto at = [&](unsigned rowIdx) -> int64_t {
    return isEq ? constraints.atEq(rowIdx, colIdx)
                : constraints.atIneq(rowIdx, colIdx);
  };
  unsigned e =
      isEq ? constraints.getNumEqualities() : constraints.getNumInequalities();
  for (*rowIdx = 0; *rowIdx < e; ++(*rowIdx)) {
    if (at(*rowIdx) != 0) {
      return true;
    }
  }
  return false;
}

// Normalizes the coefficient values across all columns in 'rowIDx' by their
// GCD in equality or inequality contraints as specified by 'isEq'.
template <bool isEq>
static void normalizeConstraintByGCD(FlatAffineConstraints *constraints,
                                     unsigned rowIdx) {
  auto at = [&](unsigned colIdx) -> int64_t {
    return isEq ? constraints->atEq(rowIdx, colIdx)
                : constraints->atIneq(rowIdx, colIdx);
  };
  uint64_t gcd = std::abs(at(0));
  for (unsigned j = 1, e = constraints->getNumCols(); j < e; ++j) {
    gcd = llvm::GreatestCommonDivisor64(gcd, std::abs(at(j)));
  }
  if (gcd > 0 && gcd != 1) {
    for (unsigned j = 0, e = constraints->getNumCols(); j < e; ++j) {
      int64_t v = at(j) / static_cast<int64_t>(gcd);
      isEq ? constraints->atEq(rowIdx, j) = v
           : constraints->atIneq(rowIdx, j) = v;
    }
  }
}

void FlatAffineConstraints::normalizeConstraintsByGCD() {
  for (unsigned i = 0, e = getNumEqualities(); i < e; ++i) {
    normalizeConstraintByGCD</*isEq=*/true>(this, i);
  }
  for (unsigned i = 0, e = getNumInequalities(); i < e; ++i) {
    normalizeConstraintByGCD</*isEq=*/false>(this, i);
  }
}

bool FlatAffineConstraints::hasConsistentState() const {
  if (inequalities.size() != getNumInequalities() * numReservedCols)
    return false;
  if (equalities.size() != getNumEqualities() * numReservedCols)
    return false;
  if (ids.size() != getNumIds())
    return false;

  // Catches errors where numDims, numSymbols, numIds aren't consistent.
  if (numDims > numIds || numSymbols > numIds || numDims + numSymbols > numIds)
    return false;

  return true;
}

/// Checks all rows of equality/inequality constraints for trivial
/// contradictions (for example: 1 == 0, 0 >= 1), which may have surfaced
/// after elimination. Returns 'true' if an invalid constraint is found;
/// 'false' otherwise.
bool FlatAffineConstraints::hasInvalidConstraint() const {
  assert(hasConsistentState());
  auto check = [&](bool isEq) -> bool {
    unsigned numCols = getNumCols();
    unsigned numRows = isEq ? getNumEqualities() : getNumInequalities();
    for (unsigned i = 0, e = numRows; i < e; ++i) {
      unsigned j;
      for (j = 0; j < numCols - 1; ++j) {
        int64_t v = isEq ? atEq(i, j) : atIneq(i, j);
        // Skip rows with non-zero variable coefficients.
        if (v != 0)
          break;
      }
      if (j < numCols - 1) {
        continue;
      }
      // Check validity of constant term at 'numCols - 1' w.r.t 'isEq'.
      // Example invalid constraints include: '1 == 0' or '-1 >= 0'
      int64_t v = isEq ? atEq(i, numCols - 1) : atIneq(i, numCols - 1);
      if ((isEq && v != 0) || (!isEq && v < 0)) {
        return true;
      }
    }
    return false;
  };
  if (check(/*isEq=*/true))
    return true;
  return check(/*isEq=*/false);
}

// Eliminate identifier from constraint at 'rowIdx' based on coefficient at
// pivotRow, pivotCol. Columns in range [elimColStart, pivotCol) will not be
// updated as they have already been eliminated.
static void eliminateFromConstraint(FlatAffineConstraints *constraints,
                                    unsigned rowIdx, unsigned pivotRow,
                                    unsigned pivotCol, unsigned elimColStart,
                                    bool isEq) {
  // Skip if equality 'rowIdx' if same as 'pivotRow'.
  if (isEq && rowIdx == pivotRow)
    return;
  auto at = [&](unsigned i, unsigned j) -> int64_t {
    return isEq ? constraints->atEq(i, j) : constraints->atIneq(i, j);
  };
  int64_t leadCoeff = at(rowIdx, pivotCol);
  // Skip if leading coefficient at 'rowIdx' is already zero.
  if (leadCoeff == 0)
    return;
  int64_t pivotCoeff = constraints->atEq(pivotRow, pivotCol);
  int64_t sign = (leadCoeff * pivotCoeff > 0) ? -1 : 1;
  int64_t lcm = mlir::lcm(pivotCoeff, leadCoeff);
  int64_t pivotMultiplier = sign * (lcm / std::abs(pivotCoeff));
  int64_t rowMultiplier = lcm / std::abs(leadCoeff);

  unsigned numCols = constraints->getNumCols();
  for (unsigned j = 0; j < numCols; ++j) {
    // Skip updating column 'j' if it was just eliminated.
    if (j >= elimColStart && j < pivotCol)
      continue;
    int64_t v = pivotMultiplier * constraints->atEq(pivotRow, j) +
                rowMultiplier * at(rowIdx, j);
    isEq ? constraints->atEq(rowIdx, j) = v
         : constraints->atIneq(rowIdx, j) = v;
  }
}

// Remove coefficients in column range [colStart, colLimit) in place.
// This removes in data in the specified column range, and copies any
// remaining valid data into place.
static void shiftColumnsToLeft(FlatAffineConstraints *constraints,
                               unsigned colStart, unsigned colLimit,
                               bool isEq) {
  assert(colStart >= 0 && colLimit <= constraints->getNumIds());
  if (colLimit <= colStart)
    return;

  unsigned numCols = constraints->getNumCols();
  unsigned numRows = isEq ? constraints->getNumEqualities()
                          : constraints->getNumInequalities();
  unsigned numToEliminate = colLimit - colStart;
  for (unsigned r = 0, e = numRows; r < e; ++r) {
    for (unsigned c = colLimit; c < numCols; ++c) {
      if (isEq) {
        constraints->atEq(r, c - numToEliminate) = constraints->atEq(r, c);
      } else {
        constraints->atIneq(r, c - numToEliminate) = constraints->atIneq(r, c);
      }
    }
  }
}

// Removes identifiers in column range [idStart, idLimit), and copies any
// remaining valid data into place, and updates member variables.
void FlatAffineConstraints::removeIdRange(unsigned idStart, unsigned idLimit) {
  assert(idLimit < getNumCols());
  // TODO(andydavis) Make 'removeIdRange' a lambda called from here.
  // Remove eliminated identifiers from equalities.
  shiftColumnsToLeft(this, idStart, idLimit, /*isEq=*/true);
  // Remove eliminated identifiers from inequalities.
  shiftColumnsToLeft(this, idStart, idLimit, /*isEq=*/false);
  // Update members numDims, numSymbols and numIds.
  unsigned numDimsEliminated = 0;
  if (idStart < numDims) {
    numDimsEliminated = std::min(numDims, idLimit) - idStart;
  }
  unsigned numColsEliminated = idLimit - idStart;
  unsigned numSymbolsEliminated =
      std::min(numSymbols, numColsEliminated - numDimsEliminated);
  numDims -= numDimsEliminated;
  numSymbols -= numSymbolsEliminated;
  numIds = numIds - numColsEliminated;
  ids.erase(ids.begin() + idStart, ids.begin() + idLimit);

  // No resize necessary. numReservedCols remains the same.
}

/// Returns the position of the identifier that has the minimum <number of lower
/// bounds> times <number of upper bounds> from the specified range of
/// identifiers [start, end). It is often best to eliminate in the increasing
/// order of these counts when doing Fourier-Motzkin elimination since FM adds
/// that many new constraints.
static unsigned getBestIdToEliminate(const FlatAffineConstraints &cst,
                                     unsigned start, unsigned end) {
  assert(start < cst.getNumIds() && end < cst.getNumIds() + 1);

  auto getProductOfNumLowerUpperBounds = [&](unsigned pos) {
    unsigned numLb = 0;
    unsigned numUb = 0;
    for (unsigned r = 0, e = cst.getNumInequalities(); r < e; r++) {
      if (cst.atIneq(r, pos) > 0) {
        ++numLb;
      } else if (cst.atIneq(r, pos) < 0) {
        ++numUb;
      }
    }
    return numLb * numUb;
  };

  unsigned minLoc = start;
  unsigned min = getProductOfNumLowerUpperBounds(start);
  for (unsigned c = start + 1; c < end; c++) {
    unsigned numLbUbProduct = getProductOfNumLowerUpperBounds(c);
    if (numLbUbProduct < min) {
      min = numLbUbProduct;
      minLoc = c;
    }
  }
  return minLoc;
}

// Checks for emptiness of the set by eliminating identifiers successively and
// using the GCD test (on all equality constraints) and checking for trivially
// invalid constraints. Returns 'true' if the constraint system is found to be
// empty; false otherwise.
bool FlatAffineConstraints::isEmpty() const {
  if (isEmptyByGCDTest() || hasInvalidConstraint())
    return true;

  // First, eliminate as many identifiers as possible using Gaussian
  // elimination.
  FlatAffineConstraints tmpCst(*this);
  unsigned currentPos = 0;
  while (currentPos < tmpCst.getNumIds()) {
    tmpCst.gaussianEliminateIds(currentPos, tmpCst.getNumIds());
    ++currentPos;
    // We check emptiness through trivial checks after eliminating each ID to
    // detect emptiness early. Since the checks isEmptyByGCDTest() and
    // hasInvalidConstraint() are linear time and single sweep on the constraint
    // buffer, this appears reasonable - but can optimize in the future.
    if (tmpCst.hasInvalidConstraint() || tmpCst.isEmptyByGCDTest())
      return true;
  }

  // Eliminate the remaining using FM.
  for (unsigned i = 0, e = tmpCst.getNumIds(); i < e; i++) {
    tmpCst.FourierMotzkinEliminate(
        getBestIdToEliminate(tmpCst, 0, tmpCst.getNumIds()));
    // FM wouldn't have modified the equalities in any way. So no need to again
    // run GCD test. Check for trivial invalid constraints.
    if (tmpCst.hasInvalidConstraint())
      return true;
  }
  return false;
}

// Runs the GCD test on all equality constraints. Returns 'true' if this test
// fails on any equality. Returns 'false' otherwise.
// This test can be used to disprove the existence of a solution. If it returns
// true, no integer solution to the equality constraints can exist.
//
// GCD test definition:
//
// The equality constraint:
//
//  c_1*x_1 + c_2*x_2 + ... + c_n*x_n = c_0
//
// has an integer solution iff:
//
//  GCD of c_1, c_2, ..., c_n divides c_0.
//
bool FlatAffineConstraints::isEmptyByGCDTest() const {
  assert(hasConsistentState());
  unsigned numCols = getNumCols();
  for (unsigned i = 0, e = getNumEqualities(); i < e; ++i) {
    uint64_t gcd = std::abs(atEq(i, 0));
    for (unsigned j = 1; j < numCols - 1; ++j) {
      gcd = llvm::GreatestCommonDivisor64(gcd, std::abs(atEq(i, j)));
    }
    int64_t v = std::abs(atEq(i, numCols - 1));
    if (gcd > 0 && (v % gcd != 0)) {
      return true;
    }
  }
  return false;
}

/// Tightens inequalities given that we are dealing with integer spaces. This is
/// analogous to the GCD test but applied to inequalities. The constant term can
/// be reduced to the preceding multiple of the GCD of the coefficients, i.e.,
///  64*i - 100 >= 0  =>  64*i - 128 >= 0 (since 'i' is an integer). This is a
/// fast method - linear in the number of coefficients.
// Example on how this affects practical cases: consider the scenario:
// 64*i >= 100, j = 64*i; without a tightening, elimination of i would yield
// j >= 100 instead of the tighter (exact) j >= 128.
void FlatAffineConstraints::GCDTightenInequalities() {
  unsigned numCols = getNumCols();
  for (unsigned i = 0, e = getNumInequalities(); i < e; ++i) {
    uint64_t gcd = std::abs(atIneq(i, 0));
    for (unsigned j = 1; j < numCols - 1; ++j) {
      gcd = llvm::GreatestCommonDivisor64(gcd, std::abs(atIneq(i, j)));
    }
    if (gcd > 0) {
      int64_t gcdI = static_cast<int64_t>(gcd);
      atIneq(i, numCols - 1) =
          gcdI * mlir::floorDiv(atIneq(i, numCols - 1), gcdI);
    }
  }
}

// Eliminates all identifer variables in column range [posStart, posLimit).
// Returns the number of variables eliminated.
unsigned FlatAffineConstraints::gaussianEliminateIds(unsigned posStart,
                                                     unsigned posLimit) {
  // Return if identifier positions to eliminate are out of range.
  assert(posLimit <= numIds);
  assert(hasConsistentState());

  if (posStart >= posLimit)
    return 0;

  GCDTightenInequalities();

  unsigned pivotCol = 0;
  for (pivotCol = posStart; pivotCol < posLimit; ++pivotCol) {
    // Find a row which has a non-zero coefficient in column 'j'.
    unsigned pivotRow;
    if (!findConstraintWithNonZeroAt(*this, pivotCol, /*isEq=*/true,
                                     &pivotRow)) {
      // No pivot row in equalities with non-zero at 'pivotCol'.
      if (!findConstraintWithNonZeroAt(*this, pivotCol, /*isEq=*/false,
                                       &pivotRow)) {
        // If inequalities are also non-zero in 'pivotCol', it can be
        // eliminated.
        continue;
      }
      break;
    }

    // Eliminate identifier at 'pivotCol' from each equality row.
    for (unsigned i = 0, e = getNumEqualities(); i < e; ++i) {
      eliminateFromConstraint(this, i, pivotRow, pivotCol, posStart,
                              /*isEq=*/true);
      normalizeConstraintByGCD</*isEq=*/true>(this, i);
    }

    // Eliminate identifier at 'pivotCol' from each inequality row.
    for (unsigned i = 0, e = getNumInequalities(); i < e; ++i) {
      eliminateFromConstraint(this, i, pivotRow, pivotCol, posStart,
                              /*isEq=*/false);
      normalizeConstraintByGCD</*isEq=*/false>(this, i);
    }
    removeEquality(pivotRow);
  }
  // Update position limit based on number eliminated.
  posLimit = pivotCol;
  // Remove eliminated columns from all constraints.
  removeIdRange(posStart, posLimit);
  return posLimit - posStart;
}

// Detect the identifier at 'pos' (say id_r) as modulo of another identifier
// (say id_n) w.r.t a constant. When this happens, another identifier (say id_q)
// could be detected as the floordiv of n. For eg:
// id_n - 4*id_q - id_r = 0, 0 <= id_r <= 3    <=>
//                          id_r = id_n mod 4, id_q = id_n floordiv 4.
// lbConst and ubConst are the constant lower and upper bounds for 'pos' -
// pre-detected at the caller.
static bool detectAsMod(const FlatAffineConstraints &cst, unsigned pos,
                        int64_t lbConst, int64_t ubConst,
                        SmallVectorImpl<AffineExpr> *memo) {
  assert(pos < cst.getNumIds() && "invalid position");

  // Check if 0 <= id_r <= divisor - 1 and if id_r is equal to
  // id_n - divisor * id_q. If these are true, then id_n becomes the dividend
  // and id_q the quotient when dividing id_n by the divisor.

  if (lbConst != 0 || ubConst < 1)
    return false;

  int64_t divisor = ubConst + 1;

  // Now check for: id_r =  id_n - divisor * id_q. As an example, we
  // are looking r = d - 4q, i.e., either r - d + 4q = 0 or -r + d - 4q = 0.
  unsigned seenQuotient = 0, seenDividend = 0;
  int quotientPos = -1, dividendPos = -1;
  for (unsigned r = 0, e = cst.getNumEqualities(); r < e; r++) {
    // id_n should have coeff 1 or -1.
    if (std::abs(cst.atEq(r, pos)) != 1)
      continue;
    for (unsigned c = 0, f = cst.getNumDimAndSymbolIds(); c < f; c++) {
      // The coeff of the quotient should be -divisor if the coefficient of
      // the pos^th identifier is -1, and divisor if the latter is -1.
      if (cst.atEq(r, c) * cst.atEq(r, pos) == divisor) {
        seenQuotient++;
        quotientPos = c;
      } else if (cst.atEq(r, c) * cst.atEq(r, pos) == -1) {
        seenDividend++;
        dividendPos = c;
      }
    }
    // We are looking for exactly one identifier as part of the dividend.
    // TODO(bondhugula): could be extended to cover multiple ones in the
    // dividend to detect mod of an affine function of identifiers.
    if (seenDividend == 1 && seenQuotient >= 1) {
      if (!(*memo)[dividendPos])
        return false;
      // Successfully detected a mod.
      (*memo)[pos] = (*memo)[dividendPos] % divisor;
      if (seenQuotient == 1 && !(*memo)[quotientPos])
        // Successfully detected a floordiv as well.
        (*memo)[quotientPos] = (*memo)[dividendPos].floorDiv(divisor);
      return true;
    }
  }
  return false;
}

// Check if the pos^th identifier can be expressed as a floordiv of an affine
// function of other identifiers (where the divisor is a positive constant).
// For eg: 4q <= i + j <= 4q + 3   <=>   q = (i + j) floordiv 4.
bool detectAsFloorDiv(const FlatAffineConstraints &cst, unsigned pos,
                      SmallVectorImpl<AffineExpr> *memo, MLIRContext *context) {
  assert(pos < cst.getNumIds() && "invalid position");
  SmallVector<unsigned, 4> lbIndices, ubIndices;

  // Gather all lower bounds and upper bound constraints of this identifier.
  // Since the canonical form c_1*x_1 + c_2*x_2 + ... + c_0 >= 0, a constraint
  // is a lower bound for x_i if c_i >= 1, and an upper bound if c_i <= -1.
  for (unsigned r = 0, e = cst.getNumInequalities(); r < e; r++) {
    if (cst.atIneq(r, pos) >= 1)
      // Lower bound.
      lbIndices.push_back(r);
    else if (cst.atIneq(r, pos) <= -1)
      // Upper bound.
      ubIndices.push_back(r);
  }

  // Check if any lower bound, upper bound pair is of the form:
  // divisor * id >=  expr - (divisor - 1)    <-- Lower bound for 'id'
  // divisor * id <=  expr                    <-- Upper bound for 'id'
  // Then, 'id' is equivalent to 'expr floordiv divisor'.  (where divisor > 1).
  //
  // For example, if -32*k + 16*i + j >= 0
  //                  32*k - 16*i - j + 31 >= 0   <=>
  //             k = ( 16*i + j ) floordiv 32
  unsigned seenDividends = 0;
  for (auto ubPos : ubIndices) {
    for (auto lbPos : lbIndices) {
      // Check if lower bound's constant term is 'divisor - 1'. The 'divisor'
      // here is cst.atIneq(lbPos, pos) and we already know that it's positive
      // (since cst.Ineq(lbPos, ...) is a lower bound expression for 'pos'.
      if (cst.atIneq(lbPos, cst.getNumCols() - 1) != cst.atIneq(lbPos, pos) - 1)
        continue;
      // Check if upper bound's constant term is 0.
      if (cst.atIneq(ubPos, cst.getNumCols() - 1) != 0)
        continue;
      // For the remaining part, check if the lower bound expr's coeff's are
      // negations of corresponding upper bound ones'.
      unsigned c, f;
      for (c = 0, f = cst.getNumCols() - 1; c < f; c++) {
        if (cst.atIneq(lbPos, c) != -cst.atIneq(ubPos, c))
          break;
        if (c != pos && cst.atIneq(lbPos, c) != 0)
          seenDividends++;
      }
      // Lb coeff's aren't negative of ub coeff's (for the non constant term
      // part).
      if (c < f)
        continue;
      if (seenDividends >= 1) {
        // The divisor is the constant term of the lower bound expression.
        // We already know that cst.atIneq(lbPos, pos) > 0.
        int64_t divisor = cst.atIneq(lbPos, pos);
        // Construct the dividend expression.
        auto dividendExpr = getAffineConstantExpr(0, context);
        unsigned c, f;
        for (c = 0, f = cst.getNumCols() - 1; c < f; c++) {
          if (c == pos)
            continue;
          int64_t ubVal = cst.atIneq(ubPos, c);
          if (ubVal == 0)
            continue;
          if (!(*memo)[c])
            break;
          dividendExpr = dividendExpr + ubVal * (*memo)[c];
        }
        // Expression can't be constructed as it depends on a yet unknown
        // identifier.
        // TODO(mlir-team): Visit/compute the identifiers in an order so that
        // this doesn't happen. More complex but much more efficient.
        if (c < f)
          continue;
        // Successfully detected the floordiv.
        (*memo)[pos] = dividendExpr.floorDiv(divisor);
        return true;
      }
    }
  }
  return false;
}

/// Computes the lower and upper bounds of the first 'num' dimensional
/// identifiers as affine maps of the remaining identifiers (dimensional and
/// symbolic identifiers). Local identifiers are themselves explicitly computed
/// as affine functions of other identifiers in this process if needed.
void FlatAffineConstraints::getSliceBounds(unsigned num, MLIRContext *context,
                                           SmallVectorImpl<AffineMap> *lbMaps,
                                           SmallVectorImpl<AffineMap> *ubMaps) {
  assert(num < getNumDimIds() && "invalid range");

  // Basic simplification.
  normalizeConstraintsByGCD();

  LLVM_DEBUG(llvm::dbgs() << "getSliceBounds on:\n");
  LLVM_DEBUG(dump());

  // Record computed/detected identifiers.
  SmallVector<AffineExpr, 8> memo(getNumIds(), AffineExpr::Null());
  // Initialize dimensional and symbolic identifiers.
  for (unsigned i = num, e = getNumDimIds(); i < e; i++)
    memo[i] = getAffineDimExpr(i - num, context);
  for (unsigned i = getNumDimIds(), e = getNumDimAndSymbolIds(); i < e; i++)
    memo[i] = getAffineSymbolExpr(i - getNumDimIds(), context);

  bool changed;
  do {
    changed = false;
    // Identify yet unknown identifiers as constants or mod's / floordiv's of
    // other identifiers if possible.
    for (unsigned pos = 0; pos < getNumIds(); pos++) {
      if (memo[pos])
        continue;

      auto lbConst = getConstantLowerBound(pos);
      auto ubConst = getConstantUpperBound(pos);
      if (lbConst.hasValue() && ubConst.hasValue()) {
        // Detect equality to a constant.
        if (lbConst.getValue() == ubConst.getValue()) {
          memo[pos] = getAffineConstantExpr(lbConst.getValue(), context);
          changed = true;
          continue;
        }

        // Detect an identifier as modulo of another identifier w.r.t a
        // constant.
        if (detectAsMod(*this, pos, lbConst.getValue(), ubConst.getValue(),
                        &memo)) {
          changed = true;
          continue;
        }
      }

      // Detect an identifier as floordiv of another identifier w.r.t a
      // constant.
      if (detectAsFloorDiv(*this, pos, &memo, context)) {
        changed = true;
        continue;
      }

      // Detect an identifier as an expression of other identifiers.
      unsigned idx;
      if (!findConstraintWithNonZeroAt(*this, pos, /*isEq=*/true, &idx)) {
        continue;
      }

      // Build AffineExpr solving for identifier 'pos' in terms of all others.
      auto expr = getAffineConstantExpr(0, context);
      unsigned j, e;
      for (j = 0, e = getNumIds(); j < e; ++j) {
        if (j == pos)
          continue;
        int64_t c = atEq(idx, j);
        if (c == 0)
          continue;
        // If any of the involved IDs hasn't been found yet, we can't proceed.
        if (!memo[j])
          break;
        expr = expr + memo[j] * c;
      }
      if (j < e)
        // Can't construct expression as it depends on a yet uncomputed
        // identifier.
        continue;

      // Add constant term to AffineExpr.
      expr = expr + atEq(idx, getNumIds());
      int64_t vPos = atEq(idx, pos);
      assert(vPos != 0 && "expected non-zero here");
      if (vPos > 0)
        expr = (-expr).floorDiv(vPos);
      else
        // vPos < 0.
        expr = expr.floorDiv(-vPos);
      // Successfully constructed expression.
      memo[pos] = expr;
      changed = true;
    }
    // This loop is guaranteed to reach a fixed point - since once an
    // identifier's explicit form is computed (in memo[pos]), it's not updated
    // again.
  } while (changed);

  // Set the lower and upper bound maps for all the identifiers that were
  // computed as affine expressions of the rest as the "detected expr" and
  // "detected expr + 1" respectively; set the undetected ones to Null().
  for (unsigned pos = 0; pos < num; pos++) {
    unsigned numMapDims = getNumDimIds() - num;
    unsigned numMapSymbols = getNumSymbolIds();
    AffineExpr expr = memo[pos];
    if (expr)
      expr = simplifyAffineExpr(expr, numMapDims, numMapSymbols);

    if (expr) {
      (*lbMaps)[pos] = AffineMap::get(numMapDims, numMapSymbols, expr, {});
      (*ubMaps)[pos] = AffineMap::get(numMapDims, numMapSymbols, expr + 1, {});
    } else {
      (*lbMaps)[pos] = AffineMap::Null();
      (*ubMaps)[pos] = AffineMap::Null();
    }
    LLVM_DEBUG(llvm::dbgs() << "lb map for pos = " << Twine(pos) << ", expr: ");
    LLVM_DEBUG(expr.dump(););
  }
}

void FlatAffineConstraints::addEquality(ArrayRef<int64_t> eq) {
  assert(eq.size() == getNumCols());
  unsigned offset = equalities.size();
  equalities.resize(equalities.size() + numReservedCols);
  std::copy(eq.begin(), eq.end(), equalities.begin() + offset);
}

void FlatAffineConstraints::addInequality(ArrayRef<int64_t> inEq) {
  assert(inEq.size() == getNumCols());
  unsigned offset = inequalities.size();
  inequalities.resize(inequalities.size() + numReservedCols);
  std::copy(inEq.begin(), inEq.end(), inequalities.begin() + offset);
}

void FlatAffineConstraints::addConstantLowerBound(unsigned pos, int64_t lb) {
  assert(pos < getNumCols());
  unsigned offset = inequalities.size();
  inequalities.resize(inequalities.size() + numReservedCols);
  std::fill(inequalities.begin() + offset,
            inequalities.begin() + offset + getNumCols(), 0);
  inequalities[offset + pos] = 1;
  inequalities[offset + getNumCols() - 1] = -lb;
}

void FlatAffineConstraints::addConstantUpperBound(unsigned pos, int64_t ub) {
  assert(pos < getNumCols());
  unsigned offset = inequalities.size();
  inequalities.resize(inequalities.size() + numReservedCols);
  std::fill(inequalities.begin() + offset,
            inequalities.begin() + offset + getNumCols(), 0);
  inequalities[offset + pos] = -1;
  inequalities[offset + getNumCols() - 1] = ub;
}

void FlatAffineConstraints::addConstantLowerBound(ArrayRef<int64_t> expr,
                                                  int64_t lb) {
  assert(expr.size() == getNumCols());
  unsigned offset = inequalities.size();
  inequalities.resize(inequalities.size() + numReservedCols);
  std::fill(inequalities.begin() + offset,
            inequalities.begin() + offset + getNumCols(), 0);
  std::copy(expr.begin(), expr.end(), inequalities.begin() + offset);
  inequalities[offset + getNumCols() - 1] += -lb;
}

void FlatAffineConstraints::addConstantUpperBound(ArrayRef<int64_t> expr,
                                                  int64_t ub) {
  assert(expr.size() == getNumCols());
  unsigned offset = inequalities.size();
  inequalities.resize(inequalities.size() + numReservedCols);
  std::fill(inequalities.begin() + offset,
            inequalities.begin() + offset + getNumCols(), 0);
  for (unsigned i = 0, e = getNumCols(); i < e; i++) {
    inequalities[offset + i] = -expr[i];
  }
  inequalities[offset + getNumCols() - 1] += ub;
}

void FlatAffineConstraints::addLowerBound(ArrayRef<int64_t> expr,
                                          ArrayRef<int64_t> lb) {
  assert(expr.size() == getNumCols());
  assert(lb.size() == getNumCols());
  unsigned offset = inequalities.size();
  inequalities.resize(inequalities.size() + numReservedCols);
  std::fill(inequalities.begin() + offset,
            inequalities.begin() + offset + getNumCols(), 0);
  for (unsigned i = 0, e = getNumCols(); i < e; i++) {
    inequalities[offset + i] = expr[i] - lb[i];
  }
}

void FlatAffineConstraints::addUpperBound(ArrayRef<int64_t> expr,
                                          ArrayRef<int64_t> ub) {
  assert(expr.size() == getNumCols());
  assert(ub.size() == getNumCols());
  unsigned offset = inequalities.size();
  inequalities.resize(inequalities.size() + numReservedCols);
  std::fill(inequalities.begin() + offset,
            inequalities.begin() + offset + getNumCols(), 0);
  for (unsigned i = 0, e = getNumCols(); i < e; i++) {
    inequalities[offset + i] = ub[i] - expr[i];
  }
}

bool FlatAffineConstraints::findId(const Value &id, unsigned *pos) const {
  unsigned i = 0;
  for (const auto &mayBeId : ids) {
    if (mayBeId.hasValue() && mayBeId.getValue() == &id) {
      *pos = i;
      return true;
    }
    i++;
  }
  return false;
}

void FlatAffineConstraints::setDimSymbolSeparation(unsigned newSymbolCount) {
  assert(newSymbolCount <= numDims + numSymbols &&
         "invalid separation position");
  numDims = numDims + numSymbols - newSymbolCount;
  numSymbols = newSymbolCount;
}

bool FlatAffineConstraints::addForInstDomain(const ForInst &forInst) {
  unsigned pos;
  // Pre-condition for this method.
  if (!findId(forInst, &pos)) {
    assert(0 && "Value not found");
    return false;
  }

  if (forInst.getStep() != 1)
    LLVM_DEBUG(llvm::dbgs()
               << "Domain conservative: non-unit stride not handled\n");

  // Adds a lower or upper bound when the bounds aren't constant.
  auto addLowerOrUpperBound = [&](bool lower) -> bool {
    auto operands = lower ? forInst.getLowerBoundOperands()
                          : forInst.getUpperBoundOperands();
    for (const auto &operand : operands) {
      unsigned loc;
      if (!findId(*operand, &loc)) {
        if (operand->isValidSymbol()) {
          addSymbolId(getNumSymbolIds(), const_cast<Value *>(operand));
          loc = getNumDimIds() + getNumSymbolIds() - 1;
          // Check if the symbol is a constant.
          if (auto *opInst = operand->getDefiningInst()) {
            if (auto constOp = opInst->dyn_cast<ConstantIndexOp>()) {
              setIdToConstant(*operand, constOp->getValue());
            }
          }
        } else {
          addDimId(getNumDimIds(), const_cast<Value *>(operand));
          loc = getNumDimIds() - 1;
        }
      }
    }
    // Record positions of the operands in the constraint system.
    SmallVector<unsigned, 8> positions;
    for (const auto &operand : operands) {
      unsigned loc;
      if (!findId(*operand, &loc))
        assert(0 && "expected to be found");
      positions.push_back(loc);
    }

    auto boundMap =
        lower ? forInst.getLowerBoundMap() : forInst.getUpperBoundMap();

    FlatAffineConstraints localVarCst;
    std::vector<SmallVector<int64_t, 8>> flatExprs;
    if (!getFlattenedAffineExprs(boundMap, &flatExprs, &localVarCst)) {
      LLVM_DEBUG(llvm::dbgs() << "semi-affine expressions not yet supported\n");
      return false;
    }
    if (localVarCst.getNumLocalIds() > 0) {
      LLVM_DEBUG(llvm::dbgs()
                 << "loop bounds with mod/floordiv expr's not yet supported\n");
      return false;
    }

    for (const auto &flatExpr : flatExprs) {
      SmallVector<int64_t, 4> ineq(getNumCols(), 0);
      ineq[pos] = lower ? 1 : -1;
      for (unsigned j = 0, e = boundMap.getNumInputs(); j < e; j++) {
        ineq[positions[j]] = lower ? -flatExpr[j] : flatExpr[j];
      }
      // Constant term.
      ineq[getNumCols() - 1] =
          lower ? -flatExpr[flatExpr.size() - 1]
                // Upper bound in flattenedExpr is an exclusive one.
                : flatExpr[flatExpr.size() - 1] - 1;
      addInequality(ineq);
    }
    return true;
  };

  if (forInst.hasConstantLowerBound()) {
    addConstantLowerBound(pos, forInst.getConstantLowerBound());
  } else {
    // Non-constant lower bound case.
    if (!addLowerOrUpperBound(/*lower=*/true))
      return false;
  }

  if (forInst.hasConstantUpperBound()) {
    addConstantUpperBound(pos, forInst.getConstantUpperBound() - 1);
    return true;
  }
  // Non-constant upper bound case.
  return addLowerOrUpperBound(/*lower=*/false);
}

/// Sets the specified identifer to a constant value.
void FlatAffineConstraints::setIdToConstant(unsigned pos, int64_t val) {
  unsigned offset = equalities.size();
  equalities.resize(equalities.size() + numReservedCols);
  std::fill(equalities.begin() + offset,
            equalities.begin() + offset + getNumCols(), 0);
  equalities[offset + pos] = 1;
  equalities[offset + getNumCols() - 1] = -val;
}

/// Sets the specified identifer to a constant value; asserts if the id is not
/// found.
void FlatAffineConstraints::setIdToConstant(const Value &id, int64_t val) {
  unsigned pos;
  if (!findId(id, &pos))
    // This is a pre-condition for this method.
    assert(0 && "id not found");
  setIdToConstant(pos, val);
}

void FlatAffineConstraints::removeEquality(unsigned pos) {
  unsigned numEqualities = getNumEqualities();
  assert(pos < numEqualities);
  unsigned outputIndex = pos * numReservedCols;
  unsigned inputIndex = (pos + 1) * numReservedCols;
  unsigned numElemsToCopy = (numEqualities - pos - 1) * numReservedCols;
  std::copy(equalities.begin() + inputIndex,
            equalities.begin() + inputIndex + numElemsToCopy,
            equalities.begin() + outputIndex);
  equalities.resize(equalities.size() - numReservedCols);
}

/// Finds an equality that equates the specified identifier to a constant.
/// Returns the position of the equality row. If 'symbolic' is set to true,
/// symbols are also treated like a constant, i.e., an affine function of the
/// symbols is also treated like a constant.
static int findEqualityToConstant(const FlatAffineConstraints &cst,
                                  unsigned pos, bool symbolic = false) {
  assert(pos < cst.getNumIds() && "invalid position");
  for (unsigned r = 0, e = cst.getNumEqualities(); r < e; r++) {
    int64_t v = cst.atEq(r, pos);
    if (v * v != 1)
      continue;
    unsigned c;
    unsigned f = symbolic ? cst.getNumDimIds() : cst.getNumIds();
    // This checks for zeros in all positions other than 'pos' in [0, f)
    for (c = 0; c < f; c++) {
      if (c == pos)
        continue;
      if (cst.atEq(r, c) != 0) {
        // Dependent on another identifier.
        break;
      }
    }
    if (c == f)
      // Equality is free of other identifiers.
      return r;
  }
  return -1;
}

void FlatAffineConstraints::setAndEliminate(unsigned pos, int64_t constVal) {
  assert(pos < getNumIds() && "invalid position");
  for (unsigned r = 0, e = getNumInequalities(); r < e; r++) {
    atIneq(r, getNumCols() - 1) += atIneq(r, pos) * constVal;
  }
  for (unsigned r = 0, e = getNumEqualities(); r < e; r++) {
    atEq(r, getNumCols() - 1) += atEq(r, pos) * constVal;
  }
  removeId(pos);
}

bool FlatAffineConstraints::constantFoldId(unsigned pos) {
  assert(pos < getNumIds() && "invalid position");
  int rowIdx;
  if ((rowIdx = findEqualityToConstant(*this, pos)) == -1)
    return false;

  // atEq(rowIdx, pos) is either -1 or 1.
  assert(atEq(rowIdx, pos) * atEq(rowIdx, pos) == 1);
  int64_t constVal = -atEq(rowIdx, getNumCols() - 1) / atEq(rowIdx, pos);
  setAndEliminate(pos, constVal);
  return true;
}

void FlatAffineConstraints::constantFoldIdRange(unsigned pos, unsigned num) {
  for (unsigned s = pos, t = pos, e = pos + num; s < e; s++) {
    if (!constantFoldId(t))
      t++;
  }
}

/// Returns the extent (upper bound - lower bound) of the specified
/// identifier if it is found to be a constant; returns None if it's not a
/// constant. This methods treats symbolic identifiers specially, i.e.,
/// it looks for constant differences between affine expressions involving
/// only the symbolic identifiers. See comments at function definition for
/// example. 'lb', if provided, is set to the lower bound associated with the
/// constant difference. Note that 'lb' is purely symbolic and thus will contain
/// the coefficients of the symbolic identifiers and the constant coefficient.
//  Egs: 0 <= i <= 15, return 16.
//       s0 + 2 <= i <= s0 + 17, returns 16. (s0 has to be a symbol)
//       i + s0 + 16 <= d0 <= i + s0  + 31, returns 16.
Optional<int64_t> FlatAffineConstraints::getConstantBoundOnDimSize(
    unsigned pos, SmallVectorImpl<int64_t> *lb) const {
  assert(pos < getNumDimIds() && "Invalid identifier position");
  assert(getNumLocalIds() == 0);

  // TODO(bondhugula): eliminate all remaining dimensional identifiers (other
  // than the one at 'pos' to make this more powerful. Not needed for
  // hyper-rectangular spaces.

  // Find an equality for 'pos'^th identifier that equates it to some function
  // of the symbolic identifiers (+ constant).
  int eqRow = findEqualityToConstant(*this, pos, /*symbolic=*/true);
  if (eqRow != -1) {
    // This identifier can only take a single value.
    if (lb) {
      // Set lb to the symbolic value.
      lb->resize(getNumSymbolIds() + 1);
      for (unsigned c = 0, f = getNumSymbolIds() + 1; c < f; c++) {
        int64_t v = atEq(eqRow, pos);
        // atEq(eqRow, pos) is either -1 or 1.
        assert(v * v == 1);
        (*lb)[c] = v < 0 ? atEq(eqRow, getNumDimIds() + c) / -v
                         : -atEq(eqRow, getNumDimIds() + c) / v;
      }
    }
    return 1;
  }

  // Check if the identifier appears at all in any of the inequalities.
  unsigned r, e;
  for (r = 0, e = getNumInequalities(); r < e; r++) {
    if (atIneq(r, pos) != 0)
      break;
  }
  if (r == e)
    // If it doesn't, there isn't a bound on it.
    return None;

  // Positions of constraints that are lower/upper bounds on the variable.
  SmallVector<unsigned, 4> lbIndices, ubIndices;

  // Gather all symbolic lower bounds and upper bounds of the variable. Since
  // the canonical form c_1*x_1 + c_2*x_2 + ... + c_0 >= 0, a constraint is a
  // lower bound for x_i if c_i >= 1, and an upper bound if c_i <= -1.
  for (unsigned r = 0, e = getNumInequalities(); r < e; r++) {
    unsigned c, f;
    for (c = 0, f = getNumDimIds(); c < f; c++) {
      if (c != pos && atIneq(r, c) != 0)
        break;
    }
    if (c < getNumDimIds())
      continue;
    if (atIneq(r, pos) >= 1)
      // Lower bound.
      lbIndices.push_back(r);
    else if (atIneq(r, pos) <= -1)
      // Upper bound.
      ubIndices.push_back(r);
  }

  // TODO(bondhugula): eliminate other dimensional identifiers to make this more
  // powerful. Not needed for hyper-rectangular iteration spaces.

  Optional<int64_t> minDiff = None;
  unsigned minLbPosition;
  for (auto ubPos : ubIndices) {
    for (auto lbPos : lbIndices) {
      // Look for a lower bound and an upper bound that only differ by a
      // constant, i.e., pairs of the form  0 <= c_pos - f(c_i's) <= diffConst.
      // For example, if ii is the pos^th variable, we are looking for
      // constraints like ii >= i, ii <= ii + 50, 50 being the difference. The
      // minimum among all such constant differences is kept since that's the
      // constant bounding the extent of the pos^th variable.
      unsigned j, e;
      for (j = 0, e = getNumCols() - 1; j < e; j++)
        if (atIneq(ubPos, j) != -atIneq(lbPos, j)) {
          break;
        }
      if (j < getNumCols() - 1)
        continue;
      int64_t diff =
          atIneq(ubPos, getNumCols() - 1) + atIneq(lbPos, getNumCols() - 1) + 1;
      if (minDiff == None || diff < minDiff) {
        minDiff = diff;
        minLbPosition = lbPos;
      }
    }
  }
  if (lb && minDiff.hasValue()) {
    // Set lb to the symbolic lower bound.
    lb->resize(getNumSymbolIds() + 1);
    for (unsigned c = 0, e = getNumSymbolIds() + 1; c < e; c++) {
      (*lb)[c] = -atIneq(minLbPosition, getNumDimIds() + c);
    }
  }
  return minDiff;
}

template <bool isLower>
Optional<int64_t>
FlatAffineConstraints::getConstantLowerOrUpperBound(unsigned pos) const {
  // Check if there's an equality equating the 'pos'^th identifier to a
  // constant.
  int eqRowIdx = findEqualityToConstant(*this, pos, /*symbolic=*/false);
  if (eqRowIdx != -1)
    // atEq(rowIdx, pos) is either -1 or 1.
    return -atEq(eqRowIdx, getNumCols() - 1) / atEq(eqRowIdx, pos);

  // Check if the identifier appears at all in any of the inequalities.
  unsigned r, e;
  for (r = 0, e = getNumInequalities(); r < e; r++) {
    if (atIneq(r, pos) != 0)
      break;
  }
  if (r == e)
    // If it doesn't, there isn't a bound on it.
    return None;

  Optional<int64_t> minOrMaxConst = None;

  // Take the max across all const lower bounds (or min across all constant
  // upper bounds).
  for (unsigned r = 0, e = getNumInequalities(); r < e; r++) {
    if (isLower) {
      if (atIneq(r, pos) <= 0)
        // Not a lower bound.
        continue;
    } else if (atIneq(r, pos) >= 0) {
      // Not an upper bound.
      continue;
    }
    unsigned c, f;
    for (c = 0, f = getNumCols() - 1; c < f; c++)
      if (c != pos && atIneq(r, c) != 0)
        break;
    if (c < getNumCols() - 1)
      // Not a constant bound.
      continue;

    int64_t boundConst =
        isLower ? mlir::ceilDiv(-atIneq(r, getNumCols() - 1), atIneq(r, pos))
                : mlir::floorDiv(atIneq(r, getNumCols() - 1), -atIneq(r, pos));
    if (isLower) {
      if (minOrMaxConst == None || boundConst > minOrMaxConst)
        minOrMaxConst = boundConst;
    } else {
      if (minOrMaxConst == None || boundConst < minOrMaxConst)
        minOrMaxConst = boundConst;
    }
  }
  return minOrMaxConst;
}

Optional<int64_t>
FlatAffineConstraints::getConstantLowerBound(unsigned pos) const {
  return getConstantLowerOrUpperBound</*isLower=*/true>(pos);
}

Optional<int64_t>
FlatAffineConstraints::getConstantUpperBound(unsigned pos) const {
  return getConstantLowerOrUpperBound</*isLower=*/false>(pos);
}

// A simple (naive and conservative) check for hyper-rectangularlity.
bool FlatAffineConstraints::isHyperRectangular(unsigned pos,
                                               unsigned num) const {
  assert(pos < getNumCols() - 1);
  // Check for two non-zero coefficients in the range [pos, pos + sum).
  for (unsigned r = 0, e = getNumInequalities(); r < e; r++) {
    unsigned sum = 0;
    for (unsigned c = pos; c < pos + num; c++) {
      if (atIneq(r, c) != 0)
        sum++;
    }
    if (sum > 1)
      return false;
  }
  for (unsigned r = 0, e = getNumEqualities(); r < e; r++) {
    unsigned sum = 0;
    for (unsigned c = pos; c < pos + num; c++) {
      if (atEq(r, c) != 0)
        sum++;
    }
    if (sum > 1)
      return false;
  }
  return true;
}

void FlatAffineConstraints::print(raw_ostream &os) const {
  assert(hasConsistentState());
  os << "\nConstraints (" << getNumDimIds() << " dims, " << getNumSymbolIds()
     << " symbols, " << getNumLocalIds() << " locals), (" << getNumConstraints()
     << " constraints)\n";
  os << "(";
  for (unsigned i = 0, e = getNumIds(); i < e; i++) {
    if (ids[i] == None)
      os << "None ";
    else
      os << "Value ";
  }
  os << " const)\n";
  for (unsigned i = 0, e = getNumEqualities(); i < e; ++i) {
    for (unsigned j = 0, f = getNumCols(); j < f; ++j) {
      os << atEq(i, j) << " ";
    }
    os << "= 0\n";
  }
  for (unsigned i = 0, e = getNumInequalities(); i < e; ++i) {
    for (unsigned j = 0, f = getNumCols(); j < f; ++j) {
      os << atIneq(i, j) << " ";
    }
    os << ">= 0\n";
  }
  os << '\n';
}

void FlatAffineConstraints::dump() const { print(llvm::errs()); }

/// Removes duplicate constraints and trivially true constraints: a constraint
/// of the form <non-negative constant> >= 0 is considered a trivially true
/// constraint.
//  Uses a DenseSet to hash and detect duplicates followed by a linear scan to
//  remove duplicates in place.
void FlatAffineConstraints::removeTrivialRedundancy() {
  DenseSet<ArrayRef<int64_t>> rowSet;

  // Check if constraint is of the form <non-negative-constant> >= 0.
  auto isTriviallyValid = [&](unsigned r) -> bool {
    for (unsigned c = 0, e = getNumCols() - 1; c < e; c++) {
      if (atIneq(r, c) != 0)
        return false;
    }
    return atIneq(r, getNumCols() - 1) >= 0;
  };

  // Detect and mark redundant constraints.
  std::vector<bool> redunIneq(getNumInequalities(), false);
  for (unsigned r = 0, e = getNumInequalities(); r < e; r++) {
    int64_t *rowStart = inequalities.data() + numReservedCols * r;
    auto row = ArrayRef<int64_t>(rowStart, getNumCols());
    if (isTriviallyValid(r) || !rowSet.insert(row).second) {
      redunIneq[r] = true;
    }
  }

  auto copyRow = [&](unsigned src, unsigned dest) {
    if (src == dest)
      return;
    for (unsigned c = 0, e = getNumCols(); c < e; c++) {
      atIneq(dest, c) = atIneq(src, c);
    }
  };

  // Scan to get rid of all rows marked redundant, in-place.
  unsigned pos = 0;
  for (unsigned r = 0, e = getNumInequalities(); r < e; r++) {
    if (!redunIneq[r])
      copyRow(r, pos++);
  }
  inequalities.resize(numReservedCols * pos);

  // TODO(bondhugula): consider doing this for equalities as well, but probably
  // not worth the savings.
}

void FlatAffineConstraints::clearAndCopyFrom(
    const FlatAffineConstraints &other) {
  FlatAffineConstraints copy(other);
  std::swap(*this, copy);
  assert(copy.getNumIds() == copy.getIds().size());
}

void FlatAffineConstraints::removeId(unsigned pos) {
  removeIdRange(pos, pos + 1);
}

static std::pair<unsigned, unsigned>
getNewNumDimsSymbols(unsigned pos, const FlatAffineConstraints &cst) {
  unsigned numDims = cst.getNumDimIds();
  unsigned numSymbols = cst.getNumSymbolIds();
  unsigned newNumDims, newNumSymbols;
  if (pos < numDims) {
    newNumDims = numDims - 1;
    newNumSymbols = numSymbols;
  } else if (pos < numDims + numSymbols) {
    assert(numSymbols >= 1);
    newNumDims = numDims;
    newNumSymbols = numSymbols - 1;
  } else {
    newNumDims = numDims;
    newNumSymbols = numSymbols;
  }
  return {newNumDims, newNumSymbols};
}

/// Eliminates identifier at the specified position using Fourier-Motzkin
/// variable elimination. This technique is exact for rational spaces but
/// conservative (in "rare" cases) for integer spaces. The operation corresponds
/// to a projection operation yielding the (convex) set of integer points
/// contained in the rational shadow of the set. An emptiness test that relies
/// on this method will guarantee emptiness, i.e., it disproves the existence of
/// a solution if it says it's empty.
/// If a non-null isResultIntegerExact is passed, it is set to true if the
/// result is also integer exact. If it's set to false, the obtained solution
/// *may* not be exact, i.e., it may contain integer points that do not have an
/// integer pre-image in the original set.
///
/// Eg:
/// j >= 0, j <= i + 1
/// i >= 0, i <= N + 1
/// Eliminating i yields,
///   j >= 0, 0 <= N + 1, j - 1 <= N + 1
///
/// If darkShadow = true, this method computes the dark shadow on elimination;
/// the dark shadow is a convex integer subset of the exact integer shadow. A
/// non-empty dark shadow proves the existence of an integer solution. The
/// elimination in such a case could however be an under-approximation, and thus
/// should not be used for scanning sets or used by itself for dependence
/// checking.
///
/// Eg: 2-d set, * represents grid points, 'o' represents a point in the set.
///            ^
///            |
///            | * * * * o o
///         i  | * * o o o o
///            | o * * * * *
///            --------------->
///                 j ->
///
/// Eliminating i from this system (projecting on the j dimension):
/// rational shadow / integer light shadow:  1 <= j <= 6
/// dark shadow:                             3 <= j <= 6
/// exact integer shadow:                    j = 1 \union  3 <= j <= 6
/// holes/splinters:                         j = 2
///
/// darkShadow = false, isResultIntegerExact = nullptr are default values.
// TODO(bondhugula): a slight modification to yield dark shadow version of FM
// (tightened), which can prove the existence of a solution if there is one.
void FlatAffineConstraints::FourierMotzkinEliminate(
    unsigned pos, bool darkShadow, bool *isResultIntegerExact) {
  LLVM_DEBUG(llvm::dbgs() << "FM input (eliminate pos " << pos << "):\n");
  LLVM_DEBUG(dump());
  assert(pos < getNumIds() && "invalid position");
  assert(hasConsistentState());

  // Check if this identifier can be eliminated through a substitution.
  for (unsigned r = 0, e = getNumEqualities(); r < e; r++) {
    if (atEq(r, pos) != 0) {
      // Use Gaussian elimination here (since we have an equality).
      bool ret = gaussianEliminateId(pos);
      (void)ret;
      assert(ret && "Gaussian elimination guaranteed to succeed");
      LLVM_DEBUG(llvm::dbgs() << "FM output:\n");
      LLVM_DEBUG(dump());
      return;
    }
  }

  // A fast linear time tightening.
  GCDTightenInequalities();

  // Check if the identifier appears at all in any of the inequalities.
  unsigned r, e;
  for (r = 0, e = getNumInequalities(); r < e; r++) {
    if (atIneq(r, pos) != 0)
      break;
  }
  if (r == getNumInequalities()) {
    // If it doesn't appear, just remove the column and return.
    // TODO(andydavis,bondhugula): refactor removeColumns to use it from here.
    removeId(pos);
    LLVM_DEBUG(llvm::dbgs() << "FM output:\n");
    LLVM_DEBUG(dump());
    return;
  }

  // Positions of constraints that are lower bounds on the variable.
  SmallVector<unsigned, 4> lbIndices;
  // Positions of constraints that are lower bounds on the variable.
  SmallVector<unsigned, 4> ubIndices;
  // Positions of constraints that do not involve the variable.
  std::vector<unsigned> nbIndices;
  nbIndices.reserve(getNumInequalities());

  // Gather all lower bounds and upper bounds of the variable. Since the
  // canonical form c_1*x_1 + c_2*x_2 + ... + c_0 >= 0, a constraint is a lower
  // bound for x_i if c_i >= 1, and an upper bound if c_i <= -1.
  for (unsigned r = 0, e = getNumInequalities(); r < e; r++) {
    if (atIneq(r, pos) == 0) {
      // Id does not appear in bound.
      nbIndices.push_back(r);
    } else if (atIneq(r, pos) >= 1) {
      // Lower bound.
      lbIndices.push_back(r);
    } else {
      // Upper bound.
      ubIndices.push_back(r);
    }
  }

  // Set the number of dimensions, symbols in the resulting system.
  const auto &dimsSymbols = getNewNumDimsSymbols(pos, *this);
  unsigned newNumDims = dimsSymbols.first;
  unsigned newNumSymbols = dimsSymbols.second;

  SmallVector<Optional<Value *>, 8> newIds;
  newIds.reserve(numIds - 1);
  newIds.append(ids.begin(), ids.begin() + pos);
  newIds.append(ids.begin() + pos + 1, ids.end());

  /// Create the new system which has one identifier less.
  FlatAffineConstraints newFac(
      lbIndices.size() * ubIndices.size() + nbIndices.size(),
      getNumEqualities(), getNumCols() - 1, newNumDims, newNumSymbols,
      /*numLocals=*/getNumIds() - 1 - newNumDims - newNumSymbols, newIds);

  assert(newFac.getIds().size() == newFac.getNumIds());

  // This will be used to check if the elimination was integer exact.
  unsigned lcmProducts = 1;

  // Let x be the variable we are eliminating.
  // For each lower bound, lb <= c_l*x, and each upper bound c_u*x <= ub, (note
  // that c_l, c_u >= 1) we have:
  // lb*lcm(c_l, c_u)/c_l <= lcm(c_l, c_u)*x <= ub*lcm(c_l, c_u)/c_u
  // We thus generate a constraint:
  // lcm(c_l, c_u)/c_l*lb <= lcm(c_l, c_u)/c_u*ub.
  // Note if c_l = c_u = 1, all integer points captured by the resulting
  // constraint correspond to integer points in the original system (i.e., they
  // have integer pre-images). Hence, if the lcm's are all 1, the elimination is
  // integer exact.
  for (auto ubPos : ubIndices) {
    for (auto lbPos : lbIndices) {
      SmallVector<int64_t, 4> ineq;
      ineq.reserve(newFac.getNumCols());
      int64_t lbCoeff = atIneq(lbPos, pos);
      // Note that in the comments above, ubCoeff is the negation of the
      // coefficient in the canonical form as the view taken here is that of the
      // term being moved to the other size of '>='.
      int64_t ubCoeff = -atIneq(ubPos, pos);
      // TODO(bondhugula): refactor this loop to avoid all branches inside.
      for (unsigned l = 0, e = getNumCols(); l < e; l++) {
        if (l == pos)
          continue;
        assert(lbCoeff >= 1 && ubCoeff >= 1 && "bounds wrongly identified");
        int64_t lcm = mlir::lcm(lbCoeff, ubCoeff);
        ineq.push_back(atIneq(ubPos, l) * (lcm / ubCoeff) +
                       atIneq(lbPos, l) * (lcm / lbCoeff));
        lcmProducts *= lcm;
      }
      if (darkShadow) {
        // The dark shadow is a convex subset of the exact integer shadow. If
        // there is a point here, it proves the existence of a solution.
        ineq[ineq.size() - 1] += lbCoeff * ubCoeff - lbCoeff - ubCoeff + 1;
      }
      // TODO: we need to have a way to add inequalities in-place in
      // FlatAffineConstraints instead of creating and copying over.
      newFac.addInequality(ineq);
    }
  }

  if (lcmProducts == 1 && isResultIntegerExact)
    *isResultIntegerExact = 1;

  // Copy over the constraints not involving this variable.
  for (auto nbPos : nbIndices) {
    SmallVector<int64_t, 4> ineq;
    ineq.reserve(getNumCols() - 1);
    for (unsigned l = 0, e = getNumCols(); l < e; l++) {
      if (l == pos)
        continue;
      ineq.push_back(atIneq(nbPos, l));
    }
    newFac.addInequality(ineq);
  }

  assert(newFac.getNumConstraints() ==
         lbIndices.size() * ubIndices.size() + nbIndices.size());

  // Copy over the equalities.
  for (unsigned r = 0, e = getNumEqualities(); r < e; r++) {
    SmallVector<int64_t, 4> eq;
    eq.reserve(newFac.getNumCols());
    for (unsigned l = 0, e = getNumCols(); l < e; l++) {
      if (l == pos)
        continue;
      eq.push_back(atEq(r, l));
    }
    newFac.addEquality(eq);
  }

  newFac.removeTrivialRedundancy();
  clearAndCopyFrom(newFac);
  LLVM_DEBUG(llvm::dbgs() << "FM output:\n");
  LLVM_DEBUG(dump());
}

void FlatAffineConstraints::projectOut(unsigned pos, unsigned num) {
  if (num == 0)
    return;

  // 'pos' can be at most getNumCols() - 2 if num > 0.
  assert(getNumCols() < 2 || pos <= getNumCols() - 2 && "invalid position");
  assert(pos + num < getNumCols() && "invalid range");

  // Eliminate as many identifiers as possible using Gaussian elimination.
  unsigned currentPos = pos;
  unsigned numToEliminate = num;
  unsigned numGaussianEliminated = 0;

  while (currentPos < getNumIds()) {
    unsigned curNumEliminated =
        gaussianEliminateIds(currentPos, currentPos + numToEliminate);
    ++currentPos;
    numToEliminate -= curNumEliminated + 1;
    numGaussianEliminated += curNumEliminated;
  }

  // Eliminate the remaining using Fourier-Motzkin.
  for (unsigned i = 0; i < num - numGaussianEliminated; i++) {
    unsigned numToEliminate = num - numGaussianEliminated - i;
    FourierMotzkinEliminate(
        getBestIdToEliminate(*this, pos, pos + numToEliminate));
  }

  // Fast/trivial simplifications.
  GCDTightenInequalities();
  // Normalize constraints after tightening since the latter impacts this, but
  // not the other way round.
  normalizeConstraintsByGCD();
}

void FlatAffineConstraints::projectOut(Value *id) {
  unsigned pos;
  bool ret = findId(*id, &pos);
  assert(ret);
  (void)ret;
  FourierMotzkinEliminate(pos);
}

bool FlatAffineConstraints::isRangeOneToOne(unsigned start,
                                            unsigned limit) const {
  assert(start <= getNumIds() - 1 && "invalid start position");
  assert(limit > start && limit <= getNumIds() && "invalid limit");

  FlatAffineConstraints tmpCst(*this);

  if (start != 0) {
    // Move [start, limit) to the left.
    for (unsigned r = 0, e = getNumInequalities(); r < e; ++r) {
      for (unsigned c = 0, f = getNumCols(); c < f; ++c) {
        if (c >= start && c < limit)
          tmpCst.atIneq(r, c - start) = atIneq(r, c);
        else if (c < start)
          tmpCst.atIneq(r, c + limit - start) = atIneq(r, c);
        else
          tmpCst.atIneq(r, c) = atIneq(r, c);
      }
    }
    for (unsigned r = 0, e = getNumEqualities(); r < e; ++r) {
      for (unsigned c = 0, f = getNumCols(); c < f; ++c) {
        if (c >= start && c < limit)
          tmpCst.atEq(r, c - start) = atEq(r, c);
        else if (c < start)
          tmpCst.atEq(r, c + limit - start) = atEq(r, c);
        else
          tmpCst.atEq(r, c) = atEq(r, c);
      }
    }
  }

  // Mark everything to the right as symbols so that we can check the extents in
  // a symbolic way below.
  tmpCst.setDimSymbolSeparation(getNumIds() - (limit - start));

  // Check if the extents of all the specified dimensions are just one (when
  // treating the rest as symbols).
  for (unsigned pos = 0, e = tmpCst.getNumDimIds(); pos < e; ++pos) {
    auto extent = tmpCst.getConstantBoundOnDimSize(pos);
    if (!extent.hasValue() || extent.getValue() != 1)
      return false;
  }
  return true;
}
