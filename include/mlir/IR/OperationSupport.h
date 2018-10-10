//===- OperationSupport.h ---------------------------------------*- C++ -*-===//
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
// This file defines a number of support types that Operation and related
// classes build on top of.
//
//===----------------------------------------------------------------------===//

#ifndef MLIR_IR_OPERATION_SUPPORT_H
#define MLIR_IR_OPERATION_SUPPORT_H

#include "mlir/IR/Identifier.h"
#include "llvm/ADT/PointerUnion.h"

namespace mlir {
class Attribute;
class Location;
class Operation;
class OperationState;
class OpAsmParser;
class OpAsmParserResult;
class OpAsmPrinter;
class SSAValue;
class Type;

/// This is a "type erased" representation of a registered operation.  This
/// should only be used by things like the AsmPrinter and other things that need
/// to be parameterized by generic operation hooks.  Most user code should use
/// the concrete operation types.
class AbstractOperation {
public:
  template <typename T> static AbstractOperation get() {
    return AbstractOperation(T::getOperationName(), T::isClassFor,
                             T::parseAssembly, T::printAssembly,
                             T::verifyInvariants, T::constantFoldHook);
  }

  /// This is the name of the operation.
  const StringRef name;

  /// Return true if this "op class" can match against the specified operation.
  bool (&isClassFor)(const Operation *op);

  /// Use the specified object to parse this ops custom assembly format.
  bool (&parseAssembly)(OpAsmParser *parser, OperationState *result);

  /// This hook implements the AsmPrinter for this operation.
  void (&printAssembly)(const Operation *op, OpAsmPrinter *p);

  /// This hook implements the verifier for this operation.  It should emits an
  /// error message and returns true if a problem is detected, or returns false
  /// if everything is ok.
  bool (&verifyInvariants)(const Operation *op);

  /// This hook implements a constant folder for this operation.  It returns
  /// true if folding failed, or returns false and fills in `results` on
  /// success.
  bool (&constantFoldHook)(const Operation *op, ArrayRef<Attribute *> operands,
                           SmallVectorImpl<Attribute *> &results);

private:
  AbstractOperation(
      StringRef name, bool (&isClassFor)(const Operation *op),
      bool (&parseAssembly)(OpAsmParser *parser, OperationState *result),
      void (&printAssembly)(const Operation *op, OpAsmPrinter *p),
      bool (&verifyInvariants)(const Operation *op),
      bool (&constantFoldHook)(const Operation *op,
                               ArrayRef<Attribute *> operands,
                               SmallVectorImpl<Attribute *> &results))
      : name(name), isClassFor(isClassFor), parseAssembly(parseAssembly),
        printAssembly(printAssembly), verifyInvariants(verifyInvariants),
        constantFoldHook(constantFoldHook) {}
};

/// NamedAttribute is a used for operation attribute lists, it holds an
/// identifier for the name and a value for the attribute.  The attribute
/// pointer should always be non-null.
typedef std::pair<Identifier, Attribute *> NamedAttribute;

class OperationName {
public:
  typedef llvm::PointerUnion<Identifier, const AbstractOperation *>
      RepresentationUnion;

  OperationName(AbstractOperation *op) : representation(op) {}
  OperationName(StringRef name, MLIRContext *context);

  /// Return the name of this operation.  This always succeeds.
  StringRef getStringRef() const;

  /// If this operation has a registered operation description in the
  /// OperationSet, return it.  Otherwise return null.
  const AbstractOperation *getAbstractOperation() const;

  void print(raw_ostream &os) const;
  void dump() const;

  void *getAsOpaquePointer() const {
    return static_cast<void *>(representation.getOpaqueValue());
  }
  static OperationName getFromOpaquePointer(void *pointer);

private:
  RepresentationUnion representation;
  OperationName(RepresentationUnion representation)
      : representation(representation) {}
};

inline raw_ostream &operator<<(raw_ostream &os, OperationName identifier) {
  identifier.print(os);
  return os;
}

inline bool operator==(OperationName lhs, OperationName rhs) {
  return lhs.getAsOpaquePointer() == rhs.getAsOpaquePointer();
}

inline bool operator!=(OperationName lhs, OperationName rhs) {
  return lhs.getAsOpaquePointer() != rhs.getAsOpaquePointer();
}

/// This represents an operation in an abstracted form, suitable for use with
/// the builder APIs.  This object is a large and heavy weight object meant to
/// be used as a temporary object on the stack.  It is generally unwise to put
/// this in a collection.
struct OperationState {
  MLIRContext *const context;
  Location *location;
  OperationName name;
  SmallVector<SSAValue *, 4> operands;
  /// Types of the results of this operation.
  SmallVector<Type *, 4> types;
  SmallVector<NamedAttribute, 4> attributes;

public:
  OperationState(MLIRContext *context, Location *location, StringRef name)
      : context(context), location(location), name(name, context) {}

  OperationState(MLIRContext *context, Location *location, OperationName name)
      : context(context), location(location), name(name) {}

  OperationState(MLIRContext *context, Location *location, StringRef name,
                 ArrayRef<SSAValue *> operands, ArrayRef<Type *> types,
                 ArrayRef<NamedAttribute> attributes = {})
      : context(context), location(location), name(name, context),
        operands(operands.begin(), operands.end()),
        types(types.begin(), types.end()),
        attributes(attributes.begin(), attributes.end()) {}

  void addOperands(ArrayRef<SSAValue *> newOperands) {
    operands.append(newOperands.begin(), newOperands.end());
  }

  void addTypes(ArrayRef<Type *> newTypes) {
    types.append(newTypes.begin(), newTypes.end());
  }

  void addAttribute(StringRef name, Attribute *attr) {
    attributes.push_back({Identifier::get(name, context), attr});
  }
};

} // end namespace mlir

namespace llvm {
// Identifiers hash just like pointers, there is no need to hash the bytes.
template <> struct DenseMapInfo<mlir::OperationName> {
  static mlir::OperationName getEmptyKey() {
    auto pointer = llvm::DenseMapInfo<void *>::getEmptyKey();
    return mlir::OperationName::getFromOpaquePointer(pointer);
  }
  static mlir::OperationName getTombstoneKey() {
    auto pointer = llvm::DenseMapInfo<void *>::getTombstoneKey();
    return mlir::OperationName::getFromOpaquePointer(pointer);
  }
  static unsigned getHashValue(mlir::OperationName Val) {
    return DenseMapInfo<void *>::getHashValue(Val.getAsOpaquePointer());
  }
  static bool isEqual(mlir::OperationName LHS, mlir::OperationName RHS) {
    return LHS == RHS;
  }
};

/// The pointer inside of an identifier comes from a StringMap, so its alignment
/// is always at least 4 and probably 8 (on 64-bit machines).  Allow LLVM to
/// steal the low bits.
template <> struct PointerLikeTypeTraits<mlir::OperationName> {
public:
  static inline void *getAsVoidPointer(mlir::OperationName I) {
    return const_cast<void *>(I.getAsOpaquePointer());
  }
  static inline mlir::OperationName getFromVoidPointer(void *P) {
    return mlir::OperationName::getFromOpaquePointer(P);
  }
  enum {
    NumLowBitsAvailable = PointerLikeTypeTraits<
        mlir::OperationName::RepresentationUnion>::NumLowBitsAvailable
  };
};

} // end namespace llvm

#endif
