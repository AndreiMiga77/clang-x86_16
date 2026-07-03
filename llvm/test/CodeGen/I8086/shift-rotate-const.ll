; RUN: llc -mtriple=i8086 -O2 -show-mc-encoding -verify-machineinstrs < %s | FileCheck %s
;
; A shift/rotate by 1 uses the D0/D1 by-1 form; by 2, the by-1 form twice (both
; faster than, and no larger than, the CL form).  Counts >= 3 use the smaller CL
; form.

target datalayout = "e-m:e-p:16:16-i32:16-i64:16-f32:16-f64:16-a:8-n8:16-S16"
target triple = "i8086"

declare i16 @llvm.fshl.i16(i16, i16, i16)

; CHECK-LABEL: shl1:
; CHECK: shl ax, 1 {{.*}}encoding: [0xd1,0xe0]
; CHECK-NOT: shl ax, 1
; CHECK-NOT: cl
define i16 @shl1(i16 %x) {
  %r = shl i16 %x, 1
  ret i16 %r
}

; CHECK-LABEL: shl2:
; CHECK: shl ax, 1 {{.*}}encoding: [0xd1,0xe0]
; CHECK: shl ax, 1 {{.*}}encoding: [0xd1,0xe0]
; CHECK-NOT: cl
define i16 @shl2(i16 %x) {
  %r = shl i16 %x, 2
  ret i16 %r
}

; Count 3 keeps the (smaller) CL form.
; CHECK-LABEL: shl3:
; CHECK: shl ax, cl
define i16 @shl3(i16 %x) {
  %r = shl i16 %x, 3
  ret i16 %r
}

; CHECK-LABEL: sar1:
; CHECK: sar ax, 1 {{.*}}encoding: [0xd1,0xf8]
define i16 @sar1(i16 %x) {
  %r = ashr i16 %x, 1
  ret i16 %r
}

; Rotate by 2 -> two by-1 rotates.
; CHECK-LABEL: rol2:
; CHECK: rol ax, 1 {{.*}}encoding: [0xd1,0xc0]
; CHECK: rol ax, 1 {{.*}}encoding: [0xd1,0xc0]
define i16 @rol2(i16 %x) {
  %r = call i16 @llvm.fshl.i16(i16 %x, i16 %x, i16 2)
  ret i16 %r
}
