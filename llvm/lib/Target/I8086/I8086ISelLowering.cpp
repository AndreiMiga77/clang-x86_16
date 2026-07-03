//===-- I8086ISelLowering.cpp - I8086 DAG Lowering Implementation ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "I8086ISelLowering.h"
#include "I8086.h"
#include "I8086MachineFunctionInfo.h"
#include "I8086Subtarget.h"
#include "I8086SelectionDAGInfo.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define DEBUG_TYPE "i8086-lower"

#include "I8086GenCallingConv.inc"

I8086TargetLowering::I8086TargetLowering(const TargetMachine &TM,
                                         const I8086Subtarget &STI)
    : TargetLowering(TM, STI) {
  addRegisterClass(MVT::i8, &I8086::GR8RegClass);
  addRegisterClass(MVT::i16, &I8086::GR16RegClass);
  computeRegisterProperties(STI.getRegisterInfo());

  setStackPointerRegisterToSaveRestore(I8086::SP);
  setBooleanContents(ZeroOrOneBooleanContent);

  // No MOVZX/MOVSX and 8-bit ops don't auto-zero the high byte, so extending
  // loads are expanded into a byte load + an explicit extend (patterns below).
  for (MVT VT : MVT::integer_valuetypes()) {
    setLoadExtAction(ISD::EXTLOAD, VT, MVT::i1, Promote);
    setLoadExtAction(ISD::SEXTLOAD, VT, MVT::i1, Promote);
    setLoadExtAction(ISD::ZEXTLOAD, VT, MVT::i1, Promote);
    setLoadExtAction(ISD::EXTLOAD, VT, MVT::i8, Expand);
    setLoadExtAction(ISD::ZEXTLOAD, VT, MVT::i8, Expand);
    setLoadExtAction(ISD::SEXTLOAD, VT, MVT::i8, Expand);
    setLoadExtAction(ISD::SEXTLOAD, VT, MVT::i16, Expand);
  }
  setTruncStoreAction(MVT::i16, MVT::i8, Expand);

  // i8->i16 sign extension uses CBW via the MOVSX16r8 pseudo (see the .td);
  // zero/any extension are handled by patterns.

  // The 8086 shifts and rotates only by 1 or CL.  SHL/SRL/SRA and ROTL/ROTR are
  // selected via the Shl*/Shr*/Sar*/Rol*/Ror* custom-inserter pseudos, which move
  // the count into CL and use the shift/rotate-by-CL instruction.  (ROTL/ROTR are
  // left Legal so the pseudo patterns match instead of the default expansion.)
  for (MVT VT : {MVT::i8, MVT::i16}) {
    setOperationAction(ISD::CTTZ, VT, Expand);
    setOperationAction(ISD::CTLZ, VT, Expand);
    setOperationAction(ISD::CTPOP, VT, Expand);
    setOperationAction(ISD::BSWAP, VT, Expand);
    setOperationAction(ISD::SHL_PARTS, VT, Expand);
    setOperationAction(ISD::SRL_PARTS, VT, Expand);
    setOperationAction(ISD::SRA_PARTS, VT, Expand);
    setOperationAction(ISD::SELECT, VT, Expand);
    setOperationAction(ISD::SETCC, VT, Custom);
    setOperationAction(ISD::SELECT_CC, VT, Custom);
    setOperationAction(ISD::BR_CC, VT, Custom);
  }

  setOperationAction(ISD::GlobalAddress, MVT::i16, Custom);
  setOperationAction(ISD::ExternalSymbol, MVT::i16, Custom);
  setOperationAction(ISD::BlockAddress, MVT::i16, Custom);
  setOperationAction(ISD::JumpTable, MVT::i16, Custom);
  setOperationAction(ISD::BR_JT, MVT::Other, Expand);
  setOperationAction(ISD::BRCOND, MVT::Other, Expand);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i1, Expand);

  // 16-bit multiply/divide use the 8086's native MUL/DIV (implicit AX/DX) via
  // the UMULLOHI16/UDIVREM16/... pseudos: MUL yields the full 32-bit product
  // (AX=lo, DX=hi) and DIV yields quotient (AX) + remainder (DX).  The plain
  // MUL/MULH/UDIV/UREM/... forms expand to the LOHI/DIVREM nodes; 8-bit ops are
  // promoted to 16-bit.
  for (auto Op : {ISD::MUL, ISD::MULHS, ISD::MULHU, ISD::UDIV, ISD::UREM,
                  ISD::SDIV, ISD::SREM}) {
    setOperationAction(Op, MVT::i8, Promote);
    setOperationAction(Op, MVT::i16, Expand);
  }
  for (auto Op : {ISD::UMUL_LOHI, ISD::SMUL_LOHI, ISD::UDIVREM, ISD::SDIVREM})
    setOperationAction(Op, MVT::i16, Legal);

  // Wide (i32/i64) add/sub: keep them as glue-carry ADDC/ADDE / SUBC/SUBE so the
  // type legalizer expands them into an ADD/ADC (SUB/SBB) carry chain rather than
  // the default cmp-and-branch carry synthesis.  These nodes are not Legal by
  // default in this LLVM version, so request it explicitly.
  for (MVT VT : {MVT::i8, MVT::i16})
    for (auto Op : {ISD::ADDC, ISD::ADDE, ISD::SUBC, ISD::SUBE})
      setOperationAction(Op, VT, Legal);

  setOperationAction(ISD::DYNAMIC_STACKALLOC, MVT::i16, Expand);
  setOperationAction(ISD::STACKSAVE, MVT::Other, Expand);
  setOperationAction(ISD::STACKRESTORE, MVT::Other, Expand);
  setOperationAction(ISD::VASTART, MVT::Other, Custom);
  setOperationAction(ISD::VAARG, MVT::Other, Expand);
  setOperationAction(ISD::VAEND, MVT::Other, Expand);
  setOperationAction(ISD::VACOPY, MVT::Other, Expand);

  // A constant rotate by more than half the width is a shorter rotate in the
  // opposite direction (rol x,12 == ror x,4; for bytes rol x,6 == ror x,2).
  setTargetDAGCombine({ISD::ROTL, ISD::ROTR});

  setMinFunctionAlignment(Align(1));
  setMaxAtomicSizeInBitsSupported(0);
}

SDValue I8086TargetLowering::PerformDAGCombine(SDNode *N,
                                               DAGCombinerInfo &DCI) const {
  unsigned Opc = N->getOpcode();
  if (Opc != ISD::ROTL && Opc != ISD::ROTR)
    return SDValue();

  auto *C = dyn_cast<ConstantSDNode>(N->getOperand(1));
  if (!C)
    return SDValue();

  EVT VT = N->getValueType(0);
  unsigned Bits = VT.getSizeInBits();
  unsigned Amt = C->getZExtValue() & (Bits - 1);
  // Amounts in the lower half (0..Bits/2) are already the cheap ones; only flip
  // direction for the upper half (e.g. 9..15 for i16, 5..7 for i8).
  if (Amt <= Bits / 2)
    return SDValue();

  unsigned NewOpc = Opc == ISD::ROTL ? ISD::ROTR : ISD::ROTL;
  SDLoc dl(N);
  SDValue NewAmt =
      DCI.DAG.getConstant(Bits - Amt, dl, N->getOperand(1).getValueType());
  return DCI.DAG.getNode(NewOpc, dl, VT, N->getOperand(0), NewAmt);
}

SDValue I8086TargetLowering::LowerOperation(SDValue Op,
                                            SelectionDAG &DAG) const {
  switch (Op.getOpcode()) {
  case ISD::GlobalAddress:  return LowerGlobalAddress(Op, DAG);
  case ISD::ExternalSymbol: return LowerExternalSymbol(Op, DAG);
  case ISD::BlockAddress:   return LowerBlockAddress(Op, DAG);
  case ISD::JumpTable:      return LowerJumpTable(Op, DAG);
  case ISD::SETCC:          return LowerSETCC(Op, DAG);
  case ISD::BR_CC:          return LowerBR_CC(Op, DAG);
  case ISD::SELECT_CC:      return LowerSELECT_CC(Op, DAG);
  case ISD::VASTART:        return LowerVASTART(Op, DAG);
  default:
    llvm_unreachable("unimplemented operation");
  }
}

//===----------------------------------------------------------------------===//
// Address wrappers.
//===----------------------------------------------------------------------===//

SDValue I8086TargetLowering::LowerGlobalAddress(SDValue Op,
                                                SelectionDAG &DAG) const {
  const GlobalValue *GV = cast<GlobalAddressSDNode>(Op)->getGlobal();
  int64_t Offset = cast<GlobalAddressSDNode>(Op)->getOffset();
  EVT PtrVT = Op.getValueType();
  SDValue Result = DAG.getTargetGlobalAddress(GV, SDLoc(Op), PtrVT, Offset);
  return DAG.getNode(I8086ISD::Wrapper, SDLoc(Op), PtrVT, Result);
}

SDValue I8086TargetLowering::LowerExternalSymbol(SDValue Op,
                                                 SelectionDAG &DAG) const {
  const char *Sym = cast<ExternalSymbolSDNode>(Op)->getSymbol();
  EVT PtrVT = Op.getValueType();
  SDValue Result = DAG.getTargetExternalSymbol(Sym, PtrVT);
  return DAG.getNode(I8086ISD::Wrapper, SDLoc(Op), PtrVT, Result);
}

SDValue I8086TargetLowering::LowerBlockAddress(SDValue Op,
                                               SelectionDAG &DAG) const {
  const BlockAddress *BA = cast<BlockAddressSDNode>(Op)->getBlockAddress();
  EVT PtrVT = Op.getValueType();
  SDValue Result = DAG.getTargetBlockAddress(BA, PtrVT);
  return DAG.getNode(I8086ISD::Wrapper, SDLoc(Op), PtrVT, Result);
}

SDValue I8086TargetLowering::LowerJumpTable(SDValue Op,
                                            SelectionDAG &DAG) const {
  JumpTableSDNode *JT = cast<JumpTableSDNode>(Op);
  EVT PtrVT = Op.getValueType();
  SDValue Result = DAG.getTargetJumpTable(JT->getIndex(), PtrVT);
  return DAG.getNode(I8086ISD::Wrapper, SDLoc(Op), PtrVT, Result);
}

//===----------------------------------------------------------------------===//
// Comparisons, branches and selects.
//===----------------------------------------------------------------------===//

// Map an ISD condition code to an 8086 condition code, emitting the CMP that
// produces FLAGS.  The 8086 has direct signed and unsigned conditions, so no
// operand swapping is required.
static SDValue emitCmp(SDValue &LHS, SDValue &RHS, SDValue &TargetCC,
                       ISD::CondCode CC, const SDLoc &dl, SelectionDAG &DAG) {
  I8086CC::CondCode TCC;
  switch (CC) {
  default: llvm_unreachable("Invalid integer condition!");
  case ISD::SETEQ:  TCC = I8086CC::COND_E;  break;
  case ISD::SETNE:  TCC = I8086CC::COND_NE; break;
  case ISD::SETULT: TCC = I8086CC::COND_B;  break;
  case ISD::SETULE: TCC = I8086CC::COND_BE; break;
  case ISD::SETUGT: TCC = I8086CC::COND_A;  break;
  case ISD::SETUGE: TCC = I8086CC::COND_AE; break;
  case ISD::SETLT:  TCC = I8086CC::COND_L;  break;
  case ISD::SETLE:  TCC = I8086CC::COND_LE; break;
  case ISD::SETGT:  TCC = I8086CC::COND_G;  break;
  case ISD::SETGE:  TCC = I8086CC::COND_GE; break;
  }
  TargetCC = DAG.getConstant(TCC, dl, MVT::i8);
  return DAG.getNode(I8086ISD::CMP, dl, MVT::Glue, LHS, RHS);
}

SDValue I8086TargetLowering::LowerBR_CC(SDValue Op, SelectionDAG &DAG) const {
  SDValue Chain = Op.getOperand(0);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(1))->get();
  SDValue LHS = Op.getOperand(2);
  SDValue RHS = Op.getOperand(3);
  SDValue Dest = Op.getOperand(4);
  SDLoc dl(Op);

  SDValue TargetCC;
  SDValue Flag = emitCmp(LHS, RHS, TargetCC, CC, dl, DAG);
  return DAG.getNode(I8086ISD::BR_CC, dl, Op.getValueType(), Chain, Dest,
                     TargetCC, Flag);
}

SDValue I8086TargetLowering::LowerSELECT_CC(SDValue Op,
                                            SelectionDAG &DAG) const {
  SDValue LHS = Op.getOperand(0);
  SDValue RHS = Op.getOperand(1);
  SDValue TrueV = Op.getOperand(2);
  SDValue FalseV = Op.getOperand(3);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(4))->get();
  SDLoc dl(Op);

  SDValue TargetCC;
  SDValue Flag = emitCmp(LHS, RHS, TargetCC, CC, dl, DAG);
  SDValue Ops[] = {TrueV, FalseV, TargetCC, Flag};
  return DAG.getNode(I8086ISD::SELECT_CC, dl, Op.getValueType(), Ops);
}

// The 8086 has no SETcc; a comparison result used as a value becomes a
// select of 1/0 (lowered to a control-flow diamond by the custom inserter).
SDValue I8086TargetLowering::LowerSETCC(SDValue Op, SelectionDAG &DAG) const {
  SDValue LHS = Op.getOperand(0);
  SDValue RHS = Op.getOperand(1);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(2))->get();
  SDLoc dl(Op);

  SDValue TargetCC;
  SDValue Flag = emitCmp(LHS, RHS, TargetCC, CC, dl, DAG);
  EVT VT = Op.getValueType();
  SDValue One = DAG.getConstant(1, dl, VT);
  SDValue Zero = DAG.getConstant(0, dl, VT);
  SDValue Ops[] = {One, Zero, TargetCC, Flag};
  return DAG.getNode(I8086ISD::SELECT_CC, dl, VT, Ops);
}

SDValue I8086TargetLowering::LowerVASTART(SDValue Op, SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();
  I8086MachineFunctionInfo *FuncInfo = MF.getInfo<I8086MachineFunctionInfo>();
  SDLoc dl(Op);
  SDValue FI = DAG.getFrameIndex(FuncInfo->getVarArgsFrameIndex(),
                                 getPointerTy(DAG.getDataLayout()));
  const Value *SV = cast<SrcValueSDNode>(Op.getOperand(2))->getValue();
  return DAG.getStore(Op.getOperand(0), dl, FI, Op.getOperand(1),
                      MachinePointerInfo(SV));
}

//===----------------------------------------------------------------------===//
// Calling convention.
//===----------------------------------------------------------------------===//

SDValue I8086TargetLowering::LowerFormalArguments(
    SDValue Chain, CallingConv::ID CallConv, bool isVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &dl,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {
  MachineFunction &MF = DAG.getMachineFunction();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  I8086MachineFunctionInfo *FuncInfo = MF.getInfo<I8086MachineFunctionInfo>();

  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, isVarArg, MF, ArgLocs, *DAG.getContext());
  CCInfo.AnalyzeFormalArguments(Ins, CC_I8086);

  if (isVarArg)
    FuncInfo->setVarArgsFrameIndex(
        MFI.CreateFixedObject(2, CCInfo.getStackSize(), true));

  for (unsigned i = 0, e = ArgLocs.size(); i != e; ++i) {
    CCValAssign &VA = ArgLocs[i];
    assert(VA.isMemLoc() && "i8086 passes all arguments on the stack");

    ISD::ArgFlagsTy Flags = Ins[i].Flags;
    if (Flags.isByVal()) {
      int FI = MFI.CreateFixedObject(Flags.getByValSize(),
                                     VA.getLocMemOffset(), true);
      InVals.push_back(DAG.getFrameIndex(FI, MVT::i16));
      continue;
    }

    unsigned ObjSize = VA.getLocVT().getSizeInBits() / 8;
    int FI = MFI.CreateFixedObject(ObjSize, VA.getLocMemOffset(), true);
    SDValue FIN = DAG.getFrameIndex(FI, MVT::i16);
    SDValue Val = DAG.getLoad(
        VA.getLocVT(), dl, Chain, FIN,
        MachinePointerInfo::getFixedStack(MF, FI));
    // i8 arguments are passed promoted to i16; truncate back.
    if (VA.getLocInfo() != CCValAssign::Full)
      Val = DAG.getNode(ISD::TRUNCATE, dl, VA.getValVT(), Val);
    InVals.push_back(Val);
  }

  // Stash the sret pointer so LowerReturn can return it in AX.
  for (unsigned i = 0, e = ArgLocs.size(); i != e; ++i) {
    if (Ins[i].Flags.isSRet()) {
      Register Reg = FuncInfo->getSRetReturnReg();
      if (!Reg) {
        Reg = MF.getRegInfo().createVirtualRegister(getRegClassFor(MVT::i16));
        FuncInfo->setSRetReturnReg(Reg);
      }
      Chain = DAG.getNode(ISD::TokenFactor, dl, MVT::Other,
                          DAG.getCopyToReg(DAG.getEntryNode(), dl, Reg,
                                           InVals[i]),
                          Chain);
    }
  }
  return Chain;
}

bool I8086TargetLowering::CanLowerReturn(
    CallingConv::ID CallConv, MachineFunction &MF, bool IsVarArg,
    const SmallVectorImpl<ISD::OutputArg> &Outs, LLVMContext &Context,
    const Type *RetTy) const {
  SmallVector<CCValAssign, 16> RVLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, RVLocs, Context);
  return CCInfo.CheckReturn(Outs, RetCC_I8086);
}

SDValue I8086TargetLowering::LowerReturn(
    SDValue Chain, CallingConv::ID CallConv, bool isVarArg,
    const SmallVectorImpl<ISD::OutputArg> &Outs,
    const SmallVectorImpl<SDValue> &OutVals, const SDLoc &dl,
    SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();
  SmallVector<CCValAssign, 16> RVLocs;
  CCState CCInfo(CallConv, isVarArg, MF, RVLocs, *DAG.getContext());
  CCInfo.AnalyzeReturn(Outs, RetCC_I8086);

  SDValue Glue;
  SmallVector<SDValue, 4> RetOps(1, Chain);
  for (unsigned i = 0; i != RVLocs.size(); ++i) {
    CCValAssign &VA = RVLocs[i];
    assert(VA.isRegLoc() && "Can only return in registers!");
    Chain = DAG.getCopyToReg(Chain, dl, VA.getLocReg(), OutVals[i], Glue);
    Glue = Chain.getValue(1);
    RetOps.push_back(DAG.getRegister(VA.getLocReg(), VA.getLocVT()));
  }

  // Struct return: return the hidden sret pointer in AX.
  if (MF.getFunction().hasStructRetAttr()) {
    I8086MachineFunctionInfo *FuncInfo = MF.getInfo<I8086MachineFunctionInfo>();
    Register Reg = FuncInfo->getSRetReturnReg();
    assert(Reg && "sret virtual register not created");
    SDValue Val = DAG.getCopyFromReg(Chain, dl, Reg, MVT::i16);
    Chain = DAG.getCopyToReg(Chain, dl, I8086::AX, Val, Glue);
    Glue = Chain.getValue(1);
    RetOps.push_back(DAG.getRegister(I8086::AX, MVT::i16));
  }

  RetOps[0] = Chain;
  if (Glue.getNode())
    RetOps.push_back(Glue);
  return DAG.getNode(I8086ISD::RET_GLUE, dl, MVT::Other, RetOps);
}

SDValue I8086TargetLowering::LowerCall(TargetLowering::CallLoweringInfo &CLI,
                                       SmallVectorImpl<SDValue> &InVals) const {
  SelectionDAG &DAG = CLI.DAG;
  SDLoc &dl = CLI.DL;
  SmallVectorImpl<ISD::OutputArg> &Outs = CLI.Outs;
  SmallVectorImpl<SDValue> &OutVals = CLI.OutVals;
  SmallVectorImpl<ISD::InputArg> &Ins = CLI.Ins;
  SDValue Chain = CLI.Chain;
  SDValue Callee = CLI.Callee;
  CallingConv::ID CallConv = CLI.CallConv;
  bool isVarArg = CLI.IsVarArg;

  CLI.IsTailCall = false; // No tail calls yet.

  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, isVarArg, DAG.getMachineFunction(), ArgLocs,
                 *DAG.getContext());
  CCInfo.AnalyzeCallOperands(Outs, CC_I8086);

  unsigned NumBytes = CCInfo.getStackSize();
  Chain = DAG.getCALLSEQ_START(Chain, NumBytes, 0, dl);

  // Pre-load the words of any byval aggregate up front.  The pushes below are
  // glued into one contiguous block, so we cannot slip a load between them; load
  // everything first (chained off the current Chain) and push the values later.
  // Each struct is broken into 16-bit words, plus a trailing byte for odd sizes.
  DenseMap<unsigned, SmallVector<SDValue, 8>> ByValWords;
  for (unsigned i = 0, e = ArgLocs.size(); i != e; ++i) {
    if (!Outs[i].Flags.isByVal())
      continue;
    SDValue Ptr = OutVals[i];
    unsigned Size = Outs[i].Flags.getByValSize();
    SmallVector<SDValue, 8> Words;
    unsigned Off = 0;
    auto AddrAt = [&](unsigned O) {
      return O == 0 ? Ptr
                    : DAG.getNode(ISD::ADD, dl, MVT::i16, Ptr,
                                  DAG.getConstant(O, dl, MVT::i16));
    };
    for (; Off + 2 <= Size; Off += 2) {
      SDValue W = DAG.getLoad(MVT::i16, dl, Chain, AddrAt(Off),
                              MachinePointerInfo());
      Chain = W.getValue(1);
      Words.push_back(W);
    }
    if (Off < Size) { // trailing odd byte (high byte of the word is padding)
      SDValue B = DAG.getExtLoad(ISD::EXTLOAD, dl, MVT::i16, Chain, AddrAt(Off),
                                 MachinePointerInfo(), MVT::i8);
      Chain = B.getValue(1);
      Words.push_back(B);
    }
    ByValWords[i] = std::move(Words);
  }

  // Push outgoing arguments right-to-left so arg0 ends up at the lowest address
  // (SP is not a ModR/M base, so we cannot store to [sp+off]).  Each PUSH is
  // glued to keep them ordered and immediately before the call.
  SDValue InGlue;
  auto EmitPush = [&](SDValue Val) {
    SmallVector<SDValue, 3> PushOps = {Chain, Val};
    if (InGlue.getNode())
      PushOps.push_back(InGlue);
    Chain = DAG.getNode(I8086ISD::PUSH, dl,
                        DAG.getVTList(MVT::Other, MVT::Glue), PushOps);
    InGlue = Chain.getValue(1);
  };
  for (unsigned i = ArgLocs.size(); i-- > 0;) {
    CCValAssign &VA = ArgLocs[i];
    assert(VA.isMemLoc() && "i8086 passes all arguments on the stack");

    // A byval struct's words were pre-loaded; push them highest-offset first so
    // word 0 lands at the lowest (last-pushed) address.
    if (Outs[i].Flags.isByVal()) {
      const SmallVectorImpl<SDValue> &Words = ByValWords[i];
      for (unsigned k = Words.size(); k-- > 0;)
        EmitPush(Words[k]);
      continue;
    }

    SDValue Arg = OutVals[i];
    switch (VA.getLocInfo()) {
    case CCValAssign::Full:
      break;
    case CCValAssign::SExt:
      Arg = DAG.getNode(ISD::SIGN_EXTEND, dl, VA.getLocVT(), Arg);
      break;
    case CCValAssign::ZExt:
      Arg = DAG.getNode(ISD::ZERO_EXTEND, dl, VA.getLocVT(), Arg);
      break;
    case CCValAssign::AExt:
      Arg = DAG.getNode(ISD::ANY_EXTEND, dl, VA.getLocVT(), Arg);
      break;
    default:
      llvm_unreachable("Unknown loc info!");
    }
    EmitPush(Arg);
  }

  if (GlobalAddressSDNode *G = dyn_cast<GlobalAddressSDNode>(Callee))
    Callee = DAG.getTargetGlobalAddress(G->getGlobal(), dl, MVT::i16);
  else if (ExternalSymbolSDNode *E = dyn_cast<ExternalSymbolSDNode>(Callee))
    Callee = DAG.getTargetExternalSymbol(E->getSymbol(), MVT::i16);

  SDVTList NodeTys = DAG.getVTList(MVT::Other, MVT::Glue);
  SmallVector<SDValue, 8> Ops;
  Ops.push_back(Chain);
  Ops.push_back(Callee);
  if (InGlue.getNode())
    Ops.push_back(InGlue);
  Chain = DAG.getNode(I8086ISD::CALL, dl, NodeTys, Ops);
  InGlue = Chain.getValue(1);

  Chain = DAG.getCALLSEQ_END(Chain, NumBytes, 0, InGlue, dl);
  InGlue = Chain.getValue(1);

  return LowerCallResult(Chain, InGlue, CallConv, isVarArg, Ins, dl, DAG,
                         InVals);
}

SDValue I8086TargetLowering::LowerCallResult(
    SDValue Chain, SDValue InGlue, CallingConv::ID CallConv, bool isVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &dl,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {
  SmallVector<CCValAssign, 16> RVLocs;
  CCState CCInfo(CallConv, isVarArg, DAG.getMachineFunction(), RVLocs,
                 *DAG.getContext());
  CCInfo.AnalyzeCallResult(Ins, RetCC_I8086);

  for (unsigned i = 0; i != RVLocs.size(); ++i) {
    Chain = DAG.getCopyFromReg(Chain, dl, RVLocs[i].getLocReg(),
                               RVLocs[i].getValVT(), InGlue)
                .getValue(1);
    InGlue = Chain.getValue(2);
    InVals.push_back(Chain.getValue(0));
  }
  return Chain;
}

//===----------------------------------------------------------------------===//
// Select expansion (control-flow diamond, since there is no CMOV/SETcc).
//===----------------------------------------------------------------------===//

// Local copy of the condition-code -> Jcc opcode mapping (also in InstrInfo).
static unsigned jccOpcode(I8086CC::CondCode CC) {
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

// Expand a shift pseudo to: copy the count into CL, then shift-by-CL.
static MachineBasicBlock *emitShift(MachineInstr &MI, MachineBasicBlock *BB) {
  const TargetInstrInfo &TII = *BB->getParent()->getSubtarget().getInstrInfo();
  DebugLoc dl = MI.getDebugLoc();
  unsigned ClOpc;
  switch (MI.getOpcode()) {
  case I8086::Shl8:  ClOpc = I8086::SHL8CL;  break;
  case I8086::Shr8:  ClOpc = I8086::SHR8CL;  break;
  case I8086::Sar8:  ClOpc = I8086::SAR8CL;  break;
  case I8086::Shl16: ClOpc = I8086::SHL16CL; break;
  case I8086::Shr16: ClOpc = I8086::SHR16CL; break;
  case I8086::Sar16: ClOpc = I8086::SAR16CL; break;
  case I8086::Rol8:  ClOpc = I8086::ROL8CL;  break;
  case I8086::Ror8:  ClOpc = I8086::ROR8CL;  break;
  case I8086::Rol16: ClOpc = I8086::ROL16CL; break;
  case I8086::Ror16: ClOpc = I8086::ROR16CL; break;
  default: llvm_unreachable("unexpected shift/rotate pseudo");
  }
  BuildMI(*BB, MI, dl, TII.get(TargetOpcode::COPY), I8086::CL)
      .addReg(MI.getOperand(2).getReg());
  BuildMI(*BB, MI, dl, TII.get(ClOpc), MI.getOperand(0).getReg())
      .addReg(MI.getOperand(1).getReg());
  MI.eraseFromParent();
  return BB;
}

// A 16-bit shift/rotate by 8..15 done as a byte op through AX plus a residual
// shift of 0..7 (see the pseudos):
//   shl -> mov ah,al; mov al,0    srl -> mov al,ah; mov ah,0
//   sar -> mov al,ah; cbw         rol/ror by 8 (byte swap) -> xchg al,ah
static MachineBasicBlock *emitShiftHi(MachineInstr &MI,
                                      MachineBasicBlock *BB) {
  const TargetInstrInfo &TII = *BB->getParent()->getSubtarget().getInstrInfo();
  const TargetRegisterInfo &TRI =
      *BB->getParent()->getSubtarget().getRegisterInfo();
  DebugLoc dl = MI.getDebugLoc();
  unsigned Opc = MI.getOpcode();
  Register Dst = MI.getOperand(0).getReg();
  Register Src = MI.getOperand(1).getReg();
  bool Rotate = Opc == I8086::Rol16by8 || Opc == I8086::Ror16by8;
  unsigned Extra = Rotate ? 0 : MI.getOperand(2).getImm(); // residual, 0..7

  // The CL shift/rotate opcode for the flags fallback and the >2 residual.
  unsigned ClOpc, B1Opc = 0;
  switch (Opc) {
  case I8086::Shl16Hi:  ClOpc = I8086::SHL16CL; B1Opc = I8086::SHL16b1; break;
  case I8086::Shr16Hi:  ClOpc = I8086::SHR16CL; B1Opc = I8086::SHR16b1; break;
  case I8086::Sar16Hi:  ClOpc = I8086::SAR16CL; B1Opc = I8086::SAR16b1; break;
  case I8086::Rol16by8: ClOpc = I8086::ROL16CL; break;
  case I8086::Ror16by8: ClOpc = I8086::ROR16CL; break;
  default: llvm_unreachable("unexpected shift-hi pseudo");
  }

  // The byte-move sequence leaves FLAGS untouched, so if the shift's flags are
  // actually consumed (e.g. a future shr+jz), fall back to the flag-setting CL
  // form.  (Today the shift's flags are always dead, so this never triggers.)
  if (!MI.registerDefIsDead(I8086::FLAGS, &TRI)) {
    BuildMI(*BB, MI, dl, TII.get(I8086::MOV8ri), I8086::CL).addImm(8 + Extra);
    BuildMI(*BB, MI, dl, TII.get(ClOpc), Dst).addReg(Src);
    MI.eraseFromParent();
    return BB;
  }

  // Byte op = shift/rotate by 8, through AX.
  BuildMI(*BB, MI, dl, TII.get(TargetOpcode::COPY), I8086::AX).addReg(Src);
  switch (Opc) {
  case I8086::Shl16Hi:
    BuildMI(*BB, MI, dl, TII.get(I8086::MOV8rr), I8086::AH).addReg(I8086::AL);
    // Zero with xor (3 cycles) rather than mov ,0 (4); FLAGS is dead here.
    // xor r,r is a zeroing idiom: mark the reads undef (no real dependency).
    BuildMI(*BB, MI, dl, TII.get(I8086::XOR8rr), I8086::AL)
        .addReg(I8086::AL, RegState::Undef)
        .addReg(I8086::AL, RegState::Undef);
    break;
  case I8086::Shr16Hi:
    BuildMI(*BB, MI, dl, TII.get(I8086::MOV8rr), I8086::AL).addReg(I8086::AH);
    // Zero with xor (3 cycles) rather than mov ,0 (4); FLAGS is dead here.
    // xor r,r is a zeroing idiom: mark the reads undef (no real dependency).
    BuildMI(*BB, MI, dl, TII.get(I8086::XOR8rr), I8086::AH)
        .addReg(I8086::AH, RegState::Undef)
        .addReg(I8086::AH, RegState::Undef);
    break;
  case I8086::Sar16Hi:
    BuildMI(*BB, MI, dl, TII.get(I8086::MOV8rr), I8086::AL).addReg(I8086::AH);
    BuildMI(*BB, MI, dl, TII.get(I8086::CBW));
    break;
  case I8086::Rol16by8:
  case I8086::Ror16by8:
    BuildMI(*BB, MI, dl, TII.get(I8086::XCHG8rr))
        .addReg(I8086::AL)
        .addReg(I8086::AH)
        .addReg(I8086::AL, RegState::ImplicitDefine)
        .addReg(I8086::AH, RegState::ImplicitDefine);
    break;
  }

  // Residual shift by Extra (shifts only): by-1 for 1/2, CL for 3..7.
  if (!Rotate && Extra) {
    if (Extra <= 2) {
      for (unsigned I = 0; I != Extra; ++I)
        BuildMI(*BB, MI, dl, TII.get(B1Opc), I8086::AX).addReg(I8086::AX);
    } else {
      BuildMI(*BB, MI, dl, TII.get(I8086::MOV8ri), I8086::CL).addImm(Extra);
      BuildMI(*BB, MI, dl, TII.get(ClOpc), I8086::AX).addReg(I8086::AX);
    }
  }

  BuildMI(*BB, MI, dl, TII.get(TargetOpcode::COPY), Dst).addReg(I8086::AX);
  MI.eraseFromParent();
  return BB;
}

// Expand a UMULLOHI16/SMULLOHI16/UDIVREM16/SDIVREM16 pseudo into the native
// single-operand MUL/IMUL/DIV/IDIV, which read/write the implicit AX (and DX)
// registers.  Operands: (out lo/q, out hi/r, in a, in b).
static MachineBasicBlock *emitMulDiv(MachineInstr &MI, MachineBasicBlock *BB) {
  const TargetInstrInfo &TII = *BB->getParent()->getSubtarget().getInstrInfo();
  DebugLoc dl = MI.getDebugLoc();
  Register Lo = MI.getOperand(0).getReg();
  Register Hi = MI.getOperand(1).getReg();
  Register A = MI.getOperand(2).getReg();
  Register B = MI.getOperand(3).getReg();

  // The dividend/multiplicand goes in AX.
  BuildMI(*BB, MI, dl, TII.get(TargetOpcode::COPY), I8086::AX).addReg(A);

  unsigned InstOpc;
  switch (MI.getOpcode()) {
  case I8086::UMULLOHI16:
    InstOpc = I8086::MUL16i;
    break;
  case I8086::SMULLOHI16:
    InstOpc = I8086::IMUL16i;
    break;
  case I8086::UDIVREM16:
    // Zero-extend AX into the DX:AX dividend.  xor is 3 cycles / 2 bytes vs 4 / 3
    // for mov ,0; the following DIV clobbers FLAGS, so flags are dead here.  xor
    // r,r is a zeroing idiom, so the reads are undef (DX is not yet defined).
    BuildMI(*BB, MI, dl, TII.get(I8086::XOR16rr), I8086::DX)
        .addReg(I8086::DX, RegState::Undef)
        .addReg(I8086::DX, RegState::Undef);
    InstOpc = I8086::DIV16i;
    break;
  case I8086::SDIVREM16:
    // Sign-extend AX into DX:AX via CWD.
    BuildMI(*BB, MI, dl, TII.get(I8086::CWD));
    InstOpc = I8086::IDIV16i;
    break;
  default:
    llvm_unreachable("unexpected mul/div pseudo");
  }
  BuildMI(*BB, MI, dl, TII.get(InstOpc)).addReg(B);

  // MUL: AX=low, DX=high.  DIV: AX=quotient, DX=remainder.
  BuildMI(*BB, MI, dl, TII.get(TargetOpcode::COPY), Lo).addReg(I8086::AX);
  BuildMI(*BB, MI, dl, TII.get(TargetOpcode::COPY), Hi).addReg(I8086::DX);
  MI.eraseFromParent();
  return BB;
}

MachineBasicBlock *
I8086TargetLowering::EmitInstrWithCustomInserter(MachineInstr &MI,
                                                 MachineBasicBlock *BB) const {
  unsigned Opc = MI.getOpcode();
  if (Opc == I8086::Shl8 || Opc == I8086::Shr8 || Opc == I8086::Sar8 ||
      Opc == I8086::Shl16 || Opc == I8086::Shr16 || Opc == I8086::Sar16 ||
      Opc == I8086::Rol8 || Opc == I8086::Ror8 ||
      Opc == I8086::Rol16 || Opc == I8086::Ror16)
    return emitShift(MI, BB);

  if (Opc == I8086::Shl16Hi || Opc == I8086::Shr16Hi ||
      Opc == I8086::Sar16Hi || Opc == I8086::Rol16by8 ||
      Opc == I8086::Ror16by8)
    return emitShiftHi(MI, BB);

  if (Opc == I8086::UMULLOHI16 || Opc == I8086::SMULLOHI16 ||
      Opc == I8086::UDIVREM16 || Opc == I8086::SDIVREM16)
    return emitMulDiv(MI, BB);

  // sext i8 -> i16 via CBW: copy the byte into AL, CBW, copy AX out.
  if (Opc == I8086::MOVSX16r8) {
    const TargetInstrInfo &TII =
        *BB->getParent()->getSubtarget().getInstrInfo();
    DebugLoc dl = MI.getDebugLoc();
    BuildMI(*BB, MI, dl, TII.get(TargetOpcode::COPY), I8086::AL)
        .addReg(MI.getOperand(1).getReg());
    BuildMI(*BB, MI, dl, TII.get(I8086::CBW));
    BuildMI(*BB, MI, dl, TII.get(TargetOpcode::COPY),
            MI.getOperand(0).getReg())
        .addReg(I8086::AX);
    MI.eraseFromParent();
    return BB;
  }

  assert((Opc == I8086::Select8 || Opc == I8086::Select16) &&
         "Unexpected instr type to insert");
  const TargetInstrInfo &TII = *BB->getParent()->getSubtarget().getInstrInfo();
  DebugLoc dl = MI.getDebugLoc();

  // Diamond:
  //   thisMBB: cmp; jCC copy1MBB; fallthrough copy0MBB
  //   copy0MBB: (false value)
  //   copy1MBB: phi
  const BasicBlock *LLVM_BB = BB->getBasicBlock();
  MachineFunction::iterator I = ++BB->getIterator();
  MachineBasicBlock *thisMBB = BB;
  MachineFunction *F = BB->getParent();
  MachineBasicBlock *copy0MBB = F->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *copy1MBB = F->CreateMachineBasicBlock(LLVM_BB);
  F->insert(I, copy0MBB);
  F->insert(I, copy1MBB);
  copy1MBB->splice(copy1MBB->begin(), BB,
                   std::next(MachineBasicBlock::iterator(MI)), BB->end());
  copy1MBB->transferSuccessorsAndUpdatePHIs(BB);
  BB->addSuccessor(copy0MBB);
  BB->addSuccessor(copy1MBB);

  I8086CC::CondCode CC =
      static_cast<I8086CC::CondCode>(MI.getOperand(3).getImm());
  BuildMI(BB, dl, TII.get(jccOpcode(CC))).addMBB(copy1MBB);

  BB = copy0MBB;
  BB->addSuccessor(copy1MBB);

  BB = copy1MBB;
  BuildMI(*BB, BB->begin(), dl, TII.get(I8086::PHI), MI.getOperand(0).getReg())
      .addReg(MI.getOperand(2).getReg())
      .addMBB(copy0MBB)
      .addReg(MI.getOperand(1).getReg())
      .addMBB(thisMBB);

  MI.eraseFromParent();
  return BB;
}

//===----------------------------------------------------------------------===//
// Inline assembly.
//===----------------------------------------------------------------------===//

TargetLowering::ConstraintType
I8086TargetLowering::getConstraintType(StringRef Constraint) const {
  if (Constraint.size() == 1) {
    switch (Constraint[0]) {
    case 'a':
    case 'b':
    case 'c':
    case 'd':
    case 'S':
    case 'D':
      return C_Register;
    case 'r':
    case 'q':
      return C_RegisterClass;
    default:
      break;
    }
  }
  return TargetLowering::getConstraintType(Constraint);
}

std::pair<unsigned, const TargetRegisterClass *>
I8086TargetLowering::getRegForInlineAsmConstraint(
    const TargetRegisterInfo *TRI, StringRef Constraint, MVT VT) const {
  if (Constraint.size() == 1) {
    bool Is8 = VT == MVT::i8;
    switch (Constraint[0]) {
    case 'r':
    case 'q':
      return Is8 ? std::make_pair(0U, &I8086::GR8RegClass)
                 : std::make_pair(0U, &I8086::GR16RegClass);
    case 'a':
      return Is8 ? std::make_pair((unsigned)I8086::AL, &I8086::GR8RegClass)
                 : std::make_pair((unsigned)I8086::AX, &I8086::GR16RegClass);
    case 'b':
      return Is8 ? std::make_pair((unsigned)I8086::BL, &I8086::GR8RegClass)
                 : std::make_pair((unsigned)I8086::BX, &I8086::GR16RegClass);
    case 'c':
      return Is8 ? std::make_pair((unsigned)I8086::CL, &I8086::GR8RegClass)
                 : std::make_pair((unsigned)I8086::CX, &I8086::GR16RegClass);
    case 'd':
      return Is8 ? std::make_pair((unsigned)I8086::DL, &I8086::GR8RegClass)
                 : std::make_pair((unsigned)I8086::DX, &I8086::GR16RegClass);
    case 'S':
      return std::make_pair((unsigned)I8086::SI, &I8086::GR16RegClass);
    case 'D':
      return std::make_pair((unsigned)I8086::DI, &I8086::GR16RegClass);
    default:
      break;
    }
  }
  return TargetLowering::getRegForInlineAsmConstraint(TRI, Constraint, VT);
}
