; RUN: llc -mtriple=i8086 < %s | FileCheck %s
;
; 16-bit multiply/divide use the native single-operand MUL/DIV (implicit AX/DX);
; wider divides fall back to compiler-rt libcalls.

target datalayout = "e-m:e-p:16:16-i32:16-i64:16-f32:16-f64:16-a:8-n8:16-S16"
target triple = "i8086"

; CHECK-LABEL: mul16:
; CHECK: imul cx
define i16 @mul16(i16 %a, i16 %b) {
  %r = mul i16 %a, %b
  ret i16 %r
}

; CHECK-LABEL: udiv16:
; CHECK: div cx
; CHECK-NOT: call
define i16 @udiv16(i16 %a, i16 %b) {
  %r = udiv i16 %a, %b
  ret i16 %r
}

; CHECK-LABEL: sdiv16:
; CHECK: cwd
; CHECK: idiv cx
define i16 @sdiv16(i16 %a, i16 %b) {
  %r = sdiv i16 %a, %b
  ret i16 %r
}

; 32-bit divide is a libcall; 32-bit multiply expands inline (no __mulsi3).
; CHECK-LABEL: udiv32:
; CHECK: call __udivsi3
define i32 @udiv32(i32 %a, i32 %b) {
  %r = udiv i32 %a, %b
  ret i32 %r
}
