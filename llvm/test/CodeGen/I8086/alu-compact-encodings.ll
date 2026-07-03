; RUN: llc -mtriple=i8086 -show-mc-encoding < %s | FileCheck %s
;
; Compact ALU immediate encodings: INC/DEC for +/-1, the 0x83 sign-extended
; imm8 group, and the AL/AX accumulator short forms.

target datalayout = "e-m:e-p:16:16-i32:16-i64:16-f32:16-f64:16-a:8-n8:16-S16"
target triple = "i8086"

; CHECK-LABEL: inc16:
; CHECK: inc ax {{.*}}encoding: [0x40]
define i16 @inc16(i16 %x) {
  %r = add i16 %x, 1
  ret i16 %r
}

; CHECK-LABEL: dec16:
; CHECK: dec ax {{.*}}encoding: [0x48]
define i16 @dec16(i16 %x) {
  %r = sub i16 %x, 1
  ret i16 %r
}

; CHECK-LABEL: inc8:
; CHECK: inc al {{.*}}encoding: [0xfe,0xc0]
define i8 @inc8(i8 %x) {
  %r = add i8 %x, 1
  ret i8 %r
}

; Small immediate: 3-byte 0x83 form, not the 4-byte 0x81 form.
; CHECK-LABEL: addimm8:
; CHECK: add ax, 5 {{.*}}encoding: [0x83,0xc0,0x05]
define i16 @addimm8(i16 %x) {
  %r = add i16 %x, 5
  ret i16 %r
}

; Large immediate on AX: 3-byte accumulator form (0x05), not the 4-byte 0x81.
; CHECK-LABEL: addimm16:
; CHECK: add ax, 300 {{.*}}encoding: [0x05,0x2c,0x01]
define i16 @addimm16(i16 %x) {
  %r = add i16 %x, 300
  ret i16 %r
}

; CHECK-LABEL: andimm8:
; CHECK: and ax, 10 {{.*}}encoding: [0x83,0xe0,0x0a]
define i16 @andimm8(i16 %x) {
  %r = and i16 %x, 10
  ret i16 %r
}

; 8-bit accumulator form: 2 bytes (0x04) instead of 3 (0x80 /0).
; CHECK-LABEL: add8acc:
; CHECK: add al, 5 {{.*}}encoding: [0x04,0x05]
define i8 @add8acc(i8 %x) {
  %r = add i8 %x, 5
  ret i8 %r
}
