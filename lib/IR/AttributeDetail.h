//===- AttributeDetail.h - MLIR Affine Map details Class --------*- C++ -*-===//
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
// This holds implementation details of Attribute.
//
//===----------------------------------------------------------------------===//

#ifndef ATTRIBUTEDETAIL_H_
#define ATTRIBUTEDETAIL_H_

#include "mlir/IR/Attributes.h"
#include "mlir/IR/MLIRContext.h"

namespace mlir {
namespace detail {

/// Base storage class appearing in an attribute.
struct AttributeStorage {
  Attribute::Kind kind : 8;

  /// This field is true if this is, or contains, a function attribute.
  bool isOrContainsFunctionCache : 1;
};

/// An attribute representing a boolean value.
struct BoolAttributeStorage : public AttributeStorage {
  bool value;
};

/// An attribute representing a integral value.
struct IntegerAttributeStorage : public AttributeStorage {
  int64_t value;
};

/// An attribute representing a floating point value.
struct FloatAttributeStorage final
    : public AttributeStorage,
      public llvm::TrailingObjects<FloatAttributeStorage, uint64_t> {
  const llvm::fltSemantics &semantics;
  size_t numObjects;

  /// Returns an APFloat representing the stored value.
  APFloat getValue() const {
    auto val = APInt(APFloat::getSizeInBits(semantics),
                     {getTrailingObjects<uint64_t>(), numObjects});
    return APFloat(semantics, val);
  }
};

/// An attribute representing a string value.
struct StringAttributeStorage : public AttributeStorage {
  StringRef value;
};

/// An attribute representing an array of other attributes.
struct ArrayAttributeStorage : public AttributeStorage {
  ArrayRef<Attribute> value;
};

// An attribute representing a reference to an affine map.
struct AffineMapAttributeStorage : public AttributeStorage {
  AffineMap value;
};

// An attribute representing a reference to an integer set.
struct IntegerSetAttributeStorage : public AttributeStorage {
  IntegerSet value;
};

/// An attribute representing a reference to a type.
struct TypeAttributeStorage : public AttributeStorage {
  Type *value;
};

/// An attribute representing a reference to a function.
struct FunctionAttributeStorage : public AttributeStorage {
  Function *value;
};

/// A base attribute representing a reference to a vector or tensor constant.
struct ElementsAttributeStorage : public AttributeStorage {
  VectorOrTensorType *type;
};

/// An attribute representing a reference to a vector or tensor constant,
/// inwhich all elements have the same value.
struct SplatElementsAttributeStorage : public ElementsAttributeStorage {
  Attribute elt;
};

/// An attribute representing a reference to a dense vector or tensor object.
struct DenseElementsAttributeStorage : public ElementsAttributeStorage {
  ArrayRef<char> data;
};

/// An attribute representing a reference to a dense integer vector or tensor
/// object.
struct DenseIntElementsAttributeStorage : public DenseElementsAttributeStorage {
  size_t bitsWidth;
};

/// An attribute representing a reference to a dense float vector or tensor
/// object.
struct DenseFPElementsAttributeStorage : public DenseElementsAttributeStorage {
};

/// An attribute representing a reference to a tensor constant with opaque
/// content.
struct OpaqueElementsAttributeStorage : public ElementsAttributeStorage {
  StringRef bytes;
};

/// An attribute representing a reference to a sparse vector or tensor object.
struct SparseElementsAttributeStorage : public ElementsAttributeStorage {
  DenseIntElementsAttr indices;
  DenseElementsAttr values;
};

} // namespace detail
} // namespace mlir

#endif // ATTRIBUTEDETAIL_H_
