//===- Identifier.h - MLIR Identifier Class ---------------------*- C++ -*-===//
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

#ifndef MLIR_IR_IDENTIFIER_H
#define MLIR_IR_IDENTIFIER_H

#include "mlir/Support/LLVM.h"
#include "llvm/ADT/DenseMapInfo.h"
#include "llvm/ADT/StringRef.h"

namespace mlir {
class MLIRContext;

/// This class represents a uniqued string owned by an MLIRContext.  Strings
/// represented by this type cannot contain nul characters, and may not have a
/// zero length.
///
/// This is a POD type with pointer size, so it should be passed around by
/// value.  The underlying data is owned by MLIRContext and is thus immortal for
/// almost all clients.
class Identifier {
public:
  /// Return an identifier for the specified string.
  static Identifier get(StringRef str, const MLIRContext *context);

  /// Return a StringRef for the string.
  StringRef str() const {
    return StringRef(pointer, size());
  }

  /// Return a pointer to the start of the string data.
  const char *data() const {
    return pointer;
  }

  /// Return the number of bytes in this string.
  unsigned size() const {
    return ::strlen(pointer);
  }

  /// Return true if this identifier is the specified string.
  bool is(StringRef string) const {
    return str().equals(string);
  }

  Identifier(const Identifier&) = default;
  Identifier &operator=(const Identifier &other) = default;

  void print(raw_ostream &os) const;
  void dump() const;

  const void *getAsOpaquePointer() const {
    return static_cast<const void *>(pointer);
  }
  static Identifier getFromOpaquePointer(const void *pointer) {
    return Identifier((const char *)pointer);
  }

private:
  /// These are the bytes of the string, which is a nul terminated string.
  const char *pointer;
  explicit Identifier(const char *pointer) : pointer(pointer) {}
};

inline raw_ostream &operator<<(raw_ostream &os, Identifier identifier) {
  identifier.print(os);
  return os;
}

inline bool operator==(Identifier lhs, Identifier rhs) {
  return lhs.data() == rhs.data();
}

inline bool operator!=(Identifier lhs, Identifier rhs) {
  return lhs.data() != rhs.data();
}

inline bool operator==(Identifier lhs, StringRef rhs) { return lhs.is(rhs); }
inline bool operator!=(Identifier lhs, StringRef rhs) { return !lhs.is(rhs); }
inline bool operator==(StringRef lhs, Identifier rhs) { return rhs.is(lhs); }
inline bool operator!=(StringRef lhs, Identifier rhs) { return !rhs.is(lhs); }

// Make identifiers hashable.
inline llvm::hash_code hash_value(Identifier arg) {
  return llvm::hash_value(arg.str());
}

} // end namespace mlir

namespace llvm {
// Identifiers hash just like pointers, there is no need to hash the bytes.
template <>
struct DenseMapInfo<mlir::Identifier> {
  static mlir::Identifier getEmptyKey() {
    auto pointer = llvm::DenseMapInfo<const void *>::getEmptyKey();
    return mlir::Identifier::getFromOpaquePointer(pointer);
  }
  static mlir::Identifier getTombstoneKey() {
    auto pointer = llvm::DenseMapInfo<const void *>::getTombstoneKey();
    return mlir::Identifier::getFromOpaquePointer(pointer);
  }
  static unsigned getHashValue(mlir::Identifier Val) {
    return DenseMapInfo<const void *>::getHashValue(Val.data());
  }
  static bool isEqual(mlir::Identifier LHS, mlir::Identifier RHS) {
    return LHS == RHS;
  }
};

/// The pointer inside of an identifier comes from a StringMap, so its alignment
/// is always at least 4 and probably 8 (on 64-bit machines).  Allow LLVM to
/// steal the low bits.
template <>
struct PointerLikeTypeTraits<mlir::Identifier> {
public:
  static inline void *getAsVoidPointer(mlir::Identifier I) {
    return const_cast<void *>(I.getAsOpaquePointer());
  }
  static inline mlir::Identifier getFromVoidPointer(void *P) {
    return mlir::Identifier::getFromOpaquePointer(P);
  }
  enum { NumLowBitsAvailable = 2 };
};

} // end namespace llvm
#endif
