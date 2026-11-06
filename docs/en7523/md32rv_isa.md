# md32rv ISA — Airoha EN7523 NPU custom RISC-V variant

The Airoha/EcoNet **EN7523 NPU** is a 4-core "md32rv" cluster. The md32rv is an **RV32IMAC-based core
with non-spec-compliant vendor extensions** — it overloads several base RISC-V
opcodes with reserved encodings, so a stock `rv32imac` toolchain cannot fully
disassemble nor execute md32rv firmware.

Everything below is **silicon-verified** unless explicitly marked otherwise.

## 1. Base ISA & non-compliance summary

md32rv is RV32IMAC at the integer/M/A/C extension level; standard instructions
decode and execute normally. On top of that, md32rv **overloads three standard
opcodes with reserved funct3/funct7 encodings** (a stock RISC-V core would trap
these as illegal):

| Standard opcode (RISC-V name) | md32rv use                                                                                               |
| ----------------------------- | -------------------------------------------------------------------------------------------------------- |
| `0x33` OP                     | broad custom ALU / bit-manipulation set (Zba/Zbb-like, non-standard encoding)                            |
| `0x67` JALR                   | the full conditional-branch family + JALR — moved out of opcode `0x63`, with a 5-bit immediate comparand |
| `0x0F` MISC-MEM               | cache-maintenance ops (funct3=4) and a few other funct3 variants                                         |
| `0x7b` custom-3 _(compliant)_ | SM4 crypto acceleration (uses the proper vendor slot)                                                    |

It also adds vendor CSRs (machine-mode):

- `0x7c0` **MCTREN** — cache / micro-arch control enable. Bits: `0`=I-cache,
  `1`=D-cache, plus vendor bits (`3/4/5/8` set in the firmware's final value
  `0x139`, which leaves D-cache **off**). [`encoding.h` `MCTREN_DCACHE_EN/ICACHE_EN`]
- `0xbc0` — set to `1` in `init` (perf-counter enable, tentative).
- `0x9c0`, `0x9dd`, `0x9df`, `0x9e0..0x9e6`, `0x9f0..0x9f6` — **MPIS**
  (Memory-Protection & Interconnect System) address-translation window pairs
  (input-base/output-base/attrs), set up in the boot prologue.

Boot is **not** the standard reset-vector / hart-self-dispatch model. The host
ARM driver loads the firmware to physical `0x84000000` and starts each core
individually by writing `CORE0..3_BOOT_BASE` (`0x1EC08020..0x1EC0802C`),
`BOOT_CONFIG` (`0x1EC08004`), `BOOT_TRIGGER` (`0x1EC08000`).

## 2. Opcode `0x33` — custom ALU / bit-manipulation family

Standard RISC-V `0x33` is the integer reg-reg opcode (ADD/SUB/AND/OR/XOR/SLL/...).
md32rv overloads it heavily (~524 in-code uses in the 69 KB image) with
reserved (`funct7`, `funct3`) values. The family splits into a Zba-like
shift-add group, a single-bit set/clear group, an immediate bit-field group,
and several unary primitives:

### Binary reg-reg (rs2 = register operand)

| funct7 | funct3 | mnemonic                  | operation                                                     |
| -----: | -----: | ------------------------- | ------------------------------------------------------------- |
| `0x10` |      0 | sh1add                    | `rd = rs1 + (rs2<<1)`                                         |
| `0x10` |      1 | sh2add                    | `rd = rs1 + (rs2<<2)`                                         |
| `0x10` |      2 | `max`                     | `rd = signed_max(rs1, rs2)` — silicon-verified                |
| `0x10` |      3 | —                         | **illegal** (traps)                                           |
| `0x40` |      0 | clrb                      | `rd = rs1 & ~(1<<(rs2&31))` — clear bit `rs2`                 |
| `0x40` |      1 | bit-reverse + rotate-left | `rd = rotl32(bit_reverse32(rs1), rs2&31)` [batch 33]          |
| `0x40` |      3 | setb (alias of f3=4)      | `rd = rs1 \| (1<<(rs2&31))` [batch 14]                        |
| `0x40` |      4 | setb                      | `rd = rs1                                                     | (1<<(rs2&31))` — set bit`rs2` |
| `0x40` |      5 | extbit                    | `rd = (rs1>>(rs2&31)) & 1` (extract bit `rs2`→0/1) [batch 14] |

> Pattern: `funct7=0x10` is the shift-add family with shift = `funct3+1`
> (Zba-style — but standard Zba uses funct3 = 2/4/6, md32rv uses 0/1).
> `funct7=0x40` is the single-bit set/clear family indexed by the rs2 _register_.

### Immediate bit-field (rs2 field = 5-bit immediate `N`; rs2 register _not_ used)

| funct7 | funct3 | operation                                     | special case     |
| -----: | -----: | --------------------------------------------- | ---------------- |
| `0x04` |      0 | `rd = rs1 & ~(1<<N)` (clear bit N)            | N=31 ⇒ clear MSB |
| `0x04` |      1 | `rd = rotl32(bit_reverse32(rs1), N)`          | [batch 33]       |
| `0x04` |      2 | `rd = rs1 & (1<<N)` (isolate bit N)           |                  |
| `0x04` |      3 | `rd = rs1 \| (1<<N)` (set bit N) [batch 14]   |                  |
| `0x04` |      4 | `rd = rs1 ^ (1<<N)` (toggle bit N) [batch 14] |                  |
| `0x04` |      5 | `rd = (rs1>>N) & 1` (extract bit N → 0/1)     |                  |
| `0x04` |      6 | `rd = rs1 & ((1<<N)-1)` (mask low N bits)     | N=16 ⇒ zext.h    |

> Pattern: `funct7=0x04` mirrors `funct7=0x40` but with the bit index _encoded
> in the rs2 field_ instead of read from a register. The earlier-named
> `zext.h` and "clear bit 31" are instances of this one family.

### Unary

| funct7 | funct3 | operation                                                                           | notes                                                                                                                                                                                   |
| -----: | -----: | ----------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `0x03` |      1 | `rd = sext<N>(rs1)` — parametric sign-extend (single mnemonic `md.sext rd, rs1, N`) | rs2-field = source bit-width N. Silicon-verified across all N ∈ {0..31}: N=1..31 matches `((rs1<<(32-N))>>(32-N))`; N=0 returns a sign mask from bit 31. Firmware uses N ∈ {8, 16, 24}. |
| `0x08` |      1 | `rd = clz(rs1)` (count leading zeros)                                               | rs2 field = `x0`                                                                                                                                                                        |
| `0x08` |      2 | `rd = ctz(rs1)` (count trailing zeros)                                              | rs2 field = `x0`                                                                                                                                                                        |
| `0x08` |   4..7 | halfword pack family (`md.packhh/hl/lh/ll`)                                         | selects high/low halfwords from `rs1` and `rs2`; silicon-verified batch 37                                                                                                              |
| `0x10` |      4 | `rd = rs1` (`md.mv` — identity, rs2 ignored)                                        | silicon-verified batch 28+29                                                                                                                                                            |
| `0x10` |      6 | `rd = abs(rs2) * sign(rs1)` (`md.copysign`)                                         | silicon-verified batch 28                                                                                                                                                               |
| `0x10` |      7 | `rd = rs2` (`md.mv2`)                                                               | silicon-verified batch 37                                                                                                                                                               |
| `0x40` |      2 | `rd = rs1 & (1 << (rs2 & 31))` (`md.andb` — isolate bit indexed by rs2 reg)         | register-indexed companion of `md.andi`; silicon-verified batch 28                                                                                                                      |
| `0x40` |      6 | `rd = rs1 & ((1 << (rs2&31)) - 1)` (`md.masklr`)                                    | register-indexed companion of `md.maskl`; silicon-verified batch 37                                                                                                                     |

### Unified parametric bit-field-insert family (`f3=0`, `f7 = 4k+3`)

**Silicon-verified in batches 22 and 35**. Batch 35 swept the full
`shift=0..31` × `rs2-field=0..31` parameter space — every observation
matches the **single unified formula**:

```
shift  = (f7 − 3) / 4         (f7 of the form 4k+3, k = 0..31)
width  = min(rs2_field + shift, 32)
fill   = rd_old[shift-1 : 0]  (low `shift` bits of rd preserved)
rd[width-1 : 0]  ←  ((rs1 << shift) | fill)  & ((1 << width) − 1)
rd[31 : width]   ←  preserved
```

All shifts use the consistent `md.bfinsrN` mnemonic. The earlier
batch-by-batch ad-hoc names (`cshl1/15/31`, `mask12shl2`, `bfins.h`)
were removed because they obscured the unification.

|         `f7` |  shift | mnemonic                           | firmware uses?                        |
| -----------: | -----: | ---------------------------------- | ------------------------------------- |
|       `0x03` |      0 | `md.bfinsr0`                       | yes (replace low N bits)              |
|       `0x07` |      1 | `md.bfinsr1`                       | yes (30 sites — was `cshl1`)          |
|       `0x0b` |      2 | `md.bfinsr2`                       | yes (38 sites — was `mask12shl2`)     |
|       `0x0f` |      3 | `md.bfinsr3`                       | yes                                   |
|       `0x13` |      4 | `md.bfinsr4`                       | yes                                   |
|       `0x17` |      5 | `md.bfinsr5`                       | not seen                              |
|       `0x1b` |      6 | `md.bfinsr6`                       | yes (1 site)                          |
|       `0x1f` |      7 | `md.bfinsr7`                       | not seen                              |
|       `0x23` |      8 | `md.bfinsr8`                       | yes (byte-insert at bit 8)            |
| `0x27..0x2f` |  9..11 | `md.bfinsrN`                       | not seen                              |
|       `0x33` |     12 | `md.bfinsr12`                      | yes (1 site)                          |
|       `0x37` |     13 | `md.bfinsr13`                      | not seen                              |
|       `0x3b` |     14 | `md.bfinsr14`                      | yes                                   |
|       `0x3f` |     15 | `md.bfinsr15`                      | yes (18 sites — was `cshl15`)         |
|       `0x43` |     16 | `md.bfinsr16`                      | yes (was `bfins.h`)                   |
|       `0x47` |     17 | `md.bfinsr17`                      | yes (heimgard_cx30 variant — 2 sites) |
| `0x4b..0x5f` | 18..23 | `md.bfinsrN`                       | not seen                              |
|       `0x63` |     24 | `md.bfinsr24`                      | yes (heimgard_cx30 variant — 2 sites) |
|       `0x67` |     25 | `md.bfinsr25`                      | yes                                   |
|       `0x6b` |     26 | `md.bfinsr26`                      | not seen                              |
|       `0x6f` |     27 | `md.bfinsr27`                      | yes                                   |
|       `0x73` |     28 | `md.bfinsr28`                      | yes                                   |
|       `0x77` |     29 | `md.bfinsr29`                      | yes                                   |
|       `0x7b` |     30 | (no encoding — opcode 0x7b is SM4) | n/a                                   |
|       `0x7f` |     31 | `md.bfinsr31`                      | yes (6 sites — was `cshl31`)          |

**Examples** (all silicon-verified):

- `f7=0x07 rs2=1, rs1=0x100, rd=0xDEADBEEF` → shift=1, width=2, low 2 bits ← `(0x100<<1)|preset[0]&3` = `(0x201 & 3) = 1`. So `rd=0xDEADBEEC | 1 = 0xDEADBEED`. ✓
- `f7=0x0f rs2=29, rs1=0x12345678, rd=0xDEADBEEF` → shift=3, width=32, rd ← `(0x12345678<<3) | preset[2:0]&7` = `0x91A2B3C0 | (0xEF&7) = 0x91A2B3C7`. ✓
- `f7=0x13 rs2=1, rs1=1, rd=0xDEADBEEF` → shift=4, width=5, low 5 ← `(1<<4)|preset[3:0]&0x1F` = `0x10|0xF = 0x1F`. So `rd=0xDEADBEE0|0x1F = 0xDEADBEFF`. ✓
- `f7=0x43 rs2=16, rs1=0x1234ABCD, rd=0xDEADBEEF` → shift=16, width=32, rd ← `(0x1234ABCD<<16)|preset[15:0] = 0xABCD0000|0xBEEF = 0xABCDBEEF`. ✓

**Implication**: the earlier interpretations of `md.cshl1/15/31` as
"conditional shifts", `md.mask12shl2` as "mask 12 + shift 2", `md.bfins.h`
as "half-word insert at bit 16", and the batch-14 "template builders
with fixed prefix bits" were all special-case views of this one
parametric op. The misleading legacy names have been removed in favor
of the consistent `md.bfinsrN` unified naming.

### Other

`funct7=0x10 funct3=2` = **signed `max`** (silicon-verified, batch 11);
`funct7=0x10 funct3=3` traps as illegal.

### Bit-field insert family (`f3=0`) — see the unified table above

The entire `f3=0` family with f7 = 4k+3 (k=0..31) is the parametric
bit-field-insert family. See the table preceding "### Other" for the
unified formula and verified shift values.

### Unified bit-field-extract family (`f3=2`)

Batch 25 (2026-05-29) extended the unification by silicon-probing the
`f3=2` family with multiple `f7` values, rs1 patterns, and rs2-field
widths. **f3=2 is the EXTRACT counterpart of the f3=0 INSERT family**:

```
shift = (f7 - 3) / 4         (f7 of the form 4k+3)
rd    = (rs1 >> shift) & ((1 << rs2_field) - 1)
```

The earlier batch-15 claim of "extract at bit 8 with width cap 24/16"
was an artifact of probing with `rs1 = −1` (all 1s) — the apparent
"cap" was simply the natural saturation of `(0xFFFFFFFF >> shift)`.
Batch 25 tests with `rs1 = 0xFFFF0000` (only high half set) split the
two: `f7=0x23` returned `0x00FFFF00` (bits 8..31 of rs1), `f7=0x43`
returned `0x0000FFFF` (bits 16..31). Conclusively, the EXTRACT POSITION
matches `shift = (f7-3)/4`, not "always bit 8".

**Silicon-verified shifts for f3=2** (from batches 15, 19, 25 combined):

|   `f7` | shift | mnemonic                                                         |
| -----: | ----: | ---------------------------------------------------------------- |
| `0x07` |     1 | `md.bfextr1`                                                     |
| `0x0b` |     2 | `md.bfextr2` (used by firmware site 0x40aa, `0x16372533`)        |
| `0x0f` |     3 | `md.bfextr3`                                                     |
| `0x13` |     4 | `md.bfextr4`                                                     |
| `0x1b` |     6 | `md.bfextr6`                                                     |
| `0x23` |     8 | `md.bfextr8` (was `md.bfext.w` — misleading "wide" suffix)       |
| `0x2f` |    11 | `md.bfextr11`                                                    |
| `0x3f` |    15 | `md.bfextr15`                                                    |
| `0x43` |    16 | `md.bfextr16` (was `md.bfext.h` — misleading "half-word" suffix) |
| `0x7f` |    31 | `md.bfextr31`                                                    |

The signed counterpart uses `f3=1` for the same `f7=4N+3` shift slots
except `N=0`, where `f7=0x03/f3=1` is already `md.sext`. Batch 37 swept
the full `shift=1..31` × `width=0..31` space and names the family
`md.bfextsrN`.

**Example — the firmware's `0x16372533` site at offset `0x40aa`**:

```text
4088:  md.maskl       a4, a0, 0x10   ; a4 = a0 & 0xFFFF (low 16 of a0)
408c:  lhu            a3, 126(a3)    ; a3 = halfword in some struct
4090:  bgeu           a3, a4, ...    ; range-check
...
40a0:  lw             a1, 168(a0)    ; load a counter
40a4:  addi           a1, a1, 1
40a6:  sw             a1, 168(a0)    ; counter++
40aa:  md.bfextr2     a0, a4, 0x3    ; a0 = (a4 >> 2) & 7   ⇐ THIS OP
40ae:  bne            s5, a3, 0x40c8
40b2:  bne            a0, a3, 0x40c8
```

The instruction extracts a 3-bit sub-field at bit 2 of `a4` (which
holds the low 16 bits of an earlier `a0`) into `a0`. The result is used
as a handler/channel index in subsequent compare-and-branch logic.

So the `f3=0/f3=2` pair under `f7=0x23` is a true insert/extract pair on a
field at bit position 8: `f3=0` packs `rs1`'s low 11 bits into the field;
`f3=2` extracts the field back out (with the width chosen by `rs2_field`).

### Non-firmware-used `f7` values at `f3=0`

Three encodings outside the unified `4k+3` shift pattern were flagged
in early notes as "silently hanging" — re-tested in **batch 26**
(2026-05-30), they are simply **illegal-instruction traps** (mcause=2),
not hangs:

|   `f7` | `f3=0` `rs2=0`                                           | `f3=0` `rs2≠0` |
| -----: | -------------------------------------------------------- | -------------- |
| `0x02` | TRAP                                                     | TRAP           |
| `0x05` | TRAP                                                     | TRAP           |
| `0x08` | **legal — `rd = clz(rs1)`** (same as `md.clz` at `f3=1`) | TRAP           |

The earlier "hang" reading was a measurement artifact of the
trap-counter bootstrap bug (clear loop left counter at `0xDEADDEAD`;
the first trap then recursed into trap-of-trap). After fixing the
counter init, every probe is trap-recoverable and these encodings
behave deterministically as illegal — except `f7=0x08 rs2=0` which is
a second legal encoding for `clz` (the firmware uses the `f3=1`
encoding exclusively; `f3=0 rs2=0` is an alias the firmware never
emits).

## 3. Opcode `0x67` — JALR + conditional branches _with a 5-bit immediate_

This is the largest deviation from standard RISC-V. md32rv **moves the entire
conditional-branch family out of opcode `0x63` into opcode `0x67`** (the JALR
slot), and crucially the "rs2" field is **not** a register — it is a 5-bit
**signed immediate** operand. The B-type immediate is still used for the branch
target.

Encoding (md32rv interpretation of the opcode `0x67` 32-bit word):

```
 31    25 24   20 19   15 14 12 11    7  6     0
[ imm[12|10:5] | imm5 | rs1  |funct3| imm[4:1|11] | 0x67 ]
                ^^^^                 ^^^^^^^^^^^^
                sign-ext             B-type branch offset
                5-bit
                imm
```

Semantics:

| funct3 | mnemonic | taken when                     |
| -----: | -------- | ------------------------------ |
|      0 | `JALR`   | (unconditional — standard)     |
|      1 | `beqi`   | `rs1 == sext(imm5)`            |
|      2 | `bnei`   | `rs1 != sext(imm5)`            |
|      3 | `blti`   | `rs1 <  sext(imm5)` (signed)   |
|      4 | `bgei`   | `rs1 >= sext(imm5)` (signed)   |
|      5 | `bltui`  | `rs1 <  sext(imm5)` (unsigned) |
|      6 | `bgeui`  | `rs1 >= sext(imm5)` (unsigned) |
|      7 | —        | illegal / unused               |

- Target: `PC + B-immediate` (the same B-type offset layout as standard RISC-V,
  spread across bits 31, 30:25, 11:8, 7).
- Comparand `imm5` is in `[24:20]`, **sign-extended to 32 bits** (range −16..+15
  as signed; 0..31 as unsigned).
- For unsigned compares (`bltui`/`bgeui`), sign-extension still applies, so
  `imm5=16` becomes `0xFFFFFFF0` (a very large unsigned value); `bltui rs1, 16`
  is therefore "is `rs1` numerically below `0xFFFFFFF0`" — i.e. almost-always
  true. Firmware uses these with small positive imm5 in practice.
- Standard RV32 `BRANCH` opcode `0x63` is **not used for 32-bit branches** by the
  Airoha compiler in this firmware; short branches use the C-extension
  `c.beqz`/`c.bnez` (RVC). All non-compressed conditional branches go through
  `0x67`.

This fuses "compare register to small constant, branch if condition" into a
**single instruction** — a major code-density and compiler-friendliness win for
loop counters, flag tests, range checks, and switch dispatch.

## 4. Opcode `0x0F` — MISC-MEM (cache ops)

Standard MISC-MEM only defines `funct3=0` (`FENCE`) and `funct3=1` (`FENCE.I`).
md32rv extends it with **one custom cache-maintenance instruction** at `funct3=4`.
With a strict in-code neighbor filter, **only `funct3 = 0, 1, 4` appear in real
code** — `f3=2,3,5,6,7` counts in the static scan were rodata/strings
mis-disassembled.

| funct3 | operation                                                                                                                 |
| -----: | ------------------------------------------------------------------------------------------------------------------------- |
|      0 | `FENCE` — standard memory ordering barrier                                                                                |
|      1 | `FENCE.I` — standard instruction-fetch barrier; the BSP also uses it as "cache write-through" (`asm("fence.i") // wmb()`) |
|      4 | **custom cache-maintenance op**, operand in `rs1`                                                                         |

### `funct3=4` operand encoding

`rs1` carries `address | op_code` — the op-code occupies the low bits (≤5 bits)
of `rs1`, and the high bits (cache-line-aligned) carry the address (when an
address is needed). Observed op-codes in the device firmware:

| `rs1` operand        | context                                              | meaning                                                                                                                                                                       |
| -------------------- | ---------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `8`                  | boot, right after enabling I+D caches                | likely invalidate-I-all (paired with code 24); silicon probe (batch 9) does _not_ show this code dropping a dirty D-line, consistent with affecting icache rather than dcache |
| `24` (`0x18`)        | boot, same place                                     | **silicon-verified: invalidate-without-write-back of the dirty D-cache line at `addr`** — the prior backing value survives, the NEW write made just before is discarded       |
| `addr + 29` (`0x1d`) | per-line in a 32-byte-stride dcache range-flush loop | flush-and-invalidate cache line at `addr` (consistent with the firmware's range-flush helper using exactly this code with an address operand)                                 |

Example range-flush loop (device fw `0x68f6..0x690e`):

```
addi x11,x11,32       # end += 32 (one past last line)
loop: addi x12,x10,0  # x12 = current address
addi x12,x12,29       # rs1 = addr | op_code(29)  — per-line op
.insn 0x0006400f      # cache op
fence 0,0             # custom barrier (pred=succ=0)
addi x10,x10,32       # advance by 32-byte cache line
bne x11,x10,loop
```

The accompanying `fence 0,0` (objdump renders `unknown,unknown` because both
sets are empty) appears to act as a completion barrier paired with the
cache op. Standard Zicbom would use distinct `cbo.clean/flush/inval` opcodes —
md32rv folds them into one instruction with an op-code embedded in the
operand.

### Silicon verification (batches 9, 10)

Tested all 32 op-codes `N=0..31` with dcache enabled, **two dirty cache lines A
and B per candidate** (B in the next 32-byte line) so line-scope vs global can
be separated. Zero traps for any N — **all 32 encodings are legal**. NPU
re-read = `NEW` if line held in cache, `OLD` if line dropped; ARM backing =
`NEW` if write reached DRAM, `OLD` if not.

| Op-code(s)  | NPU(A) | NPU(B)  | ARM(A)  | ARM(B)  | meaning                                                                                                           |
| :---------: | :----: | :-----: | :-----: | :-----: | ----------------------------------------------------------------------------------------------------------------- |
| 0–20, 26–30 |  NEW   |   NEW   |   NEW   |   NEW   | **global flush-D** (both A and B reached DRAM ⇒ write-back is global; lines kept clean)                           |
|   **21**    |  NEW   |   NEW   | **NEW** | **OLD** | **`flush-line(addr)`** — line-scoped: only the targeted line was written back                                     |
| 22, 23, 31  |  NEW   |   NEW   |   OLD   |   OLD   | **no-op** (dirty lines stayed in dcache, nothing wrote back — proves the dcache has no implicit/opportunistic WB) |
|   **24**    |  OLD   | **OLD** |   OLD   |   OLD   | **`invalidate-all-D`** (global): both lines dropped from dcache, no write-back                                    |
|   **25**    |  OLD   |   NEW   |   OLD   |   NEW   | **`invalidate-line(addr)`** — line-scoped: only the targeted line dropped, no write-back                          |

So the firmly silicon-pinned `0x0F f3=4` op-codes are:

|     op-code | name (proposed)         | semantics                                                   |
| ----------: | ----------------------- | ----------------------------------------------------------- |
|          21 | `dcache.flush.line`     | clean-and-keep: write back line at `addr`, leave line valid |
|  22, 23, 31 | reserved / no-op        | execute, do nothing observable                              |
|          24 | `dcache.inval.all`      | drop every dirty D-line; no write-back                      |
|          25 | `dcache.inval.line`     | drop the targeted D-line; no write-back                     |
| 0–20, 26–30 | global flush-D variants | each writes back at least both tested lines; lines kept     |

The "global flush-D" group still needs finer differentiation (some may be the
"flush-all-D / write-back-all", others may be enable/disable as side-effecting
flushes; the BSP API hints at `core_dcache_enable/disable`, `dcache_flush_by_addr`,
`dcache_invalidate_by_addr`, `flush_dcache_range`). Batch 9's original
contextual inferences are refined by batch 10's two-line probe: `24` is
confirmed to be a _global_ invalidate, not line-scoped; `8` and `29`, both
group-A members in the NEW/NEW classification, are most likely
`icache.inval.all` and `dcache.flush.line` respectively given firmware
context (boot pair with `24`; per-line use in the dcache range-flush loop)
— but the probe cannot exclude them being other globals.

### Cache-presence probe (batch 23 — REVISED CLASSIFICATION)

The batch-15 3-line probe was **confounded by cache-line collisions**
(128-byte address spacing was less than the silicon cache-line size, so
multiple candidates' lines mapped to the same cache set). Batch 23 uses
a more definitive probe: overwrite DRAM backing AFTER the candidate op
but BEFORE the cache re-read, then a read returns `NEW` if the line is
still cached (clean or dirty) or `WITNESS` if the line is gone (cache
miss → fetches the just-overwritten backing). Combined with a backing-
state read AFTER the op, this gives a **two-tuple** that uniquely
distinguishes four classes:

| (backing_after, post_witness_read) | classification                                              |
| ---------------------------------- | ----------------------------------------------------------- |
| (NEW, NEW)                         | **flush-only** (write back, line kept clean in cache)       |
| (NEW, WITNESS)                     | **flush+invalidate** (write back, line dropped)             |
| (OLD, WITNESS)                     | **invalidate-only** (drop, no write back)                   |
| (OLD, NEW)                         | **no-op for DCACHE** (cache still dirty; may affect ICACHE) |

Tested all 17 distinct op-codes ({0,4,8,12,16,20–30,31}) with 256-byte
candidate spacing. Zero traps; results unambiguous per candidate:

| Op-code | DCACHE                   | ICACHE                  | Final classification                              |
| ------: | ------------------------ | ----------------------- | ------------------------------------------------- |
|   **0** | no-op                    | no-op                   | **TRUE no-op** (reserved)                         |
|   **4** | no-op                    | no-op                   | **TRUE no-op**                                    |
|   **8** | no-op                    | invalidate-all          | **`md.icache.inval.all`** (boot uses)             |
|  **12** | no-op                    | no-op                   | **TRUE no-op**                                    |
|  **16** | no-op                    | no-op                   | **TRUE no-op**                                    |
|  **20** | flush+invalidate, GLOBAL | (not separately tested) | global `dcache.flush_inv.all`                     |
|  **21** | flush+invalidate, LINE   | (not separately tested) | `md.dcache.flush_inv.line`                        |
|  **22** | no-op                    | no-op                   | **TRUE no-op**                                    |
|  **23** | no-op                    | no-op                   | **TRUE no-op**                                    |
|  **24** | invalidate-only, GLOBAL  | (not separately tested) | **`md.dcache.inval.all`** (boot uses)             |
|  **25** | invalidate-only, LINE    | (not separately tested) | `md.dcache.inval.line`                            |
|  **26** | no-op                    | no-op                   | **TRUE no-op**                                    |
|  **27** | no-op                    | no-op                   | **TRUE no-op**                                    |
|  **28** | flush+invalidate, GLOBAL | (not separately tested) | global `dcache.flush_inv.all`                     |
|  **29** | flush+invalidate, LINE   | (not separately tested) | `md.dcache.flush_inv.line` (firmware range-flush) |
|  **30** | flush+invalidate, GLOBAL | (not separately tested) | global `dcache.flush_inv.all`                     |
|  **31** | no-op                    | no-op                   | **TRUE no-op**                                    |

**Four big revisions vs batch 15**:

1. **NO flush-only sub-type exists** — every flushing op also invalidates the
   line. The `dcache.flush.line` mnemonic at op-code 21 is misleading on
   this silicon: it both flushes AND invalidates.
2. Most "global flush" codes from batch 15 (0, 4, 12, 16, 26, 27, 31)
   are actually **TRUE no-ops** — they affect neither cache (silicon-verified
   batch 27 with an ICACHE probe). They're reserved encoding slots.
3. The "global flush-D" claim in batch 15 conflated `(backing=NEW, line-kept)`
   with `(backing=NEW, line-gone)`. The cache-presence probe resolves this.
4. Op-code **8 is the ONLY ICACHE op** (the firmware boot uses it for
   `icache.inval.all`). The earlier "probably ICACHE-side" hypothesis for
   {0/4/12/16/22/23/26/27/31} was wrong — silicon-verified inert.

**Line-vs-global distinction within flush+invalidate group** (resolved
batch 25 with explicit witness lines): for each of {20, 21, 28, 29, 30},
dirty target T and a far-away witness W (1 KB apart), run candidate
on T, then check both T and W via the cache-presence probe:

|   N | T (target)          | W (witness, far away)  | scope                            |
| --: | ------------------- | ---------------------- | -------------------------------- |
|  20 | flushed+invalidated | flushed+invalidated    | **GLOBAL** flush+invalidate      |
|  21 | flushed+invalidated | unchanged (dirty kept) | **LINE-scoped** flush+invalidate |
|  28 | flushed+invalidated | flushed+invalidated    | **GLOBAL** flush+invalidate      |
|  29 | flushed+invalidated | unchanged (dirty kept) | **LINE-scoped** flush+invalidate |
|  30 | flushed+invalidated | flushed+invalidated    | **GLOBAL** flush+invalidate      |

**Important correction**: the firmware's per-line `dcache_range_flush`
loop uses N=29 which is **line-scoped** — the loop properly affects
one line per iteration (NOT "wasteful global flush" as the earlier
batch-15 reading suggested).

### Original three-line probe (batch 15 — DEPRECATED)

The batch-15 three-line classification is now known to be misleading due
to cache-line aliasing. The signature table is retained below for
historical reference but the classifications above (batch 23) supersede:

| op-codes                   | NPU(A,B,C)  | ARM(A,B,C)  | OLD claim (now refined)                                                   |
| -------------------------- | :---------: | :---------: | ------------------------------------------------------------------------- |
| 0, 4, 8, 12, 16, 20, 26–30 | NEW/NEW/NEW | NEW/NEW/NEW | "global flush, lines kept" — actually a mix of no-op and flush+invalidate |
| 21                         | NEW/NEW/NEW | NEW/OLD/OLD | "line flush" — actually flush+invalidate-line                             |
| 25                         | OLD/NEW/NEW | OLD/OLD/OLD | line invalidate ✓ (still correct)                                         |
| 22, 23, 31                 | NEW/NEW/NEW | OLD/OLD/OLD | no-op ✓ (still correct, on DCACHE)                                        |
| 24                         | OLD/OLD/OLD | OLD/OLD/OLD | global invalidate-D ✓ (still correct)                                     |

For posterity — the batch 15 "global flush-D" interpretation is
DEPRECATED:
`enable-D`/`disable-D` whose visible side effect happens to be a global
flush. Distinguishing these would need bus-level tracing or vendor
documentation.

Notably, **`N=29` (used per-line in the firmware's dcache range-flush helper)
is also a global flush** — meaning that helper loop is functionally
"flush-everything, once per line iteration." Wasteful, but correct: at the
end, all dirty D-lines have been written back. The single-call versions in
the same group (e.g. `N=0`, `N=4`) would achieve the same effect in one
instruction.

## 5. Opcode `0x7b` — CUSTOM-3 (SM4 crypto)

The **one spec-compliant** custom extension. SM4 acceleration uses the standard
custom-3 opcode slot with `funct3=7`:

| funct7 | operation                                              |
| -----: | ------------------------------------------------------ |
|    0–3 | `sm4ed` (SM4 encrypt, byte select via funct7 low bits) |
|    4–7 | `sm4ks` (SM4 key schedule)                             |

## 6. Vendor CSRs (machine-mode)

| CSR              | name                               | purpose                                                                                                   |
| ---------------- | ---------------------------------- | --------------------------------------------------------------------------------------------------------- |
| `0x7c0`          | MCTREN                             | cache + micro-arch enables (bit 0 ICACHE, bit 1 DCACHE; firmware's final value `0x139` leaves DCACHE off) |
| `0xbc0`          | —                                  | enabled to `1` in `init` (perf-counter enable, tentative)                                                 |
| `0x9c0`          | MPIS ctrl                          | set to `0x7f` (likely a window-enable mask)                                                               |
| `0x9dd`, `0x9df` | MPIS pair                          | one extra base/output pair                                                                                |
| `0x9e0..0x9e6`   | MPIS input bases (+attribute byte) | 7 MPIS windows                                                                                            |
| `0x9f0..0x9f6`   | MPIS output bases                  | matching outputs                                                                                          |

Boot programs windows that map (input-base → output-base): e.g. `0x1c000000 →
0x1e800000`, `0x1e860000 → 0x1e8003e0`(_addr+attr_), `0x30000000 → 0x30000000`,
`0x30004000 → 0x30004000`, `0x80000000 → 0xc0000000`. Standard `mhartid`,
`mcycle`, `mtvec`, `mepc`, `mcause`, etc. are present and work normally.

## 7. Boot model

Not a standard reset-vector / hart self-dispatch. The host ARM driver
(`res/host/npu_rv32.c`):

```
CORE0..3_BOOT_BASE (0x1EC08020..0x1EC0802C)  ← firmware entry per core
BOOT_CONFIG        (0x1EC08004)              ← enable + per-core reboot bits
BOOT_TRIGGER       (0x1EC08000)              ← 0x1 boot / 0x2 reboot
```

Firmware loads to **physical `0x84000000`** (reserved-memory node
`npu_binary@84000000`, 1 MB) and executes there. Each core runs the same
prologue:

1. Program `MCTREN` (clear, set, settle to final `0x139`).
2. Program the 7+ MPIS window pairs at CSRs `0x9c0` / `0x9dd` / `0x9df` /
   `0x9e0..6` / `0x9f0..6`.
3. Final MCTREN value `0x139` (I-cache on, D-cache off, vendor bits 3/4/5/8).
4. Set `gp` (= 0x1e900b28, into 64K SRAM `.data`/`.sdata`).
5. Hart-0 only: liveness check (read MIB0 at `0x1EC0C140`), clear BSS.
6. `sp` ← `0x30003ff0` (core-local SRAM via MPIS).
7. `jal init` (≈ `0x84005cd0`).

## 8. Compiler / toolchain implications

A stock `riscv64-unknown-elf-gcc / objdump` configured for `rv32imac` will:

- correctly assemble & disassemble the base I/M/A/C instructions;
- render every md32rv overload as `.insn 4, 0x…`;
- be unable to encode any md32rv custom op without raw `.4byte` directives.

## 9. Binutils-gdb validation (2026-05-28/29)

The Airoha `md32rv` patches were imported into a binutils-gdb tree at
`bsp/binutils-gdb/`. The vendor's official table is in
`opcodes/riscv-opc.c` (lines 3595..3635) with `MATCH_*`/`MASK_*` macros in
`include/opcode/riscv-opc.h` (lines 3856..3925). A wrapper
`md32rv-objdump.py` exists; the actual built `objdump` in
`build/binutils/binutils/objdump` now decodes the EN7523 firmware natively,
emitting `md.*` mnemonics for **838 / 957** custom-opcode sites in the 69 KB
image (the remaining 119 `.insn` entries are the ones discussed below).

### 9.1 Confirmed by binutils (silicon ↔ vendor mnemonic)

| Silicon decode (this doc)                    | Binutils mnemonic | `MATCH`      | `MASK`       | operands  |
| -------------------------------------------- | ----------------- | ------------ | ------------ | --------- |
| `0x33 f7=0x10 f3=0` shift-add `<<1`          | `md.sh1add`       | `0x20000033` | `0xfe00707f` | `d,s,t`   |
| `0x33 f7=0x10 f3=1` shift-add `<<2`          | `md.sh2add`       | `0x20001033` | `0xfe00707f` | `d,s,t`   |
| `0x33 f7=0x10 f3=2` signed max               | `md.max`          | `0x20002033` | `0xfe00707f` | `d,s,t`   |
| `0x33 f7=0x40 f3=0` clear-bit (reg)          | `md.clrb`         | `0x80000033` | `0xfe00707f` | `d,s,t`   |
| `0x33 f7=0x40 f3=4` set-bit (reg)            | `md.setb`         | `0x80004033` | `0xfe00707f` | `d,s,t`   |
| `0x33 f7=0x04 f3=0` clear-bit (imm N)        | `md.clri`         | `0x08000033` | `0xfe00707f` | `d,s,<`   |
| `0x33 f7=0x04 f3=2` isolate-bit (imm N)      | `md.andi` (alias) | `0x08002033` | `0xfe00707f` | `d,s,<`   |
| `0x33 f7=0x04 f3=5` extract-bit (imm N)      | `md.extui`        | `0x08005033` | `0xfe00707f` | `d,s,<`   |
| `0x33 f7=0x04 f3=6` mask-low-N               | `md.maskl`        | `0x08006033` | `0xfe00707f` | `d,s,<`   |
| `0x33 f7=0x03 f3=1` parametric sext          | `md.sext`         | `0x06001033` | `0xfe00707f` | `d,s,<`   |
| `0x33 f7=0x08 f3=1` clz                      | `md.clz`          | `0x10001033` | `0xfff0707f` | `d,s`     |
| `0x33 f7=0x07 f3=0` conditional shift `<<1`  | `md.cshl1`        | `0x0e000033` | `0xfe00707f` | `d,s,t`   |
| `0x33 f7=0x3f f3=0` conditional shift `<<15` | `md.cshl15`       | `0x7e000033` | `0xfe00707f` | `d,s,t`   |
| `0x33 f7=0x7f f3=0` conditional shift `<<31` | `md.cshl31`       | `0xfe000033` | `0xfe00707f` | `d,s,t`   |
| `0x33 f7=0x0b f3=0` `(rs1 & 0xFFF) << 2`     | `md.mask12shl2`   | `0x16000033` | `0xfff0707f` | `d,s`     |
| `0x67 f3=1` beqi                             | `md.beqi`         | `0x1067`     | `0x707f`     | `s,Xc4,p` |
| `0x67 f3=2` bnei                             | `md.bnei`         | `0x2067`     | `0x707f`     | `s,Xc4,p` |
| `0x67 f3=3` blti                             | `md.blti`         | `0x3067`     | `0x707f`     | `s,Xc4,p` |
| `0x67 f3=4` bgei                             | `md.bgei`         | `0x4067`     | `0x707f`     | `s,Xc4,p` |
| `0x67 f3=5` bltui                            | `md.bltui`        | `0x5067`     | `0x707f`     | `s,Xc4,p` |
| `0x67 f3=6` bgeui                            | `md.bgeui`        | `0x6067`     | `0x707f`     | `s,Xc4,p` |
| `0x0F f3=4` cache op (generic)               | `md.cache.op`     | `0x400f`     | `0xfff07fff` | `s`       |
| `0x7b` SM4 encrypt                           | `md.sm4ed`        | `0x0000707b` | `0xfe00707f` | `d,s,t,y` |
| `0x7b` SM4 key-sched                         | `md.sm4ks`        | `0x0800707b` | `0xfe00707f` | `d,s,t,y` |

Operand letter `<` = 5-bit immediate at rs2-field (`OP_MASK_SHAMTW`); `Xc4`
= 5-bit signed immediate at bits [24:20] (reused from the CORE-V `cv.beqimm`
family); `p` = B-type branch offset; `y` = SM4 byte-select (`bs`, 2 bits in
funct7's low bits). `md.andi` carries `INSN_ALIAS` because its encoding
overlaps with the same `(f7,f3)` slot serving another canonical meaning the
vendor preferred (verified: it's `rd = rs1 & (1<<N)` — a single-bit isolate,
treated as the `andi` form because the immediate is interpreted as a mask).
`md.sm4ed/ks` share `(opcode,funct3)` and the `match_sm4_md32` helper checks
only `funct7` bit `2` to distinguish them, matching standard Zksed.

**Counts disassembled by the vendor table on the 69 KB image:**
`md.beqi` 257, `md.bnei` 188, `md.blti` 97, `md.bgei` 87, `md.maskl` 85,
`md.andi` 42, `md.sh2add` 40, `md.bgeui` 36, `md.sh1add` 32, `md.cshl1` 30,
`md.extui` 29, `md.bltui` 28, `md.clz` 23, `md.clri` 23, `md.cshl15` 18,
`md.cshl31` 6, `md.clrb` 5, `md.cache.op` 4, `md.max` 3, `md.setb` 1.

### 9.2 Cache-op named forms — silicon does NOT honor the rs1-field path

**Silicon-verified in batch 17** (2026-05-29): the vendor binutils' named
cache-op forms (`md.icache.inval.all`, `md.dcache.inval.all`, etc.) encode
the op-code as a 5-bit literal in the rs1 _field_. The silicon does **not**
read the rs1-field — it reads the _register_ that field selects, taking
its runtime value.

| Mnemonic               | `MATCH`   | rs1-field | rs1-register at runtime |
| ---------------------- | --------- | --------- | ----------------------- |
| `md.icache.inval.all`  | `0x4400f` | 8 (=s0)   | reads value in s0       |
| `md.dcache.inval.all`  | `0xc400f` | 24 (=s8)  | reads value in s8       |
| `md.dcache.inval.line` | `0xc800f` | 25 (=s9)  | reads value in s9       |
| `md.dcache.flush.line` | `0xa800f` | 21 (=s5)  | reads value in s5       |
| `md.dcache.flush.line` | `0xe800f` | 29 (=t4)  | reads value in t4       |

**Test** (batch 17): three sub-tests with dcache enabled, dirty line at
`CACHE_BASE+i*0x80`, then run `.insn 0xc400f` (binutils' `md.dcache.inval.all`)
with x24 set to `22 / 25 / 0` respectively. If silicon used the rs1-field
literal `24` (= inval-all-D), the NEW dirty value would be dropped and a
re-read would return OLD. In every case the NEW value persisted →
silicon used `x24`'s register value (22 = no-op, 25 = inval-other-line,
0 = global-flush-keeps-line), all of which preserve the dirty line.

**Consequence**: the binutils named forms in `bsp/binutils-gdb/` are
**incorrect for this silicon** — running them as-assembled would interpret
the rs1-field as a register selector and use whatever value happens to be
in that register. The correct idiom is the firmware's `li a0, OP; .insn
0x0005400f` (which `md.cache.op a0` decodes to). The named forms remain
in the table only because the vendor included them.

### 9.3 Binutils gaps (silicon-decoded ops the vendor table does not name)

These appear in firmware as 32-bit `.insn 4, 0x…` after disassembly because
no entry matches them. All are silicon-verified in earlier batches:

| Site count (FW) |       (`f7`,`f3`)        | This doc's name                                    | Notes                                                                                                                                                                                                                                                                                                          |
| --------------: | :----------------------: | -------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
|              29 |     `0x03,1` rs2=16      | `sext.h`-variant                                   | Binutils `md.sext.h` mask requires rs2=0; firmware always emits rs2-field=16. Either the vendor table's canonical form is "wrong" (low-priority encoding choice the SDK never emits) or the rs2-field is a _width_ parameter and `=16` is the half-word case. Silicon: rs2-field=16 ⇒ sign-extend low 16 bits. |
|              14 |      `0x0b,0` rs2≠0      | `mask12shl2`                                       | `md.mask12shl2` mask also requires rs2=0; firmware emits rs2-field ∈ {2,6,8}. Behaviour is unaffected — silicon-verified to ignore rs2 for the value computation.                                                                                                                                              |
|               7 |         `0x04,3`         | `setbi` (imm)                                      | `rd = rs1 \| (1<<N)`, batch 14                                                                                                                                                                                                                                                                                 |
|               2 |         `0x04,4`         | `togbi` (imm)                                      | `rd = rs1 ^ (1<<N)`, batch 14                                                                                                                                                                                                                                                                                  |
|               2 |         `0x40,3`         | `setb` alias                                       | functional alias of `md.setb` (`f3=4`), batch 14                                                                                                                                                                                                                                                               |
|             ~10 |      `0x23/0x43,2`       | field-extract pair                                 | `rd = (rs1>>8) & ((1<<min(rs2_field,W))−1)`, W∈{24,16}, batch 15. Counterpart of the `f7=0x23 f3=0` template builder.                                                                                                                                                                                          |
|              11 | `0x03/0x0f/0x13/0x23,0`  | template builders                                  | insert 11 bits of rs1 into fixed instruction template (batch 14). The `f7=0x23,f3=0` form produces a full JAL template `0xDEA800EF                                                                                                                                                                             | ((rs1<<8)&0x7FF00)`. |
|               0 | `0x07/0x3f/0x7f,0` rs2≠0 | cshl1/15/31 — _all_ gate-on cases match binutils ✓ | covered by `md.cshl1/15/31`                                                                                                                                                                                                                                                                                    |
|             ~12 | `0x0f/0x13/0x23,0` rs2≠0 | undocumented `cshl3/4/8`                           | the shift family extends past binutils' named 1/15/31 — silicon shows shifts at 3/4/8 follow the same formula. The vendor toolchain does not emit these but the firmware contains them.                                                                                                                        |
|               6 |         `0x42,3`         | unknown                                            | `rd=x14, rs1=x28, rs2=x4` site; not yet probed.                                                                                                                                                                                                                                                                |

### 9.4 New opcode visible only after binutils decoding: **opcode `0x0b`** (custom-0)

Twenty .insn sites in the 69 KB image have low-7 bits `0x0b` — the standard
RISC-V CUSTOM-0 slot. They distribute as `f3=0` × 2, `f3=1` × 1, `f3=2` × 5,
`f3=3` × 2, `f3=4` × 7, `f3=7` × 3, with I-type bit layout (12-bit immediate
in `[31:20]`, ranging from `0x004` to `0xfef`-signed-negative). The most
common form `0x00b5210b` (4 sites) decodes as opcode=0x0b, funct3=2, rs1=x1,
rd=x4, imm12=11 — pattern consistent with a custom load/store or a register
move helper.

These were **never noticed** in the earlier opcode survey because the
0x33/0x67/0x0F overload work consumed attention. Their semantics are
unknown and not silicon-verified. Binutils-gdb has no entry for them —
they are a **new open item**.

### 9.5 Binutils extensions made in this project (post-import)

The vendor binutils-gdb import was extended in two passes:

**Pass A — opcode 0x0b mnemonics added** (matching the silicon decode
in §9bis): `md.lb`, `md.sb`, `md.sh`, `md.lbu`, `md.sw`, `md.mv` —
all six f3 values used by firmware. New `MATCH_*/MASK_*` macros in
`riscv-opc.h`; entries in `riscv-opc.c` use standard `j(s)` and `q(s)`
operand letters so the assembler bit-check passes.

**Pass B — silicon-only decodings added**: `md.setbi` (f7=0x04 f3=3),
`md.togbi` (f7=0x04 f3=4), `md.setb3` (f7=0x40 f3=3 — distinct
mnemonic to avoid the gas duplicate-mnemonic check; functional alias of
`md.setb`), `md.extb` (f7=0x40 f3=5).

**MASK corrections** for two vendor entries whose original MASKs pinned
`rs2-field=0` but firmware actually emits non-zero rs2-fields:

- `md.sext.h`: MATCH/MASK changed to require `rs2-field=16` (every one
  of the 29 firmware sites uses this).
- `md.mask12shl2`: three additional disassembly-only `INSN_ALIAS`
  entries cover the firmware's `rs2-field ∈ {2, 6, 8}` variants; the
  canonical assembler-side encoding still uses `rs2-field=0`.

**Coverage now**: 1177 / (1177 + 191) ≈ **86 %** of custom-opcode
sites in the 69 KB image decode to a vendor or silicon-named `md.*`
mnemonic. The 191 remaining `.insn` lines are dominated by:

- 92 sites at opcode `0x33 f3=0` (instruction-template builders at
  `f7=0x03/0x0f/0x13/0x23` — ad-hoc, no clean mnemonic);
- 41 sites at opcode `0x33 f3=2` (the field-extract pair at
  `f7=0x23/0x43` — could be added as `md.bfext.w / md.bfext.h`).

CI roundtrip (`make ci`) confirms the firmware still re-assembles
bit-exact (SHA256 unchanged) after all binutils extensions.

### 9.6 New toolchain artifacts

- `bsp/binutils-gdb/` — vendor patched tree (objdump+gas+ld+gdb).
- `build/binutils/binutils/objdump` — built `objdump` 2.46.50.20260528.
- `md32rv-objdump.py` — Python post-processor wrapper that re-decodes any
  `.insn` lines stock objdump emits (superseded for most cases by the
  vendor-built objdump above; still useful as a portable disassembler
  without the build tree).
- `ci_roundtrip.py` + `Makefile` — disassemble→reassemble→hash roundtrip
  CI for the firmware (uses `.byte` directives for every instruction to
  guarantee bit-exact output, decoupling from mnemonic round-tripping).
- New SM4 mnemonics (`md.sm4ed`, `md.sm4ks`) confirm what `sm4_core.c`
  already showed: SM4 lives in the **compliant** custom-3 slot (`0x7b`).

## 9bis. Opcode `0x0b` — uncached load/store family (batch 16, 2026-05-29)

Discovered via binutils validation: 20 firmware sites at `f3 = 0/1/2/3/4/7`,
all I-type-shaped, none in the vendor binutils-gdb table. Silicon-probed in
batches 16b (firmware-encoding preserved, rd observed via the encoded
architectural register) and 16c (PROBE_BUF snapshot to confirm
load-vs-store and target address). 0 traps across all 29 tests.

|  f3 | mnemonic | semantic                        | imm12   | rd written? |
| --: | -------- | ------------------------------- | ------- | ----------- |
| `0` | `md.lb`  | `rd ← sext8(byte[rs1])`         | ignored | yes         |
| `1` | `md.sb`  | `byte[rs1] ← rs2 & 0xFF`        | ignored | no          |
| `2` | `md.sh`  | `halfword[rs1] ← rs2 & 0xFFFF`  | ignored | no          |
| `3` | `md.lbu` | `rd ← byte[rs1]` (zero-ext)     | ignored | yes         |
| `4` | `md.sw`  | `word[rs1] ← rs2`               | ignored | no          |
| `5` | `md.lh`  | `rd ← sext16(halfword[rs1])`    | ignored | yes         |
| `6` | `md.lhu` | `rd ← halfword[rs1]` (zero-ext) | ignored | yes         |
| `7` | `md.lw`  | `rd ← word[rs1]`                | ignored | yes         |

**Opcode 0x0b is fully decoded** (batch 21, 2026-05-29: sentinel
`0x77669988` at PROBE_BUF[0x80] disambiguates byte/halfword/word and
signed/unsigned). All 8 f3 values are populated.

Key invariant: **the address is exactly `rs1`** — there is no `imm12 + rs1`
displacement, unlike standard RISC-V loads/stores at opcodes `0x03` / `0x23`.

**Earlier mis-decodes corrected in batch 17/18**:

- `f3=3` was thought to have an `imm12 bit-0 ⇒ rs1−8` quirk. That was a
  mirage — the "0x78 byte" observed in batch 16b came from a prior
  half-word store in the same boot session that had written 0x5678 to
  `PROBE_BUF[0x80]`. A clean probe (batch 18, six rs2 variants × eight
  imm12 variants) showed `rd = byte[rs1]` unconditionally. **md.lbu is
  plain LBU.**
- `f3=7` was thought to be a "register move" (`rd ← rs2`). That was also
  a side-effect of test ordering — a prior store had coincidentally
  written 0x12345678 (the rs2 sentinel value) to the loaded address. A
  clean probe (batch 17, three rs1 offsets) showed `rd ← word[rs1]`.
  **f3=7 is plain LW.**
  The rd-FIELD is part of the silicon decode (rewriting it from `x14` to
  `x16` in batch 16a hung the core, refuting the "rd-field is a register
  selector" assumption that holds for standard RISC-V loads).

Almost certainly an **uncached / MMIO access path** parallel to the
cacheable standard L/S opcodes. Firmware evidence supports this:

- `0x77c..0x782`: tight 2000-iteration `md.sh` loop (counter `a1`) — burst
  fill of an MMIO region or descriptor ring.
- `0xbcae..0xbcd0`: a 4-wide unrolled load (`f3=7` ops) plus 4-wide store
  (`f3=4`) sequence — packed copy through an uncached window.

All 8 `f3` values are silicon-decoded: 0/3/5/6/7 are LB/LBU/LH/LHU/LW,
1/2/4 are SB/SH/SW. Coverage matches the standard RISC-V load/store
families packed into a single opcode — the same slot allocation that the
standard uses across two opcodes (0x03 LOAD + 0x23 STORE).

## 10. Open / partially open items

The md32rv ISA has been comprehensively silicon-characterised across
27 harness batches (2026-05-27 → 2026-05-30). The only remaining
open item is the missing vendor documentation; everything else is
resolved (see §10.1 for the full ledger).

- **Vendor ISA documentation** — the Airoha md32rv ISA specification and
  the closed-source `md32rv-sdk` toolchain remain unavailable. Everything
  documented here is silicon-derived, and `bsp/binutils-gdb/` is our own
  binutils extension (not vendor-supplied). Any official document would
  authoritatively settle terminology and might disambiguate the few corner
  encodings — but its absence doesn't block use of md32rv.

- **Performance-oriented rotate / bit-op probes** — resolved in later
  harnesses. Standard Zbb `rol`/`ror`/`rori` encodings trap, but md32rv has
  custom single-instruction `md.ror` / `md.rori` forms at
  `0x33 f7=0x40/0x04 f3=7`. The formerly listed bit-op holes are now
  decoded: `f7=0x40 f3=1/6/7` are `brevrotl`, `masklr`, `ror`;
  `f7=0x04 f3=1/7` are `brevrotli`, `rori`; and `f7=0x08 f3=4..7`
  are halfword pack operations.

- **Rotate probe results from harness31/harness32** — silicon does not accept
  the standard Zbb encodings above: all 105 `rol`/`ror`/`rori` standard slots
  trapped with illegal-instruction cause `2`. The useful rotate encodings are
  md32rv custom `ror` forms:
  - variable right rotate: opcode `0x33`, `funct7=0x40`, `funct3=7`, with
    normal `rs2` as the rotate amount;
  - immediate right rotate: opcode `0x33`, `funct7=0x04`, `funct3=7`, with the
    5-bit `rs2` field used as the immediate shift amount.
    Harness32 then matched all 768 rotate cases with zero traps: `md.rori`
    immediates `0..31`, `md.ror` low amounts `0..31`, register amount masking
    for `N+32`, `N+64`, and `N|0x80000000`, plus destination aliasing
    `rd==rs1` and `rd==rs2`. Harness33 resolved the `funct3=1` candidates:
    `f7=0x40/f3=1` and `f7=0x04/f3=1` both compute
    `rotl32(bit_reverse32(rs1), amount)`, with the register form masking the
    amount as `rs2&31`. They are useful bit-permutation ops, but not reliable
    rotate-left instructions on the original value.

### 10.1 Resolved (this work — silicon-derived)

Items resolved over the batch sequence, in approximate decoding order:

| Topic                                                                  | Resolution                                                                                                                                                                                                                                                                                                                                                                                                                                    | Batch(es)                                                                                                        |
| ---------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------- |
| `0x33 f7=0x10 f3=2`                                                    | signed `max` (was thought to return rs1)                                                                                                                                                                                                                                                                                                                                                                                                      | 11                                                                                                               |
| `0x33 f7=0x10 f3=3`                                                    | illegal-instruction                                                                                                                                                                                                                                                                                                                                                                                                                           | 11                                                                                                               |
| `0x67` family                                                          | conditional branches relocated into JALR slot with 5-bit signed-immediate comparand and B-type offset (`md.beqi/bnei/blti/bgei/bltui/bgeui`); `f3=0`=JALR, `f3=7`=illegal                                                                                                                                                                                                                                                                     | 7, 8                                                                                                             |
| `0x0F f3=4` cache-op classification                                    | full silicon classification across all 32 op-codes: `8` ICACHE-inval-all, `24` global-D-inval, `25` line-D-inval, `21/29` line D-flush+inval, `20/28/30` global D-flush+inval, the other nine (`0/4/12/16/22/23/26/27/31`) are TRUE no-ops (verified against both DCACHE AND ICACHE). The earlier batch-15 "11 codes are global flush" claim was confounded by cache-line aliasing                                                            | 10, 15, 23, 25, **27**                                                                                           |
| `0x0F f3` variants beyond `0/1/4`                                      | confirmed absent from real code (high-`f3` counts in early surveys were rodata mis-disassembled)                                                                                                                                                                                                                                                                                                                                              | 15                                                                                                               |
| `0x33 f3=0` shift / template builder zoo                               | **unified** under one parametric bit-field-insert family: `shift = (f7−3)/4`, `width = min(rs2_field+shift, 32)`, `rd[w−1:0] ← ((rs1<<shift)                                                                                                                                                                                                                                                                                                  | rd_old[shift−1:0])`. The legacy names `cshl1/15/31`, `mask12shl2`, `bfins.h`, the "template builders" and the "` | 0x7` mystery" are all special cases of this one op. **Batch 35 swept all 1024 shift/width combinations.** | 11, 14, 22, **35** |
| `0x33 f3=2` extract zoo                                                | **unified** under the symmetric extract family: `rd = (rs1 >> shift) & ((1<<rs2_field)−1)` with the same `shift = (f7−3)/4`. The earlier "width caps" of 24/16 were rs1=−1 saturation artifacts — `f7=0x23` extracts at bit 8, `f7=0x43` extracts at bit 16 (not both at bit 8 as batch 15 thought). User-flagged firmware op `0x16372533` at offset `0x40aa` = `md.bfextr2 a0, a4, 3`. **Batch 34 swept all 1024 shift/width combinations.** | 15, 25, **34**                                                                                                   |
| `0x33 f3=1` signed extract zoo                                         | **newly resolved** under `md.bfextsrN`: `f7=4N+3`, `N=1..31`, signed extraction from `rs1 >> N`, with width capped at `32-N`; `width=0` returns a sign mask from bit `N-1`.                                                                                                                                                                                                                                                                   | 36, **37**                                                                                                       |
| `0x33 f7=0x04` immediate bit-field / bit-permute family                | fully decoded for `f3=0/1/2/3/4/5/6/7`: `clri`, `brevrotli`, `andi`, `setbi`, `togbi`, `extui`, `maskl`, `rori`; `f3=6 N=16` is `zext.h` (66 firmware sites)                                                                                                                                                                                                                                                                                  | 4, 14, 31, 32, **33**                                                                                            |
| `0x33 f7=0x40` register bit-op / bit-permute family                    | fully decoded for `f3=0/1/2/3/4/5/6/7`: `clrb`, `brevrotl`, `andb`, `setb3`, `setb`, `extb`, `masklr`, `ror` (f3=3 is a functional alias of f3=4)                                                                                                                                                                                                                                                                                             | 4, 14, 28, 31, 32, 33, **37**                                                                                    |
| `0x33 f7=0x03 f3=1` parametric sign-extend                             | `rs2-field = source bit-width`. **Batch 34** swept all N ∈ {0..31}: N ∈ {1..31} matches `((rs1<<(32-N))>>(32-N))`; N=0 returns a sign mask from bit 31. The three former mnemonics `md.sext.b/h/24` are collapsed into a single parametric `md.sext rd, rs1, N`.                                                                                                                                                                              | 1, 3, 28, 30, **34**                                                                                             |
| `0x33 f7=0x08`                                                         | `f3=1 rs2=0` = `clz`; `f3=2 rs2=0` = `ctz`; `f3=0 rs2=0` is also `clz` — an unused encoding alias. `rs2≠0` traps as illegal                                                                                                                                                                                                                                                                                                                   | 1, 17, 26                                                                                                        |
| `0x33 f7=0x10` f3=4                                                    | **`md.mv`** — `rd = rs1` (identity / register move). Silicon-verified across 13 inputs incl. negative rs1. Firmware uses 1 site at 0xa074                                                                                                                                                                                                                                                                                                     | **28, 29**                                                                                                       |
| `0x33 f7=0x10` f3=6                                                    | **`md.copysign`** — `rd = abs(rs2) * sign(rs1)` ≡ `(rs1<0) ? -rs2 : rs2`. Silicon-verified across 4 sign-combinations. Firmware uses 1 site at 0xe1a4                                                                                                                                                                                                                                                                                         | **28**                                                                                                           |
| `0x33 f7=0x10` f3=7                                                    | **`md.mv2`** — `rd = rs2`; `rs1` ignored                                                                                                                                                                                                                                                                                                                                                                                                      | **37**                                                                                                           |
| `0x33 f7=0x08` f3=4..7                                                 | **halfword pack family** — `packhh/packhl/packlh/packll`, selecting high/low halfwords from `rs1` and `rs2`                                                                                                                                                                                                                                                                                                                                   | **37**                                                                                                           |
| Reserved `0x33` ALU/bit-op selector space                              | **closed** — every currently unallocated `funct7=0..127`, `funct3=0..7`, `rs2-field=0..31` selector was tested after excluding standard RV32IM and specified md32rv families. Batch38 covered 28,993 reserved slots with zero legal markers; batch39 retested the 46 readback anomalies and all trapped with `status=1`, `trap_delta=1`.                                                                                                      | **38, 39**                                                                                                       |
| `0x33 f7=0x40` f3=2                                                    | **`md.andb`** — register-indexed isolate-bit: `rd = rs1 & (1 << (rs2 & 31))`. Companion of `md.andi`. Silicon-verified across 5 inputs. Firmware uses 2 sites (0x73ce, 0x7a6a)                                                                                                                                                                                                                                                                | **28**                                                                                                           |
| "Hang ops" `f7 ∈ {0x02, 0x05, 0x08}`                                   | **NOT hangs** — they trap with mcause=2 (illegal instruction). The earlier "silently hang" reading was a measurement artifact of the trap-counter bootstrap bug (clear-loop left counter at `0xDEADDEAD`; first trap recursed)                                                                                                                                                                                                                | **26**                                                                                                           |
| `0x33 f7=0x18/0x19/0x1b f3=3 rs2-field=3` "context-dependent legality" | **rodata mis-disassembly** — all 4 "firmware sites" are .rodata strings (ASCII `"_init\0"`, `"_Cnt[6"`, `"s_Cn"`) treated as code. Firmware never executes them                                                                                                                                                                                                                                                                               | 18, 19, 22, 24, **25**                                                                                           |
| Opcode `0x0b` (CUSTOM-0) — uncached L/S                                | fully decoded — all 8 f3 values populated as `md.lb / sb / sh / lbu / sw / lh / lhu / lw`. Address is exactly `rs1`; imm12 silicon-ignored. The earlier "imm12 quirk for f3=3" and "f3=7 is mv" hypotheses were cross-test memory contamination                                                                                                                                                                                               | 16b–c, 17, 18, **21**                                                                                            |
| `0x0F` cache-op rs1-field path                                         | silicon honours the **register VALUE** of the field-selected register, not the 5-bit literal. The binutils named forms (`md.dcache.inval.all` etc.) which encode the op-code in the rs1 field would misbehave at runtime; the firmware idiom is `li a0, opcode ; md.cache.op a0`                                                                                                                                                              | 17                                                                                                               |

### 10.2 Final `.insn` residual — surveyed (batches 28 + 29)

After all silicon-verified additions, **88 `.insn` lines remain** in the
disassembly. Surveyed exhaustively (every md32rv-opcode site looked at
in context):

| Source                                                                                                  |  Count | Status                                                            |
| ------------------------------------------------------------------------------------------------------- | -----: | ----------------------------------------------------------------- |
| Non-md32rv opcodes (0x77, 0x5b, 0x53, etc.) — rodata strings, RV64-only ops, FP ops, all-zeros, padding | **80** | irrecoverable without code/data boundary info                     |
| 0x33 f3=3 (`f7=0x18/0x19/0x1b rs2=3`)                                                                   |  **4** | confirmed rodata (`"_init"`, `"_Cnt[6"`, `"s_Cn"` adjacent ASCII) |
| 0x67 f3=7 ASCII                                                                                         |  **3** | rodata (`0x656d7567` = "gume", etc.)                              |
| 0x0f end-of-firmware bit-mask table                                                                     |  **1** | rodata (lookup table at 0x10f88)                                  |
| **Real-code md32rv-custom remaining**                                                                   |  **0** | ✓                                                                 |

All 6 previously-mysterious real-code .insn lines are now decoded
(batches 28 + 29): `md.sext` ×2 (with N=8), `md.andb` ×2, `md.mv`, `md.copysign`.

### 10.3 Methodology lessons (worth recording for future probes)

- **Always re-fill the probe buffer per test.** Cross-test memory
  contamination produced two false decodes (md.lbu "imm12 quirk", md.mv
  vs md.lw) before the methodology was tightened.
- **Always zero the trap counter explicitly after the clear loop.**
  Initialising it to the clear sentinel (`0xDEADDEAD`) makes the first
  trap fault again inside the trap handler (counter << 3 = invalid
  store address). Three batches were unnecessarily blocked by this.
- **The `rd` field is part of the silicon's sub-op decode.** Rewriting
  it from `x14` to `x16` (batch 16a) hung the core. Always preserve the
  firmware's encoding bit-exact when probing unknown ops.
- **Probe across multiple `rs1` patterns to disambiguate.** Many "caps"
  and "conditional" patterns were artifacts of using `rs1 = −1` or
  preset-bit aliasing; varying rs1 reveals the real formula.
