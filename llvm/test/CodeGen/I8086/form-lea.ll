; RUN: llc -mtriple=i8086 -O2 -verify-machineinstrs < %s | FileCheck %s
;
; A mov/add chain computing base+index+disp collapses into one LEA when the
; operands are addressing registers (base BX/BP, index SI/DI) and the LEA is
; smaller.  A single register or a plain reg,imm in AX (not an addressing reg) is
; left alone.

target datalayout = "e-m:e-p:16:16-i32:16-i64:16-f32:16-f64:16-a:8-n8:16-S16"
target triple = "i8086"

; mov ax,si; add ax,100  ->  lea ax,[si+100]
; CHECK-LABEL: regimm:
; CHECK: lea ax, [si + 100]
; CHECK-NOT: add ax,
define i16 @regimm() {
  %si = call i16 asm sideeffect "", "={si}"()
  %r = add i16 %si, 100
  ret i16 %r
}

; mov ax,bx; add ax,si  ->  lea ax,[bx+si]
; CHECK-LABEL: baseidx:
; CHECK: lea ax, [bx + si]
define i16 @baseidx() {
  %bx = call i16 asm sideeffect "", "={bx}"()
  %si = call i16 asm sideeffect "", "={si}"()
  %r = add i16 %bx, %si
  ret i16 %r
}

; mov ax,bx; add ax,si; add ax,8  ->  lea ax,[bx+si+8]
; CHECK-LABEL: base_index_disp:
; CHECK: lea ax, [bx + si + 8]
; CHECK-NOT: add ax,
define i16 @base_index_disp() {
  %bx = call i16 asm sideeffect "", "={bx}"()
  %si = call i16 asm sideeffect "", "={si}"()
  %t = add i16 %bx, %si
  %r = add i16 %t, 8
  ret i16 %r
}

; A plain reg (in AX, not an addressing register) + imm is not a LEA candidate.
; CHECK-LABEL: axreg:
; CHECK: add ax,
; CHECK-NOT: lea
define i16 @axreg(i16 %x) {
  %r = add i16 %x, 100
  ret i16 %r
}
