# The `i8086` LLVM Target

An **assembler-only** LLVM backend for the original **Intel 8086** (16-bit),
together with **`lld`** support for producing flat **DOS `.COM`** executables.

It plugs a brand-new target (`Triple::i8086`) into LLVM's MC layer so that
`llvm-mc` can assemble 8086 assembly into ELF objects, and adds an `lld/ELF`
architecture so `ld.lld` can link those objects into a runnable `.COM` file
loaded at `CS:0x100`.

```text
hello.s ──llvm-mc──▶ hello.o (ELF, EM_I8086) ──ld.lld ──oformat binary──▶ hello.com (flat)
```

There is **no CodeGen backend** (no instruction selection, register allocation,
etc.) — this target exists purely to emit and link 8086 machine code. That keeps
the footprint small: register/instruction TableGen, a hand-written MC code
emitter, an Intel-syntax assembly parser, an ELF object writer, and a small lld
relocation handler.

---

## What it can do

* Assemble the **complete original 8086 instruction set** (Intel syntax):
  * Data movement — `mov`, `push`, `pop`, `xchg`, `in`, `out`, `lea`, `lds`,
    `les`, `lahf`, `sahf`, `pushf`, `popf`, `xlat`
  * Arithmetic — `add`, `adc`, `sub`, `sbb`, `cmp`, `inc`, `dec`, `neg`, `mul`,
    `imul`, `div`, `idiv`, `daa`, `das`, `aaa`, `aas`, `aam`, `aad`, `cbw`, `cwd`
  * Logic / shifts — `and`, `or`, `xor`, `not`, `test`, `shl`/`sal`, `shr`,
    `sar`, `rol`, `ror`, `rcl`, `rcr` (by `1` and by `cl`)
  * String ops — `movs`, `cmps`, `scas`, `lods`, `stos` (`b`/`w`), with `rep`,
    `repe`/`repz`, `repne`/`repnz`, `lock` prefixes
  * Control flow — `jmp`/`call` (near, direct and indirect), all 16 `Jcc`,
    `loop`/`loope`/`loopne`, `jcxz`, `ret`/`retf` (with/without immediate),
    `int`, `int3`, `into`, `iret`
  * Flag / processor control — `clc`, `stc`, `cmc`, `cld`, `std`, `cli`, `sti`,
    `nop`, `hlt`, `wait`
* Full 8086 memory addressing: `[bx+si+disp]`, `[bp-2]`, `[si]`, `[0x1234]`,
  with segment overrides (`es:[bx]`) and `byte ptr` / `word ptr` size hints.
* Correct ModR/M, displacement, immediate and PC-relative encoding.
* Emit **ELF** relocatable objects (`e_machine = EM_I8086`, value `0x8086`) with
  `R_I8086_{8,16,8_PCREL,16_PCREL}` relocations.
* Link with `ld.lld` to a flat `.COM` binary at `ORG 0x100`.

### Not implemented (by design)

* No CodeGen / C compilation — this is an assembler + linker path only.
* No disassembler.
* Accumulator-short forms (e.g. `04 ib` for `add al, imm`) and far **direct**
  `jmp/call seg:off` are intentionally omitted from the parser to avoid matcher
  ambiguity; the equivalent general encodings are used instead.

---

## Building

Requirements: CMake, Ninja, a C++17 compiler.

Configure a minimal build with only the `i8086` target plus `lld`:

```sh
cd llvm-project
cmake -G Ninja -S llvm -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_TARGETS_TO_BUILD="" \
  -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD=I8086 \
  -DLLVM_ENABLE_PROJECTS=lld \
  -DLLVM_ENABLE_ASSERTIONS=ON

ninja -C build llvm-mc ld.lld
```

`i8086` is registered as an **experimental** target, so it must be requested via
`LLVM_EXPERIMENTAL_TARGETS_TO_BUILD` (not `LLVM_TARGETS_TO_BUILD`).

---

## Usage

### 1. Write some 8086 assembly

`hello.s` — a DOS "Hello, World!":

```asm
	.text
	.globl  _start
_start:
	mov     dx, msg         ; DS:DX -> string (offset within the segment)
	mov     ah, 0x09        ; DOS: print '$'-terminated string
	int     0x21
	mov     ax, 0x4c00      ; DOS: terminate with exit code 0
	int     0x21
msg:
	.ascii  "Hello, World!$"
```

### 2. Assemble to an ELF object

```sh
build/bin/llvm-mc -triple=i8086 -filetype=obj hello.s -o hello.o
```

Inspect encodings with `-show-encoding` (text output instead of an object):

```sh
build/bin/llvm-mc -triple=i8086 -show-encoding hello.s
#   mov  dx, msg   ; encoding: [0xba,A,A]  (fixup A = msg, R_I8086_16)
```

### 3. Link to a `.COM`

Using this linker script (`com.ld`):

```ld
/* DOS .COM: single segment, code/data start at offset 0x100 */
ENTRY(_start)
SECTIONS {
  . = 0x100;
  .text : { *(.text) *(.text.*) *(.rodata) *(.rodata.*) *(.data) *(.data.*) }
  .bss  : { *(.bss) *(COMMON) }
  /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame) }
}
```

```sh
build/bin/ld.lld -o hello.com -T com.ld --oformat=binary hello.o
```

The result is a flat binary whose first byte is the entry point (address
`0x100`) — exactly the DOS `.COM` format. Make sure your entry code is the first
thing in `.text`.

### 4. Run it

Under DOSBox (headless, capturing DOS stdout via redirection):

```sh
mkdir -p dos && cp hello.com dos/HELLO.COM
SDL_VIDEODRIVER=dummy dosbox \
  -c "mount c dos" -c "c:" -c "HELLO.COM > OUT.TXT" -c "exit"
cat dos/OUT.TXT      # -> Hello, World!
```

---

## How it works

### Encoding

Each instruction packs a small descriptor into `TSFlags` (see
[`MCTargetDesc/I8086BaseInfo.h`](MCTargetDesc/I8086BaseInfo.h) and
[`I8086InstrFormats.td`](I8086InstrFormats.td)):

* **Form** — `RawFrm`, `AddRegFrm`, `MRMDestReg`/`MRMSrcReg`,
  `MRMDestMem`/`MRMSrcMem`, the `/digit` group forms `MRM0r..MRM7m`, and
  `RawFrmFar`.
* **ImmType** — none / `imm8` / `imm16` / `rel8` / `rel16`.
* **Opcode** — the primary opcode byte.

The hand-written [`I8086MCCodeEmitter`](MCTargetDesc/I8086MCCodeEmitter.cpp)
reads these and emits the segment-override prefix, opcode, ModR/M + displacement,
and trailing immediate. Memory operands are represented as four MCOperands
(`base`, `index`, `disp`, `seg`).

### Sizes and ambiguity

Where a register operand implies the width (`mov [bx], al`), the memory size may
be left unspecified. Where nothing implies it (`mov [bx], 5`, `inc [bx]`,
`shl [bx], 1`), an explicit `byte ptr` / `word ptr` is **required** — matching
NASM/MASM — otherwise the assembler reports an error.

### Relocations & `.COM` layout

Intra-section PC-relative branches are resolved by the assembler. Absolute
16-bit symbol references become `R_I8086_16` relocations that `lld` resolves to
`0x100 + offset` (the offset within the DOS segment), which is the correct value
for `DS`/`CS`-relative access in a `.COM`.

---

## Source layout

| Path | Purpose |
|------|---------|
| `llvm/lib/Target/I8086/I8086*.td` | Registers, instruction formats, and the full instruction set |
| `llvm/lib/Target/I8086/MCTargetDesc/` | MC asm info, code emitter, asm backend, ELF object writer, instruction printer |
| `llvm/lib/Target/I8086/AsmParser/` | Intel-syntax assembly parser |
| `llvm/lib/Target/I8086/TargetInfo/` | Target registration |
| `lld/ELF/Arch/I8086.cpp` | Linker relocation handling for `EM_I8086` |
| `llvm/include/llvm/BinaryFormat/ELFRelocs/I8086.def` | `R_I8086_*` relocation definitions |

Core integration points also touched:
`llvm/include/llvm/TargetParser/Triple.h` + `llvm/lib/TargetParser/Triple.cpp`
(the `Triple::i8086` arch), `llvm/include/llvm/BinaryFormat/ELF.h`
(`EM_I8086`, reloc enum), `llvm/lib/Object/ELF.cpp` (reloc names), and
`lld/ELF/Target.{cpp,h}` (dispatch on `EM_I8086`).
