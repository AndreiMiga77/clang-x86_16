//===- I8086AsmParser.cpp - Parse 8086 assembly to MCInst -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// A small Intel-syntax assembly parser for the original 8086.  Memory operands
// use the form [base + index + disp] with an optional "byte/word ptr" size and
// an optional segment override (es:[...]).
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/I8086MCTargetDesc.h"
#include "TargetInfo/I8086TargetInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCParser/AsmLexer.h"
#include "llvm/MC/MCParser/MCParsedAsmOperand.h"
#include "llvm/MC/MCParser/MCTargetAsmParser.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Compiler.h"

using namespace llvm;

#define DEBUG_TYPE "i8086-asm-parser"

namespace {

/// A parsed 8086 assembly operand.
class I8086Operand : public MCParsedAsmOperand {
  enum KindTy { k_Token, k_Reg, k_Imm, k_Mem } Kind;

  struct MemOp {
    MCRegister Base;
    MCRegister Index;
    const MCExpr *Disp;
    MCRegister Seg;
    unsigned Size; // 0 = unspecified, 1 = byte, 2 = word
  };

  StringRef Tok;
  MCRegister Reg;
  const MCExpr *Imm = nullptr;
  MemOp Mem{};

  SMLoc Start, End;

public:
  I8086Operand(KindTy K, SMLoc S, SMLoc E) : Kind(K), Start(S), End(E) {}

  SMLoc getStartLoc() const override { return Start; }
  SMLoc getEndLoc() const override { return End; }

  bool isToken() const override { return Kind == k_Token; }
  bool isReg() const override { return Kind == k_Reg; }
  bool isImm() const override { return Kind == k_Imm; }
  bool isMem() const override { return Kind == k_Mem; }

  // Used where a register operand implies the width (size may be unspecified).
  bool isMem8() const {
    return Kind == k_Mem && (Mem.Size == 0 || Mem.Size == 1);
  }
  bool isMem16() const {
    return Kind == k_Mem && (Mem.Size == 0 || Mem.Size == 2);
  }
  // Used where nothing else implies the width, so an explicit "byte/word ptr"
  // is required (e.g. memory + immediate, or unary memory operands).
  bool isMem8Explicit() const { return Kind == k_Mem && Mem.Size == 1; }
  bool isMem16Explicit() const { return Kind == k_Mem && Mem.Size == 2; }
  bool isImmOne() const {
    if (Kind != k_Imm)
      return false;
    const auto *CE = dyn_cast<MCConstantExpr>(Imm);
    return CE && CE->getValue() == 1;
  }

  StringRef getToken() const {
    assert(Kind == k_Token);
    return Tok;
  }
  MCRegister getReg() const override {
    assert(Kind == k_Reg);
    return Reg;
  }

  void addExpr(MCInst &Inst, const MCExpr *Expr) const {
    int64_t Res;
    if (!Expr)
      Inst.addOperand(MCOperand::createImm(0));
    else if (Expr->evaluateAsAbsolute(Res))
      Inst.addOperand(MCOperand::createImm(Res));
    else
      Inst.addOperand(MCOperand::createExpr(Expr));
  }

  void addRegOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1);
    Inst.addOperand(MCOperand::createReg(Reg));
  }
  void addImmOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1);
    addExpr(Inst, Imm);
  }
  void addMemOperands(MCInst &Inst, unsigned N) const {
    assert(N == 4);
    Inst.addOperand(MCOperand::createReg(Mem.Base));
    Inst.addOperand(MCOperand::createReg(Mem.Index));
    addExpr(Inst, Mem.Disp);
    Inst.addOperand(MCOperand::createReg(Mem.Seg));
  }

  void print(raw_ostream &O, const MCAsmInfo &MAI) const override {
    switch (Kind) {
    case k_Token: O << "Token:" << Tok; break;
    case k_Reg:   O << "Reg:" << Reg.id(); break;
    case k_Imm:   O << "Imm"; break;
    case k_Mem:   O << "Mem"; break;
    }
  }

  static std::unique_ptr<I8086Operand> createToken(StringRef Str, SMLoc S) {
    auto Op = std::make_unique<I8086Operand>(k_Token, S, S);
    Op->Tok = Str;
    return Op;
  }
  static std::unique_ptr<I8086Operand> createReg(MCRegister R, SMLoc S,
                                                 SMLoc E) {
    auto Op = std::make_unique<I8086Operand>(k_Reg, S, E);
    Op->Reg = R;
    return Op;
  }
  static std::unique_ptr<I8086Operand> createImm(const MCExpr *Val, SMLoc S,
                                                 SMLoc E) {
    auto Op = std::make_unique<I8086Operand>(k_Imm, S, E);
    Op->Imm = Val;
    return Op;
  }
  static std::unique_ptr<I8086Operand>
  createMem(MCRegister Base, MCRegister Index, const MCExpr *Disp,
            MCRegister Seg, unsigned Size, SMLoc S, SMLoc E) {
    auto Op = std::make_unique<I8086Operand>(k_Mem, S, E);
    Op->Mem = {Base, Index, Disp, Seg, Size};
    return Op;
  }
};

class I8086AsmParser : public MCTargetAsmParser {
  MCAsmParser &Parser;

#define GET_ASSEMBLER_HEADER
#include "I8086GenAsmMatcher.inc"

  MCAsmParser &getParser() const { return Parser; }
  AsmLexer &getLexer() const { return Parser.getLexer(); }

  bool matchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                               OperandVector &Operands, MCStreamer &Out,
                               uint64_t &ErrorInfo,
                               bool MatchingInlineAsm) override;
  bool parseRegister(MCRegister &Reg, SMLoc &StartLoc, SMLoc &EndLoc) override;
  ParseStatus tryParseRegister(MCRegister &Reg, SMLoc &StartLoc,
                               SMLoc &EndLoc) override;
  bool parseInstruction(ParseInstructionInfo &Info, StringRef Name,
                        SMLoc NameLoc, OperandVector &Operands) override;
  ParseStatus parseDirective(AsmToken DirectiveID) override { return ParseStatus::NoMatch; }

  bool parseOperand(OperandVector &Operands);
  bool parseMemOperand(OperandVector &Operands, unsigned Size, MCRegister Seg,
                       SMLoc StartLoc);

  bool isPrefixMnemonic(StringRef Name, unsigned &Opc) const;

public:
  I8086AsmParser(const MCSubtargetInfo &STI, MCAsmParser &Parser,
                 const MCInstrInfo &MII, const MCTargetOptions &Options)
      : MCTargetAsmParser(Options, STI, MII), Parser(Parser) {
    MCAsmParserExtension::Initialize(Parser);
    setAvailableFeatures(ComputeAvailableFeatures(STI.getFeatureBits()));
  }
};

} // namespace

static MCRegister MatchRegisterName(StringRef Name);

bool I8086AsmParser::matchAndEmitInstruction(SMLoc Loc, unsigned &Opcode,
                                             OperandVector &Operands,
                                             MCStreamer &Out,
                                             uint64_t &ErrorInfo,
                                             bool MatchingInlineAsm) {
  MCInst Inst;
  unsigned MatchResult =
      MatchInstructionImpl(Operands, Inst, ErrorInfo, MatchingInlineAsm);
  switch (MatchResult) {
  case Match_Success:
    Inst.setLoc(Loc);
    Out.emitInstruction(Inst, getSTI());
    return false;
  case Match_MnemonicFail:
    return Error(Loc, "invalid instruction mnemonic");
  case Match_InvalidOperand: {
    SMLoc ErrorLoc = Loc;
    if (ErrorInfo != ~0ULL) {
      if (ErrorInfo >= Operands.size())
        return Error(Loc, "too few operands for instruction");
      ErrorLoc = ((I8086Operand &)*Operands[ErrorInfo]).getStartLoc();
      if (ErrorLoc == SMLoc())
        ErrorLoc = Loc;
    }
    return Error(ErrorLoc, "invalid operand for instruction");
  }
  default:
    return Error(Loc, "unable to encode instruction");
  }
}

bool I8086AsmParser::parseRegister(MCRegister &Reg, SMLoc &StartLoc,
                                   SMLoc &EndLoc) {
  ParseStatus Res = tryParseRegister(Reg, StartLoc, EndLoc);
  if (Res.isSuccess())
    return false;
  return true;
}

ParseStatus I8086AsmParser::tryParseRegister(MCRegister &Reg, SMLoc &StartLoc,
                                             SMLoc &EndLoc) {
  if (getLexer().getKind() != AsmToken::Identifier)
    return ParseStatus::NoMatch;
  std::string Name = getLexer().getTok().getIdentifier().lower();
  Reg = MatchRegisterName(Name);
  if (!Reg)
    return ParseStatus::NoMatch;
  StartLoc = getParser().getTok().getLoc();
  EndLoc = getParser().getTok().getEndLoc();
  getLexer().Lex();
  return ParseStatus::Success;
}

bool I8086AsmParser::isPrefixMnemonic(StringRef Name, unsigned &Opc) const {
  StringRef N = Name.lower();
  if (N == "rep" || N == "repe" || N == "repz") {
    Opc = I8086::REP_PREFIX;
    return true;
  }
  if (N == "repne" || N == "repnz") {
    Opc = I8086::REPNE_PREFIX;
    return true;
  }
  if (N == "lock") {
    Opc = I8086::LOCK_PREFIX;
    return true;
  }
  return false;
}

bool I8086AsmParser::parseInstruction(ParseInstructionInfo &Info,
                                      StringRef Name, SMLoc NameLoc,
                                      OperandVector &Operands) {
  // Handle prefix mnemonics (rep/repe/repne/lock): emit the prefix byte(s) and
  // continue with the actual instruction on the same line.
  unsigned PfxOpc;
  while (isPrefixMnemonic(Name, PfxOpc)) {
    MCInst Pfx;
    Pfx.setOpcode(PfxOpc);
    getParser().getStreamer().emitInstruction(Pfx, getSTI());
    if (getLexer().is(AsmToken::EndOfStatement)) {
      getParser().Lex();
      return false;
    }
    if (getLexer().isNot(AsmToken::Identifier))
      return Error(getLexer().getLoc(), "expected instruction after prefix");
    Name = getParser().getTok().getIdentifier();
    NameLoc = getParser().getTok().getLoc();
    getLexer().Lex();
  }

  Operands.push_back(I8086Operand::createToken(Name, NameLoc));

  if (getLexer().is(AsmToken::EndOfStatement))
    return false;

  if (parseOperand(Operands))
    return true;
  while (getLexer().is(AsmToken::Comma)) {
    getLexer().Lex();
    if (parseOperand(Operands))
      return true;
  }

  if (getLexer().isNot(AsmToken::EndOfStatement)) {
    SMLoc Loc = getLexer().getLoc();
    getParser().eatToEndOfStatement();
    return Error(Loc, "unexpected token in operand list");
  }
  getParser().Lex(); // consume EndOfStatement
  return false;
}

bool I8086AsmParser::parseOperand(OperandVector &Operands) {
  SMLoc StartLoc = getParser().getTok().getLoc();

  // Optional "byte/word [ptr]" size specifier (introduces a memory operand).
  unsigned Size = 0;
  if (getLexer().is(AsmToken::Identifier)) {
    StringRef Id = getLexer().getTok().getIdentifier();
    if (Id.equals_insensitive("byte"))
      Size = 1;
    else if (Id.equals_insensitive("word"))
      Size = 2;
    if (Size) {
      getLexer().Lex();
      if (getLexer().is(AsmToken::Identifier) &&
          getLexer().getTok().getIdentifier().equals_insensitive("ptr"))
        getLexer().Lex();
    }
  }

  // Optional segment override "seg:" preceding a memory operand.
  MCRegister Seg;
  if (getLexer().is(AsmToken::Identifier)) {
    std::string Id = getLexer().getTok().getIdentifier().lower();
    MCRegister R = MatchRegisterName(Id);
    if (R == I8086::ES || R == I8086::CS || R == I8086::SS || R == I8086::DS) {
      // Peek: only a segment override if followed by ':'.
      AsmToken Next = getLexer().peekTok();
      if (Next.is(AsmToken::Colon)) {
        Seg = R;
        getLexer().Lex(); // eat seg
        getLexer().Lex(); // eat ':'
      }
    }
  }

  if (getLexer().is(AsmToken::LBrac))
    return parseMemOperand(Operands, Size, Seg, StartLoc);

  if (Size || Seg)
    return Error(StartLoc, "expected memory operand");

  // Register operand?
  MCRegister Reg;
  SMLoc RegStart, RegEnd;
  if (tryParseRegister(Reg, RegStart, RegEnd).isSuccess()) {
    Operands.push_back(I8086Operand::createReg(Reg, RegStart, RegEnd));
    return false;
  }

  // Otherwise an immediate expression.
  const MCExpr *Val;
  SMLoc ExprEnd;
  if (getParser().parseExpression(Val, ExprEnd))
    return Error(StartLoc, "invalid operand");
  Operands.push_back(I8086Operand::createImm(Val, StartLoc, ExprEnd));
  return false;
}

bool I8086AsmParser::parseMemOperand(OperandVector &Operands, unsigned Size,
                                     MCRegister Seg, SMLoc StartLoc) {
  getLexer().Lex(); // eat '['

  MCRegister Base, Index;
  const MCExpr *Disp = nullptr;
  bool First = true;

  while (getLexer().isNot(AsmToken::RBrac)) {
    bool Negate = false;
    if (First) {
      if (getLexer().is(AsmToken::Minus)) {
        Negate = true;
        getLexer().Lex();
      } else if (getLexer().is(AsmToken::Plus)) {
        getLexer().Lex();
      }
      First = false;
    } else {
      if (getLexer().is(AsmToken::Plus))
        getLexer().Lex();
      else if (getLexer().is(AsmToken::Minus)) {
        Negate = true;
        getLexer().Lex();
      } else
        return Error(getLexer().getLoc(), "expected '+' or '-' in address");
    }

    // A register term (base or index)?
    if (getLexer().is(AsmToken::Identifier)) {
      std::string Id = getLexer().getTok().getIdentifier().lower();
      MCRegister R = MatchRegisterName(Id);
      if (R) {
        if (Negate)
          return Error(getLexer().getLoc(), "cannot negate a register");
        if (R == I8086::BX || R == I8086::BP) {
          if (Base)
            return Error(getLexer().getLoc(), "too many base registers");
          Base = R;
        } else if (R == I8086::SI || R == I8086::DI) {
          if (Index)
            return Error(getLexer().getLoc(), "too many index registers");
          Index = R;
        } else {
          return Error(getLexer().getLoc(),
                       "invalid register in 8086 address");
        }
        getLexer().Lex();
        continue;
      }
    }

    // Otherwise a displacement term (constant or symbol).
    const MCExpr *Term;
    SMLoc TermEnd;
    if (getParser().parseExpression(Term, TermEnd))
      return Error(getLexer().getLoc(), "invalid displacement");
    if (Negate)
      Term = MCUnaryExpr::createMinus(Term, getContext());
    Disp = Disp ? MCBinaryExpr::createAdd(Disp, Term, getContext()) : Term;
  }

  SMLoc EndLoc = getLexer().getTok().getEndLoc();
  getLexer().Lex(); // eat ']'

  if (!Disp)
    Disp = MCConstantExpr::create(0, getContext());

  Operands.push_back(
      I8086Operand::createMem(Base, Index, Disp, Seg, Size, StartLoc, EndLoc));
  return false;
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeI8086AsmParser() {
  RegisterMCAsmParser<I8086AsmParser> X(getTheI8086Target());
}

#define GET_REGISTER_MATCHER
#define GET_MATCHER_IMPLEMENTATION
#include "I8086GenAsmMatcher.inc"
