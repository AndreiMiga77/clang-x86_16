; RUN: llc -mtriple=i8086 -O2 -show-mc-encoding -verify-machineinstrs < %s | FileCheck %s
;
; Rotations use the native ROL/ROR (group 2, by CL): the count is moved into CL
; and rotated, mirroring the shift-by-CL lowering.

target datalayout = "e-m:e-p:16:16-i32:16-i64:16-f32:16-f64:16-a:8-n8:16-S16"
target triple = "i8086"

declare i16 @llvm.fshl.i16(i16, i16, i16)
declare i16 @llvm.fshr.i16(i16, i16, i16)
declare i8 @llvm.fshl.i8(i8, i8, i8)

; CHECK-LABEL: rol16:
; CHECK: rol ax, cl {{.*}}encoding: [0xd3,0xc0]
define i16 @rol16(i16 %x, i16 %n) {
  %r = call i16 @llvm.fshl.i16(i16 %x, i16 %x, i16 %n)
  ret i16 %r
}

; CHECK-LABEL: ror16:
; CHECK: ror ax, cl {{.*}}encoding: [0xd3,0xc8]
define i16 @ror16(i16 %x, i16 %n) {
  %r = call i16 @llvm.fshr.i16(i16 %x, i16 %x, i16 %n)
  ret i16 %r
}

; CHECK-LABEL: rol8:
; CHECK: rol al, cl {{.*}}encoding: [0xd2,0xc0]
define i8 @rol8(i8 %x, i8 %n) {
  %r = call i8 @llvm.fshl.i8(i8 %x, i8 %x, i8 %n)
  ret i8 %r
}

; A constant rotate past the half-width flips direction for a shorter count:
; rol16 by 12 == ror16 by 4.
; CHECK-LABEL: rol12:
; CHECK: mov cl, 4
; CHECK: ror ax, cl
define i16 @rol12(i16 %x) {
  %y = xor i16 %x, 4660
  %r = call i16 @llvm.fshl.i16(i16 %y, i16 %y, i16 12)
  ret i16 %r
}

; ror16 by 11 == rol16 by 5.
; CHECK-LABEL: ror11:
; CHECK: mov cl, 5
; CHECK: rol ax, cl
define i16 @ror11(i16 %x) {
  %y = xor i16 %x, 4660
  %r = call i16 @llvm.fshr.i16(i16 %y, i16 %y, i16 11)
  ret i16 %r
}

; For bytes the pivot is 4: rol8 by 6 == ror8 by 2 (two by-1 rotates).
; CHECK-LABEL: rolb6:
; CHECK: ror al, 1
; CHECK: ror al, 1
; CHECK-NOT: cl
define i8 @rolb6(i8 %x) {
  %y = xor i8 %x, 66
  %r = call i8 @llvm.fshl.i8(i8 %y, i8 %y, i8 6)
  ret i8 %r
}
