#!/usr/bin/env bash
#
# End-to-end runtime checks for the i8086 target.  These are NOT lit tests: they
# compile freestanding C to a DOS .COM, run it under DOSBox, and compare the
# output captured through INT 21h file I/O.  They require a built clang/ld.lld,
# DOSBox, and the compiler-rt helpers (compiler-rt/lib/builtins/i8086_builtins.c).
#
# Usage: run.sh <path-to-llvm-build-bin>
set -euo pipefail
BIN="${1:?usage: run.sh <llvm-build-bin-dir>}"
CLANG="$BIN/clang"
LLD="$BIN/ld.lld"
DIR="$(cd "$(dirname "$0")" && pwd)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

# -ffunction-sections lets the linker script put main's section first, so the
# .COM entry point (offset 0x100) is main regardless of definition order.
cflags=(-target i8086 -ffreestanding -O2 -ffunction-sections -c)
# runtime helpers
"$CLANG" "${cflags[@]}" \
  "$DIR/../../../../../compiler-rt/lib/builtins/i8086_builtins.c" -o "$WORK/rt.o"

link() { "$LLD" -o "$WORK/$1.com" --oformat=binary -T "$DIR/com.ld" "${@:2}"; }

run() { # <com> <outfile>
  rm -f "$WORK/$2"
  ( cd "$WORK" && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
      timeout 30 dosbox -c "mount c $WORK" -c "c:" -c "$1" -c "exit" >/dev/null 2>&1 )
  tr -d '\r' < "$WORK/$2"
}

check() { # <name> <got> <want>
  if [ "$2" = "$3" ]; then echo "PASS $1"; else
    echo "FAIL $1: got [$2] want [$3]"; FAILED=1; fi
}
FAILED=0

# wide arithmetic (native mul/div, compiler-rt 32/64-bit, carry chains)
"$CLANG" "${cflags[@]}" "$DIR/wide-arith.c" -o "$WORK/w.o"; link wide "$WORK/w.o" "$WORK/rt.o"
check wide-arith "$(run wide.com RESULT.TXT)" \
"u32mul=700000
u32div=14285
u32mod=5
s32div=4294933963 (=-33333 as u32=4294933963)
u64mul=999000000000
u64div=1001001
shift =800000"

# inc/dec/cmp loop
"$CLANG" "${cflags[@]}" "$DIR/loop.c" -o "$WORK/l.o"; link loop "$WORK/l.o" "$WORK/rt.o"
check loop "$(run loop.com L.TXT)" "5050"

# global access (moffs)
"$CLANG" "${cflags[@]}" "$DIR/globals.c" -o "$WORK/g.o"; link glob "$WORK/g.o" "$WORK/rt.o"
check globals "$(run glob.com G.TXT)" "42"

# swap peephole
"$CLANG" "${cflags[@]}" "$DIR/swap.c" -o "$WORK/s.o"; link swap "$WORK/s.o" "$WORK/rt.o"
check swap "$(run swap.com SW.TXT)" "22,11 11,22"

# struct return + byval, cross-module
"$CLANG" "${cflags[@]}" "$DIR/struct-driver.c" -o "$WORK/sd.o"
"$CLANG" "${cflags[@]}" "$DIR/struct-lib.c" -o "$WORK/sl.o"
link structs "$WORK/sd.o" "$WORK/sl.o" "$WORK/rt.o"
check structs "$(run structs.com OUT.TXT)" \
"use_point=73
sum_big=100520"

# rotations (native ROL/ROR)
"$CLANG" "${cflags[@]}" "$DIR/rotate.c" -o "$WORK/r.o"; link rot "$WORK/r.o" "$WORK/rt.o"
check rotate "$(run rot.com ROT.TXT)" "9025 16675 33"

# shift/rotate by 8 (byte ops through AX)
"$CLANG" "${cflags[@]}" "$DIR/shift-by-8.c" -o "$WORK/s8.o"; link s8 "$WORK/s8.o" "$WORK/rt.o"
check shift-by-8 "$(run s8.com S8.TXT)" "13312 18 65410 13330 18"

exit $FAILED
