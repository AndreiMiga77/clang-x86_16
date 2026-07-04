; RUN: llc -mtriple=i8086 -O2 -verify-machineinstrs < %s | FileCheck %s
;
; i8->i16 sign extension is done in place in a hi-half-capable register.  When AX
; is free the allocator uses it and the SEXT16 pseudo expands to a 1-byte CBW.
; When AX is live, the value is sign-extended in another lo/hi pair with
; `rol lo,1; sbb hi,hi; ror lo,1` -- longer, but it avoids spilling AX.

target datalayout = "e-m:e-p:16:16-i32:16-i64:16-f32:16-f64:16-a:8-n8:16-S16"
target triple = "i8086"

; CHECK-LABEL: axfree:
; CHECK: cbw
; CHECK-NOT: sbb
define i16 @axfree(i8 %x) {
  %r = sext i8 %x to i16
  ret i16 %r
}

; A value pinned in AX across the sext forces the rotate form in another pair.
; CHECK-LABEL: axbusy:
; CHECK: rol {{[cd]}}l, 1
; CHECK: sbb {{[cd]}}h, {{[cd]}}h
; CHECK: ror {{[cd]}}l, 1
; CHECK-NOT: cbw
define i16 @axbusy(i8 %x) {
  %ax = call i16 asm sideeffect "nop", "={ax}"()
  %e = sext i8 %x to i16
  %r = add i16 %e, %ax
  ret i16 %r
}
