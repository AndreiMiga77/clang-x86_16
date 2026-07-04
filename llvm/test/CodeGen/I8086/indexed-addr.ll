; RUN: llc -mtriple=i8086 -O2 -verify-machineinstrs < %s | FileCheck %s
;
; Folding address arithmetic into a two-register [base+index] operand is only a
; win when the index is *already* in a callee-saved SI/DI (so no move/spill is
; introduced) -- a post-register-allocation fact.  So there is no isel folding on
; any CPU; the post-RA I8086FoldIndexAddr pass does it precisely when the index
; register is already SI/DI and the base BX/BP.

target datalayout = "e-m:e-p:16:16-i32:16-i64:16-f32:16-f64:16-a:8-n8:16-S16"
target triple = "i8086"

; Leaf a[i]: the index (i*2) lands in a caller-saved register (AX), not SI/DI, so
; folding would force a callee-saved push/pop that costs more than the add saves.
; Left as an explicit add + single-register access -- and no SI/DI is touched.
; CHECK-LABEL: notfolded:
; CHECK: add bx, [bp + {{[0-9]+}}]
; CHECK: mov ax, [bx]
; CHECK-NOT: [bx + si]
; CHECK-NOT: push si
define i16 @notfolded(i16* %a, i16 %i) {
  %p = getelementptr i16, i16* %a, i16 %i
  %v = load i16, i16* %p
  ret i16 %v
}

; When the base is already in a register (here a global's address, materialized
; with mov) and the index is already in SI, the address add folds into a single
; [bx+si] load.  (A base that is itself a memory value would instead be pulled
; into the address arithmetic by the ALU load-fold.)
@arr = external global [0 x i16]
; CHECK-LABEL: folded:
; CHECK: mov bx, arr
; CHECK: mov ax, [bx + si]
; CHECK-NOT: add bx,
define i16 @folded() {
  %i = call i16 asm sideeffect "", "={si}"()
  %p = getelementptr [0 x i16], [0 x i16]* @arr, i16 0, i16 %i
  %v = load i16, i16* %p
  ret i16 %v
}
