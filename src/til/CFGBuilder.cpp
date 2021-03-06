//===- CFGBuilder.h --------------------------------------------*- C++ --*-===//
// Copyright 2014  Google
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//===----------------------------------------------------------------------===//


#include "CFGBuilder.h"


namespace ohmu {
namespace til  {


SCFG* CFGBuilder::beginCFG(SCFG *Cfg, unsigned NumBlocks, unsigned NumInstrs) {
  assert(!CurrentCFG && !CurrentBB && "Already inside a CFG");

  if (!Cfg) {
    CurrentCFG = new (Arena) SCFG(Arena, 0);
    // CurrentCFG->blocks().reserve(Arena, NumBlocks);
  }
  else {
    CurrentCFG = Cfg;
  }
  return CurrentCFG;
}


void CFGBuilder::endCFG() {
  assert(CurrentCFG && "Not inside a CFG.");
  // assert(!CurrentBB && "Never finished the last block.");

  CurrentCFG = nullptr;
}



void CFGBuilder::beginBlock(BasicBlock *B) {
  assert(!CurrentBB && "Haven't finished current block.");
  assert(CurrentArgs.empty());
  assert(CurrentInstrs.empty());

  CurrentBB = B;
  if (!B->cfg())
    CurrentCFG->add(B);
}


void CFGBuilder::endBlock(Terminator *Term) {
  assert(CurrentBB && "No current block.");
  assert(OverwriteInstructions || CurrentBB->instructions().size() == 0 &&
         "Already finished block.");

  // Ovewrite existing arguments with CurrentArgs, if requested.
  if (OverwriteArguments)
    CurrentBB->arguments().clear();

  if (CurrentArgs.size() > 0) {
    auto Sz = CurrentBB->arguments().size();
    CurrentBB->arguments().reserve(Arena, Sz + CurrentArgs.size());
    for (auto *E : CurrentArgs)
      CurrentBB->addArgument(E);
  }

  // Overwrite existing instructions with CurrentInstrs, if requested.
  if (OverwriteInstructions)
    CurrentBB->instructions().clear();

  if (CurrentInstrs.size() > 0) {
    auto Sz = CurrentBB->instructions().size();
    CurrentBB->instructions().reserve(Arena, Sz + CurrentInstrs.size());
    for (auto *E : CurrentInstrs)
      CurrentBB->addInstruction(E);
  }
  CurrentBB->setTerminator(Term);

  CurrentArgs.clear();
  CurrentInstrs.clear();
  CurrentBB = nullptr;
}



BasicBlock* CFGBuilder::newBlock(unsigned Nargs, unsigned Npreds) {
  BasicBlock *B = new (Arena) BasicBlock(Arena);
  if (Nargs > 0) {
    B->predecessors().reserve(Arena, Npreds);
    B->arguments().reserve(Arena, Nargs);
    for (unsigned i = 0; i < Nargs; ++i) {
      auto *Ph = new (Arena) Phi();
      Ph->values().reserve(Arena, Npreds);
      B->addArgument(Ph);
    }
  }
  return B;
}


Branch* CFGBuilder::newBranch(SExpr *Cond, BasicBlock *B0, BasicBlock *B1) {
  assert(CurrentBB && "No current block.");

  if (!B0)
    B0 = newBlock();
  if (!B1)
    B1 = newBlock();

  assert(B0->arguments().size() == 0 && "Cannot branch to a block with args.");
  assert(B1->arguments().size() == 0 && "Cannot branch to a block with args.");

  B0->addPredecessor(CurrentBB);
  B1->addPredecessor(CurrentBB);

  // Terminate current basic block with a branch
  auto *Nt = new (Arena) Branch(Cond, B0, B1);
  endBlock(Nt);
  return Nt;
}


void CFGBuilder::setPhiArgument(Phi* Ph, SExpr* E, unsigned Idx) {
  Instruction *I = dyn_cast<Instruction>(E);
  if (!I) {
    diag.error("Invalid argument to Phi node: ") << E;
    return;
  }

  // Set the Phi argument.
  if (!OverwriteInstructions)
    assert(!Ph->values()[Idx].get() && "We already handled this node.");

  Ph->values().resize(Arena, Idx+1, nullptr);  // Make room if we need to.
  Ph->values()[Idx].reset(I);

  // Futures don't yet have types...
  // TODO: We could wind up with untyped phi nodes.
  if (isa<Future>(I))
    return;

  // Update the type of the Phi node.
  // All phi arguments must have the exact same type.
  if (Idx == 0 && Ph->baseType().Base == BaseType::BT_Void) {
    // Set the initial type of the Phi node.
    Ph->setBaseType(I->baseType());
  }
  else if (Ph->baseType() != I->baseType()) {
    diag.error("Type mismatch in branch: ")
      << I << " does not have type " << Ph->baseType().getTypeName();
  }
}


Goto* CFGBuilder::newGoto(BasicBlock *B, SExpr* Result) {
  assert(CurrentBB && "No current block.");

  unsigned Idx = B->addPredecessor(CurrentBB);
  if (Result) {
    assert(B->arguments().size() == 1);
    Phi *Ph = B->arguments()[0];
    setPhiArgument(Ph, Result, Idx);
  }

  auto *Nt = new (Arena) Goto(B, Idx);
  endBlock(Nt);
  return Nt;
}


Goto* CFGBuilder::newGoto(BasicBlock *B, ArrayRef<SExpr*> Args) {
  assert(CurrentBB && "No current block.");
  assert(B->arguments().size() == Args.size() && "Wrong number of args.");

  unsigned Idx = B->addPredecessor(CurrentBB);
  for (unsigned i = 0, n = Args.size(); i < n; ++i) {
    Phi *Ph = B->arguments()[i];
    setPhiArgument(Ph, Args[i], Idx);
  }

  auto *Nt = new (Arena) Goto(B, Idx);
  endBlock(Nt);
  return Nt;
}


// This is a really a reducer method, not a builder method,
// But it's shared between CopyReducer and InplaceReducer.
void CFGBuilder::rewritePhiArg(SExpr *Ne, Goto *NG, SExpr *Res) {
  // Ne is what the Phi node was rewritten to.
  Phi *Ph = dyn_cast_or_null<Phi>(Ne);
  if (Ph && Ph->block() == NG->targetBlock()) {
    // The blocks match, so we know that Ph is a rewritten Phi node.
    // (The original might have been eliminated by rewriting to something else.)
    setPhiArgument(Ph, Res, NG->phiIndex());
  }
}


}  // end namespace til
}  // end namespace ohmu

