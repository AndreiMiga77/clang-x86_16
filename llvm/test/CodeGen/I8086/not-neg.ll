; RUN: llc -mtriple=i8086 -O2 -show-mc-encoding -verify-machineinstrs < %s | FileCheck %s
;
; ~x lowers to NOT (2 bytes, 3 cycles, leaves FLAGS untouched) rather than
; xor r,-1; -x lowers to NEG (one in-place instruction that sets FLAGS) rather
; than xor r,r + sub.

target datalayout = "e-m:e-p:16:16-i32:16-i64:16-f32:16-f64:16-a:8-n8:16-S16"
target triple = "i8086"

; CHECK-LABEL: not16:
; CHECK: not ax {{.*}}encoding: [0xf7,0xd0]
; CHECK-NOT: xor
define i16 @not16(i16 %x) {
  %r = xor i16 %x, -1
  ret i16 %r
}

; CHECK-LABEL: not8:
; CHECK: not al {{.*}}encoding: [0xf6,0xd0]
; CHECK-NOT: xor
define i8 @not8(i8 %x) {
  %r = xor i8 %x, -1
  ret i8 %r
}

; CHECK-LABEL: neg16:
; CHECK: neg ax {{.*}}encoding: [0xf7,0xd8]
; CHECK-NOT: sub
define i16 @neg16(i16 %x) {
  %r = sub i16 0, %x
  ret i16 %r
}

; CHECK-LABEL: neg8:
; CHECK: neg al {{.*}}encoding: [0xf6,0xd8]
; CHECK-NOT: sub
define i8 @neg8(i8 %x) {
  %r = sub i8 0, %x
  ret i8 %r
}
