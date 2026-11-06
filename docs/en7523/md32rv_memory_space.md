# md32rv Memory Space — Airoha EN7523 NPU

Per-region map of the address space visible to the md32rv RISC-V cores in
the Airoha EN7523 NPU cluster. Numbers are silicon-observed via the boot
prologue's MPIS configuration, device-tree reserved-memory declarations,
and harness probes against real hardware (Genexis E655-R2 board).

The NPU sees a 32-bit physical address space. Some of it is hard-wired
(on-die SRAM, boot/MMIO registers); some is mediated by the **MPIS**
(Memory-Protection and Interconnect System) translation windows that the
boot prologue programs into vendor CSRs `0x9c0` / `0x9dd` / `0x9df` /
`0x9e0..0x9e6` / `0x9f0..0x9f6`. ARM-host physical addresses can differ
for regions that overlap host DRAM.

---

## 1. High-level layout (NPU view, post-boot)

| Range                                           | Type                      | Description                                                                                                                                                                                                                                                                                                                                                                                                                       |
| ----------------------------------------------- | ------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `0x00000000` – `0x000FFFFF` (≈)                 | (vendor reserved)         | accessible via MPIS window 0; the boot prologue programs a control word (`0x360`) and base (`0x400`) here. Exact purpose unverified.                                                                                                                                                                                                                                                                                              |
| `0x1C000000` – `0x1C0000DF` (≈)                 | MMIO peripheral input     | mapped via MPIS window 1 (input region) — the CPU-side view of an external peripheral block.                                                                                                                                                                                                                                                                                                                                      |
| `0x1E800000` – `0x1E85FFFF`                     | **SRAM-A** (384 KB)       | shared on-die SRAM, accessible from all 4 cores. Code may be placed here at runtime; harnesses use higher offsets for result regions.                                                                                                                                                                                                                                                                                             |
| `0x1E860000` – `0x1E8FFFFF` (≈)                 | **SRAM-B**                | secondary on-die SRAM. MPIS window 2 maps `0x1E8003E0+` to here.                                                                                                                                                                                                                                                                                                                                                                  |
| `0x1E900000` – `0x1E90FFFF`                     | **SRAM-C** (64 KB)        | data SRAM. Hosts the firmware's `.data` and `.bss` sections (loaded from `npu_data.bin` via the ARM driver).                                                                                                                                                                                                                                                                                                                      |
| `0x1E900000` – `0x1E900327` (firmware-specific) | `.data` initialised       | function pointer/handler tables.                                                                                                                                                                                                                                                                                                                                                                                                  |
| `0x1E900328` – `0x1E901F6F` (firmware-specific) | `.bss` cleared at boot    | hart-0 zero-fills this range.                                                                                                                                                                                                                                                                                                                                                                                                     |
| `0x1E900B28` (firmware-specific)                | **gp** target             | RISC-V `gp` register set up here in the boot prologue (mid-`.bss`); enables gp-relative addressing for hot global variables.                                                                                                                                                                                                                                                                                                      |
| `0x1E908000` +                                  | result region (harness)   | not used by firmware — silicon probes scratch here during reverse-engineering.                                                                                                                                                                                                                                                                                                                                                    |
| `0x1EC08000`                                    | **BOOT_TRIGGER** (MMIO)   | ARM-host writes here to start NPU cores after configuring the per-core base addresses.                                                                                                                                                                                                                                                                                                                                            |
| `0x1EC08004`                                    | **BOOT_CONFIG** (MMIO)    | enable mask for cores; value `0x0F` = all 4 cores.                                                                                                                                                                                                                                                                                                                                                                                |
| `0x1EC08020` – `0x1EC0802C`                     | **CORE0..3_BOOT_BASE**    | per-core boot address. Firmware sets these to `0x84000000`.                                                                                                                                                                                                                                                                                                                                                                       |
| `0x1EC0C140`                                    | **MIB0**                  | per-core information / liveness register; hart-0 reads this once in the boot prologue (skips BSS clear if it reads `-1`, meaning "warm boot — already initialised").                                                                                                                                                                                                                                                              |
| `0x1EC11080`                                    | **PMC**                   | a performance-monitor register the firmware reads via the `0x67 f3=4` instrumentation path.                                                                                                                                                                                                                                                                                                                                       |
| `0x1EC12044`                                    | example MMIO              | a firmware-specific peripheral register (counter); seen written by sequences in `md.bfinsr` decode contexts.                                                                                                                                                                                                                                                                                                                      |
| `0x20000000` – `0x200000DF` (≈)                 | MMIO                      | mapped via MPIS window 3 input.                                                                                                                                                                                                                                                                                                                                                                                                   |
| `0x30000000` – `0x300000BF` (≈)                 | MMIO/peripheral input     | window 4 input.                                                                                                                                                                                                                                                                                                                                                                                                                   |
| `0x30000000` – `0x30003FFF`                     | **per-core SRAM / stack** | stack pointer is initialised to `0x30003FF0` (16 bytes below the top of this region).                                                                                                                                                                                                                                                                                                                                             |
| `0x30004000+`                                   | MMIO/peripheral           | window 5 output. The auxiliary MPIS CSR `0x9DD`/`0x9DF` are also set to addresses in this range, distinguishing read-vs-write access.                                                                                                                                                                                                                                                                                             |
| `0x80000000` – `0xBFFFFFFF`                     | **ARM-host shared DRAM**  | the full host DRAM range, reachable via MPIS window 6 (input base `0x80000000`). The `0x9E6 = 0x800003E0` value records only the _attribute_ on the input base — the window itself spans the whole 1 GB-class DRAM aperture, and silicon-observed firmware accesses range well into the high tens of MB (e.g. `0xBA9040D0` seen in a real run). The 1 MB code slot at `0x84000000` is carved out of this region (see next entry). |
| `0x84000000` – `0x840FFFFF`                     | **`npu_binary` (1 MB)**   | the firmware image's cached DRAM region. ARM-host device-tree reserves this slot (`reserved-memory/npu_binary@84000000`). Linux loads `/lib/firmware/npu_rv32.bin` here.                                                                                                                                                                                                                                                          |
| `0x84000000`                                    | firmware code entry       | NPU reset vector — every core starts executing here at offset `0`.                                                                                                                                                                                                                                                                                                                                                                |
| `0x84094000` +                                  | scratch (`PROBE_BUF`)     | not used by firmware. Reverse-engineering harnesses pre-fill this with sentinels.                                                                                                                                                                                                                                                                                                                                                 |
| `0x84095000` +                                  | scratch (`CACHE_BASE`)    | cache-op silicon probes' test buffer.                                                                                                                                                                                                                                                                                                                                                                                             |
| `0x84020000` +                                  | scratch (functions)       | ICACHE probe places self-modifying test functions here.                                                                                                                                                                                                                                                                                                                                                                           |
| `0xC0000000` – `0xFFFFFFFF`                     | DRAM (uncached alias)     | window 6 output base. The 1 GB DRAM aperture reappears here with un-cached attributes — same backing storage, different cacheability.                                                                                                                                                                                                                                                                                             |

> **Note on cache attributes** — the NPU has both an I-cache and a D-cache
> (controllable via the **MCTREN** CSR `0x7c0`). The boot prologue leaves
> them in a specific state — see §4 ("Cache attributes") below and
> [`md32rv_bootup.md`](md32rv_bootup.md).

---

## 2. ARM host's view of the same regions

Linux on the ARM host sees the NPU code DRAM region at the same physical
address `0x84000000` (the device-tree declares `reserved-memory/
npu_binary@84000000`, size `0x100000`). All MMIO accesses go through
`/dev/mem` + `devmem`.

```
$ devmem 0x84000000        # read first word of NPU firmware
0x00000013                  # = nop (the first of 8 nops in the boot pad)
```

The on-die SRAM regions (`0x1E800000+`, `0x1E900000+`) are also
host-visible via `/dev/mem` — this is how harness probes read back
NPU-written result regions. There is some attribute aliasing: writes from
the ARM side reach the same backing storage but bypass NPU dcache.

---

## 3. MPIS — Memory-Protection and Interconnect System

The MPIS provides input-base → output-base address translation windows
programmed into CSRs `0x9E0..0x9E6` (input bases) and `0x9F0..0x9F6`
(output bases), plus auxiliary CSRs at `0x9C0`, `0x9DD`, `0x9DF`. Each
window has a 12-bit attribute field embedded in the low bits of the
input/output value (`0xE0` for 224-byte ranges, `0x3E0` for larger, etc.
— the exact attribute encoding is undocumented, but the boot prologue
shows the values used in practice).

The boot prologue at offsets `0x40..0xDC` writes the following windows
(values come from the silicon-verified disassembly):

| CSR     | Value        | Interpretation                                                  |
| ------- | ------------ | --------------------------------------------------------------- |
| `0x9E0` | `0x00000360` | window 0 control: 0x360 (12-bit attrs) on a small base address  |
| `0x9F0` | `0x00000400` | window 0 size/output: 0x400                                     |
| `0x9E1` | `0x1C0000E0` | window 1 input base = `0x1C000000`, attr = `0xE0`               |
| `0x9F1` | `0x1E800000` | window 1 output base = SRAM-A                                   |
| `0x9E2` | `0x1E8003E0` | window 2 input = `0x1E800000`, attr = `0x3E0`                   |
| `0x9F2` | `0x1E860000` | window 2 output = SRAM-B                                        |
| `0x9E3` | `0x1E8600E0` | window 3 input = `0x1E860000`, attr = `0xE0`                    |
| `0x9F3` | `0x20000000` | window 3 output = MMIO 0x20000000 region                        |
| `0x9E4` | `0x200000E0` | window 4 input = `0x20000000`, attr = `0xE0`                    |
| `0x9F4` | `0x30000000` | window 4 output = `0x30000000` region                           |
| `0x9E5` | `0x300000C0` | window 5 input = `0x30000000`, attr = `0xC0`                    |
| `0x9F5` | `0x30004000` | window 5 output = `0x30004000` region                           |
| `0x9DD` | `0x30000000` | auxiliary control 1                                             |
| `0x9DF` | `0x30004000` | auxiliary control 2 (paired with `0x9DD`)                       |
| `0x9E6` | `0x800003E0` | window 6 input = `0x80000000`, attr = `0x3E0`                   |
| `0x9F6` | `0xC0000000` | window 6 output = host DRAM aliasing region                     |
| `0x9C0` | `0x0000007F` | MPIS global enable / mask (bits 0..6 set ⇒ windows 0..6 active) |

> The "attribute" sub-field of each input base appears to be a 12-bit
> bitfield encoding cacheability, write-allow, and the window size. The
> firmware never reads these back so the exact bit layout isn't visible
> from binaries alone, only the values it uses.

---

## 4. Cache attributes

Two on-chip caches sit between the cores and the address space:

- **I-cache** — enabled via MCTREN bit `0`. Invalidate with op-code `8`
  of `md.cache.op` (boot uses this once before MPIS setup).
- **D-cache** — enabled via MCTREN bit `1`. Invalidate with op-code `24`
  (global) or `25` (line-scoped); flush+invalidate with op-codes `20` /
  `21` / `28` / `29` / `30`.

The boot prologue's final write to MCTREN (`0x139`) **leaves D-cache
OFF** for the running firmware:

```
0x139 = 0001_0011_1001  →  bits  0 (I-cache),
                                3, 4, 5 (vendor micro-arch flags),
                                8       (vendor)
                          set; bit 1 (D-cache) CLEAR.
```

Implications:

- All firmware loads/stores go straight to memory (no D-cache hits).
- The `md.lb` / `md.sb` / `md.lh` / `md.sh` / `md.lw` / `md.sw` family
  at opcode `0x0b` is the "uncached load/store" path — but with D-cache
  off, these are equivalent to standard `lb`/`sb`/etc. The vendor uses
  them anyway, presumably for forward compatibility with D-cache-on
  configurations.
- I-cache IS on, so the firmware's code stream is cached. The boot
  prologue uses `md.icache.inval.all` (op 8) once before code stream
  takes effect; `fence.i` is used between subsequent code modifications
  (though the firmware never self-modifies in practice).

---

## 5. Reserved memory (device-tree)

From the ARM host's device tree:

```
reserved-memory {
    #address-cells = <1>;
    #size-cells = <1>;
    ranges;

    npu_binary@84000000 {
        reg = <0x84000000 0x100000>;   /* 1 MB */
        no-map;
    };
};
```

The `no-map` attribute keeps Linux from mapping this region into its
own page tables, leaving it free for the NPU. Driver `npu_load` reads
`/lib/firmware/npu_rv32.bin` and copies it byte-for-byte to physical
`0x84000000`, then prods the NPU boot registers (see
`md32rv_bootup.md`).

---

## 6. Worked-example: where the firmware lives

After boot, with the firmware image flashed and the NPU running:

| Address                       | Content                                               |
| ----------------------------- | ----------------------------------------------------- |
| `0x84000000`                  | first nop of the boot pad                             |
| `0x84000020`                  | start of MCTREN/MPIS-setup code                       |
| `0x84000108`                  | end of pad — per-hart init code begins                |
| `0x84000160`                  | `jal 0x84005cd0` — jump into init / main loop         |
| `0x84005cd0`                  | start of the `init` function (firmware-specific)      |
| `0x84010F50` (approx, varies) | end of firmware image                                 |
| `0x84094000`                  | unused 1MB-region tail (used by harness probes only)  |
| `0x1E900000`                  | start of `.data` section (loaded from `npu_data.bin`) |
| `0x1E900328`                  | start of `.bss`                                       |
| `0x1E900B28`                  | **`gp`** register's target                            |
| `0x1E901F70`                  | end of `.bss`                                         |
| `0x30003FF0`                  | top of hart-0 stack (sp initial value)                |

See `md32rv_bootup.md` for the step-by-step bring-up sequence that
establishes this layout.
