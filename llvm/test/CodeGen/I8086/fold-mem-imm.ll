; RUN: llc -mtriple=i8086 -O2 -verify-machineinstrs < %s | FileCheck %s
;
; Post-RA I8086FoldMemImm reorders `mov R,[mem]; op R,imm` into
; `mov R,imm; op R,[mem]` when strictly smaller: commutative op, R not AX/AL, and
; the reg,imm form larger than mov reg,imm (16-bit non-compressible imm, or 8-bit).

target datalayout = "e-m:e-p:16:16-i32:16-i64:16-f32:16-f64:16-a:8-n8:16-S16"
target triple = "i8086"

@g = external global i16
@g8 = external global i8

; non-AX reg + non-compressible imm16: fold (mov r,imm16=3B beats or r,imm16=4B).
; CHECK-LABEL: big:
; CHECK: mov [[R:[cd]x]], -21555
; CHECK: or [[R]], [bx]
; CHECK-NOT: or {{[cd]x}}, -21555
define void @big(i16* %p) {
  %v = load i16, i16* %p
  %r = or i16 %v, 43981
  %k = call i16 asm sideeffect "nop", "={ax}"()
  %s = add i16 %r, %k
  store i16 %s, i16* @g
  ret void
}

; 8-bit non-AL: fold (mov r,imm8=2B beats and r,imm8=3B).
; CHECK-LABEL: b8:
; CHECK: mov [[R8:[cd]l]], 66
; CHECK: and [[R8]], [bx]
define void @b8(i8* %p) {
  %v = load i8, i8* %p
  %r = and i8 %v, 66
  %k = call i8 asm sideeffect "nop", "={al}"()
  %s = add i8 %r, %k
  store i8 %s, i8* @g8
  ret void
}

; AX: not folded (a [disp16] would load via the shorter moffs).
; CHECK-LABEL: axcase:
; CHECK: or ax, -21555
; CHECK-NOT: or ax, [
define i16 @axcase(i16* %p) {
  %v = load i16, i16* %p
  %r = or i16 %v, 43981
  ret i16 %r
}

; compressible imm (fits imm8, 0x83 form): neutral, not folded.
; CHECK-LABEL: small:
; CHECK: or {{[cd]x}}, 5
; CHECK-NOT: or {{[cd]x}}, [
define void @small(i16* %p) {
  %v = load i16, i16* %p
  %r = or i16 %v, 5
  %k = call i16 asm sideeffect "nop", "={ax}"()
  %s = add i16 %r, %k
  store i16 %s, i16* @g
  ret void
}
