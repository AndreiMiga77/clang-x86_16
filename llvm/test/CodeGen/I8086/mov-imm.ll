; RUN: llc -mtriple=i8086 -O2 -show-mc-encoding < %s | FileCheck %s
;
; MOV immediate into a register uses the short B0-BF form (never the C6/C7
; register-direct form), and a store of an immediate to memory is folded into a
; single C6/C7 instruction instead of load-to-reg-then-store.

target datalayout = "e-m:e-p:16:16-i32:16-i64:16-f32:16-f64:16-a:8-n8:16-S16"
target triple = "i8086"

; Register destination -> 0xB8 (not 0xC7).
; CHECK-LABEL: regimm:
; CHECK: mov ax, 5 {{.*}}encoding: [0xb8,0x05,0x00]
define i16 @regimm() {
  ret i16 5
}

; Store i16 immediate -> a single C7 to memory (ModRM is a memory form, mod!=11).
; CHECK-LABEL: storeimm16:
; CHECK: mov [bx], 5 {{.*}}encoding: [0xc7,0x07,0x05,0x00]
; CHECK-NOT: mov ax, 5
define void @storeimm16(ptr %p) {
  store i16 5, ptr %p
  ret void
}

; Store i8 immediate -> a single C6 to memory.
; CHECK-LABEL: storeimm8:
; CHECK: mov [bx], 7 {{.*}}encoding: [0xc6,0x07,0x07]
define void @storeimm8(ptr %p) {
  store i8 7, ptr %p
  ret void
}
