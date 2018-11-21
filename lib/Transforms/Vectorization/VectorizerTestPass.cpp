//===- VectorizerTestPass.cpp - VectorizerTestPass Pass Impl --------------===//
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
// This file implements a simple testing pass for vectorization functionality.
//
//===----------------------------------------------------------------------===//

#include "mlir/Analysis/MLFunctionMatcher.h"
#include "mlir/Analysis/SliceAnalysis.h"
#include "mlir/Analysis/VectorAnalysis.h"
#include "mlir/Pass.h"
#include "mlir/Support/Functional.h"
#include "mlir/Support/STLExtras.h"
#include "mlir/Transforms/Passes.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "vectorizer-test"

using namespace mlir;

using llvm::outs;
using llvm::SetVector;

using functional::map;

static llvm::cl::list<int> clTestVectorShapeRatio(
    "vector-shape-ratio",
    llvm::cl::desc("Specify the HW vector size for vectorization"),
    llvm::cl::ZeroOrMore);

static llvm::cl::opt<bool> clTestForwardStaticSlicingAnalysis(
    "forward-slicing",
    llvm::cl::desc(
        "Specify to enable testing forward static slicing and topological sort "
        "functionalities"));
static llvm::cl::opt<bool> clTestBackwardStaticSlicingAnalysis(
    "backward-slicing",
    llvm::cl::desc(
        "Specify to enable testing backward staticslicing and topological sort "
        "functionalities"));
static llvm::cl::opt<bool> clTestStaticSlicingAnalysis(
    "slicing",
    llvm::cl::desc(
        "Specify to enable testing static slicing and topological sort "
        "functionalities"));

namespace {

struct VectorizerTestPass : public FunctionPass {
  VectorizerTestPass() : FunctionPass(&VectorizerTestPass::passID) {}

  PassResult runOnMLFunction(MLFunction *f) override;
  void testVectorShapeRatio(MLFunction *f);
  void testForwardStaticSlicing(MLFunction *f);
  void testBackwardStaticSlicing(MLFunction *f);
  void testStaticSlicing(MLFunction *f);

  // Thread-safe RAII contexts local to pass, BumpPtrAllocator freed on exit.
  MLFunctionMatcherContext MLContext;

  static char passID;
};

} // end anonymous namespace

char VectorizerTestPass::passID = 0;

void VectorizerTestPass::testVectorShapeRatio(MLFunction *f) {
  using matcher::Op;
  SmallVector<int, 8> shape(clTestVectorShapeRatio.begin(),
                            clTestVectorShapeRatio.end());
  auto subVectorType = VectorType::get(shape, Type::getF32(f->getContext()));
  // Only filter statements that operate on a strict super-vector and have one
  // return. This makes testing easier.
  auto filter = [subVectorType](const Statement &stmt) {
    auto *opStmt = dyn_cast<OperationStmt>(&stmt);
    if (!opStmt) {
      return false;
    }
    assert(subVectorType.getElementType() ==
               Type::getF32(subVectorType.getContext()) &&
           "Only f32 supported for now");
    if (!matcher::operatesOnStrictSuperVectors(*opStmt, subVectorType)) {
      return false;
    }
    if (opStmt->getNumResults() != 1) {
      return false;
    }
    return true;
  };
  auto pat = Op(filter);
  auto matches = pat.match(f);
  for (auto m : matches) {
    auto *opStmt = cast<OperationStmt>(m.first);
    // This is a unit test that only checks and prints shape ratio.
    // As a consequence we write only Ops with a single return type for the
    // purpose of this test. If we need to test more intricate behavior in the
    // future we can always extend.
    auto superVectorType = opStmt->getResult(0)->getType().cast<VectorType>();
    auto ratio = shapeRatio(superVectorType, subVectorType);
    if (!ratio.hasValue()) {
      opStmt->emitNote("NOT MATCHED");
    } else {
      outs() << "\nmatched: " << *opStmt << " with shape ratio: ";
      interleaveComma(MutableArrayRef<unsigned>(*ratio), outs());
    }
  }
}

static std::string toString(Statement *stmt) {
  std::string res;
  auto os = llvm::raw_string_ostream(res);
  stmt->print(os);
  return res;
}

static MLFunctionMatches matchTestSlicingOps(MLFunction *f) {
  // Just use a custom op name for this test, it makes life easier.
  constexpr auto kTestSlicingOpName = "slicing-test-op";
  using functional::map;
  using matcher::Op;
  // Match all OpStatements with the kTestSlicingOpName name.
  auto filter = [](const Statement &stmt) {
    const auto &opStmt = cast<OperationStmt>(stmt);
    return opStmt.getName().getStringRef() == kTestSlicingOpName;
  };
  auto pat = Op(filter);
  return pat.match(f);
}

void VectorizerTestPass::testBackwardStaticSlicing(MLFunction *f) {
  auto matches = matchTestSlicingOps(f);
  for (auto m : matches) {
    SetVector<Statement *> backwardStaticSlice;
    getBackwardStaticSlice(m.first, &backwardStaticSlice);
    auto strs = map(toString, backwardStaticSlice);
    outs() << "\nmatched: " << *m.first << " backward static slice: ";
    for (const auto &s : strs) {
      outs() << "\n" << s;
    }
  }
}

void VectorizerTestPass::testForwardStaticSlicing(MLFunction *f) {
  auto matches = matchTestSlicingOps(f);
  for (auto m : matches) {
    SetVector<Statement *> forwardStaticSlice;
    getForwardStaticSlice(m.first, &forwardStaticSlice);
    auto strs = map(toString, forwardStaticSlice);
    outs() << "\nmatched: " << *m.first << " forward static slice: ";
    for (const auto &s : strs) {
      outs() << "\n" << s;
    }
  }
}

void VectorizerTestPass::testStaticSlicing(MLFunction *f) {
  auto matches = matchTestSlicingOps(f);
  for (auto m : matches) {
    SetVector<Statement *> staticSlice = getStaticSlice(m.first);
    auto strs = map(toString, staticSlice);
    outs() << "\nmatched: " << *m.first << " static slice: ";
    for (const auto &s : strs) {
      outs() << "\n" << s;
    }
  }
}

PassResult VectorizerTestPass::runOnMLFunction(MLFunction *f) {
  if (!clTestVectorShapeRatio.empty()) {
    testVectorShapeRatio(f);
  }
  if (clTestForwardStaticSlicingAnalysis) {
    testForwardStaticSlicing(f);
  }
  if (clTestBackwardStaticSlicingAnalysis) {
    testBackwardStaticSlicing(f);
  }
  if (clTestStaticSlicingAnalysis) {
    testStaticSlicing(f);
  }
  return PassResult::Success;
}

FunctionPass *mlir::createVectorizerTestPass() {
  return new VectorizerTestPass();
}

static PassRegistration<VectorizerTestPass>
    pass("vectorizer-test", "Tests vectorizer standalone functionality.");

#undef DEBUG_TYPE
