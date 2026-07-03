// RUN: llvm-mc -triple=i8086 --show-encoding %s | FileCheck %s
//
// The assembler prefers the shortest available encoding: the xchg-ax accumulator
// form, the 0x83 sign-extended-imm8 ALU group, the AL/AX accumulator forms, and
// the MOV moffs forms; other operands fall back to ModRM.

// CHECK: xchg ax, cx {{.*}}encoding: [0x91]
xchg ax, cx
// CHECK: xchg ax, cx {{.*}}encoding: [0x91]
xchg cx, ax
// CHECK: xchg ax, di {{.*}}encoding: [0x97]
xchg ax, di
// CHECK: xchg cx, dx {{.*}}encoding: [0x87,0xd1]
xchg cx, dx

// The 0x83 group with all eight operations (/0../7 in the reg field).
// CHECK: add bx, 5 {{.*}}encoding: [0x83,0xc3,0x05]
add bx, 5
// CHECK: or bx, 5 {{.*}}encoding: [0x83,0xcb,0x05]
or bx, 5
// CHECK: adc bx, 5 {{.*}}encoding: [0x83,0xd3,0x05]
adc bx, 5
// CHECK: sbb bx, 5 {{.*}}encoding: [0x83,0xdb,0x05]
sbb bx, 5
// CHECK: and bx, 5 {{.*}}encoding: [0x83,0xe3,0x05]
and bx, 5
// CHECK: sub bx, 5 {{.*}}encoding: [0x83,0xeb,0x05]
sub bx, 5
// CHECK: xor bx, 5 {{.*}}encoding: [0x83,0xf3,0x05]
xor bx, 5
// CHECK: cmp bx, 5 {{.*}}encoding: [0x83,0xfb,0x05]
cmp bx, 5

// Accumulator forms.
// CHECK: add al, 5 {{.*}}encoding: [0x04,0x05]
add al, 5
// CHECK: add ax, 300 {{.*}}encoding: [0x05,0x2c,0x01]
add ax, 300
// CHECK: cmp al, 7 {{.*}}encoding: [0x3c,0x07]
cmp al, 7
// CHECK: test ax, 300 {{.*}}encoding: [0xa9,0x2c,0x01]
test ax, 300

// MOV moffs (accumulator <-> [disp16]).
// CHECK: mov ax, [4660] {{.*}}encoding: [0xa1,0x34,0x12]
mov ax, [0x1234]
// CHECK: mov [4660], ax {{.*}}encoding: [0xa3,0x34,0x12]
mov [0x1234], ax
// CHECK: mov al, [4660] {{.*}}encoding: [0xa0,0x34,0x12]
mov al, [0x1234]
