; RUN: llc -mtriple=i8086 -O2 -i8086-annotate-cost < %s | FileCheck %s --check-prefixes=CHECK,I86
; RUN: llc -mtriple=i8086 -mcpu=i8088 -O2 -i8086-annotate-cost < %s | FileCheck %s --check-prefixes=CHECK,I88
;
; The cost model prints an estimated size and cycle cost per instruction:
; max(EU cycles from costs.md, bus occupancy).  The bus term is tuning-specific:
; the 8086's 16-bit bus moves 2 bytes / 4 clocks, the 8088's 8-bit bus 1 byte /
; 4 clocks (and adds a bus cycle per word memory access in execution).  So a
; 2-byte mov reg,reg is fetch-bound at 4 clocks on the 8086 but 8 on the 8088.

target datalayout = "e-m:e-p:16:16-i32:16-i64:16-f32:16-f64:16-a:8-n8:16-S16"
target triple = "i8086"

; mov reg,reg (2 bytes): fetch-bound -> 4 (8086) / 8 (8088).
; a [bp+disp] load: EA 9, EU 8+9 = 17 (8086); +4 word-access penalty = 21 (8088).
; a folded add reg,[bp+disp]: EU 9+9 = 18 (8086); +4 = 22 (8088).
; imul is execution-bound (same on both).  add reg,imm16 is 3 bytes.
; CHECK-LABEL: costs:
; I86: mov bp, sp {{.*}}cost: 4 cyc, 2 B
; I88: mov bp, sp {{.*}}cost: 8 cyc, 2 B
; I86: mov {{[a-z]+}}, [bp + {{[0-9]+}}] {{.*}}cost: 17 cyc, 3 B
; I88: mov {{[a-z]+}}, [bp + {{[0-9]+}}] {{.*}}cost: 21 cyc, 3 B
; I86: add {{[a-z]+}}, [bp + {{[0-9]+}}] {{.*}}cost: 18 cyc, 3 B
; I88: add {{[a-z]+}}, [bp + {{[0-9]+}}] {{.*}}cost: 22 cyc, 3 B
; CHECK: imul {{[a-z]+}} {{.*}}cost: 141 cyc, 2 B
; I86: add {{[a-z]+}}, 5 {{.*}}cost: 8 cyc, 3 B
; I88: add {{[a-z]+}}, 5 {{.*}}cost: 12 cyc, 3 B
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
