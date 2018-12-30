//===- OpStats.cpp - Prints stats of operations in module -----------------===//
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

#include "mlir/IR/InstVisitor.h"
#include "mlir/IR/Instructions.h"
#include "mlir/IR/Module.h"
#include "mlir/IR/OperationSupport.h"
#include "mlir/Pass.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlir;

namespace {
struct PrintOpStatsPass : public ModulePass, InstWalker<PrintOpStatsPass> {
  explicit PrintOpStatsPass(llvm::raw_ostream &os = llvm::errs())
      : ModulePass(&PrintOpStatsPass::passID), os(os) {}

  // Prints the resultant operation statistics post iterating over the module.
  PassResult runOnModule(Module *m) override;

  // Updates the operation statistics for the given instruction.
  void visitOperationInst(OperationInst *inst);

  // Print summary of op stats.
  void printSummary();

  static char passID;

private:
  llvm::StringMap<int64_t> opCount;

  llvm::raw_ostream &os;
};
} // namespace

char PrintOpStatsPass::passID = 0;

PassResult PrintOpStatsPass::runOnModule(Module *m) {
  for (auto &fn : *m)
    walk(&fn);
  printSummary();
  return success();
}

void PrintOpStatsPass::visitOperationInst(OperationInst *inst) {
  ++opCount[inst->getName().getStringRef()];
}

void PrintOpStatsPass::printSummary() {
  os << "Operations encountered:\n";
  os << "-----------------------\n";
  std::vector<StringRef> sorted(opCount.keys().begin(), opCount.keys().end());
  llvm::sort(sorted);

  // Returns the lenght of the dialect prefix of an op.
  auto dialectLen = [](StringRef opName) -> size_t {
    auto dialectEnd = opName.find_last_of('.');
    if (dialectEnd == StringRef::npos)
      return 0;
    // Count the period too.
    return dialectEnd + 1;
  };

  // Left-align the names (aligning on the dialect) and right-align count below.
  // The alignment is for readability and does not affect CSV/FileCheck parsing.
  size_t maxLenName = 0;
  size_t maxLenNamePrefixLen = 0;
  size_t maxLenDialect = 0;
  int maxLenCount = 0;
  for (const auto &key : sorted) {
    size_t len = key.size();
    size_t prefix = dialectLen(key);
    if (len > maxLenName) {
      maxLenName = len;
      maxLenNamePrefixLen = prefix;
    }
    maxLenDialect = max(maxLenDialect, prefix);
    // This takes advantage of the fact that opCount[key] > 0.
    maxLenCount = max(maxLenCount, (int)log10(opCount[key]) + 1);
  }
  // Adjust the max name length to account for the dialect.
  maxLenName += (maxLenDialect - maxLenNamePrefixLen);

  for (const auto &key : sorted) {
    size_t prefix = maxLenDialect - dialectLen(key);
    os.indent(2 + prefix) << '\'' << key << '\'';
    // Add one to compensate for the period of the dialect.
    os.indent(maxLenName + 1 - key.size() - prefix) << " ,";
    os.indent(maxLenCount - (int)log10(opCount[key])) << opCount[key] << "\n";
  }
}

static PassRegistration<PrintOpStatsPass>
    pass("print-op-stats", "Print statistics of operations");
