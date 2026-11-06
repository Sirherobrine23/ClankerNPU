# EN7523 NPU v1 diagnostic mailbox

Follow-up to the diagnostic boot foundation. The four harts retain the
byte-identical stock 0x108-byte cache/MPIS prefix and stackless trap capture, then call a freestanding C initializer.
Hart 0 polls hardware mailbox queue 0; other harts publish heartbeats and can run an explicit terminal trap test.
No PLIC, UART, timers, mutexes, DMA or WiFi datapath is initialized.

## Build

```sh
make SOC=EN7523
make SOC=EN7523 check disasm
make SOC=EN7523 check REFERENCE=/absolute/path/to/stock/npu_rv32.bin
```

GNU RISC-V GCC and binutils are required for the firmware. Outputs are in
`build/EN7523_DIAG/`. The companion data file now includes a C initialization sentinel. Replace both
firmware files together; a missing sentinel parks the core with state 3.
Build the host client with the OpenWrt target C compiler, not the RISC-V compiler:

```sh
"$TARGET_CC" -std=c11 -O2 -Wall -Wextra mailbox-host.c -o mailbox-host
```

The supplied `check` executes the built RV32 diagnostic body in a small software
instruction model, testing success/error replies and worker-hart isolation.
This model is not a simulator of MPIS, cache, mailbox MMIO or RISC-V privilege
hardware. It does not replace silicon tests.

## Host loader contract

The companion kernel patch is based on the previously supplied demand-start
and xx530v profile source snapshots, not a fresh checkout of the current branch.
It selects the diagnostic image at Ethernet readiness and rejects WLAN/PPE
consumers and normal driver mailbox sends. No legacy WLAN version request is
sent. The dedicated client owns queue 0 in this mode.

Add to the NPU node, retaining its existing SoC-compatible and memory regions:

```dts
&npu {
        airoha,diagnostic-firmware;
};
```

Install the new files as:

- `/lib/firmware/airoha/en7523_diag_npu_rv32.bin`
- `/lib/firmware/airoha/en7523_diag_npu_data.bin`

The loader requires EN7523/v1 and a binary reservation starting at 0x84000000
with at least 1 MiB. Do not change compatible/version solely to bypass a
rejection: resolve the running kernel's incorrect v2 profile first.
Code occupies the beginning of this reservation; records are at 0x840f0000,
and the mailbox packet at 0x840f1000. Both pages must remain reserved from Linux.
The current 4 MiB binary reservation covers them. Cold reboot after rebuilding
kernel, DTB and firmware. Never switch from an active offload image in place.

Without this loader isolation, do not run the client concurrently with the
kernel NPU mailbox transport. Its flock serializes only other client processes.
The kernel debugfs mailbox counters do not count these direct client requests.

## Test on the ARM host

```sh
sh read-diag.sh
./mailbox-host ping
./mailbox-host status
sh read-diag.sh
```

The client first requires all four records to advertise diagnostic record ABI 3
and running/trapped state, and checks hart 0 heartbeat changes. It maps /dev/mem with
O_SYNC, uses 64-bit physical offsets and fences around the request/response,
and bounds each poll to 500 ms. /dev/mem must be enabled by the running kernel.
Expected response: diagnostic ABI 1.1, four cores and a request counter.
Ping/status return the same version/state snapshot without changing core state. A timeout or an abandoned request requires reboot;
the client does not reset the cluster or retry automatically.

## Wire contract

Transport: queue 0 at NPU/host MMIO 0x1ec0c030, function ID 7, blocking bit 0,
DONE bit 1, status bits 4:2 (1 success, 0 rejection). Length is exactly 64 bytes,
pointer exactly 0x840f1000; static-buffer mode is rejected. Requests to any
other function, pointer or size are rejected without dereferencing the pointer.
The pending queue-0 interrupt bit is acknowledged by W1C; no interrupt is used.
Only the final CTRL3 write publishes completion, after a fence.

| Word | Field |
|---|---|
| 0 | magic 0x4e50444d |
| 1 | wire ABI 1 |
| 2 | host sequence, preserved |
| 3 | command: 0 ping, 1 status, 2 terminal worker ECALL |
| 4 | response status: 0 success, 1 unsupported command |
| 5 | firmware version 0x00010000 |
| 6 | hart count 4 |
| 7..10 | state of harts 0..3 |
| 11 | total handled requests, including rejected frames |
| 12 | target hart for command 2 (1..3 only) |
| 13..15 | reserved |

Diagnostic records retain the base layout: 64 bytes per hart at
0x840f0000 + hart*64. Offsets 0 magic 0x454e5055, 4 record ABI (now 3),
8 hart, 12 state (1 polling, 2 trapped, 3 C self-test failed), 16 heartbeat, 20 mcause, 24 mepc,
28 mtval, 32 mstatus, 36 MCTREN, 40 MPIS mask, 44 sp, 48 request counter
(hart 0 only), 52 C signature, 56 C-entry SP, 60 worker command. Snapshots are not atomic.
Stack pointers are now in reserved DRAM. Terminal traps
still park without printing or automatic restart.

Firmware version replies are not proof of a working datapath. Hardware
validation of polling, W1C acknowledgement, DRAM visibility and malformed
requests remains outstanding. The initial boot foundation has been observed
running on all four cores; that does not validate this new mailbox code.

## C runtime and stack smoke test

The linker reserves four disjoint 4 KiB stacks at 0x840e0000..0x840e3fff.
Each entry stack pointer is 16-byte aligned: 0x840e1000, 0x840e2000,
0x840e3000 and 0x840e4000. Code must end before 0x840e0000; diagnostic
records and packet remain at 0x840f0000 and 0x840f1000.

Every core calls en7523_runtime_init(), which checks an initialized .data
sentinel, stack position/alignment, a volatile local array in a nested C call,
and a bottom-of-stack guard. The guard is a smoke test, not ongoing overflow
protection. On success record offset 52 is 0x43525400 | hart. Offset 56 is
the SP observed on entry to C after its compiler-generated frame, not the
stack's deepest watermark. Offset 44 remains the initial stack top.
State 3 indicates C self-test failure, with 0x45525200 | error at offset 52
(error 1: hart/alignment/data, 2: stack range, 3: frame/guard). Shared BSS is not supported yet: a
linker assertion rejects any image requiring it. The C code only uses
initialized read-only shared data, per-hart records and per-hart stacks.

## Explicit terminal trap test

With updated firmware and client, first run ping and status. Then optionally:

```sh
./mailbox-host trap 1
./mailbox-host status
sh read-diag.sh
```

This deliberately stops hart 1 until reboot. Targets 1, 2 or 3 are accepted;
hart 0 is rejected to preserve mailbox service. The request is asynchronous:
look for state 2 and mcause 11 (M-mode ECALL) on the selected worker. mepc
should point to the ECALL instruction, mtval is hardware-provided. The
handler records the fault and parks without using its stack or printing.
The other harts must continue advancing. Status remains available with
trapped workers; a new trap request for an already trapped worker is rejected.
No automatic recovery or timer/interrupt runtime is introduced by this patch.

The software instruction model covers the compiled C path on all four harts,
failure with a missing data sentinel, worker request flags and ECALL capture.
It still does not model real cache, MPIS, memory interconnect or trap hardware.
Runtime and fault injection require new hardware validation, beyond the
previous boot/mailbox tests. Keep the airoha_all diagnostic loader patch;
there is no additional kernel change for this runtime step.
