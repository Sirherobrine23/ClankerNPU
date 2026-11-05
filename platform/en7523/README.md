# EN7523 NPU v1 diagnostic foundation

This is a boot diagnostic target, not an EN7523 WiFi offload implementation.
It deliberately does not call the AN75xx runtime or initialize DMA, QDMA,
PLIC, timers, hardware mutexes, UART or the mailbox. No C runtime is used yet.

## Build

From the repository root, with GNU RISC-V binutils installed:

```sh
make SOC=EN7523
make SOC=EN7523 check disasm
make SOC=EN7523 check REFERENCE=/absolute/path/to/stock/npu_rv32.bin
```

Outputs are in `build/EN7523_DIAG/`. `npu_rv32.bin` contains the executable
image. `npu_data.bin` contains a single zero word for loaders that require a
nonempty companion image; this firmware does not consume it. No proprietary
firmware bytes or binaries are included in the patch.

The base instructions use `rv32im_zicsr_zifencei`, with compression and linker
relaxation disabled. No A/C or standard bit-manipulation extensions are needed
by this diagnostic. The two custom cache instructions use explicit `.insn`.
A vendor compiler is not required to assemble this target.

## Boot contract

- Load the code at NPU-visible/host DRAM address `0x84000000`.
- Reserve at least `0x84000000..0x840fffff` from Linux. The current 4 MiB
  reservation also covers this range. Do not use a live Linux allocation.
- The companion data slot is `0x1e900000`; it is unused by this program.
- Use the EN7523 v1 host boot/reset sequence for four cores. This patch does
  not add or modify the Linux loader. The generic AN75xx reset path is not
  substituted for the EN7523 reset path.
- Start from a full reset. This target has no warm restart or stop protocol.
- This does not implement WLAN mailbox commands. It is not a drop-in for the
  mt76 demand-start firmware: that consumer will time out during WLAN setup.
  Use a diagnostic loader path before attempting a hardware boot.

The prologue at offsets `0..0x107` is reconstructed as instructions and matches
all five distinct EN7523 profiles previously extracted from the xx230v/xx530v
GPLs. It programs MCTREN, performs the two vendor cache operations and writes
MPIS CSRs 0x9e0..0x9e6, 0x9f0..0x9f6, 0x9dd, 0x9df and 0x9c0.
The fixed register writes are evidence from those images, not a complete
specification of MPIS cacheability, aperture sizes or protection semantics.
The code does not assume that setting MCTREN bits clears unrelated bits.

The four harts then disable maskable interrupts, set the documented stack
pointer and a linker-derived GP, install a terminal direct-mode trap handler,
and publish per-hart progress. They never access their stacks; the banking and
usable size of the documented `0x30000000` window still require hardware
validation. No shared BSS clearing or inter-core barrier is needed: every hart
owns its record and only initialized registers are used.

## Diagnostic records

The host reads four 64-byte records at `0x840f0000 + hart * 0x40`. This stays
within the smallest documented 1 MiB firmware reservation and is disjoint from
the executable image. Clear/publish ordering uses fences; the stock prologue's
D-cache configuration is retained. Hardware visibility is not yet tested.

| Offset | Field | Meaning |
|---|---|---|
| 0x00 | magic | `0x454e5055`, written after initialization |
| 0x04 | ABI | 1 |
| 0x08 | hart | 0..3 |
| 0x0c | state | 1 = polling, 2 = terminal exception |
| 0x10 | heartbeat | increments with polling; not a timer or cycle counter |
| 0x14 | mcause | trap cause, zero before a trap |
| 0x18 | mepc | faulting PC |
| 0x1c | mtval | trap value |
| 0x20 | mstatus | snapshot with maskable interrupts disabled |
| 0x24 | MCTREN | readback of CSR 0x7c0 |
| 0x28 | MPIS mask | readback of CSR 0x9c0 |
| 0x2c | sp | initial stack pointer, `0x30003ff0` |
| 0x30..0x3f | reserved | zero |

Read after an explicitly configured diagnostic boot:

```sh
sh platform/en7523/read-diag.sh
```

Compare two readings. All four records should have valid magic/ABI/hart/state
and changing heartbeats. Firmware must actually have been freshly loaded:
stale records from an earlier boot do not prove liveness. Record snapshots
are not atomic. On a fault, the handler records CSRs and parks; it does not
print, acquire locks, use the stack, retry the access or reset other harts.
A fault before record setup, or on a record write, may leave no valid record;
use host-visible core PCs in that case. The handler parks on a nested fault.

## Evidence and validation scope

References supplied by merbanam: `md32rv_bootup.md`, `md32rv_memory_space.md`,
`md32rv_isa.md`, `md32rv_isa_ops_spec.md`, and `md32rv-objdump.py`.
They report Genexis E655-R2 silicon tests; these tests have not been repeated
on the user's xx230v. Firmware-specific GP/BSS/init addresses from the notes
are not copied into this target.

The input notes have a 4 KiB/16 KiB stack-size discrepancy, and one boot note
suggests cache opcode 24 as a flush although the operations specification
identifies it as invalidate-only. This diagnostic neither uses the stack nor
enables a cached DMA path. Keep these questions open for later bring-up.

Next milestones: validate boot and exceptions on hardware, then add a bounded
mailbox implementation with an explicit diagnostic ABI. Port the remaining
platform facilities before attempting the WiFi datapath.
