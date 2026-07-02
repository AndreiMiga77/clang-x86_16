//===-- I8086InstrInfo.cpp - I8086 Instruction Information ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "I8086InstrInfo.h"
#include "I8086.h"
#include "I8086Subtarget.h"
#include "MCTargetDesc/I8086BaseInfo.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCInstrDesc.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"

using namespace llvm;

#define GET_INSTRINFO_CTOR_DTOR
#include "I8086GenInstrInfo.inc"

void I8086InstrInfo::anchor() {}

I8086InstrInfo::I8086InstrInfo(const I8086Subtarget &STI)
    : I8086GenInstrInfo(STI, RI, I8086::ADJCALLSTACKDOWN,
                        I8086::ADJCALLSTACKUP),
      RI() {}

//===----------------------------------------------------------------------===//
// Condition-code <-> Jcc opcode mapping.  Unlike a target with a single
// parameterized conditional branch, the 8086 has 16 distinct Jcc opcodes.
//===----------------------------------------------------------------------===//

static I8086CC::CondCode getCondFromBranchOpc(unsigned Opc) {
  switch (Opc) {
  case I8086::JO:  return I8086CC::COND_O;
  case I8086::JNO: return I8086CC::COND_NO;
  case I8086::JB:  return I8086CC::COND_B;
  case I8086::JAE: return I8086CC::COND_AE;
  case I8086::JE:  return I8086CC::COND_E;
  case I8086::JNE: return I8086CC::COND_NE;
  case I8086::JBE: return I8086CC::COND_BE;
  case I8086::JA:  return I8086CC::COND_A;
  case I8086::JS:  return I8086CC::COND_S;
  case I8086::JNS: return I8086CC::COND_NS;
  case I8086::JP:  return I8086CC::COND_P;
  case I8086::JNP: return I8086CC::COND_NP;
  case I8086::JL:  return I8086CC::COND_L;
  case I8086::JGE: return I8086CC::COND_GE;
  case I8086::JLE: return I8086CC::COND_LE;
  case I8086::JG:  return I8086CC::COND_G;
  default:         return I8086CC::COND_INVALID;
  }
}

static unsigned getBranchOpcFromCond(I8086CC::CondCode CC) {
  switch (CC) {
  case I8086CC::COND_O:  return I8086::JO;
  case I8086CC::COND_NO: return I8086::JNO;
  case I8086CC::COND_B:  return I8086::JB;
  case I8086CC::COND_AE: return I8086::JAE;
  case I8086CC::COND_E:  return I8086::JE;
  case I8086CC::COND_NE: return I8086::JNE;
  case I8086CC::COND_BE: return I8086::JBE;
  case I8086CC::COND_A:  return I8086::JA;
  case I8086CC::COND_S:  return I8086::JS;
  case I8086CC::COND_NS: return I8086::JNS;
  case I8086CC::COND_P:  return I8086::JP;
  case I8086CC::COND_NP: return I8086::JNP;
  case I8086CC::COND_L:  return I8086::JL;
  case I8086CC::COND_GE: return I8086::JGE;
  case I8086CC::COND_LE: return I8086::JLE;
  case I8086CC::COND_G:  return I8086::JG;
  default: llvm_unreachable("Invalid condition code");
  }
}

static I8086CC::CondCode getOppositeCond(I8086CC::CondCode CC) {
  switch (CC) {
  case I8086CC::COND_O:  return I8086CC::COND_NO;
  case I8086CC::COND_NO: return I8086CC::COND_O;
  case I8086CC::COND_B:  return I8086CC::COND_AE;
  case I8086CC::COND_AE: return I8086CC::COND_B;
  case I8086CC::COND_E:  return I8086CC::COND_NE;
  case I8086CC::COND_NE: return I8086CC::COND_E;
  case I8086CC::COND_BE: return I8086CC::COND_A;
  case I8086CC::COND_A:  return I8086CC::COND_BE;
  case I8086CC::COND_S:  return I8086CC::COND_NS;
  case I8086CC::COND_NS: return I8086CC::COND_S;
  case I8086CC::COND_P:  return I8086CC::COND_NP;
  case I8086CC::COND_NP: return I8086CC::COND_P;
  case I8086CC::COND_L:  return I8086CC::COND_GE;
  case I8086CC::COND_GE: return I8086CC::COND_L;
  case I8086CC::COND_LE: return I8086CC::COND_G;
  case I8086CC::COND_G:  return I8086CC::COND_LE;
  default: llvm_unreachable("Invalid condition code");
  }
}

//===----------------------------------------------------------------------===//
// Register moves and stack spills/reloads.
//===----------------------------------------------------------------------===//

void I8086InstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator I,
                                 const DebugLoc &DL, Register DestReg,
                                 Register SrcReg, bool KillSrc,
                                 bool RenamableDest, bool RenamableSrc) const {
  unsigned Opc;
  if (I8086::GR16RegClass.contains(DestReg, SrcReg))
    Opc = I8086::MOV16rr;
  else if (I8086::GR8RegClass.contains(DestReg, SrcReg))
    Opc = I8086::MOV8rr;
  else
    llvm_unreachable("Impossible reg-to-reg copy");

  BuildMI(MBB, I, DL, get(Opc), DestReg)
      .addReg(SrcReg, getKillRegState(KillSrc));
}

// Add a frame-index memory operand (base=FI, index=none, disp=0, seg=none).
static void addFrameMemOperand(MachineInstrBuilder &MIB, int FrameIdx) {
  MIB.addFrameIndex(FrameIdx)  // base
      .addReg(0)               // index
      .addImm(0)               // disp
      .addReg(0);              // segment
}

void I8086InstrInfo::storeRegToStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI, Register SrcReg,
    bool isKill, int FrameIdx, const TargetRegisterClass *RC, Register VReg,
    MachineInstr::MIFlag Flags) const {
  DebugLoc DL;
  if (MI != MBB.end())
    DL = MI->getDebugLoc();
  MachineFunction &MF = *MBB.getParent();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  MachineMemOperand *MMO = MF.getMachineMemOperand(
      MachinePointerInfo::getFixedStack(MF, FrameIdx),
      MachineMemOperand::MOStore, MFI.getObjectSize(FrameIdx),
      MFI.getObjectAlign(FrameIdx));

  unsigned Opc;
  if (I8086::GR16RegClass.hasSubClassEq(RC))
    Opc = I8086::MOV16mr;
  else if (I8086::GR8RegClass.hasSubClassEq(RC))
    Opc = I8086::MOV8mr;
  else
    llvm_unreachable("Cannot store this register to a stack slot!");

  MachineInstrBuilder MIB = BuildMI(MBB, MI, DL, get(Opc));
  addFrameMemOperand(MIB, FrameIdx);
  MIB.addReg(SrcReg, getKillRegState(isKill)).addMemOperand(MMO);
}

void I8086InstrInfo::loadRegFromStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI, Register DestReg,
    int FrameIdx, const TargetRegisterClass *RC, Register VReg, unsigned SubReg,
    MachineInstr::MIFlag Flags) const {
  DebugLoc DL;
  if (MI != MBB.end())
    DL = MI->getDebugLoc();
  MachineFunction &MF = *MBB.getParent();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  MachineMemOperand *MMO = MF.getMachineMemOperand(
      MachinePointerInfo::getFixedStack(MF, FrameIdx),
      MachineMemOperand::MOLoad, MFI.getObjectSize(FrameIdx),
      MFI.getObjectAlign(FrameIdx));

  unsigned Opc;
  if (I8086::GR16RegClass.hasSubClassEq(RC))
    Opc = I8086::MOV16rm;
  else if (I8086::GR8RegClass.hasSubClassEq(RC))
    Opc = I8086::MOV8rm;
  else
    llvm_unreachable("Cannot load this register from a stack slot!");

  MachineInstrBuilder MIB =
      BuildMI(MBB, MI, DL, get(Opc), DestReg);
  addFrameMemOperand(MIB, FrameIdx);
  MIB.addMemOperand(MMO);
}

//===----------------------------------------------------------------------===//
// Branch analysis.
//===----------------------------------------------------------------------===//

unsigned I8086InstrInfo::removeBranch(MachineBasicBlock &MBB,
                                      int *BytesRemoved) const {
  if (BytesRemoved)
    *BytesRemoved = 0;
  MachineBasicBlock::iterator I = MBB.end();
  unsigned Count = 0;
  while (I != MBB.begin()) {
    --I;
    if (I->isDebugInstr())
      continue;
    if (I->getOpcode() != I8086::JMP16 &&
        getCondFromBranchOpc(I->getOpcode()) == I8086CC::COND_INVALID)
      break;
    if (BytesRemoved)
      *BytesRemoved += getInstSizeInBytes(*I);
    I->eraseFromParent();
    I = MBB.end();
    ++Count;
  }
  return Count;
}

bool I8086InstrInfo::reverseBranchCondition(
    SmallVectorImpl<MachineOperand> &Cond) const {
  assert(Cond.size() == 1 && "Invalid branch condition!");
  Cond[0].setImm(getOppositeCond(static_cast<I8086CC::CondCode>(Cond[0].getImm())));
  return false;
}

bool I8086InstrInfo::analyzeBranch(MachineBasicBlock &MBB,
                                   MachineBasicBlock *&TBB,
                                   MachineBasicBlock *&FBB,
                                   SmallVectorImpl<MachineOperand> &Cond,
                                   bool AllowModify) const {
  MachineBasicBlock::iterator I = MBB.end();
  while (I != MBB.begin()) {
    --I;
    if (I->isDebugInstr())
      continue;
    if (!isUnpredicatedTerminator(*I))
      break;
    if (!I->isBranch())
      return true;

    // Indirect branches can't be analyzed.
    if (I->getOpcode() == I8086::JMP16r || I->getOpcode() == I8086::JMP16m)
      return true;

    // Unconditional branch.
    if (I->getOpcode() == I8086::JMP16) {
      if (!AllowModify) {
        TBB = I->getOperand(0).getMBB();
        continue;
      }
      MBB.erase(std::next(I), MBB.end());
      Cond.clear();
      FBB = nullptr;
      if (MBB.isLayoutSuccessor(I->getOperand(0).getMBB())) {
        TBB = nullptr;
        I->eraseFromParent();
        I = MBB.end();
        continue;
      }
      TBB = I->getOperand(0).getMBB();
      continue;
    }

    // Conditional branch.
    I8086CC::CondCode BranchCode = getCondFromBranchOpc(I->getOpcode());
    if (BranchCode == I8086CC::COND_INVALID)
      return true;

    if (Cond.empty()) {
      FBB = TBB;
      TBB = I->getOperand(0).getMBB();
      Cond.push_back(MachineOperand::CreateImm(BranchCode));
      continue;
    }

    assert(Cond.size() == 1);
    assert(TBB);
    if (TBB != I->getOperand(0).getMBB())
      return true;
    if (static_cast<I8086CC::CondCode>(Cond[0].getImm()) == BranchCode)
      continue;
    return true;
  }
  return false;
}

unsigned I8086InstrInfo::insertBranch(MachineBasicBlock &MBB,
                                      MachineBasicBlock *TBB,
                                      MachineBasicBlock *FBB,
                                      ArrayRef<MachineOperand> Cond,
                                      const DebugLoc &DL, int *BytesAdded) const {
  assert(TBB && "insertBranch must not be told to insert a fallthrough");
  assert((Cond.size() == 1 || Cond.size() == 0) &&
         "i8086 branch conditions have one component!");
  if (BytesAdded)
    *BytesAdded = 0;

  if (Cond.empty()) {
    assert(!FBB && "Unconditional branch with multiple successors!");
    BuildMI(&MBB, DL, get(I8086::JMP16)).addMBB(TBB);
    if (BytesAdded)
      *BytesAdded += 3; // E9 + rel16
    return 1;
  }

  unsigned Count = 0;
  unsigned Opc = getBranchOpcFromCond(static_cast<I8086CC::CondCode>(Cond[0].getImm()));
  BuildMI(&MBB, DL, get(Opc)).addMBB(TBB);
  if (BytesAdded)
    *BytesAdded += 2; // Jcc + rel8
  ++Count;
  if (FBB) {
    BuildMI(&MBB, DL, get(I8086::JMP16)).addMBB(FBB);
    if (BytesAdded)
      *BytesAdded += 3;
    ++Count;
  }
  return Count;
}

unsigned I8086InstrInfo::getInstSizeInBytes(const MachineInstr &MI) const {
  const MCInstrDesc &Desc = MI.getDesc();
  switch (Desc.getOpcode()) {
  case TargetOpcode::CFI_INSTRUCTION:
  case TargetOpcode::EH_LABEL:
  case TargetOpcode::IMPLICIT_DEF:
  case TargetOpcode::KILL:
  case TargetOpcode::DBG_VALUE:
    return 0;
  case TargetOpcode::COPY:
    return 2; // becomes a MOV reg,reg
  case TargetOpcode::INLINEASM:
  case TargetOpcode::INLINEASM_BR: {
    const MachineFunction *MF = MI.getParent()->getParent();
    const TargetInstrInfo &TII = *MF->getSubtarget().getInstrInfo();
    return TII.getInlineAsmLength(MI.getOperand(0).getSymbolName(),
                                  *MF->getTarget().getMCAsmInfo());
  }
  }

  // Our instructions are variable-length (Size == 0 in the .td); compute the
  // exact size from the encoding form and operands (mirrors the code emitter).
  uint64_t TSFlags = Desc.TSFlags;
  unsigned Form = I8086II::getForm(TSFlags);
  if (Form == I8086II::Pseudo)
    return 0;

  unsigned Size = 1; // opcode

  // Logical operand list, skipping operands tied to an earlier one (same as
  // the emitter), so memory operands are located correctly.
  SmallVector<unsigned, 8> Ops;
  for (unsigned I = 0, E = Desc.getNumOperands(); I != E; ++I) {
    if (Desc.getOperandConstraint(I, MCOI::TIED_TO) != -1)
      continue;
    Ops.push_back(I);
  }

  bool HasModRMReg = Form == I8086II::MRMDestReg || Form == I8086II::MRMSrcReg ||
                     I8086II::isMRMGroupReg(Form);
  int MemLogical = -1;
  if (Form == I8086II::MRMDestMem)
    MemLogical = 0;
  else if (Form == I8086II::MRMSrcMem)
    MemLogical = 1;
  else if (I8086II::isMRMGroup(Form) && !I8086II::isMRMGroupReg(Form))
    MemLogical = 0;

  if (HasModRMReg)
    Size += 1; // ModR/M byte
  if (MemLogical >= 0) {
    Size += 1; // ModR/M byte
    unsigned P = Ops[MemLogical];
    MCRegister Base = MI.getOperand(P).getReg();
    MCRegister Index = MI.getOperand(P + 1).getReg();
    const MachineOperand &Disp = MI.getOperand(P + 2);
    if (MI.getOperand(P + 3).getReg())
      Size += 1; // segment override prefix

    // Normalize a lone SI/DI base into the index slot (as the emitter does).
    if (!Index && (Base == I8086::SI || Base == I8086::DI)) {
      Index = Base;
      Base = MCRegister();
    }
    bool Direct = !Base && !Index;
    bool BPOnly = (Base == I8086::BP && !Index); // ModR/M rm == 6
    if (Direct || !Disp.isImm())
      Size += 2; // [disp16] or an unresolved symbolic displacement
    else if (Disp.getImm() == 0 && !BPOnly)
      Size += 0; // no displacement
    else if (isInt<8>(Disp.getImm()))
      Size += 1; // disp8
    else
      Size += 2; // disp16
  }

  switch (I8086II::getImmType(TSFlags)) {
  case I8086II::Imm8:
  case I8086II::Rel8:
    Size += 1;
    break;
  case I8086II::Imm16:
  case I8086II::Rel16:
    Size += 2;
    break;
  }
  if (Form == I8086II::RawFrmFar)
    Size += 4; // off16 + seg16
  return Size;
}

MachineBasicBlock *
I8086InstrInfo::getBranchDestBlock(const MachineInstr &MI) const {
  assert((MI.getOpcode() == I8086::JMP16 ||
          getCondFromBranchOpc(MI.getOpcode()) != I8086CC::COND_INVALID) &&
         "expected a direct branch");
  return MI.getOperand(0).getMBB();
}

bool I8086InstrInfo::isBranchOffsetInRange(unsigned BranchOpc,
                                           int64_t BrOffset) const {
  // The near JMP is rel16 and, thanks to 16-bit wraparound, reaches anywhere in
  // the 64K segment; conditional Jcc are rel8 only.
  if (BranchOpc == I8086::JMP16)
    return true;
  assert(getCondFromBranchOpc(BranchOpc) != I8086CC::COND_INVALID &&
         "expected a conditional branch");
  return isInt<8>(BrOffset);
}

void I8086InstrInfo::insertIndirectBranch(MachineBasicBlock &MBB,
                                          MachineBasicBlock &NewDestBB,
                                          MachineBasicBlock &RestoreBB,
                                          const DebugLoc &DL, int64_t BrOffset,
                                          RegScavenger *RS) const {
  // Despite the name, branch relaxation uses this to insert a long *direct*
  // unconditional branch; the near JMP always reaches within the segment.
  BuildMI(&MBB, DL, get(I8086::JMP16)).addMBB(&NewDestBB);
}
