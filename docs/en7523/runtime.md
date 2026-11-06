# EN7523 runtime and SDK mailbox

`make SOC=EN7523` builds the four-hart md32rv firmware using the common
startup, interrupt, timer, mutex, mailbox and debug sources. NOWIFI is the
only supported EN7523 build until its packet datapath is ported.

## Startup and memory

The original 0x108-byte cache/MPIS boot prefix is retained in `crt0.S`.
Execution continues directly into startup after this prefix. Hart 0 clears
BSS and releases the other harts through an initialized barrier word; each
hart receives a separate 16 KiB stack. `npu_init()` follows the common SoC
sequence: synchronize harts, configure local interrupt state, initialize shared
services on hart 0, release secondaries, run the common per-hart self-test and
publish MIB31. Hart 2 services timer 0 at a ten millisecond period.

EN7523 skips the AN75xx internal-bus reset, thread-manager writes and internal
boot-UART RX setup. Native controller priorities are initialized once; each
hart enables its own mailbox source and MCTREN interrupt delivery after shared
setup is published. Globals remain bounded by the EN7523 SRAM-C limit.

The common startup prints the banner, CPU frequency, source version, cluster
state and each hart's ISA/self-test through the host-configured SoC UART at
`0x1fbf0000`. The printf path uses hardware mutex 15. Output honors MIB21:
zero enables printing, nonzero suppresses it. The internal boot UART remains
untouched. The v1 cluster probe waits at most 500 ms for the host's sequential
hart release and a nonzero PC on the last hart before inspecting all four
native debug slots.

`link.ld` selects these reservations for EN7523:

| Region | Address |
| --- | --- |
| Code and read-only data | `0x84000000`, below `0x840d0000` |
| Four stacks | `0x840d0000..0x840dffff` |
| Initialized data and BSS | `0x1e900000`, below `0x1e906800` |
| Common firmware debug block | `0x1e906800..0x1e9077ff` |

There is no private image descriptor, fixed mailbox packet, per-hart progress
page, service page or function-7 protocol. The existing common debug block
continues to provide heartbeat and exception information as on AN75xx.
The EN7523 trap path records exceptions there and parks the faulting hart.

Native interrupt dispatch claims through CSR `0x5c0` and completes through
CSR `0x5c2`. Source IDs are zero-based. Unknown sources are disabled and
completed without printing. See [platform-services.md](platform-services.md)
for the register derivation.

## SDK control transport

The reference is the attached `release_bsp.tgz` profile
`UNION_EN7523_GLIBC_7915_7915_ActiveEthWan_demo/global_inc/modules/npu/`:

- `npuMboxAPI.h`: mailbox function IDs, blocking/completion/status bits,
  queue-8 host notifications and the eight callback slots.
- `wifi_mail_type.h`: `WIFI_MAIL_Data`, GET/SET operation types and command IDs.

Host requests use the ordinary function-0 WiFi mailbox with a dynamically
allocated DMA buffer. Its first word contains interface ID in bits 3:0 and
operation type in bits 7:4; the second word contains the command ID and the
remaining bytes contain its payload. With the current boot mapping and
D-cache disabled, EN7523 reads the host DRAM address directly. AN75xx address
aliases must not be substituted for this path.

The current NOWIFI image implements `GET_WAIT_NPU_INFO` (operation 3,
command 0), returning the same zero information word as the common kite
handler. It also implements GET_WAIT_NPU_VERSION (command 10), returning
`0x00070008` (7.8) as one little-endian word. Other WiFi commands return
NPU_MBOX_EOPNOTSUPP (3). Invalid
addresses, lengths, operation types and out-of-range function slots return
NPU_MBOX_EINVAL (2). An unregistered hart/function endpoint returns
NPU_MBOX_ENODEV (4). Legacy error/success values remain 0/1. Blocking completion uses CTRL3; asynchronous completion notifies
queue 8 using the common hardware mutex. Static-buffer registration remains
unsupported and returns EOPNOTSUPP. The host must publish CTRL0/1/3 and fence before incrementing
CTRL2, which raises the mailbox interrupt.

The legacy EN7523/MT7915 SDK GET_WAIT table ends at command 5,
`WCID_DBG_COUNTER`. ClankerNPU extends it to the current kernel IDs in
`npu_wlan.h`; the kernel no longer blocks newer GET commands on v1. The
shared dispatcher returns EOPNOTSUPP for missing handlers, including the
support-map and MDC-lock queries. See [mailbox.md](../mailbox.md) for the
complete command map and older SDK numbering differences.

## Kernel loading and installation

The NPU driver loads firmware and starts the cores from its own probe.
Startup is independent of Ethernet registration. If firmware files are not
available yet, the driver retries automatically every second; pending retries
are cancelled before driver resources are released. Other startup errors are
returned from probe. No ClankerNPU-specific preparation function or handshake
is required.
Use `airoha,en7523-npu` and retain the existing reserved memory. The binary
reservation must cover both the linked code and the fixed stacks.

Install the matching pair from `build/EN7523_NOWIFI/`. The current local
kernel filenames are:

- `npu_rv32.bin` as `/lib/firmware/airoha/npu_rv32.bin`.
- `npu_data.bin` as `/lib/firmware/airoha/npu_data.bin`.

A device-tree `firmware-name` property overrides these names. Ensure its code
and data names select the same build. Replace both files together and reboot.
The normal version query reports 7.8 with this firmware; it remains optional
for older firmware that returns EOPNOTSUPP.
Successful loading and core release do not establish packet-offload support.

## Validation and remaining work

```sh
make SOC=EN7523 GITREV=x check disasm
make all-variants GITREV=x
```

`tests/en7523-core.py` executes the linked image with dirty BSS and four hart
models. It checks the boot prefix, independent stacks, SDK GET_WAIT requests
at different buffer addresses, the version query across interface IDs,
asynchronous notifications, unsupported commands,
malformed requests, native IRQ completion, timer W1C and exception isolation.
It checks exact error codes 2/3/4 in blocking responses and asynchronous
queue-8 notifications, including stale status bits and payload guards.
It also verifies the common startup logs, four passing self-tests and mutex
ownership during UART output. A quiet, staggered-boot run checks that MIB21 is
preserved and that secondary harts can start after primary initialization
begins. Virtual cycle samples exercise delay loops without modeling real
clock timing. Writes to the removed telemetry pages, AN75xx reset/thread
registers or the internal boot UART fail the test.

This model does not simulate MPIS translation, cache behavior, interconnect
timing or real packet traffic. Previous hardware evidence applies to the
superseded v2.1 bring-up image; this image requires a new hardware boot.
TDMA, host-adaptor rings, MT7915 reorder and PPE offload remain to be ported.
Their unimplemented commands must keep returning failure.
