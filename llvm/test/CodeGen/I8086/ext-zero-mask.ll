; RUN: llc -mtriple=i8086 -show-mc-encoding < %s | FileCheck %s
;
; 8086 has no MOVZX/MOVSX: zero-extend clears the high byte (xor), sign-extend
; uses CBW, zeroing uses xor, and a byte mask becomes a high-byte clear.

target datalayout = "e-m:e-p:16:16-i32:16-i64:16-f32:16-f64:16-a:8-n8:16-S16"
target triple = "i8086"

; zext i8->i16 clears the paired high byte instead of AND reg,0xff.
; CHECK-LABEL: zx:
; CHECK: xor ah, ah {{.*}}encoding: [0x30,0xe4]
; CHECK-NOT: and
define i16 @zx(i8 %c) {
  %r = zext i8 %c to i16
  ret i16 %r
}

; sext i8->i16 via CBW.
; CHECK-LABEL: sx:
; CHECK: cbw {{.*}}encoding: [0x98]
define i16 @sx(i8 %c) {
  %r = sext i8 %c to i16
  ret i16 %r
}

; Zeroing uses xor (2 bytes), not mov reg,0 (3 bytes).
; CHECK-LABEL: zero:
; CHECK: xor ax, ax {{.*}}encoding: [0x31,0xc0]
define i16 @zero() {
  ret i16 0
}

; `and reg,0xff` with flags dead -> byte-clearing xor.
; CHECK-LABEL: mask:
; CHECK: xor ah, ah {{.*}}encoding: [0x30,0xe4]
; CHECK-NOT: and
define i16 @mask(i16 %x) {
  %r = and i16 %x, 255
  ret i16 %r
}
