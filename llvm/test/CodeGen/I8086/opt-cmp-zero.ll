; RUN: llc -mtriple=i8086 -O2 -verify-machineinstrs < %s | FileCheck %s
;
; Equality tests against zero:
;  * when a flag-setting ALU op already produced the value, the `cmp reg,0` is
;    redundant (the op set ZF) and is dropped;
;  * otherwise `cmp reg,0` becomes the one-byte-shorter `test reg,reg`.
; Only equality (je/jne) uses are touched; a memory operand is left as `cmp`.

target datalayout = "e-m:e-p:16:16-i32:16-i64:16-f32:16-f64:16-a:8-n8:16-S16"
target triple = "i8086"

declare void @use(i16)

; dec sets ZF: the back-edge test needs no compare.
; CHECK-LABEL: countdown:
; CHECK: dec si
; CHECK-NEXT: jne
; CHECK-NOT: cmp
define void @countdown(i16 %n) {
entry:
  br label %loop
loop:
  %i = phi i16 [ %n, %entry ], [ %d, %loop ]
  call void @use(i16 %i)
  %d = sub i16 %i, 1
  %c = icmp eq i16 %d, 0
  br i1 %c, label %exit, label %loop
exit:
  ret void
}

; sub by 2 (canonicalized to add -2) also sets ZF -> no compare.
; CHECK-LABEL: subby2:
; CHECK: add si, -2
; CHECK-NEXT: jne
; CHECK-NOT: cmp
define void @subby2(i16 %n) {
entry:
  br label %loop
loop:
  %i = phi i16 [ %n, %entry ], [ %d, %loop ]
  call void @use(i16 %i)
  %d = sub i16 %i, 2
  %c = icmp eq i16 %d, 0
  br i1 %c, label %exit, label %loop
exit:
  ret void
}

; add produces the value and its flags -> compare against 0 elided.
; CHECK-LABEL: addzero:
; CHECK: add ax,
; CHECK-NEXT: je
; CHECK-NOT: cmp
define i16 @addzero(i16 %x, i16 %y) {
  %a = add i16 %x, %y
  %c = icmp eq i16 %a, 0
  %s = select i1 %c, i16 10, i16 20
  ret i16 %s
}

; a value only loaded (no flags) -> shrink cmp reg,0 to test reg,reg.
; CHECK-LABEL: testreg:
; CHECK: test ax, ax
; CHECK-NOT: cmp
define void @testreg() {
  %v = load volatile i16, ptr inttoptr (i16 512 to ptr)
  %c = icmp eq i16 %v, 0
  br i1 %c, label %a, label %b
a:
  call void @use(i16 %v)
  br label %b
b:
  ret void
}

; A signed (non-equality) test against zero is out of scope: keep `cmp`, since
; `test` would leave CF/OF meaningless and dropping it is unsafe for jl.
; CHECK-LABEL: signed:
; CHECK: cmp
; CHECK-NOT: test
define i16 @signed(i16 %x, i16 %y) {
  %a = add i16 %x, %y
  %c = icmp slt i16 %a, 0
  %s = select i1 %c, i16 10, i16 20
  ret i16 %s
}
