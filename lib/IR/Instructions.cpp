//===- Instructions.cpp - MLIR CFGFunction Instruction Classes ------------===//
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

#include "mlir/IR/Instructions.h"
#include "mlir/IR/CFGFunction.h"
#include "mlir/IR/MLIRContext.h"
using namespace mlir;

/// Replace all uses of 'this' value with the new value, updating anything in
/// the IR that uses 'this' to use the other value instead.  When this returns
/// there are zero uses of 'this'.
void IRObjectWithUseList::replaceAllUsesWith(IRObjectWithUseList *newValue) {
  assert(this != newValue && "cannot RAUW a value with itself");
  while (!use_empty()) {
    use_begin()->set(newValue);
  }
}

/// Return the result number of this result.
unsigned InstResult::getResultNumber() const {
  // Results are always stored consecutively, so use pointer subtraction to
  // figure out what number this is.
  return this - &getOwner()->getInstResults()[0];
}

//===----------------------------------------------------------------------===//
// Instruction
//===----------------------------------------------------------------------===//

// Instructions are deleted through the destroy() member because we don't have
// a virtual destructor.
Instruction::~Instruction() {
  assert(block == nullptr && "instruction destroyed but still in a block");
}

/// Destroy this instruction or one of its subclasses.
void Instruction::destroy() {
  switch (getKind()) {
  case Kind::Operation:
    cast<OperationInst>(this)->destroy();
    break;
  case Kind::Branch:
    delete cast<BranchInst>(this);
    break;
  case Kind::CondBranch:
    delete cast<CondBranchInst>(this);
    break;
  case Kind::Return:
    cast<ReturnInst>(this)->destroy();
    break;
  }
}

void OperationInst::destroy() {
  this->~OperationInst();
  free(this);
}

CFGFunction *Instruction::getFunction() {
  auto *block = getBlock();
  return block ? block->getFunction() : nullptr;
}

unsigned Instruction::getNumOperands() const {
  switch (getKind()) {
  case Kind::Operation:
    return cast<OperationInst>(this)->getNumOperands();
  case Kind::Branch:
    return cast<BranchInst>(this)->getNumOperands();
  case Kind::CondBranch:
    return cast<CondBranchInst>(this)->getNumOperands();
  case Kind::Return:
    return cast<ReturnInst>(this)->getNumOperands();
  }
}

MutableArrayRef<InstOperand> Instruction::getInstOperands() {
  switch (getKind()) {
  case Kind::Operation:
    return cast<OperationInst>(this)->getInstOperands();
  case Kind::Branch:
    return cast<BranchInst>(this)->getInstOperands();
  case Kind::CondBranch:
    return cast<CondBranchInst>(this)->getInstOperands();
  case Kind::Return:
    return cast<ReturnInst>(this)->getInstOperands();
  }
}

/// This drops all operand uses from this instruction, which is an essential
/// step in breaking cyclic dependences between references when they are to
/// be deleted.
void Instruction::dropAllReferences() {
  for (auto &op : getInstOperands())
    op.drop();

  if (auto *term = dyn_cast<TerminatorInst>(this))
    for (auto &dest : term->getBasicBlockOperands())
      dest.drop();

  if (OperationInst *opInst = dyn_cast<OperationInst>(this)) {
    if (opInst->isTerminator())
      for (auto &dest : opInst->getBasicBlockOperands())
        dest.drop();
  }
}

/// Emit a note about this instruction, reporting up to any diagnostic
/// handlers that may be listening.
void Instruction::emitNote(const Twine &message) const {
  getContext()->emitDiagnostic(getLoc(), message,
                               MLIRContext::DiagnosticKind::Note);
}

/// Emit a warning about this operation, reporting up to any diagnostic
/// handlers that may be listening.
void Instruction::emitWarning(const Twine &message) const {
  getContext()->emitDiagnostic(getLoc(), message,
                               MLIRContext::DiagnosticKind::Warning);
}

/// Emit an error about fatal conditions with this instruction, reporting up to
/// any diagnostic handlers that may be listening.  NOTE: This may terminate
/// the containing application, only use when the IR is in an inconsistent
/// state.
void Instruction::emitError(const Twine &message) const {
  getContext()->emitDiagnostic(getLoc(), message,
                               MLIRContext::DiagnosticKind::Error);
}

//===----------------------------------------------------------------------===//
// OperationInst
//===----------------------------------------------------------------------===//

/// Create a new OperationInst with the specified fields.
OperationInst *OperationInst::create(Location location, OperationName name,
                                     ArrayRef<CFGValue *> operands,
                                     ArrayRef<Type> resultTypes,
                                     ArrayRef<NamedAttribute> attributes,
                                     ArrayRef<BasicBlock *> successors,
                                     MLIRContext *context) {
  unsigned numSuccessors = successors.size();
  auto byteSize = totalSizeToAlloc<InstResult, BasicBlockOperand, unsigned>(
      resultTypes.size(), numSuccessors, numSuccessors);
  void *rawMem = malloc(byteSize);

  // Initialize the OperationInst part of the instruction.
  auto inst = ::new (rawMem) OperationInst(location, name, resultTypes.size(),
                                           numSuccessors, attributes, context);

  // Initialize the results and operands.
  auto instResults = inst->getInstResults();
  for (unsigned i = 0, e = resultTypes.size(); i != e; ++i)
    new (&instResults[i]) InstResult(resultTypes[i], inst);

  unsigned operandIt = 0, operandE = operands.size();
  for (; operandIt != operandE; ++operandIt) {
    // Null operands are used as sentinals between successor operand lists. If
    // we encounter one here, break and handle the successor operands lists
    // separately below.
    if (!operands[operandIt])
      break;
    inst->operands.push_back(InstOperand(inst, operands[operandIt]));
  }

  // Check to see if a sentinal operand was encountered.
  unsigned currentSuccNum = 0;
  if (operandIt != operandE) {
    assert(inst->isTerminator() &&
           "Sentinal operand found in non terminator operand list.");
    auto instBlockOperands = inst->getBasicBlockOperands();
    unsigned *succOperandCountIt = inst->getTrailingObjects<unsigned>();
    unsigned *succOperandCountE = succOperandCountIt + numSuccessors;
    (void)succOperandCountE;

    for (; operandIt != operandE; ++operandIt) {
      // If we encounter a sentinal branch to the next operand update the count
      // variable.
      if (!operands[operandIt]) {
        assert(currentSuccNum < numSuccessors);

        // After the first iteration update the successor operand count
        // variable.
        if (currentSuccNum != 0) {
          ++succOperandCountIt;
          assert(succOperandCountIt != succOperandCountE &&
                 "More sentinal operands than successors.");
        }

        new (&instBlockOperands[currentSuccNum])
            BasicBlockOperand(inst, successors[currentSuccNum]);
        *succOperandCountIt = 0;
        ++currentSuccNum;
        continue;
      }
      inst->operands.push_back(InstOperand(inst, operands[operandIt]));
      ++(*succOperandCountIt);
    }
  }

  // Verify that the amount of sentinal operands is equivalent to the number of
  // successors.
  assert(currentSuccNum == numSuccessors);
  return inst;
}

OperationInst *OperationInst::clone() const {
  SmallVector<CFGValue *, 8> operands;
  SmallVector<Type, 8> resultTypes;
  SmallVector<BasicBlock *, 1> successors;

  // Put together the results.
  for (auto *result : getResults())
    resultTypes.push_back(result->getType());

  // If the instruction is a terminator the successor and non-successor operand
  // lists are interleaved with sentinal(nullptr) operands.
  if (isTerminator()) {
    // To interleave the operand lists we iterate in reverse and insert the
    // operands in-place.
    operands.resize(getNumOperands() + getNumSuccessors());
    successors.resize(getNumSuccessors());
    int cloneOperandIt = operands.size() - 1, operandIt = getNumOperands() - 1;
    for (int succIt = getNumSuccessors() - 1, succE = 0; succIt >= succE;
         --succIt) {
      successors[succIt] = getSuccessor(succIt);

      // Add the successor operands in-place in reverse order.
      for (unsigned i = 0, e = getNumSuccessorOperands(succIt); i != e;
           ++i, --cloneOperandIt, --operandIt) {
        operands[cloneOperandIt] =
            const_cast<CFGValue *>(getOperand(operandIt));
      }

      // Add a null operand for the barrier.
      operands[cloneOperandIt--] = nullptr;
    }

    // Add the rest of the non-successor operands.
    for (; cloneOperandIt >= 0; --cloneOperandIt, --operandIt)
      operands[cloneOperandIt] = const_cast<CFGValue *>(getOperand(operandIt));
    // For non terminators we can simply add each of the instructions in place.
  } else {
    for (auto *operand : getOperands())
      operands.push_back(const_cast<CFGValue *>(operand));
  }

  return create(getLoc(), getName(), operands, resultTypes, getAttrs(),
                successors, getContext());
}

OperationInst::OperationInst(Location location, OperationName name,
                             unsigned numResults, unsigned numSuccessors,
                             ArrayRef<NamedAttribute> attributes,
                             MLIRContext *context)
    : Operation(/*isInstruction=*/true, name, attributes, context),
      Instruction(Kind::Operation, location), numResults(numResults),
      numSuccs(numSuccessors) {}

OperationInst::~OperationInst() {
  // Explicitly run the destructors for the results and successors.
  for (auto &result : getInstResults())
    result.~InstResult();

  if (isTerminator())
    for (auto &successor : getBasicBlockOperands())
      successor.~BasicBlockOperand();
}

void OperationInst::addSuccessorOperand(unsigned index, CFGValue *value) {
  assert(isTerminator() && "Only terminators have successors.");
  assert(index < getNumSuccessors());
  assert(std::accumulate(getTrailingObjects<unsigned>() + index + 1,
                         getTrailingObjects<unsigned>() + numSuccs, 0u) == 0 &&
         "All successor operands must be added before moving to the next.");

  operands.push_back(InstOperand(this, value));
  ++getTrailingObjects<unsigned>()[index];
}

void OperationInst::addSuccessorOperands(unsigned index,
                                         ArrayRef<CFGValue *> values) {
  operands.reserve(operands.size() + values.size());
  for (auto *value : values)
    addSuccessorOperand(index, value);
}

void llvm::ilist_traits<::mlir::OperationInst>::deleteNode(
    OperationInst *inst) {
  inst->destroy();
}

mlir::BasicBlock *
llvm::ilist_traits<::mlir::OperationInst>::getContainingBlock() {
  size_t Offset(
      size_t(&((BasicBlock *)nullptr->*BasicBlock::getSublistAccess(nullptr))));
  iplist<OperationInst> *Anchor(static_cast<iplist<OperationInst> *>(this));
  return reinterpret_cast<BasicBlock *>(reinterpret_cast<char *>(Anchor) -
                                        Offset);
}

/// This is a trait method invoked when an instruction is added to a block.  We
/// keep the block pointer up to date.
void llvm::ilist_traits<::mlir::OperationInst>::addNodeToList(
    OperationInst *inst) {
  assert(!inst->getBlock() && "already in a basic block!");
  inst->block = getContainingBlock();
}

/// This is a trait method invoked when an instruction is removed from a block.
/// We keep the block pointer up to date.
void llvm::ilist_traits<::mlir::OperationInst>::removeNodeFromList(
    OperationInst *inst) {
  assert(inst->block && "not already in a basic block!");
  inst->block = nullptr;
}

/// This is a trait method invoked when an instruction is moved from one block
/// to another.  We keep the block pointer up to date.
void llvm::ilist_traits<::mlir::OperationInst>::transferNodesFromList(
    ilist_traits<OperationInst> &otherList, instr_iterator first,
    instr_iterator last) {
  // If we are transferring instructions within the same basic block, the block
  // pointer doesn't need to be updated.
  BasicBlock *curParent = getContainingBlock();
  if (curParent == otherList.getContainingBlock())
    return;

  // Update the 'block' member of each instruction.
  for (; first != last; ++first)
    first->block = curParent;
}

/// Unlink this instruction from its BasicBlock and delete it.
void OperationInst::erase() {
  assert(getBlock() && "Instruction has no parent");
  getBlock()->getOperations().erase(this);
}

/// Unlink this operation instruction from its current basic block and insert
/// it right before `existingInst` which may be in the same or another block
/// in the same function.
void OperationInst::moveBefore(OperationInst *existingInst) {
  assert(existingInst && "Cannot move before a null instruction");
  return moveBefore(existingInst->getBlock(), existingInst->getIterator());
}

/// Unlink this operation instruction from its current basic block and insert
/// it right before `iterator` in the specified basic block.
void OperationInst::moveBefore(BasicBlock *block,
                               llvm::iplist<OperationInst>::iterator iterator) {
  block->getOperations().splice(iterator, getBlock()->getOperations(),
                                getIterator());
}

//===----------------------------------------------------------------------===//
// TerminatorInst
//===----------------------------------------------------------------------===//

/// Remove this terminator from its BasicBlock and delete it.
void TerminatorInst::erase() {
  assert(getBlock() && "Instruction has no parent");
  getBlock()->setTerminator(nullptr);
  destroy();
}

/// Return the list of destination entries that this terminator branches to.
MutableArrayRef<BasicBlockOperand> TerminatorInst::getBasicBlockOperands() {
  switch (getKind()) {
  case Kind::Operation:
    llvm_unreachable("not a terminator");
  case Kind::Branch:
    return cast<BranchInst>(this)->getBasicBlockOperands();
  case Kind::CondBranch:
    return cast<CondBranchInst>(this)->getBasicBlockOperands();
  case Kind::Return:
    // Return has no basic block successors.
    return {};
  }
}

//===----------------------------------------------------------------------===//
// ReturnInst
//===----------------------------------------------------------------------===//

/// Create a new OperationInst with the specific fields.
ReturnInst *ReturnInst::create(Location location,
                               ArrayRef<CFGValue *> operands) {
  auto byteSize = totalSizeToAlloc<InstOperand>(operands.size());
  void *rawMem = malloc(byteSize);

  // Initialize the ReturnInst part of the instruction.
  auto inst = ::new (rawMem) ReturnInst(location, operands.size());

  // Initialize the operands and results.
  auto instOperands = inst->getInstOperands();
  for (unsigned i = 0, e = operands.size(); i != e; ++i)
    new (&instOperands[i]) InstOperand(inst, operands[i]);
  return inst;
}

ReturnInst::ReturnInst(Location location, unsigned numOperands)
    : TerminatorInst(Kind::Return, location), numOperands(numOperands) {}

void ReturnInst::destroy() {
  this->~ReturnInst();
  free(this);
}

ReturnInst::~ReturnInst() {
  // Explicitly run the destructors for the operands.
  for (auto &operand : getInstOperands())
    operand.~InstOperand();
}

//===----------------------------------------------------------------------===//
// BranchInst
//===----------------------------------------------------------------------===//

BranchInst::BranchInst(Location location, BasicBlock *dest,
                       ArrayRef<CFGValue *> operands)
    : TerminatorInst(Kind::Branch, location), dest(this, dest) {
  addOperands(operands);
}

void BranchInst::setDest(BasicBlock *block) { dest.set(block); }

/// Add one value to the operand list.
void BranchInst::addOperand(CFGValue *value) {
  operands.emplace_back(InstOperand(this, value));
}

/// Add a list of values to the operand list.
void BranchInst::addOperands(ArrayRef<CFGValue *> values) {
  operands.reserve(operands.size() + values.size());
  for (auto *value : values)
    addOperand(value);
}

//===----------------------------------------------------------------------===//
// CondBranchInst
//===----------------------------------------------------------------------===//

CondBranchInst::CondBranchInst(Location location, CFGValue *condition,
                               BasicBlock *trueDest, BasicBlock *falseDest)
    : TerminatorInst(Kind::CondBranch, location),
      condition(condition), dests{{this}, {this}}, numTrueOperands(0) {
  dests[falseIndex].set(falseDest);
  dests[trueIndex].set(trueDest);
}

/// Add one value to the true operand list.
void CondBranchInst::addTrueOperand(CFGValue *value) {
  assert(getNumFalseOperands() == 0 &&
         "Must insert all true operands before false operands!");
  operands.emplace_back(InstOperand(this, value));
  ++numTrueOperands;
}

/// Add a list of values to the true operand list.
void CondBranchInst::addTrueOperands(ArrayRef<CFGValue *> values) {
  operands.reserve(operands.size() + values.size());
  for (auto *value : values)
    addTrueOperand(value);
}

/// Add one value to the false operand list.
void CondBranchInst::addFalseOperand(CFGValue *value) {
  operands.emplace_back(InstOperand(this, value));
}

/// Add a list of values to the false operand list.
void CondBranchInst::addFalseOperands(ArrayRef<CFGValue *> values) {
  operands.reserve(operands.size() + values.size());
  for (auto *value : values)
    addFalseOperand(value);
}
