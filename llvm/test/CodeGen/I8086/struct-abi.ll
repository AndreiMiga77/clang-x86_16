; RUN: llc -mtriple=i8086 -O2 -verify-machineinstrs < %s | FileCheck %s
;
; cdecl tiny-model aggregate ABI: struct return via a hidden sret pointer that is
; returned in AX, and byval aggregate arguments pushed on the stack (caller
; cleaned).

target datalayout = "e-m:e-p:16:16-i32:16-i64:16-f32:16-f64:16-a:8-n8:16-S16"
target triple = "i8086"

%struct.P = type { i16, i16 }

; The hidden sret pointer arrives at [bp+4] and is returned in AX.
; CHECK-LABEL: make:
; CHECK: mov bx, [bp + 4]
; CHECK: mov [bx], ax
; CHECK: mov ax, bx
; CHECK: ret
define void @make(ptr noalias sret(%struct.P) %ret, i16 %x, i16 %y) {
  %px = getelementptr %struct.P, ptr %ret, i16 0, i32 0
  store i16 %x, ptr %px
  %py = getelementptr %struct.P, ptr %ret, i16 0, i32 1
  store i16 %y, ptr %py
  ret void
}

declare i16 @callee(ptr byval(%struct.P))

; A byval struct is pushed word by word, then the caller cleans the stack.
; CHECK-LABEL: caller:
; CHECK: push cx
; CHECK: push ax
; CHECK: call callee
; CHECK: add sp, 4
define i16 @caller(i16 %x, i16 %y) {
  %p = alloca %struct.P
  %px = getelementptr %struct.P, ptr %p, i16 0, i32 0
  store i16 %x, ptr %px
  %r = call i16 @callee(ptr byval(%struct.P) %p)
  ret i16 %r
}
