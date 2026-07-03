; RUN: llc -mtriple=i8086 -O2 -show-mc-encoding -verify-machineinstrs < %s | FileCheck %s
;
; Unconditional branches select the short rel8 JMP8 (2 bytes, opcode 0xEB).  When
; the target is out of rel8 range, branch relaxation grows it to the near rel16
; JMP16 (0xE9); a too-far conditional Jcc is inverted over such a near jump.

target datalayout = "e-m:e-p:16:16-i32:16-i64:16-f32:16-f64:16-a:8-n8:16-S16"
target triple = "i8086"

declare void @sink(i16)

; A near back-edge fits in rel8: short jmp (0xEB).
; CHECK-LABEL: nearloop:
; CHECK: jmp {{.*}}encoding: [0xeb,A]
; CHECK-NOT: encoding: [0xe9
define void @nearloop() {
entry:
  br label %l
l:
  call void @sink(i16 0)
  br label %l
}

; A back-edge more than 127 bytes away no longer fits in rel8: relaxed to the
; near jmp (0xE9).
; CHECK-LABEL: farloop:
; CHECK: jmp {{.*}}encoding: [0xe9
; CHECK-NOT: encoding: [0xeb
define void @farloop() {
entry:
  br label %l
l:
  call void asm sideeffect ".space 200", ""()
  br label %l
}

; A too-far conditional branch is inverted and jumps over a near (rel16) jump to
; the real target.
; CHECK-LABEL: farcond:
; CHECK: jne {{.*}}encoding: [0x75,A]
; CHECK: jmp {{.*}}encoding: [0xe9
define void @farcond(i16 %n) {
entry:
  %c = icmp eq i16 %n, 0
  br i1 %c, label %skip, label %big
big:
  call void asm sideeffect ".space 200", ""()
  br label %skip
skip:
  ret void
}
