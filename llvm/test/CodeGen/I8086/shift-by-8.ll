; RUN: llc -mtriple=i8086 -O2 -verify-machineinstrs < %s | FileCheck %s
;
; A 16-bit shift/rotate by 8 becomes byte-register moves through AX instead of a
; 40-cycle shift-by-CL.  (The xor keeps the value in a register so it is not
; folded into a narrowed load.)

target datalayout = "e-m:e-p:16:16-i32:16-i64:16-f32:16-f64:16-a:8-n8:16-S16"
target triple = "i8086"

declare i16 @llvm.fshl.i16(i16, i16, i16)

; shl by 8: high := low, low := 0.
; CHECK-LABEL: shl8:
; CHECK: mov ah, al
; CHECK: mov al, 0
; CHECK-NOT: cl
define i16 @shl8(i16 %x) {
  %y = xor i16 %x, 4660
  %r = shl i16 %y, 8
  ret i16 %r
}

; srl by 8: low := high, high := 0.
; CHECK-LABEL: shr8:
; CHECK: mov al, ah
; CHECK: mov ah, 0
; CHECK-NOT: cl
define i16 @shr8(i16 %x) {
  %y = xor i16 %x, 4660
  %r = lshr i16 %y, 8
  ret i16 %r
}

; sar by 8: low := high, high := sign extension (CBW).
; CHECK-LABEL: sar8:
; CHECK: mov al, ah
; CHECK: cbw
define i16 @sar8(i16 %x) {
  %y = xor i16 %x, 4660
  %r = ashr i16 %y, 8
  ret i16 %r
}

; rotate by 8 = byte swap.
; CHECK-LABEL: rol8:
; CHECK: xchg al, ah
; CHECK-NOT: cl
define i16 @rol8(i16 %x) {
  %y = xor i16 %x, 4660
  %r = call i16 @llvm.fshl.i16(i16 %y, i16 %y, i16 8)
  ret i16 %r
}

; (u8)(x >> 8): just the high byte, no zeroing.
; CHECK-LABEL: shr8trunc:
; CHECK-NOT: mov ah, 0
; CHECK-NOT: cl
define i8 @shr8trunc(i16 %x) {
  %y = xor i16 %x, 4660
  %s = lshr i16 %y, 8
  %r = trunc i16 %s to i8
  ret i8 %r
}

; A shift by 9..15 is the byte op plus a residual shift of 1..7.  For a residual
; of 1 or 2 the by-1 form is used; for 3..7 the CL form.
; shl by 9 = (hi:=lo, lo:=0) then one shl-by-1.
; CHECK-LABEL: shl9:
; CHECK: mov ah, al
; CHECK: mov al, 0
; CHECK: shl ax, 1
; CHECK-NOT: shl ax, 1
define i16 @shl9(i16 %x) {
  %y = xor i16 %x, 4660
  %r = shl i16 %y, 9
  ret i16 %r
}

; shl by 12 = byte op then residual shift of 4 via CL.
; CHECK-LABEL: shl12:
; CHECK: mov ah, al
; CHECK: mov al, 0
; CHECK: mov cl, 4
; CHECK: shl ax, cl
define i16 @shl12(i16 %x) {
  %y = xor i16 %x, 4660
  %r = shl i16 %y, 12
  ret i16 %r
}

; ashr by 11 = (lo:=hi, hi:=sign) then residual sar of 3 via CL.
; CHECK-LABEL: sar11:
; CHECK: mov al, ah
; CHECK: cbw
; CHECK: mov cl, 3
; CHECK: sar ax, cl
define i16 @sar11(i16 %x) {
  %y = xor i16 %x, 4660
  %r = ashr i16 %y, 11
  ret i16 %r
}
