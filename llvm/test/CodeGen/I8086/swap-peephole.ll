; RUN: llc -mtriple=i8086 -O2 -show-mc-encoding -verify-machineinstrs < %s | FileCheck %s
;
; A register swap that the allocator lowers to 3 MOVs through a scratch is folded
; into a single XCHG.  Since AX is involved, the 1-byte accumulator form is used.

target datalayout = "e-m:e-p:16:16-i32:16-i64:16-f32:16-f64:16-a:8-n8:16-S16"
target triple = "i8086"

declare i16 @use(i16, i16)

; CHECK-LABEL: f:
; CHECK: xchg ax, bx {{.*}}encoding: [0x93]
; CHECK-NOT: mov ax, bx
define i16 @f(i16 %n, i16 %a, i16 %b) {
entry:
  br label %loop
loop:
  %nn = phi i16 [ %n, %entry ], [ %nd, %loop ]
  %aa = phi i16 [ %a, %entry ], [ %bb, %loop ]
  %bb = phi i16 [ %b, %entry ], [ %aa, %loop ]
  %nd = sub i16 %nn, 1
  %c = icmp sgt i16 %nn, 0
  br i1 %c, label %loop, label %done
done:
  %r = call i16 @use(i16 %aa, i16 %bb)
  ret i16 %r
}
