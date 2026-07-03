# i8086 end-to-end runtime tests

These compile freestanding C to a DOS `.COM`, run it under DOSBox, and check the
output (captured via `INT 21h` file I/O).  They are **not** lit tests (the
directory is marked `config.unsupported` so lit skips it) because they need
DOSBox, `ld.lld`, and the compiler-rt helpers.

Run with:

    ./run.sh <path-to-llvm-build>/bin

Programs and what they exercise:

| file               | exercises                                             | output              |
|--------------------|-------------------------------------------------------|---------------------|
| `wide-arith.c`     | native 16-bit mul/div, compiler-rt 32/64-bit, carry   | u32/u64 mul/div/... |
| `loop.c`           | inc/dec, cmp, branches (sum 1..100)                   | `5050`              |
| `globals.c`        | direct global load/store (moffs)                      | `42`                |
| `swap.c`           | the swap->XCHG peephole                                | `22,11 11,22`       |
| `struct-driver.c` + `struct-lib.c` | struct return (sret) + byval args    | `73` / `100520`     |

`com.ld` is the linker script that lays a `.COM` out at `ORG 0x100`.
