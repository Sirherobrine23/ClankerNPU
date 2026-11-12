# EN7523 NPU stall profiler

Build with `STALLPROF=1` (and leave `PROF=0` initially). In the OpenWrt feed use
`CLANKERNPU_STALLPROF=1` on your package build command. The profiler does
**not print in datapaths**. It records one entry per hart/event; the `STAL`
symbol exported in NDBG identifies the NPU-side address of the stats block.

Events: 0 GDMA polling; 1 total DMA copy; 2 hw_mutex_lock;
3 hw_mutex_lock_pri; 4 host_ring_submit; 5 pinode_drain (nonempty);
6 rxnode_drain (nonempty); 7 mbox_notify_host; 8 npu_printf;
9 PLIC interrupt handler; 10 buf_id_alloc_ring; 11 BA timeout sweep;
12 BA sequence scan; 13 TDMA TX submit; 14 pkt_forward (incl. queue full).

On EN7523, the debug symbol table starts at host address `0x1e906a00`.
Use `sys memory 1e906a00 80` to find `STAL` (`53 54 41 4c`), then
read its next word (NPU address) and map NPU address to host address:
`host_addr = npu_addr & 0x1fffffff`. Read with `sys memory host_addr SIZE`.
Alternatively, after the traffic ends, issue an **on-demand** NDBG STATUS
(mask bit 15) on a responsive hart:
`sys memwl 1e906850 8000; sys memwl 1e906848 0; sys memwl 1e90684c 6`.
This prints only nonzero `[STAL]` records in the UART and does not log during traffic.

The `STAL` block layout is 16 bytes of header (magic `STAL`, slow-cycle
threshold 64, hart count, event count), followed by 15 records per hart
(4 harts on EN7523). A record is 28 bytes = 7 words:

0. calls;
1. slow (duration >= 64 cycles);
2. failures;
3. maximum cycle latency;
4. sum cycles low 32;
5. sum cycles high 32;
6. caller return address for the maximum.

Average = `(sum_hi << 32 | sum_lo) / calls`. Clock is 333 MHz on the
xx230v, i.e. 333 cycles per microsecond. The total time of `GDMA_COPY`
contains `GDMA_WAIT`, so **do not sum** these categories.

A read may catch one hart updating its own counters, so repeat snapshots
for consistent results. These counters are non-atomic and diagnostic only;
`npu_printf` itself is also measured, but never invoked by the profiler.

A hart that blocks _indefinitely_ in MMIO cannot complete a timing record:
use NDBG hart heartbeats and `PROF=1` PC sampling from another hart to
locate it. Hardware mutex acquisition can stall an entire hart; the
`failures` field records acquisition failure when the call actually returns.

Disable all extra per-packet `npu_printf()` from development traces before
running throughput tests. Compare throughput with and without `STALLPROF=1`.
