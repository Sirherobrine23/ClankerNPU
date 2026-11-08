# EN7523 vendor MT7915 vs ClankerNPU: validated static differences

Primary evidence: `input/npu_bins/en7523_7915_npu_rv32.bin` and the current
`clanker.tar(1).gz` provided in the conversation. Vendor symbol names in this
note are descriptive analyst labels, not recovered ELF symbols.

## Evidence categories

- **Observed**: instruction bytes, MMIO addresses and control-flow operations
  directly identifiable in `recovered/en7523_7915.annotated.S`.
- **Clanker source**: directly visible in `clanker_reference/npu_qdma_en7523.c`.
- **Inferred**: explanatory C abstractions, packet ownership and runtime impact;
  these need testing before they can be considered equivalent to vendor source.

## PPE return path (`0x84003004`)

| Behavior | Vendor EN7523 MT7915 | ClankerNPU | Evidence |
|---|---|---|---|
| Pending FIFO entries | Reads `0x1fb50fe4 & 0xffff` | Same, capped at 64 entries per poll | vendor `0x84003016–0x84003022`; Clanker `ppe_return_poll()` |
| Empty/invalid result | Checks `DATA` bit 31 and exits | Same | vendor `0x84003044–0x84003048` |
| Return-path selection | Extracts **only `DATA` bit 16** using `md.extui` | Uses `!(DATA >> 16)` to classify CPU exceptions | vendor `0x8400304c`; Clanker source |
| `PPE_RETURN_INFO` read | Only when bit 16 is clear | Read for every non-invalid return | vendor `0x84003050–0x84003052`; Clanker source |
| CPU reason | `md.bfextr16(INFO,5)` = `(INFO >> 16) & 31` | `(metadata >> 16) & 31` | vendor `0x8400307e`; Clanker source |
| Hash | `md.maskl(INFO,15)` = low 15 bits | Preserved as part of metadata; not separately decoded there | vendor `0x84003072` |
| ACK | `0x40000000` to `0x1fb50fe0` | Same | vendor `0x840030b4`; Clanker source |
| Event ownership | Calls proprietary callback then completion helper | Marks `PACKET_PPE_DONE`, completes only when QDMA+PPE ownership allows | Vendor callback body incompletely reconstructed; Clanker source |

**Why it matters:** if `DATA[30:17] != 0` while `DATA[16] == 0`, the two
classification conditions are not equivalent. There is no verified sample
of that hardware state in the provided logs. Instrument the condition,
do not change it blindly. Reading `INFO` for successful packets could
also be unnecessary MMIO work, but no throughput impact is demonstrated.

```c
/* Proposed Clanker diagnostics only; no control-flow change. */
if (!(data & BIT32(16)) && (data & 0x7ffe0000u))
    ppe_data_other_upper_flags++;
```

## QDMA TX ring (`0x84003180`)

| Property | Vendor | ClankerNPU |
|---|---|---|
| Setup source | Firmware reads `QDMA_TX_BASE` and builds its own bookkeeping list | Firmware validates host-prepared ring and manages free IDs |
| Descriptor stride | 32 bytes | 32 bytes |
| Ring population | Explicit loop of 1024 iterations | Expects 1024 descriptors configured by host |
| Software node stride | 12 bytes | Separate `free_ring`, ownership records and lookup tables |
| Completion events | IRQ ring and separate PPE return FIFO | IRQ completion + PPE return FIFO tracked before final buffer recycle |

The vendor's initial descriptor `word1` updates are visible at
`0x840031fe–0x84003212`, but the entire layout should not be copied
without a byte-for-byte descriptor dump. Check the host DMA addresses,
ownership bits and IRQ-pop progression first.

## QDMA TX submit (`0x840030c0`)

The vendor writes descriptor words, executes a fence at `0x84003148`,
then writes the selected descriptor index to `QDMA_TX_CPU` at
`0x8400315a`. The analogous operation in Clanker is
`en7523_qdma_submit()` (`npu_qdma_en7523.c`).

## QDMA IRQ poll (`0x84002ecc`)

The vendor uses 128 as an IRQ processing bound and checks an IRQ-ring
entry type against `0x00060000` after masking the index to 12 bits.
Clanker limits one poll pass to 64 entries. These differences alone do
not indicate a bug; they can affect batching under sustained load.

## Boot / stack

The common 0x108-byte boot prefix is identical on the reference EN7523
images. After that, vendor startup initializes `sp` from `0x30004000`
for the private SRAM window, whereas Clanker allocates per-hart stacks
in reserved DRAM. Whether using SRAM changes throughput or improves
cache behavior is unmeasured. Do not switch stacks without confirming
per-hart mapping and interrupt-stack requirements.

## Host receive ABI / WED

Clanker's host ring layout is `clanker-kite-24`, which is not the vendor
16-byte host ring. Incompatible initialized data, callbacks, completion
queues and software structures mean the vendor code cannot be transplanted
as a binary or source drop-in.

These NPU binaries do not by themselves decode the Linux WED error
`0x00080000`. The WED driver and WDMA register definitions have to be
analyzed separately. CPU reasons `0x0d`, `0x0e`, `0x0f` originate from
PPE flow lookup, not a RISC-V CPU exception.

## Controlled verification plan

1. Log the counts of `DATA[16] == 0 && DATA[30:17] != 0`, plus the usual
   `reason`, `hash` and successful-completion count.
2. Compare TX descriptor bytes and ownership flags at kernel initialization,
   after the first flow and under sustained `iperf3`.
3. Compare PPE completion and IRQ-pop backlog with Wi-Fi upload and download.
4. Separately instrument WED0/1 status `0x00080000` in the host driver.
5. Only then consider replacing Clanker's selected `DATA` logic or the
   host-owned QDMA ring initialization contract.
