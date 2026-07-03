; RUN: llc -mtriple=i8086 < %s | FileCheck %s
;
; Wide add/sub lower to an ADD/ADC (SUB/SBB) carry chain, not a cmp-and-branch
; carry synthesis.

target datalayout = "e-m:e-p:16:16-i32:16-i64:16-f32:16-f64:16-a:8-n8:16-S16"
target triple = "i8086"

; CHECK-LABEL: add32:
; CHECK: add ax, cx
; CHECK: adc dx, bx
; CHECK-NOT: {{jb|jae}}
define i32 @add32(i32 %a, i32 %b) {
  %r = add i32 %a, %b
  ret i32 %r
}

; CHECK-LABEL: sub32:
; CHECK: sub ax, cx
; CHECK: sbb dx, bx
define i32 @sub32(i32 %a, i32 %b) {
  %r = sub i32 %a, %b
  ret i32 %r
}

; A zero high word uses `adc reg,0` (immediate), never a flag-clobbering xor.
; CHECK-LABEL: addk:
; CHECK: add ax, 1
; CHECK: adc dx, 0
define i32 @addk(i32 %a) {
  %r = add i32 %a, 1
  ret i32 %r
}

; CHECK-LABEL: add64:
; CHECK: add ax,
; CHECK: adc
; CHECK: adc
; CHECK: adc
define i64 @add64(i64 %a, i64 %b) {
  %r = add i64 %a, %b
  ret i64 %r
}
