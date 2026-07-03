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
