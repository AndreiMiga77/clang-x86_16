//===-- I8086ISelDAGToDAG.cpp - DAG->DAG selector for I8086 ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Instruction selector for the 8086.  The addressing-mode matcher is written
// from scratch (the 8086 has no SIB byte and a restricted base/index set), see
// CODEGEN_PLAN.md §7.2.
//
//===----------------------------------------------------------------------===//

#include "I8086.h"
#include "I8086SelectionDAGInfo.h"
#include "I8086TargetMachine.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/Debug.h"
using namespace llvm;

#define DEBUG_TYPE "i8086-isel"
#define PASS_NAME "I8086 DAG->DAG Pattern Instruction Selection"

namespace {
class I8086DAGToDAGISel : public SelectionDAGISel {
public:
  I8086DAGToDAGISel() = delete;
  I8086DAGToDAGISel(I8086TargetMachine &TM, CodeGenOptLevel OptLevel)
      : SelectionDAGISel(TM, OptLevel) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    Subtarget = &MF.getSubtarget<I8086Subtarget>();
    return SelectionDAGISel::runOnMachineFunction(MF);
  }

private:
  // Set per function; consulted by generated pattern predicates (e.g. FastEA).
  const I8086Subtarget *Subtarget = nullptr;

#include "I8086GenDAGISel.inc"

  void Select(SDNode *N) override;
  bool selectMemAddr(SDValue N, SDValue &Base, SDValue &Index, SDValue &Disp,
                     SDValue &Seg);
  bool selectMemAddrIdx(SDValue N, SDValue &Base, SDValue &Index, SDValue &Disp,
                        SDValue &Seg);
  bool SelectInlineAsmMemoryOperand(const SDValue &Op,
                                    InlineAsm::ConstraintCode ConstraintID,
                                    std::vector<SDValue> &OutOps) override;
};

class I8086DAGToDAGISelLegacy : public SelectionDAGISelLegacy {
public:
  static char ID;
  I8086DAGToDAGISelLegacy(I8086TargetMachine &TM, CodeGenOptLevel OptLevel)
      : SelectionDAGISelLegacy(
            ID, std::make_unique<I8086DAGToDAGISel>(TM, OptLevel)) {}
};
} // namespace

char I8086DAGToDAGISelLegacy::ID;

INITIALIZE_PASS(I8086DAGToDAGISelLegacy, DEBUG_TYPE, PASS_NAME, false, false)

FunctionPass *llvm::createI8086ISelDag(I8086TargetMachine &TM,
                                       CodeGenOptLevel OptLevel) {
  return new I8086DAGToDAGISelLegacy(TM, OptLevel);
}

// Match a 16-bit address into (base, index, disp, seg).  base/index are GR16
// registers or NoRegister; disp is a target constant or a symbolic reference;
// seg is always NoRegister (tiny model).
bool I8086DAGToDAGISel::selectMemAddr(SDValue N, SDValue &Base, SDValue &Index,
                                      SDValue &Disp, SDValue &Seg) {
  SDLoc dl(N);
  SDValue NoReg = CurDAG->getRegister(0, MVT::i16);
  Index = NoReg;
  Seg = NoReg;

  // Frame index (stack slot).
  if (FrameIndexSDNode *FIN = dyn_cast<FrameIndexSDNode>(N)) {
    Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), MVT::i16);
    Disp = CurDAG->getTargetConstant(0, dl, MVT::i16);
    return true;
  }

  // (add X, constant)
  if (N.getOpcode() == ISD::ADD) {
    if (ConstantSDNode *C = dyn_cast<ConstantSDNode>(N.getOperand(1))) {
      SDValue X = N.getOperand(0);
      int64_t Off = C->getSExtValue();
      if (FrameIndexSDNode *FIN = dyn_cast<FrameIndexSDNode>(X))
        Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), MVT::i16);
      else
        Base = X;
      Disp = CurDAG->getSignedTargetConstant(Off, dl, MVT::i16);
      return true;
    }
  }

  // Symbolic / direct [disp16] via the address wrapper.
  if (N.getOpcode() == I8086ISD::Wrapper) {
    SDValue N0 = N.getOperand(0);
    Base = NoReg;
    if (auto *G = dyn_cast<GlobalAddressSDNode>(N0)) {
      Disp = CurDAG->getTargetGlobalAddress(G->getGlobal(), dl, MVT::i16,
                                            G->getOffset());
      return true;
    }
    if (auto *E = dyn_cast<ExternalSymbolSDNode>(N0)) {
      Disp = CurDAG->getTargetExternalSymbol(E->getSymbol(), MVT::i16);
      return true;
    }
    if (auto *BA = dyn_cast<BlockAddressSDNode>(N0)) {
      Disp = CurDAG->getTargetBlockAddress(BA->getBlockAddress(), MVT::i16);
      return true;
    }
  }

  // Constant address -> direct [disp16].
  if (ConstantSDNode *C = dyn_cast<ConstantSDNode>(N)) {
    Base = NoReg;
    Disp = CurDAG->getSignedTargetConstant(C->getSExtValue(), dl, MVT::i16);
    return true;
  }

  // Plain register base: [reg].
  Base = N;
  Disp = CurDAG->getTargetConstant(0, dl, MVT::i16);
  return true;
}

// Match a two-register [base+index] (+ constant displacement) address.  Folding
// the address arithmetic into the memory operand saves the explicit add, but the
// indexed effective address costs 2-3 clocks more per access than a single
// register -- so this is only a win when the address feeds exactly one memory
// op.  The one-use guards enforce that; a multiply-used address falls through to
// selectMemAddr, which materializes the add once and uses [reg] at each access.
bool I8086DAGToDAGISel::selectMemAddrIdx(SDValue N, SDValue &Base,
                                         SDValue &Index, SDValue &Disp,
                                         SDValue &Seg) {
  SDLoc dl(N);
  Seg = CurDAG->getRegister(0, MVT::i16);
  int64_t Off = 0;

  // Optional outer (add addr, const) supplies the displacement.
  if (N.getOpcode() == ISD::ADD && N.hasOneUse())
    if (ConstantSDNode *C = dyn_cast<ConstantSDNode>(N.getOperand(1))) {
      Off = C->getSExtValue();
      N = N.getOperand(0);
    }

  // Core must be (reg + reg), single-use, with neither side a constant or a
  // frame index (those are single-register [reg+disp] addresses).
  if (N.getOpcode() != ISD::ADD || !N.hasOneUse())
    return false;
  SDValue A = N.getOperand(0), B = N.getOperand(1);
  if (isa<ConstantSDNode>(A) || isa<ConstantSDNode>(B) ||
      isa<FrameIndexSDNode>(A) || isa<FrameIndexSDNode>(B))
    return false;

  // Either assignment is a legal base+index sum; the register classes on the
  // operand (BASE16 / IDX16) force a valid {BX,BP}+{SI,DI} pairing.
  Base = A;
  Index = B;
  Disp = CurDAG->getSignedTargetConstant(Off, dl, MVT::i16);
  return true;
}

void I8086DAGToDAGISel::Select(SDNode *N) {
  if (N->isMachineOpcode()) {
    N->setNodeId(-1);
    return;
  }

  // A frame index used as a value materializes its address.
  if (N->getOpcode() == ISD::FrameIndex) {
    int FI = cast<FrameIndexSDNode>(N)->getIndex();
    SDValue TFI = CurDAG->getTargetFrameIndex(FI, MVT::i16);
    SDValue Zero = CurDAG->getTargetConstant(0, SDLoc(N), MVT::i16);
    CurDAG->SelectNodeTo(N, I8086::ADDframe, MVT::i16, TFI, Zero);
    return;
  }

  SelectCode(N);
}

bool I8086DAGToDAGISel::SelectInlineAsmMemoryOperand(
    const SDValue &Op, InlineAsm::ConstraintCode ConstraintID,
    std::vector<SDValue> &OutOps) {
  SDValue Base, Index, Disp, Seg;
  if (!selectMemAddr(Op, Base, Index, Disp, Seg))
    return true;
  OutOps.push_back(Base);
  OutOps.push_back(Index);
  OutOps.push_back(Disp);
  OutOps.push_back(Seg);
  return false;
}
