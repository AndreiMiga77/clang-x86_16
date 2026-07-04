; RUN: llc -mtriple=i8086 -O2 -verify-machineinstrs < %s | FileCheck %s
;
; i8->i16 sign extension is done in place in a hi-half-capable register.  When AX
; is free the allocator uses it and the SEXT16 pseudo expands to a 1-byte CBW.
; Otherwise the value is swapped into AX and back around a CBW
; (`xchg ax,r; cbw; xchg ax,r`) -- 3 bytes, no AX spill, and FLAGS-preserving.

target datalayout = "e-m:e-p:16:16-i32:16-i64:16-f32:16-f64:16-a:8-n8:16-S16"
target triple = "i8086"

; CHECK-LABEL: axfree:
; CHECK: cbw
; CHECK-NOT: xchg
define i16 @axfree(i8 %x) {
  %r = sext i8 %x to i16
  ret i16 %r
}

; A byte kept in another register is sexted via xchg-cbw-xchg (not a rotate).
; CHECK-LABEL: elsewhere:
; CHECK: xchg ax, {{[a-z]+}}
; CHECK: cbw
; CHECK: xchg ax, {{[a-z]+}}
; CHECK-NOT: sbb
; CHECK-NOT: rol
define i16 @elsewhere(i8 %x) {
  %ax = call i16 asm sideeffect "nop", "={ax}"()
  %e = sext i8 %x to i16
  %r = add i16 %e, %ax
  ret i16 %r
}
