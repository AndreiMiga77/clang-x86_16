; RUN: llc -mtriple=i8086 -O2 -verify-machineinstrs < %s | FileCheck %s
;
; The shift/rotate custom-inserter pseudos bring the value through AX and use CL
; for the count (and residual), so they must declare AX/CL as clobbers.  These
; tests keep an unrelated value live in AX/CX across such a shift: the value must
; survive (it is moved out of, or produced clear of, the clobbered register), not
; be corrupted by the shift's `mov cl,4` / byte moves.

target datalayout = "e-m:e-p:16:16-i32:16-i64:16-f32:16-f64:16-a:8-n8:16-S16"
target triple = "i8086"

; An ={ax} inline-asm result live across a shl-by-12 (byte op + CL residual).
; The asm's AX value and the shift result must not share AX unguarded.
; CHECK-LABEL: axlive:
; CHECK: shl ax, cl
; CHECK: mov cx, ax
; CHECK: nop
; CHECK: add ax, cx
define i16 @axlive(i16 %y) {
  %a = call i16 asm sideeffect "nop", "={ax}"()
  %s = shl i16 %y, 12
  %r = add i16 %a, %s
  ret i16 %r
}

; An ={cx} inline-asm result live across a shl-by-12: the CL residual must not
; clobber the live CX before the asm produces it (shift is ordered first).
; CHECK-LABEL: cxlive:
; CHECK: mov cl, 4
; CHECK: shl ax, cl
; CHECK: nop
; CHECK: add ax, cx
define i16 @cxlive(i16 %y) {
  %a = call i16 asm sideeffect "nop", "={cx}"()
  %s = shl i16 %y, 12
  %r = add i16 %a, %s
  ret i16 %r
}

; A variable-count shift keeps the count live across a constant shl-by-12 whose
; residual also needs CL; the two CL uses must be serialized correctly.
; CHECK-LABEL: countlive:
; CHECK: shl dx, cl
; CHECK: shl dx, cl
; CHECK: mov cl, 4
; CHECK: shl ax, cl
define i16 @countlive(i16 %x, i16 %n, i16 %y) {
  %a = shl i16 %x, %n
  %b = shl i16 %y, 12
  %c = shl i16 %a, %n
  %d = add i16 %b, %c
  ret i16 %d
}
