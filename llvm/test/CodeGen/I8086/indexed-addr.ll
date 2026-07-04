; RUN: llc -mtriple=i8086 -O2 < %s | FileCheck %s --check-prefix=BASE
; RUN: llc -mtriple=i8086 -mcpu=i8088 -O2 < %s | FileCheck %s --check-prefix=BASE
; RUN: llc -mtriple=i8086 -mcpu=i80186 -O2 < %s | FileCheck %s --check-prefix=FASTEA
;
; Two-register [base+index] addressing is only folded by isel when the effective
; address is cheap (80186+, FeatureFastEA).  On the 8086/8088 the high EA cost
; makes it a loss in general -- and the index would have to occupy a callee-saved
; SI/DI -- so the address arithmetic stays an explicit add feeding a
; single-register [reg] access.

target datalayout = "e-m:e-p:16:16-i32:16-i64:16-f32:16-f64:16-a:8-n8:16-S16"
target triple = "i8086"

; BASE-LABEL: load1:
; BASE: add bx, {{[a-z]+}}
; BASE: mov ax, [bx]
; BASE-NOT: [bx + si]
;
; FASTEA-LABEL: load1:
; FASTEA: mov ax, [bx + si]
; FASTEA-NOT: add bx,
define i16 @load1(i16* %a, i16 %i) {
  %p = getelementptr i16, i16* %a, i16 %i
  %v = load i16, i16* %p
  ret i16 %v
}
