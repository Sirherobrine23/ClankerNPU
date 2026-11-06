# md32rv ISA — Operations Specification

Per-opcode, per-variant reference for the md32rv custom RISC-V variant in
the Airoha EN7523 NPU. Every variant is silicon-verified on real hardware
(Genexis E655-R2 board); see `harness/results_decoded.md` for raw
evidence per batch.

**Conventions**

- `rd`, `rs1`, `rs2` refer to register operands.
- `rs2-field` refers to bits [24:20] of the instruction word interpreted
  as a 5-bit immediate (vs. as a register selector).
- All arithmetic is 32-bit. Sign-extension is explicit.
- The base ISA is `rv32imac` + `zicsr` + `zifencei`. Standard RISC-V
  encodings (other than the overloaded opcodes documented here) behave
  per the RISC-V Unprivileged Spec.

---

## Opcode `0x33` — OP (custom ALU / bit-manipulation)

Standard RISC-V uses this opcode for ALU register-register operations
(ADD/SUB/AND/OR/XOR/SLL/SRL/SRA/SLT/SLTU/MUL/DIV/REM). md32rv overloads
it heavily with reserved `(funct7, funct3)` combinations.

R-type layout (all variants):

```
 31 25 24 20 19 15 14 12 11 7 6 0
+---------+------+------+------+------+--------+
| funct7 | rs2 | rs1 | funct3| rd | 0x33 |
+---------+------+------+------+------+--------+
```

The `rs2` field is either a register operand (binary reg-reg ops) or a
5-bit immediate (immediate-bit-field ops and parametric shift-width ops);
which one depends on `(funct7, funct3)`.

Exhaustive reserved-space coverage: batches 38 and 39 swept every
currently unallocated `opcode 0x33` selector across
`funct7=0..127`, `funct3=0..7`, and `rs2-field=0..31`. Batch38 found no
clean legal execution markers among 28,993 reserved slots. Batch39
retested the 46 fixed-offset readback anomalies from batch38 and confirmed
all 46 trap with `status=1` and `trap_delta=1`. Therefore the legal
ALU/bit-operation space for opcode `0x33` is fully specified below for the
tested silicon; all other selector triples trap as illegal instruction.

### md.sh1add

Shift-add by 1 — a Zba-style fused shift-then-add.

- **Encoding**: `funct7 = 0x10, funct3 = 0`. R-type: `d, s, t`.
- **Semantics**: `rd = rs1 + (rs2 << 1)`
- **Operands**: `md.sh1add rd, rs1, rs2`
- **Notes**: standard RISC-V Zba `sh1add` uses `funct3 = 2` at `funct7 = 0x10`;
  md32rv moves it to `funct3 = 0`.

### md.sh2add

Shift-add by 2.

- **Encoding**: `funct7 = 0x10, funct3 = 1`. R-type: `d, s, t`.
- **Semantics**: `rd = rs1 + (rs2 << 2)`
- **Operands**: `md.sh2add rd, rs1, rs2`

### md.max

Signed maximum.

- **Encoding**: `funct7 = 0x10, funct3 = 2`. R-type: `d, s, t`.
- **Semantics**: `rd = (signed(rs1) > signed(rs2)) ? rs1 : rs2`
- **Operands**: `md.max rd, rs1, rs2`
- **Notes**: signed comparison (not unsigned). The pair `funct3 = 3` traps as
  illegal.

### md.mv

Register move (identity). `rs2` ignored.

- **Encoding**: `funct7 = 0x10, funct3 = 4`. R-type: `d, s, t` (t is encoding-only).
- **Semantics**: `rd = rs1`
- **Operands**: `md.mv rd, rs1` (the `rs2` slot is functionally inert but
  encoded with a register field that the silicon ignores).
- **Firmware**: 1 site at 0xa074.

### md.copysign

Copy sign from rs1 onto rs2's magnitude.

- **Encoding**: `funct7 = 0x10, funct3 = 6`. R-type: `d, s, t`.
- **Semantics**: `rd = (signed(rs1) < 0) ? -rs2 : rs2` (equivalently
  `rd = abs(rs2) * sign(rs1)` when |rs2| ≠ 0x80000000)
- **Operands**: `md.copysign rd, rs1, rs2`
- **Firmware**: 1 site at 0xe1a4.
- **Notes**: behaves like `copysign(rs2, rs1)` from the standard C library.

### md.mv2

Register move from `rs2`.

- **Encoding**: `funct7 = 0x10, funct3 = 7`. R-type: `d, s, t`
  (`rs1` is ignored).
- **Semantics**: `rd = rs2`
- **Operands**: `md.mv2 rd, rs2`
- **Verification coverage**: Harness37 tested representative `rs2`
  values `0`, `1`, `0x12345678`, `0xffffffff`, and `0x80000000`; all
  matched and no instruction trapped.

### md.clrb

Clear a single bit, bit index from a register.

- **Encoding**: `funct7 = 0x40, funct3 = 0`. R-type: `d, s, t`.
- **Semantics**: `rd = rs1 & ~(1 << (rs2 & 31))`
- **Operands**: `md.clrb rd, rs1, rs2`

### md.brevrotl _(provisional mnemonic)_

Bit-reverse `rs1`, then rotate the reversed value left by an amount from a
register.

- **Encoding**: `funct7 = 0x40, funct3 = 1`. R-type: `d, s, t`.
- **Semantics verified by harness33**:

```
rd = rotl32(bit_reverse32(rs1), rs2 & 31)
   = bit_reverse32(rotr32(rs1, rs2 & 31))
```

- **Operands**: `md.brevrotl rd, rs1, rs2`
- **Verification coverage**: harness33 matched all 576 register-form cases
  and trapped none. It tested `rs1` values `0x12345678`, `0x80000001`,
  and `0xdeadbeef`; low amounts `0..31`; high-bit amount aliases `N + 32`,
  `N + 64`, and `N | 0x80000000`; and destination overlap cases
  `rd == rs1` and `rd == rs2`. This proves the amount is masked with
  `rs2 & 31` for the tested values.
- **Notes**: this is not a reliable rotate-left of `rs1`. It only appears
  rotate-like for values equal to their own bit-reversal, such as
  `0x80000001`.

### md.andb

Isolate a single bit, bit index from a register (register-indexed companion
of `md.andi`).

- **Encoding**: `funct7 = 0x40, funct3 = 2`. R-type: `d, s, t`.
- **Semantics**: `rd = rs1 & (1 << (rs2 & 31))` — the result is either 0
  or the single-bit mask itself.
- **Operands**: `md.andb rd, rs1, rs2`
- **Firmware**: 2 sites (0x73ce, 0x7a6a) used in MMIO bit-test setup.

### md.setb / md.setb3

Set a single bit, bit index from a register.

- **Encoding**: two distinct encodings producing the same behaviour:
- `md.setb`: `funct7 = 0x40, funct3 = 4`
- `md.setb3`: `funct7 = 0x40, funct3 = 3` (functional alias)
- **Semantics**: `rd = rs1 | (1 << (rs2 & 31))`
- **Operands**: `md.setb rd, rs1, rs2` / `md.setb3 rd, rs1, rs2`
- **Notes**: `md.setb3` and `md.setb` produce identical bytes only when
  assembled with the same operands but at different `funct3` slots; the
  silicon implements both as identical operations. The firmware uses
  `md.setb3` at a handful of sites (see binutils for distinct mnemonics).

### md.extb

Extract a single bit, bit index from a register; result is `0` or `1`.

- **Encoding**: `funct7 = 0x40, funct3 = 5`. R-type: `d, s, t`.
- **Semantics**: `rd = (rs1 >> (rs2 & 31)) & 1`
- **Operands**: `md.extb rd, rs1, rs2`

### md.masklr

Mask to the low `N` bits, where `N` is read from a register.

- **Encoding**: `funct7 = 0x40, funct3 = 6`. R-type: `d, s, t`.
- **Semantics**: `rd = rs1 & ((1 << (rs2 & 31)) - 1)`, with
  `rs2&31 = 0` producing `0`.
- **Operands**: `md.masklr rd, rs1, rs2`
- **Verification coverage**: Harness37 tested `rs1` values
  `0x12345678`, `0x80000001`, and `0xffffffff` with amounts `0..31`,
  `32`, `33`, `63`, and `0x80000010`; all matched the `rs2&31`
  masking rule and no instruction trapped.

### md.ror _(provisional mnemonic)_

Rotate right, amount from a register. This is an md32rv custom encoding,
not the standard Zbb `ror` encoding.

- **Encoding**: `funct7 = 0x40, funct3 = 7`. R-type: `d, s, t`.
- **Semantics verified by harness32**:

```
rd = (rs1 >> (rs2 & 31)) | (rs1 << ((32 - rs2) & 31))
```

- **Operands**: `md.ror rd, rs1, rs2`
- **Verification coverage**: harness32 matched all 576 `md.ror` cases and
  trapped none. It tested `rs1` values `0x12345678`, `0x80000001`, and
  `0xdeadbeef`; low rotate amounts `0..31`; high-bit amount aliases
  `N + 32`, `N + 64`, and `N | 0x80000000`; and destination overlap cases
  `rd == rs1` and `rd == rs2`. This proves the amount is masked with
  `rs2 & 31` for the tested values.

### md.clri

Clear a single bit at immediate position `N`.

- **Encoding**: `funct7 = 0x04, funct3 = 0`. R-type: `d, s, <` (with `<` =
  5-bit immediate in the rs2 field).
- **Semantics**: `rd = rs1 & ~(1 << N)` where `N = rs2-field` (0..31)
- **Operands**: `md.clri rd, rs1, N`

### md.brevrotli _(provisional mnemonic)_

Bit-reverse `rs1`, then rotate the reversed value left by an immediate
amount encoded in the `rs2` field.

- **Encoding**: `funct7 = 0x04, funct3 = 1`. R-type: `d, s, <` (with `<` =
  5-bit rotate amount in the rs2 field).
- **Semantics verified by harness33**:

```
rd = rotl32(bit_reverse32(rs1), N)
   = bit_reverse32(rotr32(rs1, N))
```

where `N = rs2-field`.

- **Operands**: `md.brevrotli rd, rs1, N`
- **Verification coverage**: harness33 matched all 192 immediate-form cases
  and trapped none. It tested `rs1` values `0x12345678`, `0x80000001`,
  and `0xdeadbeef`, all immediate amounts `0..31`, and destination overlap
  `rd == rs1`.
- **Notes**: the `funct3=1` immediate form mirrors the `funct7=0x40`
  register-indexed form, as the surrounding immediate bit operations do.

### md.andi

Isolate a single bit at immediate position `N` — yields either `0` or
the bit-mask `(1 << N)`.

- **Encoding**: `funct7 = 0x04, funct3 = 2`. R-type: `d, s, <`.
- **Semantics**: `rd = rs1 & (1 << N)`
- **Operands**: `md.andi rd, rs1, N`

### md.setbi

Set a single bit at immediate position `N`.

- **Encoding**: `funct7 = 0x04, funct3 = 3`. R-type: `d, s, <`.
- **Semantics**: `rd = rs1 | (1 << N)`
- **Operands**: `md.setbi rd, rs1, N`

### md.togbi

Toggle a single bit at immediate position `N`.

- **Encoding**: `funct7 = 0x04, funct3 = 4`. R-type: `d, s, <`.
- **Semantics**: `rd = rs1 ^ (1 << N)`
- **Operands**: `md.togbi rd, rs1, N`

### md.extui

Extract a single bit at immediate position `N`; result is `0` or `1`.

- **Encoding**: `funct7 = 0x04, funct3 = 5`. R-type: `d, s, <`.
- **Semantics**: `rd = (rs1 >> N) & 1`
- **Operands**: `md.extui rd, rs1, N`
- **Notes**: naming inherited from Xtensa-style "extract unsigned immediate"
  but the silicon implementation is strictly 1-bit. For wider extracts,
  use the parametric `md.bfextrN` family below.

### md.maskl

Mask to the low `N` bits.

- **Encoding**: `funct7 = 0x04, funct3 = 6`. R-type: `d, s, <`.
- **Semantics**: `rd = rs1 & ((1 << N) - 1)` where `N = rs2-field` (0..31)
- **Operands**: `md.maskl rd, rs1, N`
- **Notes**: `N = 16` is special-cased as `md.zext.h` (see next).

### md.zext.h

Zero-extend halfword — equivalent to `md.maskl rd, rs1, 16`. Specialised
mnemonic because 66 firmware sites use this exact form.

- **Encoding**: `funct7 = 0x04, funct3 = 6, rs2-field = 16`. Type: `d, s`.
- **Semantics**: `rd = rs1 & 0xFFFF`
- **Operands**: `md.zext.h rd, rs1`
- **Notes**: matches standard Zbb `zext.h` semantics. Placed before
  `md.maskl` in the binutils opcode table so the disassembler picks the
  specialized mnemonic when `N = 16`.

### md.rori _(provisional mnemonic)_

Rotate right by an immediate amount encoded in the `rs2` field. This is an
md32rv custom R-type immediate encoding, not standard Zbb `rori`, which uses
opcode `0x13`.

- **Encoding**: `funct7 = 0x04, funct3 = 7`. R-type: `d, s, <` (with `<` =
  5-bit rotate amount in the rs2 field).
- **Semantics verified by harness32**:

```
rd = (rs1 >> N) | (rs1 << ((32 - N) & 31))
```

where `N = rs2-field`.

- **Operands**: `md.rori rd, rs1, N`
- **Verification coverage**: harness32 matched all 192 `md.rori` cases and
  trapped none. It tested `rs1` values `0x12345678`, `0x80000001`, and
  `0xdeadbeef`, all immediate amounts `0..31`, and destination overlap
  `rd == rs1`.
- **ChaCha20 mapping**: `rotl16(x) = md.rori x, x, 16`,
  `rotl12(x) = md.rori x, x, 20`, `rotl8(x) = md.rori x, x, 24`,
  `rotl7(x) = md.rori x, x, 25`.

### md.sext

Parametric sign-extend from an N-bit field, where N is encoded in the
rs2 field (5-bit immediate). The earlier separately-named `md.sext.b`
(N=8), `md.sext.h` (N=16), and `md.sext24` are all special cases of this
single op.

- **Encoding**: `funct7 = 0x03, funct3 = 1, rs2-field = N`. R-type:
  `d, s, <` (with `<` = 5-bit immediate `N` in the rs2 field).
- **Semantics**:

```
if N = 0:
    rd = (rs1 & 0x80000000) ? 0xffffffff : 0x00000000
else:
    rd = sext_from_bit_(N-1)(rs1)
       ≡ ((rs1 << (32-N)) >> (32-N)) (arithmetic shift right)
```

For N in `1..31` on this silicon the operation is exactly the formula
above. For N=0, the operation degenerates to a sign-mask from bit 31,
not an illegal encoding and not an identity operation.

- **Operands**: `md.sext rd, rs1, N`
- **Notes**:
- N = 8 is `sext.b`; N = 16 is `sext.h`; N = 24 sign-extends from
  bit 23 (a 24-bit value treated as signed).
- Firmware uses N ∈ {8, 16, 24}: 2 sites at N=8 (0x2446, 0x246c),
  29 sites at N=16, no firmware sites at N=24.
- Harness34 swept all N values `0..31` across 9 inputs, including the
  previously open `N >= 25` range, with zero traps. All `N=1..31`
  cases matched the sign-extension formula; all `N=0` cases matched
  the sign-mask rule above.

### md.clz

Count leading zeros.

- **Encoding**: `funct7 = 0x08, funct3 = 1, rs2-field = 0`. Type: `d, s`.
- **Semantics**: `rd =` number of leading 0-bits in `rs1`. Returns 32 if
  `rs1 = 0`.
- **Operands**: `md.clz rd, rs1`
- **Notes**: a second legal encoding exists at `funct3 = 0, rs2-field = 0`
  () producing the same `clz` result; the
  firmware uses only the `funct3 = 1` form. With `rs2-field ≠ 0` the
  instruction traps as illegal regardless of `funct3`.

### md.ctz

Count trailing zeros.

- **Encoding**: `funct7 = 0x08, funct3 = 2, rs2-field = 0`. Type: `d, s`.
- **Semantics**: `rd =` number of trailing 0-bits in `rs1`. Returns 32 if
  `rs1 = 0`.
- **Operands**: `md.ctz rd, rs1`

### md.packhh / md.packhl / md.packlh / md.packll

Halfword pack operations. The mnemonic suffix names which halfword is
taken from `rs1` and `rs2`, respectively.

- **Encoding**: `funct7 = 0x08`, `funct3 = 4..7`. R-type: `d, s, t`.
- **Semantics**:

```
funct3=4: rd = (rs1[31:16] << 16) | rs2[31:16]   // md.packhh
funct3=5: rd = (rs1[31:16] << 16) | rs2[15:0]    // md.packhl
funct3=6: rd = (rs1[15:0]  << 16) | rs2[31:16]   // md.packlh
funct3=7: rd = (rs1[15:0]  << 16) | rs2[15:0]    // md.packll
```

- **Operands**: `md.packhh rd, rs1, rs2`, `md.packhl rd, rs1, rs2`,
  `md.packlh rd, rs1, rs2`, `md.packll rd, rs1, rs2`
- **Verification coverage**: Batch36 first identified these as legal;
  Harness37 tested three `(rs1, rs2)` pairs across all four variants and
  all matched with zero traps.

### md.bfinsrN — parametric bit-field-insert family

A single parametric op that unifies 32 distinct `funct7` values. Selected
by `funct7 = 4k + 3` for `k = 0..31`, where `k` is the shift amount baked
into the mnemonic (e.g., `funct7 = 0x07 → md.bfinsr1`).

- **Encoding**: `funct7 = 4*N + 3, funct3 = 0`. R-type: `d, s, <` (with
  `<` = 5-bit immediate `width` in the rs2 field).
- **Formula**:

```
shift = (funct7 - 3) / 4 (= N, from the mnemonic suffix)
width = min(rs2_field + shift, 32)
rd[width-1 : 0] ← ((rs1 << shift) | rd_old[shift-1:0]) & ((1<<width)-1)
rd[31 : width] ← preserved
```

- **Operands**: `md.bfinsrN rd, rs1, width` — `rd` is read-modify-write.
- **Verification coverage**: Harness35 swept all `shift=0..31` and all
  `rs2_field=0..31` combinations (1024 cases) with `rs1=0x12345678`
  and `rd_old=0xdeadbeef`. Every case matched the formula and no
  instruction trapped.
- **Notes**: when `rs2_field = 0` the op is a no-op (`width = shift`,
  so only the preserved low `shift` bits replace themselves). When
  `width = 32`, the entire `rd` is overwritten with `(rs1 << shift) |
 rd_old[shift-1:0]`. The mnemonic family replaces the earlier
  misleading legacy names `cshl1` (= `bfinsr1`), `cshl15` (= `bfinsr15`),
  `cshl31` (= `bfinsr31`), `mask12shl2` (= `bfinsr2`), and `bfins.h`
  (= `bfinsr16`).

**Defined mnemonics in this family**:

| Mnemonic                                                                                                     |   `funct7` | shift | MATCH base   | Firmware-emitted?                                              |
| ------------------------------------------------------------------------------------------------------------ | ---------: | ----: | ------------ | -------------------------------------------------------------- |
| `md.bfinsr0`                                                                                                 |       0x03 |     0 | `0x06000033` | yes (7 sites)                                                  |
| `md.bfinsr1`                                                                                                 |       0x07 |     1 | `0x0e000033` | yes (30 sites, formerly `cshl1`)                               |
| `md.bfinsr2`                                                                                                 |       0x0b |     2 | `0x16000033` | yes (38 sites, formerly `mask12shl2`)                          |
| `md.bfinsr3`                                                                                                 |       0x0f |     3 | `0x1e000033` | yes (10 sites)                                                 |
| `md.bfinsr4`                                                                                                 |       0x13 |     4 | `0x26000033` | yes (11 sites)                                                 |
| `md.bfinsr6`                                                                                                 |       0x1b |     6 | `0x36000033` | yes (1 site)                                                   |
| `md.bfinsr8`                                                                                                 |       0x23 |     8 | `0x46000033` | yes (8 sites)                                                  |
| `md.bfinsr12`                                                                                                |       0x33 |    12 | `0x66000033` | yes (1 site)                                                   |
| `md.bfinsr14`                                                                                                |       0x3b |    14 | `0x76000033` | yes (4 sites)                                                  |
| `md.bfinsr15`                                                                                                |       0x3f |    15 | `0x7e000033` | yes (18 sites, formerly `cshl15`)                              |
| `md.bfinsr16`                                                                                                |       0x43 |    16 | `0x86000033` | yes (3 sites, formerly `bfins.h`)                              |
| `md.bfinsr25`                                                                                                |       0x67 |    25 | `0xce000033` | yes (2 sites)                                                  |
| `md.bfinsr27`                                                                                                |       0x6f |    27 | `0xde000033` | yes (2 sites)                                                  |
| `md.bfinsr28`                                                                                                |       0x73 |    28 | `0xe6000033` | yes (2 sites)                                                  |
| `md.bfinsr29`                                                                                                |       0x77 |    29 | `0xee000033` | yes (3 sites)                                                  |
| `md.bfinsr31`                                                                                                |       0x7f |    31 | `0xfe000033` | yes (6 sites, formerly `cshl31`)                               |
| `md.bfinsr5`, `md.bfinsr7`, `md.bfinsr9..11`, `md.bfinsr13`, `md.bfinsr17..24`, `md.bfinsr26`, `md.bfinsr30` | (the rest) |     — | —            | not seen in this firmware; silicon-verified legal by harness35 |

The MASK for every entry is `0xfe00707f` — pinning the f7 (= shift slot)
and opcode+f3, leaving rd / rs1 / rs2-field (= width) all free.

### md.bfextrN — parametric bit-field-extract family

The symmetric extract counterpart of `md.bfinsr`.

- **Encoding**: `funct7 = 4*N + 3, funct3 = 2`. R-type: `d, s, <`.
- **Formula**:

```
shift = (funct7 - 3) / 4 (= N, from the mnemonic suffix)
rd = (rs1 >> shift) & ((1 << rs2_field) - 1)
```

- **Operands**: `md.bfextrN rd, rs1, width` — `rd` is write-only.
- **Verification coverage**: Harness34 swept all `shift=0..31` and all
  `rs2_field=0..31` combinations (1024 cases) with
  `rs1=0xf0f01234`. Every case matched the formula and no instruction
  trapped.
- **Notes**:
- `rs2_field = 0` zeros `rd`.
- The earlier batch-15 "width caps of 24 and 16" for `bfext.w` /
  `bfext.h` were artifacts of using `rs1 = −1`: when all bits of `rs1`
  are 1, `(rs1 >> shift)` has only `(32 − shift)` significant bits,
  so the result saturates at `(1 << (32 − shift)) − 1` regardless of
  a wider `rs2_field` value. There is no explicit saturation logic in
  the silicon — it's a natural shift-out effect.
- The mnemonic family replaces the earlier misleading legacy names
  `bfext.w` (= `bfextr8`) and `bfext.h` (= `bfextr16`).

**Defined mnemonics in this family**:

| Mnemonic      | `funct7` | shift | MATCH base   | Firmware-emitted?                                   |
| ------------- | -------: | ----: | ------------ | --------------------------------------------------- |
| `md.bfextr1`  |     0x07 |     1 | `0x0e002033` | yes (3 sites)                                       |
| `md.bfextr2`  |     0x0b |     2 | `0x16002033` | yes (2 sites, includes 0x16372533 at offset 0x40aa) |
| `md.bfextr3`  |     0x0f |     3 | `0x1e002033` | yes (3 sites)                                       |
| `md.bfextr4`  |     0x13 |     4 | `0x26002033` | yes (1 site)                                        |
| `md.bfextr6`  |     0x1b |     6 | `0x36002033` | yes                                                 |
| `md.bfextr8`  |     0x23 |     8 | `0x46002033` | yes (10 sites, formerly `bfext.w`)                  |
| `md.bfextr11` |     0x2f |    11 | `0x5e002033` | yes (3 sites)                                       |
| `md.bfextr15` |     0x3f |    15 | `0x7e002033` | yes (1 site)                                        |
| `md.bfextr16` |     0x43 |    16 | `0x86002033` | yes (14 sites, formerly `bfext.h`)                  |
| `md.bfextr31` |     0x7f |    31 | `0xfe002033` | yes                                                 |

MASK for every entry is `0xfe00707f`.

### md.bfextsrN — parametric signed bit-field-extract family

Signed counterpart of `md.bfextrN`. This family starts at shift 1:
`funct7 = 0x03, funct3 = 1` is already occupied by `md.sext`, so there
is no `md.bfextsr0` encoding.

- **Encoding**: `funct7 = 4*N + 3, funct3 = 1`, for `N = 1..31`.
  R-type: `d, s, <`.
- **Formula**:

```
shift = (funct7 - 3) / 4 (= N, from the mnemonic suffix)
if rs2_field = 0:
    rd = ((rs1 >> (shift - 1)) & 1) ? 0xffffffff : 0
else:
    width = min(rs2_field, 32 - shift)
    rd = sign_extend_width((rs1 >> shift) & ((1 << width) - 1), width)
```

- **Operands**: `md.bfextsrN rd, rs1, width`
- **Verification coverage**: Batch36 found the legal `funct3=1`
  encodings for all `funct7=4N+3` shift slots. Harness37 swept all
  `shift=1..31` and all `rs2_field=0..31` combinations (992 cases) with
  `rs1=0xf0f01234`; every case matched the formula and no instruction
  trapped.

MASK for every entry is `0xfe00707f`, with the same `funct7=4N+3`
shift-slot convention as `md.bfinsrN` and `md.bfextrN`.

### Hang / illegal `funct7` values at `funct3 = 0`

- `funct7 = 0x02`: traps `mcause=2` for any `rs2-field`.
- `funct7 = 0x05`: traps for any `rs2-field`.
- `funct7 = 0x08`: legal only when `rs2-field = 0` (then it's a `clz`
  alias as noted above); traps for `rs2-field ≠ 0`.

These are silicon-verified as TRAPS (not hangs as previously believed —
). They lie outside the `funct7 = 4k+3` pattern that
defines the unified `bfinsr` family. The firmware never emits them.

---

## Opcode `0x67` — JALR + conditional branches with 5-bit immediate

Standard RISC-V uses this opcode only for `JALR` (`funct3 = 0`); the
other `funct3` values are reserved. md32rv overloads `funct3 = 1..6`
with conditional branches that compare a register against a 5-bit
signed immediate (not against another register, as in standard RISC-V).

This is the _largest_ architectural deviation from standard RISC-V — it
moves the entire conditional branch family out of opcode `0x63`.

B-type layout with the rs2 field reused as a 5-bit signed immediate:

```
 31 25 24 20 19 15 14 12 11 7 6 0
+---------+-----+-----+------+---+--------+
| imm[12]+| imm5| rs1 |funct3| imm[11]+ | 0x67 |
|imm[10:5]|=cmp | | |+imm[4:1]| |
+---------+-----+-----+------+---+--------+
```

The 5-bit signed immediate (`rs2`-field at bits [24:20], called `imm5`)
is the comparand against `rs1`. The B-type immediate (split per standard
B-type layout) gives the branch _offset_ added to PC.

### jalr — standard JALR

- **Encoding**: `funct3 = 0`. I-type: `d, j(s)` (rd, imm12, rs1).
- **Semantics**: `rd = pc + 4 ; pc = (rs1 + imm12) & ~1`
- **Operands**: standard RISC-V `jalr rd, imm12(rs1)`.

### md.beqi

Branch if `rs1 == sext(imm5)`.

- **Encoding**: `funct3 = 1`. Custom B-type: `s, Xc4, p`
- `s` = rs1 register
- `Xc4` = 5-bit signed immediate at bits [24:20] (sign-extended for
  comparison)
- `p` = B-type branch offset (PC-relative)
- **Semantics**: `if signed(rs1) == signed_sext5(imm5): pc += offset; else pc += 4`
- **Operands**: `md.beqi rs1, imm5, target`
- **Firmware**: 257 sites — the most-used branch variant.
- **Verification coverage**: Branch harness `BRN1` swept all `imm5=0..31`
  values with representative `rs1` values around both `sext5(imm5)` and
  `zext5(imm5)`. All 533 `beqi` cases matched the signed equality rule
  with zero errors.

### md.bnei

Branch if `rs1 != sext(imm5)`.

- **Encoding**: `funct3 = 2`. Same B-type layout as `md.beqi`.
- **Semantics**: `if signed(rs1) != signed_sext5(imm5): pc += offset`
- **Operands**: `md.bnei rs1, imm5, target`
- **Firmware**: 188 sites.
- **Verification coverage**: Branch harness `BRN1` swept all `imm5=0..31`
  values with the same value set as `beqi`. All 533 `bnei` cases matched
  the signed inequality rule with zero errors.

### md.blti

Branch if `rs1 <signed sext(imm5)`.

- **Encoding**: `funct3 = 3`. Same B-type layout.
- **Semantics**: `if signed(rs1) < signed_sext5(imm5): pc += offset`
- **Operands**: `md.blti rs1, imm5, target`
- **Firmware**: 97 sites.
- **Verification coverage**: Branch harness `BRN1` swept all `imm5=0..31`
  values with signed-neighborhood `rs1` values, plus unsigned edge values.
  All 533 `blti` cases matched the signed less-than rule with zero errors.

### md.bgei

Branch if `rs1 >=signed sext(imm5)`.

- **Encoding**: `funct3 = 4`. Same B-type layout.
- **Semantics**: `if signed(rs1) >= signed_sext5(imm5): pc += offset`
- **Operands**: `md.bgei rs1, imm5, target`
- **Firmware**: 87 sites.
- **Verification coverage**: Branch harness `BRN1` swept all `imm5=0..31`
  values with the same value set as `blti`. All 533 `bgei` cases matched
  the signed greater-or-equal rule with zero errors.

### md.bltui

Branch if `rs1 <unsigned zext(imm5)`.

- **Encoding**: `funct3 = 5`. Same B-type layout.
- **Semantics**: `if unsigned(rs1) < unsigned_zext5(imm5): pc += offset`
- **Operands**: `md.bltui rs1, imm5, target`
- **Firmware**: 28 sites.
- **Verification coverage**: Branch harness `BRN1` swept all `imm5=0..31`
  values with unsigned-neighborhood `rs1` values, plus signed edge values.
  All 533 `bltui` cases matched the unsigned less-than rule with zero errors.

### md.bgeui

Branch if `rs1 >=unsigned zext(imm5)`.

- **Encoding**: `funct3 = 6`. Same B-type layout.
- **Semantics**: `if unsigned(rs1) >= unsigned_zext5(imm5): pc += offset`
- **Operands**: `md.bgeui rs1, imm5, target`
- **Firmware**: 36 sites.
- **Verification coverage**: Branch harness `BRN1` swept all `imm5=0..31`
  values with the same value set as `bltui`. All 533 `bgeui` cases matched
  the unsigned greater-or-equal rule with zero errors.

### Illegal: `funct3 = 7`

Silicon-verified illegal-instruction trap (`mcause = 2`). Never emitted
by firmware.

Branch harness `BRN1` executed a representative `funct3=7` opcode and
observed exactly one illegal-instruction trap while the harness continued
through its trap handler (`traps=1`, `f3_7_traps=1`).

### Branch condition sweep summary

The branch ISA harness in `branch_isa/` generated 3,198 conditional-branch
cases across `funct3=1..6`. Each case initialized `rs1`, executed one
opcode-`0x67` branch with a `+8` byte target, and checked whether the
actual control-flow path matched the expected condition. DUT result:

| Branch     | Total | Expected taken | Expected not-taken | Errors |
| ---------- | ----: | -------------: | -----------------: | -----: |
| `md.beqi`  |   533 |             32 |                501 |      0 |
| `md.bnei`  |   533 |            501 |                 32 |      0 |
| `md.blti`  |   533 |            179 |                354 |      0 |
| `md.bgei`  |   533 |            354 |                179 |      0 |
| `md.bltui` |   533 |            160 |                373 |      0 |
| `md.bgeui` |   533 |            373 |                160 |      0 |

This confirms the documented signed-vs-unsigned `imm5` interpretation for
all immediate values `0..31` on the tested silicon.

Separate offset harnesses confirmed that opcode-`0x67` uses the standard
B-type PC-relative offset field. Forward offsets `+4`, `+8`, `+12`, `+16`,
`+32`, `+256`, `+1024`, `+2048`, and `+4092` passed with zero errors.
Backward offsets `-8`, `-12`, `-16`, `-32`, `-256`, `-1024`, `-2048`,
and `-4092` also passed with zero errors. The `-4` offset was intentionally
not used because it would branch to itself.

Standard opcode-`0x63` was also checked separately. Legal standard branch
forms `beq`, `bne`, `blt`, `bge`, `bltu`, and `bgeu` all behaved as standard
RISC-V in taken and not-taken cases. Reserved `funct3=2` and `funct3=3`
at opcode `0x63` produced illegal-instruction traps.

---

## Opcode `0x0F` — MISC-MEM (FENCE family + cache operations)

Standard RISC-V uses this opcode for `FENCE` (`funct3 = 0`) and `FENCE.I`
(`funct3 = 1`). md32rv adds custom cache-maintenance operations at
`funct3 = 4`.

### fence — standard ordering fence

- **Encoding**: `funct3 = 0`. Standard RISC-V FENCE encoding.
- **Semantics**: per RV spec — pred/succ memory ordering.
- **Notes**: behaves as standard RISC-V.

### fence.i — standard instruction-fetch fence

- **Encoding**: `funct3 = 1`. Standard RISC-V Zifencei encoding.
- **Semantics**: per RV spec — flush instruction stream to make recent
  data stores visible to instruction fetch.
- **Notes**: behaves as standard RISC-V. On this silicon, `fence.i` also
  invalidates the ICACHE (necessary to make modified instructions
  visible).

### md.cache.op — custom cache-maintenance dispatcher

A single instruction that performs cache maintenance; the specific
operation is selected by the runtime _value_ of `rs1` (its low 5 bits).

- **Encoding**: `funct3 = 4, opcode = 0x0F`. Operand format `s` (rs1).
- **Operand**: `md.cache.op rs1`
- **Op-code source**: the silicon decodes bits `[4:0]` of the _register
  value_ in `rs1` (NOT the rs1-field literal in the encoding). The
  firmware idiom is `li a0, OPCODE_N ; md.cache.op a0`.

The 32 possible 5-bit op-code values resolve as follows.
Where `addr` is mentioned, it means the _upper bits_ of
the same `rs1` value can carry an address (since only bits [4:0] are
consumed as the op-code, the remaining 27 bits remain available for
line-scoped variants to specify which line).

#### Op-code 8: `md.icache.inval.all`

- **Effect**: invalidate the entire instruction cache.
- **Scope**: global (all ICACHE lines dropped).
- **DCACHE effect**: none.
- **Firmware use**: boot prologue at offset 0x38 (`li a0, 8; md.cache.op a0`).

#### Op-code 20: `md.dcache.flush_inv.all` (global)

- **Effect**: write back AND invalidate every dirty data-cache line.
- **Scope**: global (all DCACHE lines).
- **ICACHE effect**: none separately tested.

#### Op-code 21: `md.dcache.flush_inv.line`

- **Effect**: write back AND invalidate the single DCACHE line containing
  the address carried in the upper bits of rs1.
- **Scope**: line (one cache line).

#### Op-code 24: `md.dcache.inval.all`

- **Effect**: invalidate (drop without writing back) every DCACHE line.
- **Scope**: global.
- **Firmware use**: boot prologue at offset 0x30 (`li a0, 24; md.cache.op a0`).

#### Op-code 25: `md.dcache.inval.line`

- **Effect**: invalidate the single DCACHE line containing the address
  in rs1's upper bits; no write-back.
- **Scope**: line.

#### Op-codes 28, 30: `md.dcache.flush_inv.all` (global)

- **Effect**: identical to op-code 20 (write back + invalidate, all
  DCACHE lines).
- **Scope**: global.
- **Notes**: these are silicon-distinct encodings producing identical
  observable behaviour. May correspond to internal variants the vendor
  exposes for forward compatibility.

#### Op-code 29: `md.dcache.flush_inv.line`

- **Effect**: write back AND invalidate the single DCACHE line for the
  address in rs1.
- **Scope**: line.
- **Firmware use**: per-line `dcache_range_flush` loop.

#### True no-op op-codes: 0, 4, 12, 16, 22, 23, 26, 27, 31

These 9 op-codes execute legally (no trap) but have no observable cache
effect — neither DCACHE nor ICACHE state changes. They occupy reserved
encoding slots that the silicon decodes as inert.

### Illegal: other `funct3` values

`funct3 ∈ {2, 3, 5, 6, 7}` are silicon-verified absent from real code
(early disassembly counts at high `funct3` were rodata mis-disassembled).

---

## Opcode `0x73` — SYSTEM-space vendor cache/system forms

Standard RISC-V uses opcode `0x73` for SYSTEM instructions, including
`ecall`, `ebreak`, and CSR access. The current disassembly-driven pass found
ten real-code md32rv residuals in this opcode space that are not standard CSR
forms.

Observed encoding shape:

```
 31    20 19 15 14 12 11 7 6 0
+--------+-----+-----+----+------+
|funct12 | rs1 | 000 | x0 | 0x73 |
+--------+-----+-----+----+------+
```

Observed selectors:

| Provisional mnemonic | `funct12` | Observed `rs1` fields     | Count | Evidence                                                                          |
| -------------------- | --------: | ------------------------- | ----: | --------------------------------------------------------------------------------- |
| `md.sys.fc0 rs1`     |   `0xfc0` | `x0`, `x10`               |     4 | `npu_rv32.bin` offsets `0x72f6`, `0x730a`, `0x7358`, `0x7368`                     |
| `md.sys.fc2 rs1`     |   `0xfc2` | `x0`, `x10`, `x14`, `x15` |     6 | `npu_rv32.bin` offsets `0x2270`, `0x2414`, `0x7300`, `0x7310`, `0x7384`, `0x7394` |

Current status:

- **Encoding**: firmware-observed, real code.
- **Minimal-harness behavior**: all six distinct observed words trap as
  illegal instructions on the DUT when executed under the minimal harness
  state (`mcause=2`, `mtval` equal to the tested word).
- **MCTREN/0xbc0 matrix behavior**: all 48 combinations of six words across
  tested MCTREN/`0xbc0` states also trap as illegal. Setup did not trap and
  MCTREN readback matched each requested value.
- **Semantics**: not implemented on this DUT under the tested states, or gated
  by an unknown mechanism outside MCTREN/`0xbc0`.
- **CSR correction**: these are not normal CSR instructions; standard CSR
  operations use `funct3=1..7`, while these use `funct3=0`.
- **Context inference**: likely vendor cache or memory-system maintenance.
  The `0x72f6..0x7394` helper cluster pairs the instructions with
  `fence iorw,iorw`, and several forms consume address-like values in `a0`.
- **Coverage note**: the ten `npu_bins/en7523*_npu_rv32.bin` variants add no
  executable residuals after filtering their embedded string tables.

Do not treat the provisional mnemonics as final semantics. They are stable
labels for the discovered encoding family while the enable-state/context
dependency is investigated.

---

## Opcode `0x0b` — CUSTOM-0 uncached load/store family

Standard RISC-V reserves opcode `0x0b` as CUSTOM-0. md32rv populates
all 8 `funct3` values with a complete load/store family. **The address
is exactly `rs1`** — the 12-bit immediate field present in the
encoding is silicon-ignored. The `rd`-field is part of the silicon's
sub-op decode (it is NOT a free register selector), so the firmware's
exact encoding must be preserved bit-for-bit.

This family is the firmware's path for **uncached / MMIO accesses**,
parallel to the standard cached opcodes 0x03 (LOAD) and 0x23 (STORE).

I-type layout for loads (operand format `d, j(s)`):

```
 31 20 19 15 14 12 11 7 6 0
+-------------+-----+------+---+--------+
| imm[11:0] | rs1 |funct3| rd| 0x0b |
+-------------+-----+------+---+--------+
```

S-type layout for stores (operand format `t, q(s)`):

```
 31 25 24 20 19 15 14 12 11 7 6 0
+---------+-----+-----+------+-------+--------+
|imm[11:5]| rs2 | rs1 |funct3|imm[4:0]| 0x0b |
+---------+-----+-----+------+-------+--------+
```

The imm12 in either layout is **silicon-ignored** but kept syntactically
present in the mnemonic so the assembler accepts standard `offset(reg)`
syntax for readability and so all 32 bits are accounted for.

### md.lb — load byte (signed)

- **Encoding**: `funct3 = 0`.
- **Semantics**: `rd = sign_extend_to_32(byte at rs1)`
- **Operands**: `md.lb rd, offset(rs1)` (offset is silicon-ignored).

### md.sb — store byte

- **Encoding**: `funct3 = 1`.
- **Semantics**: `byte at rs1 ← rs2 & 0xFF`
- **Operands**: `md.sb rs2, offset(rs1)`

### md.sh — store halfword

- **Encoding**: `funct3 = 2`.
- **Semantics**: `halfword at rs1 ← rs2 & 0xFFFF`
- **Operands**: `md.sh rs2, offset(rs1)`
- **Firmware example**: a 2000-iteration burst-init loop at 0x77c uses
  `md.sh` repeatedly.

### md.lbu — load byte (unsigned)

- **Encoding**: `funct3 = 3`.
- **Semantics**: `rd = (byte at rs1) & 0xFF` (zero-extended)
- **Operands**: `md.lbu rd, offset(rs1)`

### md.sw — store word

- **Encoding**: `funct3 = 4`.
- **Semantics**: `word at rs1 ← rs2`
- **Operands**: `md.sw rs2, offset(rs1)`

### md.lh — load halfword (signed)

- **Encoding**: `funct3 = 5`.
- **Semantics**: `rd = sign_extend_to_32(halfword at rs1)`
- **Operands**: `md.lh rd, offset(rs1)`

### md.lhu — load halfword (unsigned)

- **Encoding**: `funct3 = 6`.
- **Semantics**: `rd = (halfword at rs1) & 0xFFFF` (zero-extended)
- **Operands**: `md.lhu rd, offset(rs1)`

### md.lw — load word

- **Encoding**: `funct3 = 7`.
- **Semantics**: `rd = word at rs1`
- **Operands**: `md.lw rd, offset(rs1)`
- **Firmware example**: a 4-wide unrolled copy at 0xbcae uses four `md.lw`
  ops, each loading from `rs1 = a0` (after an `add a0, a0, a2` adjusts the
  pointer between iterations).

---

## Opcode `0x7b` — CUSTOM-3 SM4 crypto

Standard RISC-V reserves opcode `0x7b` as CUSTOM-3. md32rv places the
**SM4 block-cipher round** instructions here, matching the encoding of
the standard Zksed extension (which the public spec also puts at the
opcode 0x33-ish slot, but vendors are free to use CUSTOM-3 as md32rv
does). The mnemonic syntax is the same as standard Zksed.

R-type layout with the byte-select operand `bs` encoded in `funct7`
bits [26:25]:

```
 31 27 26 25 24 20 19 15 14 12 11 7 6 0
+----------+------+-----+-----+------+----+--------+
| 0...0/1.0| bs | rs2 | rs1 | 7 | rd | 0x7b |
+----------+------+-----+-----+------+----+--------+
```

`funct7` bit 2 (value `0x04`) distinguishes `sm4ed` (bit clear) from
`sm4ks` (bit set); bits [26:25] hold the `bs` byte-select operand
(0..3); the remaining bits of `funct7` (and the `funct3 = 7` slot) are
fixed.

### md.sm4ed

SM4 encryption round.

- **Encoding**: `funct3 = 7`, `funct7 & 0x04 = 0`, `bs = funct7 & 0x03`.
- **Semantics**: one round of the SM4 block cipher, applying the S-box
  to byte `bs` of `rs2` and XOR-ing the substituted value into `rs1`
  (full operation per the standard Zksed spec).
- **Operands**: `md.sm4ed rd, rs1, rs2, bs` where `bs ∈ {0, 1, 2, 3}`.

### md.sm4ks

SM4 key-schedule round.

- **Encoding**: `funct3 = 7`, `funct7 & 0x04 = 1`, `bs = funct7 & 0x03`.
- **Semantics**: one round of the SM4 key-schedule transformation
  (per Zksed spec).
- **Operands**: `md.sm4ks rd, rs1, rs2, bs`

---

## Cross-references

- Higher-level architectural overview, CSRs, boot model, ISA-deviation
  rationale: `md32rv_isa.md`.
- Silicon-evidence raw data per batch: `harness/results_decoded.md`.
- Binutils opcode table: `bsp/binutils-gdb/opcodes/riscv-opc.c` and
  `bsp/binutils-gdb/include/opcode/riscv-opc.h`.
- Python disassembler wrapper (portable): `md32rv-objdump.py`.
