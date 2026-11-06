# md32rv Boot Sequence — Airoha EN7523 NPU

How the md32rv NPU cluster is brought up from cold reset to executing
the firmware's `init` function. Reconstructed from silicon experiments
on the Genexis E655-R2 board plus disassembly of the firmware boot
prologue (offsets `0x00..0x180` of `npu_rv32.bin`).

The md32rv NPU does **not** use the standard RISC-V reset-vector flow.
Instead the host ARM core is responsible for placing the firmware image
in DRAM, configuring per-core boot registers, and pulsing a trigger
register. Each NPU core then begins executing the same firmware image
from address `0x84000000`, and hart-id-aware code differentiates them.

For the memory-region details referenced below, see
[`md32rv_memory_space.md`](md32rv_memory_space.md).

---

## 1. Pre-boot (ARM host's role)

The Linux driver on the ARM host does the following before the NPU
runs a single instruction:

1. Read `/lib/firmware/npu_rv32.bin` (the md32rv image, typically 60–90 KB).
2. `mmap` the reserved region at physical `0x84000000` (declared as
   `reserved-memory/npu_binary@84000000, size=0x100000` in the device
   tree with `no-map`).
3. Copy the firmware bytes into that region (the entire image, byte-exact).
4. Optionally read `/lib/firmware/npu_data.bin` and copy it to NPU
   SRAM-C at `0x1E900000` — this holds the firmware's `.data` section
   (function-pointer tables, configuration constants, etc.). The
   driver may also place the BSS-clear region's initial contents here.
5. Configure NPU boot registers via `/dev/mem` MMIO writes (see §2).
6. Pulse the BOOT_TRIGGER register to release the cores from reset.

Once triggered, all 4 cores start fetching at the same address.

---

## 2. NPU boot registers (per-core dispatch)

The boot-control registers are MMIO at the following NPU-side physical
addresses (each is a 32-bit register):

| Register            | Address      | Purpose                                     |
| ------------------- | ------------ | ------------------------------------------- |
| **BOOT_TRIGGER**    | `0x1EC08000` | write to trigger releasing cores from reset |
| **BOOT_CONFIG**     | `0x1EC08004` | enable mask for cores: `0x0F` = all 4       |
| **CORE0_BOOT_BASE** | `0x1EC08020` | start address for core 0                    |
| **CORE1_BOOT_BASE** | `0x1EC08024` | start address for core 1                    |
| **CORE2_BOOT_BASE** | `0x1EC08028` | start address for core 2                    |
| **CORE3_BOOT_BASE** | `0x1EC0802C` | start address for core 3                    |

The driver writes `0x84000000` into each of the four `BOOT_BASE`
registers (so every core enters the firmware at the same code address),
`0x0F` into `BOOT_CONFIG`, and finally a non-zero value into
`BOOT_TRIGGER`. The driver supports per-core reboot (used by the
watchdog path) — but the bring-up flow always uses all four.

This is fundamentally different from a standard RISC-V CPU which would
start at a hard-wired reset vector (typically `0x00000000` or
`0x80000000`). The md32rv has no reset vector at all — the cores stay
held in reset until the ARM driver explicitly hands them a starting PC.

---

## 3. Common boot prologue (offsets `0x00..0x108`)

Every core, on release-from-reset, fetches `0x84000000` and executes
the prologue. This prologue is **hart-agnostic** — all four cores run
the same code in parallel. (Per-hart differentiation happens
immediately after, at the `_start` symbol at offset `0x108`.)

### 3.1 Nop-pad — offsets `0x00..0x1F`

Eight `nop` instructions (`0x00000013`) fill the first 32 bytes. This
serves two purposes: (a) it puts the actual setup code at a
16-byte-aligned offset, which improves I-cache line behaviour on first
fetch, and (b) it acts as a safety pad in case a future revision of
the silicon decides to alias the reset vector to offset 0 — the cores
would harmlessly walk through nops before reaching the meaningful
setup.

### 3.2 MCTREN initial state — offsets `0x20..0x2C`

```
0x20:  li    a0, 0x103       # 0001_0000_0011
0x24:  csrc  0x7c0, a0        # clear bits 0, 1, 8 of MCTREN
0x28:  li    a0, 0x003       # 0000_0000_0011
0x2C:  csrs  0x7c0, a0        # set bits 0, 1 of MCTREN
```

After this sequence, MCTREN = `0x003`: **both I-cache and D-cache
enabled, vendor flag bit 8 cleared**. This is a transient state used
only for the cache-flush sequence that follows. The boot doesn't yet
need either cache to actually function — it's just enabling them so
the subsequent flush operations have something to act on.

### 3.3 Cache flush via `md.cache.op` — offsets `0x30..0x3F`

```
0x30:  li    a0, 24           # cache op-code 24 = md.dcache.inval.all
0x34:  md.cache.op a0          # → invalidate every D-cache line
0x38:  li    a0, 8            # cache op-code  8 = md.icache.inval.all
0x3C:  md.cache.op a0          # → invalidate every I-cache line
```

This wipes any stale entries inherited from a prior boot (or from the
post-reset undefined state). Both caches are now in a known-empty state.

### 3.4 MPIS window setup — offsets `0x40..0xDC`

A long block configures the 7 MPIS translation windows plus auxiliary
controls (CSRs `0x9C0`, `0x9DD`, `0x9DF`, `0x9E0..0x9E6`, `0x9F0..0x9F6`).
The values are documented in [`md32rv_memory_space.md`](md32rv_memory_space.md) §3.

After this block, the NPU's effective address space is established: all
peripheral MMIO blocks, SRAM-A/B/C, and the DRAM windows are reachable
via their NPU-side addresses.

The final write — `csrw 0x9c0, 0x7F` at offset `0xDC` — enables windows
0..6 simultaneously (bits 0..6 of the global mask).

### 3.5 Final MCTREN configuration — offsets `0xE4..0x108`

```
0xE4:  li    a0, 0x103
0xE8:  csrs  0x7c0, a0        # set bits 0, 1, 8 of MCTREN
0xEC:  fence.i                 # flush instruction stream after cache-config change

0xF0:  li    a0, 0x103
0xF4:  csrc  0x7c0, a0        # clear those bits again (squelch the previous setting)
0xF8:  fence.i

0xFC:  li    a0, 0x139        # 0001_0011_1001 — bits 0, 3, 4, 5, 8
0x100: csrs  0x7c0, a0
0x104: fence.i
```

The set-then-clear ritual on bits 0/1/8 forces a deterministic flush
of any pending cache work before the final value is written. The
**final MCTREN value is `0x139`** which sets:

- bit `0` — I-cache enabled
- bits `3`, `4`, `5` — vendor micro-arch flags (purpose undocumented)
- bit `8` — vendor flag (purpose undocumented)
- bit `1` (D-cache) is **CLEAR** ⇒ **D-cache is OFF** for normal firmware execution.

The three `fence.i` ops ensure each MCTREN change takes effect before
the next instruction fetches. This is the only place in the firmware
where `fence.i` is used during initialisation; after `_start`, the
code stream is stable.

### 3.6 Hardware performance counters

Standalone firmware does not get working `mcycle` reads merely by
executing the normal prologue above. Silicon tests with the ChaCha20
measurement harness showed that `mcycle` read back as zero until the
same vendor CSR enable sequence used by the stock firmware was added.

The stock firmware enables the core performance counters in
`initial_boot()` at original address `0x8400609e` with:

```asm
csrwi 0xbc0, 1
```

CSR `0xbc0` is therefore treated as the md32rv counter-enable control
for the hardware cycle counter domain. For standalone benchmark
firmware, use this minimal sequence before reading `mcycle`:

```asm
csrwi 0xbc0, 1      # vendor counter enable; required on NPUv1
csrw  0x320, x0     # clear standard mcountinhibit, if implemented
csrr  t0, mcycle
```

`0x320` is the standard RISC-V `mcountinhibit` CSR. Clearing it is kept
in the harness as a harmless compatibility step, but the required
operation on this NPU is the vendor CSR write to `0xbc0`.

This controls the architectural cycle counter (`mcycle`) used inside the
md32rv core. It is separate from the APB scheduler timers documented
under the `0x1EC10100` timer block and separate from the host-visible
clock-select diagnostic at `0x1FA201BC`.

---

## 4. Per-hart code (offsets `0x108` and onward)

At offset `0x108` the cores start to diverge. This is the `_start`
symbol — the per-hart entry that the rest of the firmware treats as
the C-style `main` predecessor.

### 4.1 `gp` register setup — offsets `0x108..0x10C`

```
0x108: auipc gp, 0x9a901       # gp = pc + 0x9a901000
0x10C: addi  gp, gp, -1504     # gp = 0x1E900B28 (after the high-bits wrap)
```

The pseudo-instruction `la gp, 0x1E900B28` expands to this auipc+addi
pair. The arithmetic: PC at `0x84000108` plus the immediate, then
subtract `1504` (`0x5E0`), modulo 2³², yields `0x1E900B28` — a point
in the middle of `.bss` in SRAM-C. From here all gp-relative loads
within ±2 KB cover the most-frequently-accessed globals.

### 4.2 Hart-id guard — offsets `0x110..0x118`

```
0x110: csrr  t0, mhartid       # t0 = this core's ID (0..3)
0x114: li    t1, 0
0x118: bne   t0, t1, 0x150     # if t0 != 0, jump to the post-bss-clear path
```

Only **hart 0** runs the BSS-clear loop below. Harts 1–3 skip ahead to
the stack setup. This matches the conventional pattern of "boot CPU
does all-cores' shared setup, then signals secondary CPUs to start
their per-core work."

### 4.3 Warm-boot detection — offsets `0x11C..0x12F`

```
0x11C: lui   t0, 0x1EC0C
0x120: addi  t0, t0, 0x140      # t0 = 0x1EC0C140 (MIB0)
0x124: lw    t1, 0(t0)          # read MIB0
0x128: li    t2, -1
0x12C: beq   t1, t2, 0x150      # if MIB0 == -1, skip BSS clear
```

A non-zero MIB0 (i.e., **not `-1`**) means "this is a real first boot
— clear BSS". MIB0 == `-1` indicates a warm boot (the driver having
restarted the cores without re-flashing the firmware, e.g., after the
watchdog), so the BSS is already initialised — don't trample it.

### 4.4 BSS clear — offsets `0x130..0x14F`

```
0x130: auipc t0, 0x9a900
0x134: addi  t0, t0, 504        # t0 = 0x1E900328 (.bss start)
0x138: auipc t1, 0x9a902
0x13C: addi  t1, t1, -456       # t1 = 0x1E901F70 (.bss end)
0x140: addi  t1, t1, 4
0x144: sw    zero, 0(t0)        # *t0 = 0
0x148: addi  t0, t0, 4          # t0 += 4
0x14C: bne   t0, t1, 0x144     # loop until t0 == t1
```

A simple 4-byte-stride clear of the `.bss` region: ~7 KB of zeroes
written word-by-word. Hart 0 only; harts 1–3 have already branched
past this code.

### 4.5 Stack pointer and call into init — offsets `0x150..0x163`

```
0x150: lui   sp, 0x30004
0x154: auipc ra, 0x0
0x158: addi  sp, sp, -16        # sp = 0x30003FF0
0x15C: sw    ra, 8(sp)          # save return address
0x160: jal   ra, 0x5cd0         # jump to init / main entry
0x164: li    a0, 0
0x168: li    a1, 0
0x16C: addiw s4, s4, 5          # padding
0x16E: j     0x16E              # infinite self-loop (returned-from-main trap)
```

All four cores execute this. Each loads `sp = 0x30003FF0` — the top of
the per-core SRAM region. The `0x30000000+` region is per-core SRAM (4
KB each, the cores see different physical backings via MPIS window 5).

`jal ra, 0x5cd0` jumps to the firmware's real `init` function at
`0x84005CD0`. From here the firmware takes over.

If `init` ever returns, control falls through into `0x164..0x16E` which
ends in `j 0x16E` (busy-loop). This is a safety net — the firmware is
designed never to return from `init`.

---

## 5. Multi-core synchronisation

The boot model leaves each core to start independently:

- All 4 cores run the prologue (offsets `0x00..0x108`) in parallel.
- Hart 0 runs the BSS-clear and main `init` call.
- Harts 1–3 skip the BSS clear and **also jump to `0x150`** — they too
  set their stacks and call `init`. The firmware's `init` function is
  hart-aware (reads `mhartid` internally) and behaves differently per
  core: hart 0 typically becomes the "primary" hart with the wifi
  fast-path scheduler, while harts 1–3 act as worker threads or
  per-queue handlers.

Inter-hart synchronisation after this point is via:

- **Hart-info MMIO registers** (e.g., MIB0 at `0x1EC0C140` and
  per-core variants).
- **Atomic operations** on shared SRAM (the `a` extension is present
  in the base ISA, so `lr.w` / `sc.w` / `amoxxx.w` are usable).
- **Per-core mailbox MMIO** (peripheral block hosted in the
  `0x20000000+` window, accessed via the MPIS-translated input space).

Specific synchronisation idioms (e.g., spin-loops on MIB registers
waiting for "core ready" handshakes) live in the firmware's `init` and
beyond — outside the scope of this boot doc.

---

## 6. Bring-up timing

From cold:

| Event                                          | Approximate timing                      |
| ---------------------------------------------- | --------------------------------------- |
| ARM driver opens `/dev/mem`, mmaps DRAM region | ~ms                                     |
| Copy 69 KB firmware to `0x84000000`            | ~µs                                     |
| Optional: copy `npu_data.bin` to `0x1E900000`  | ~µs                                     |
| Write BOOT_BASE × 4, BOOT_CONFIG = `0x0F`      | a few MMIO writes                       |
| Pulse BOOT_TRIGGER                             | single MMIO write                       |
| Cores released from reset                      | start fetching at `0x84000000`          |
| Prologue (0..0x108) executes                   | tens of cycles per core (parallel)      |
| Hart-0 BSS clear (~7 KB, word loop)            | ~2000 cycles                            |
| `jal init`                                     | one jump                                |
| Firmware-specific `init` to "ready"            | firmware-dependent (typically µs to ms) |

The reverse-engineering harness uses this same path — it replaces
`/lib/firmware/npu_rv32.bin` with a stub binary that includes the
same 0..0x108 boot prologue (copied byte-for-byte from the real
firmware), then has custom test code at `_start`. A `reboot` from the
Linux side is enough to make the kernel reload firmware and re-trigger
the NPU.

---

## 7. Failure modes observed during reverse-engineering

A few boot-related pitfalls noted from the harness work:

- **Rewriting the `rd` field of an unknown md32rv-custom op while still
  inside the prologue's MPIS-setup window will hang the core.** The
  silicon sub-decoder uses rd-field bits as part of the opcode for some
  encodings; mutating them during early boot before MPIS is fully set
  up can lead to a fetch-from-unmapped-address freeze. Always preserve
  the firmware's exact encoding when probing.

- **The trap counter must be explicitly zeroed** after the result-clear
  loop in any harness. If left at the clear-sentinel value (`0xDEADDEAD`),
  the first `mcause=2` trap will compute `slli x, 3 ; sw rs2, 0x410(x)`
  with `x = 0xDEADDEAD`, generating a wild store that recurses
  through the trap handler indefinitely. This was a silent-death failure
  mode in several early batches.

- **Cache-coherent reads from the ARM side require the firmware to
  flush** with `md.cache.op` op-code `24` (or any of the `flush+inval`
  variants 20/21/28/29/30). The firmware boot sequence above leaves
  D-cache _off_, so this isn't usually an issue — but if a stub
  harness enables D-cache to test cache behaviour and then forgets to
  flush, ARM-side `devmem` reads see stale DRAM content.

---

## 8. Quick reference

```
Cold boot — host ARM driver:
  load /lib/firmware/npu_rv32.bin → phys 0x84000000      (1 MB reserved)
  load /lib/firmware/npu_data.bin → phys 0x1E900000      (data section, optional)
  *BOOT_BASE_0..3 ← 0x84000000                            (0x1EC08020..0x1EC0802C)
  *BOOT_CONFIG    ← 0x0F                                  (0x1EC08004 — 4 cores)
  *BOOT_TRIGGER   ← (anything ≠ 0)                        (0x1EC08000)

Per-core, common prologue (0x00..0x108):
  8 × nop                                                  (offset 0x00..0x1C)
  MCTREN init                                              (offset 0x20..0x2C)
  md.dcache.inval.all + md.icache.inval.all                (offset 0x30..0x3F)
  MPIS windows 0..6                                        (offset 0x40..0xDC)
  MCTREN final = 0x139 + fence.i × 3                       (offset 0xE4..0x107)

Per-hart entry (0x108..):
  gp = 0x1E900B28
  if (mhartid != 0) goto stack_setup
  if (MIB0 != -1):    bss_clear()                          (0x1E900328..0x1E901F70)
stack_setup:
  sp = 0x30003FF0
  jal 0x84005CD0   (init)
  if init returns → busy-loop forever
```
