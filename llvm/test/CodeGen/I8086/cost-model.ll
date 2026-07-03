; RUN: llc -mtriple=i8086 -O2 -i8086-annotate-cost < %s | FileCheck %s
;
; The cost model prints an estimated 8086 size and cycle cost per instruction.
; The cycle figure is max(EU cycles from costs.md, 4 * bus words), so cheap
; register instructions are correctly bus/prefetch-bound (a 2-clock mov reg,reg
; really costs ~4), while slow ones stay dominated by their execution time.

target datalayout = "e-m:e-p:16:16-i32:16-i64:16-f32:16-f64:16-a:8-n8:16-S16"
target triple = "i8086"

; mov reg,reg: EU 2, but one 2-byte word of prefetch -> 4 cycles.
; add reg,reg: EU 3, likewise fetch-bound at 4.
; a 3-byte reg,imm add is 2 words -> 8.  imul is execution-bound.
; a [bp+disp] load: EA = 5 (BP) + 4 (disp) = 9, EU 8+9 = 17.
; CHECK-LABEL: costs:
; CHECK: mov bp, sp {{.*}}cost: 4 cyc, 2 B
; CHECK: mov {{[a-z]+}}, [bp + {{[0-9]+}}] {{.*}}cost: 17 cyc, 3 B
; CHECK: add {{[a-z]+}}, {{[a-z]+}} {{.*}}cost: 4 cyc, 2 B
; CHECK: imul {{[a-z]+}} {{.*}}cost: 141 cyc, 2 B
; CHECK: add {{[a-z]+}}, 5 {{.*}}cost: 8 cyc, 3 B
; CHECK: ret {{.*}}cost: 16 cyc, 1 B
define i16 @costs(i16 %a, i16 %b) {
  %x = add i16 %a, %b
  %y = mul i16 %x, %a
  %z = add i16 %y, 5
  ret i16 %z
}

; A short jump is 2 bytes; a control transfer refills the queue (EU 15).
; CHECK-LABEL: jumpcost:
; CHECK: jmp {{.*}}cost: 15 cyc, 2 B
define void @jumpcost() {
entry:
  br label %l
l:
  br label %l
}
