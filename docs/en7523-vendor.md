# EN7523 NPU vendor reverse engineering — first bring-up

This document records _observed_ facts from the EN7523 `npu_rv32.bin` /
`npu_data.bin` pairs. The original files are **not** ELFs and have no
symbol table. Source-like names below describe behaviour or correspond to
Clanker functions; they are not claimed to be recovered original symbols.

## Identical images

The supplied 13 pairs collapse to **seven distinct** `rv32+data` image
pairs. Important aliases:

- `en7523_7915` = `en7523_7916d_7623` = `en7523_xx230v` = `en7523_xx231v`.
- `en7523_7916` = `en7523_xx530v_v1`.
- `en7523_7592_7615` = `en7523_7592_7615_tr181`.
- `en7523_npu` = `en7523_heimgard_cx30`.
- `en7523_ex530v`, `askey_rtf8225vw`, and `old_askey_rtf8225vw` are separate.

The MT7915 reference has 60,264 bytes of code / rodata and 728 bytes of
initialized data; the MT7916 has 59,048 + 728 bytes. No original C source
is available from the raw image.

## Observed boot sequence (MT7915)

At **0x84000000** are eight **32-bit** NOPs (`13 00 00 00`), ending
at `0x8400001c`. At **0x84000020** the firmware configures the proprietary
RISC-V translation/cache CSRs. The complete listing is in the companion
`bootstrap-words.tsv`; the table below is extracted from vendor code, not
from the existing Clanker linker script.

| CSR           |                       value | status                                           |
| ------------- | --------------------------: | ------------------------------------------------ |
| 0x9E0 / 0x9F0 | `0x00000360` / `0x00000400` | observed                                         |
| 0x9E1 / 0x9F1 | `0x1C0000E0` / `0x1E800000` | observed                                         |
| 0x9E2 / 0x9F2 | `0x1E8003E0` / `0x1E860000` | observed; local SRAM window ends at `0x1E860000` |
| 0x9E3 / 0x9F3 | `0x1E8600E0` / `0x20000000` | observed                                         |
| 0x9E4 / 0x9F4 | `0x200000E0` / `0x30000000` | observed                                         |
| 0x9E5 / 0x9F5 | `0x300000C0` / `0x30004000` | observed                                         |
| 0x9DD / 0x9DF | `0x30000000` / `0x30004000` | observed                                         |
| 0x9E6 / 0x9F6 | `0x800003E0` / `0xC0000000` | observed                                         |
| 0x9C0         |                      `0x7F` | observed                                         |

Additionally, `0x7C0` is cleared/set around cache barriers. The word
`0x0005400F` appears twice (at `0x84000034`, `0x8400003c` in vendor MT7915);
upstream LLVM does not decode this instruction. It has been kept as a raw
`.4byte`, **not** reinterpreted as standard RISC-V `fence`. The precise
hardware semantics of these private opcodes remain unverified.

The code then builds a `gp` pointing into `0x1E900000` and checks
`MIB0 = [0x1EC0C140]` before zeroing `.bss`. Data references throughout
the MT7915 image use **0x1E900xxx** (not `0x3E900xxx`). Its SRAM buffer
base is **0x1E800000**, while the PCIe descriptors are passed to the host
as physical addresses. The vendor uses a `0x30004000` initial stack
pointer; Clanker retains independent per-hart DRAM stacks instead of
sharing that address. This is a deliberate deviation requiring hardware
verification.

The existing Clanker `crt0.S` only had `fence.i` / `fence` and lacked
all the above window setup. It also emitted compressed 16-bit `nop`s
when compiled for RV32IMC; the EN7523 patch explicitly uses `.4byte`
for the eight entry NOPs.

## PCIe / Kite observation (MT7915)

The firmware loads `0x1FA90038`, `0x1FA9003C` and
`0x1FA90030`, `0x1FA90034` when programming the two PCIe descriptor
windows (directly visible near `0x84002a28`). It uses physical
`0x1E800000`, `0x1E806020`, and `0x1E80C020` as window endpoints
for the Kite ring layout, confirming the **`0x6020` stride**.

In Clanker, the AN758x path uses a different PCIe window base; the
EN7523 build now selects the dedicated `0x1FA90030` registers instead.

The vendor image also contains a real Kite/BA implementation: RX ring
setup messages, piNode/rxNode debug strings, reorder handling, PCIe band
selection and WiFi mailbox handlers. That is sufficient to reuse the
existing Clanker Kite path for **MT7915/MT7916**, but it is **not proof**
that the BME / buffer manager register programming is identical to a
later AN75xx SoC.

## Four timers and multi-hart

The vendor log strings for timer validation list four timer indices.
The EN7523 target uses four timers (`18..21`) and four harts, and keeps
the non-AN7581 per-hart timer IRQ setup. This is part of the initial
port; testing on the EN7523 should verify IRQ counts and reload values.

## Decode files

Run from this tree with `clang`, `ld.lld`, `llvm-objdump` and Python 3:

```sh
python3 tools/decode_en7523.py /path/to/vendor-bins ./decoded-en7523
```

The decoder produces one directory per **unique binary pair**, with:

- `code.dis`: native RV32 instruction listing at load address 0x84000000,
  stopping before the probable string region.
- `bootstrap-words.tsv`: exact first 0x110 bytes as little-endian words,
  preserving unknown private opcodes.
- `data.tsv`: each initialized 32-bit slot, its NPU-side address,
  code/rodata pointer recognition or scalar value.
- `strings.tsv`: ASCII format strings with _absolute load addresses_.
- `calls.tsv`: direct branch/call targets and their referring PCs.
- `absolute-addresses.tsv`: LUI+ADDI address reconstruction for MMIO,
  local SRAM, and string references.

`<unknown>` in the disassembly denotes an instruction not recognized by
stock LLVM. The scripts do **not** claim to fully decompile the vendor
firmware to equivalent C; that would require custom instruction decoding,
function-boundary recovery, control-flow analysis and semantic validation.

## First bring-up checklist

Build with `SOC=EN7523 WIFI=MT7915` for xx230v and
`SOC=EN7523 WIFI=MT7916` for xx530v. On the host side, verify:

1. Boot hart enable bitmask, each hart's live PC and `MIB31` ready flag.
2. `gp`, `.data`, `.bss` and stack pointer after the first instruction;
   stop on **the first trap**, before attempting high-throughput traffic.
3. The NPU-visible window setup (especially CSRs `0x9E2` / `0x9F2`),
   and the PCIe windows (`0x1FA90030..3C`).
4. SRAM allocations stay **below `0x1E860000`** and local globals below
   `0x1E906800` (debug block reservation).
5. Wifi `SET_WAIT` mailbox sequence and packet ID pool, BME/BMGR IRQs,
   PLIC claim/complete, TDMA transmit queue indices.
6. Only then test MT7915 rx reorder, WED/PPE forwarding and `iperf3`.

### Known gaps / areas NOT validated by hardware

The EN7523 `HAS_BME` path, BMGR/BME register base, private cache opcode
semantics, clock and timer reload formula, and driver-host ABI require
runtime confirmation. The patch does not claim performance correctness.
Do **not** put it on the production routing datapath without diagnostics.

## Annotated behavioral reconstruction (not original C)

The following is intentionally **pseudocode**: the raw image contains no
symbols and the proprietary CSR writes have not been exhaustively decoded.
It summarizes the high-confidence operations visible in the disassembly and
cannot be compiled as a drop-in replacement for the vendor image.

```c
// NPU entry, vendor EN7523 MT7915 reference (conceptual)
entry_0x84000000(void)
{
    emit_8_nops_32bit();                  // reserved 0x20-byte reset header
    csr_clear(0x7c0, 0x103);             // private cache/address control
    csr_set(0x7c0, 3);
    emit_word(0x0005400f);               // proprietary operation, semantics unknown
    emit_word(0x0005400f);

    csr_write(0x9e0, 0x00000360);
    csr_write(0x9f0, 0x00000400);
    csr_write(0x9e1, 0x1c0000e0);
    csr_write(0x9f1, 0x1e800000);        // local SRAM mapping
    csr_write(0x9e2, 0x1e8003e0);
    csr_write(0x9f2, 0x1e860000);        // end of mapped window
    csr_write(0x9e3, 0x1e8600e0);
    csr_write(0x9f3, 0x20000000);
    csr_write(0x9e4, 0x200000e0);
    csr_write(0x9f4, 0x30000000);
    csr_write(0x9e5, 0x300000c0);
    csr_write(0x9f5, 0x30004000);
    csr_write(0x9dd, 0x30000000);
    csr_write(0x9df, 0x30004000);
    csr_write(0x9e6, 0x800003e0);
    csr_write(0x9f6, 0xc0000000);
    csr_write(0x9c0, 0x7f);

    /* further CSR toggles and instruction-cache barriers */
    init_gp_to_0x1e900000_region();
    init_core_stack_and_local_data();
    dispatch_hart_using_mhartid();
}
```

### Component map from strings and data references

| Observable family         | Evidence in vendor MT7915 blob                                                                  | Corresponding Clanker area           |
| ------------------------- | ----------------------------------------------------------------------------------------------- | ------------------------------------ |
| Boot/initialization       | `npu_init`, `core0_main` to `core7_main`, `MSTATUS`                                             | `crt0.S`, `npu_main.c`               |
| WiFi command mailbox      | `wifi_mail_set_wait_operation fail`, `wifi_mail_get_wait_operation fail`, `npuMbox_notify_host` | `npu_mbox.c`, `npu_wifi.c`           |
| Kite RX descriptor setup  | `npu_npu_init_rxd`, `npu_npu_init_rxd_5G`, `hw_rx_desc`                                         | `npu_wifi_init.c`, `npu_wifi_kite.c` |
| Block ACK reorder         | `baNode_array_init`, `ba_indicate_reordering_mpdus_le_seq`, `piNode->crsn`                      | `npu_wifi_ba.c`                      |
| DMA/queue management      | `qdma_init`, `pkt_queue_init`, `Ring Index error!`                                              | `npu_tdma.c`, `npu_wifi_fwd.c`       |
| Runtime timers/interrupts | `timer_isr`, `timer_init`, `UnKnow Interrupts`                                                  | `npu_timer.c`, `npu_plic.c`          |

These are _functional correlations_, not proof that a C function with the
same name implements the exact vendor semantics or mailbox ABI. For precise
control flow, use the `code.dis`, `calls.tsv` and `.data` pointer tables of
the variant that matches your router, then annotate individual functions.

## XX230v vendor boot-log cross-check (September 2025 capture)

Source: `boot_tplink_2025_09_05.log` and the shorter
`npu_an7523_boot.txt`. These are runtime observations for an XX230v v1
with an MT7915D, **not** claims about all EN7523 packages.

### Firmware load and multi-hart start

- ARM host copies `npu_rv32.bin` **60,264 B** into its mapped address
  `0xd0f00000`, and `npu_data.bin` **728 B** into `0xd0b80000`.
  Both `0xd...` values are ARM virtual ioremap addresses, **not** NPU
  physical addresses.
- NPU reports core frequency **333000000 Hz** and timer input
  **125 MHz**. The CPU PLL fallback table in Clanker does not express
  333 MHz; more importantly, CPU/4 would be ~83 MHz, *not* 125 MHz.
- Hart 0: BA/wcid pools, queue mutex, `qdma_init`, mailbox packet-buffer
  address. Hart 1: `core1_main`. Hart 2: timer ISR referencing
  **`0x1e903000`**, then `core2_main`. Hart 3: `core3_main`,
  `do npu_enq_deq_init`.
- Packet buffer address **0x8dff37c0**, allocation size **0x1600000**
  (22 MiB). This is a boot-specific host allocation passed to the NPU;
  **do not hardcode the address** into the Clanker firmware.
- The vendor prints `(core0_main) just return, because QDMA driver for ARM
  is not ready`; the boot continues afterward and WiFi rings initialize.
  This is a phase-specific return, not evidence that hart 0 crashed.

### PCIe/WiFi mailbox and RX descriptor geometry

Driver reports `DriverModel=1` and later gives the NPU two PCIe
register-block pointers, **0x200d4500** (band 0, 2.4 GHz) and
**0x200d4510** (band 1, 5 GHz). These are WiFi PCIe MMIO addresses,
not NPU SRAM ring addresses.

| Value | Vendor runtime | Meaning |
|---|---|---|
| NPU version | `MT7915D_NPU_V1.001.testv2` | WWAN/NPU firmware version |
| ring 0 / band 0 | `0x1e806020` | NPU RXD, 1536 entries |
| ring 1 / band 1 | `0x1e800000` | NPU RXD, 1536 entries |
| ring byte allocation | `0x6000` (24576) | `1536 * 16` bytes |
| allocation block type 1 | `0x0c040` | 2 rings + 0x40 B gap/padding |
| ring offset | `0x6020` | 24 KiB ring + 0x20 B gap |
| ring0 end (exclusive) | `0x1e80c020` | band 0 ring end |
| ring1 end (exclusive) | `0x1e806000` | band 1 ring end |

The log explicitly prints
`WIFI_MAIL_API_GET_WAIT_RXDESC_BASE info=1e806020` and
`... info=1e800000` after the two `npu_npu_init_rxd` commands.
The two rings are in the *first 0xC040 bytes* of NPU SRAM;
this does **not** establish the full SRAM capacity.

**Timing:** an early `host_notify_npuMbox timeout for core_id:0,func_id:0`
and `WIFI_MAIL_API_GET_WAIT_COUNTER fail!` occur **before** loading the NPU
firmware, so those first warnings cannot demonstrate a failed NPU boot.
The later GET_WAIT_RXDESC_BASE calls succeed.

**WED:** `wed_init(1)` assigns the working WED slot and successfully
initializes its rings. A later `whnat_probe() ... whnat is full` concerns
an additional `wed2` probe and must not be read as proof that the already
initialized WED1 failed. The log also records
`ECNT WED_WDMA_CFG_BASE = 0x1fa00000`. Be careful to distinguish that
register value from a mapped WDMA device address.

### Clanker reconciliation (revision 2)

1. The EN7523 board defaults are now `EN7523_CORE_CLK_MHZ=333` and
   `EN7523_TIMER_CLK_MHZ=125`, separately configurable in `npu_config.h`.
   The *timer reload formula* remains the old Clanker approximation;
   the log alone cannot prove its tick divider. Check timer IRQ counts on
   a real board before relying on timeout accuracy.
2. RXD initializers now reject zero, oversized, missing, or misaligned
   ring addresses instead of merely warning about oversized allocations.
3. The pipeline ring/classifier path is **not disabled** merely because its
   diagnostic text does not appear in this log. Additional disassembly
   and runtime traces are needed before changing core 2 scheduling.
4. NPU packet buffer addresses must come from the host mailbox; the
   observed `0x8dff37c0` is an example, not a firmware constant.
