; RUN: llc -mtriple=i8086 -show-mc-encoding < %s | FileCheck %s
;
; AL/AX to/from a bare direct address use the moffs opcodes (0xA0-0xA3), one
; byte shorter than the ModRM form.

target datalayout = "e-m:e-p:16:16-i32:16-i64:16-f32:16-f64:16-a:8-n8:16-S16"
target triple = "i8086"

@g = global i16 0
@b = global i8 0

; CHECK-LABEL: gload:
; CHECK: mov ax, [g] {{.*}}encoding: [0xa1,A,A]
define i16 @gload() {
  %v = load i16, ptr @g
  ret i16 %v
}

; CHECK-LABEL: gstore:
; CHECK: mov [g], ax {{.*}}encoding: [0xa3,A,A]
define void @gstore(i16 %v) {
  store i16 %v, ptr @g
  ret void
}

; CHECK-LABEL: bload:
; CHECK: mov al, [b] {{.*}}encoding: [0xa0,A,A]
define i8 @bload() {
  %v = load i8, ptr @b
  ret i8 %v
}
