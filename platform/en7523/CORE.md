# EN7523 in the main ClankerNPU runtime

Apply this follow-up after the diagnostic base, mailbox and C-runtime patches.
The first main-runtime target is **EN7523/NOWIFI**. This is not an EN7523
WiFi/DMA offload implementation. Selecting an EN7523 WiFi chip fails the build
rather than using AN75xx DMA setup on unverified hardware.

## Build and install

```sh
make SOC=EN7523 WIFI=NOWIFI
make SOC=EN7523 WIFI=NOWIFI check disasm
```

For EN7523, NOWIFI is now the default. `make SOC=EN7523 WIFI=DIAG check`
retains the previous standalone diagnostic target.

Main-runtime outputs are in `build/EN7523_NOWIFI/`. Rebuild the host client in
platform/en7523/mailbox-host.c with the OpenWrt ARM compiler. Apply the matching
normal-loader kernel follow-up on airoha_all, remove
`airoha,diagnostic-firmware;` from the NPU node and install BOTH images:

- npu_rv32.bin -> /lib/firmware/airoha/en7523_npu_rv32.bin
- npu_data.bin -> /lib/firmware/airoha/en7523_npu_data.bin

Remove stale firmware-name overrides or point them to these two names.
The kernel recognizes the core-only image from its descriptor, confirms all
four harts and queries version 2.0 using the existing function-7 protocol.
WLAN/PPE consumers remain unavailable because their commands are not yet
implemented. Rebuild kernel/DTB/firmware together and cold reboot. The previous
diagnostic loader cannot select these images without its old property; the
previous descriptor-less core image is not supported by the new loader.

On the router:

```sh
./mailbox-host ping
./mailbox-host status
```

Expected: `ClankerNPU core ABI 2.0`, four cores, all states 1, advancing
heartbeats and C signatures 0x434f5200..0x434f5203 (COR + hart).
The client supports diagnostic record ABI 3 and main-core record ABI 4.
Wire ABI remains 1, transport function 7 and the bounded 64-byte request remain
unchanged. The version field distinguishes this runtime from diagnostic 1.1.
The kernel reports ClankerNPU core v2.0 after a successful startup handshake.
The firmware request count includes that first kernel request. Response word
13 now reports capabilities (zero for this core-only target).

Optional: `./mailbox-host trap 1` deliberately parks worker 1 until reboot.
A following status must show state 2 / cause 11 on that worker while the other
harts and mailbox advance. This now exercises the main trap_vector and
trap_dispatch rather than the standalone stackless diagnostic handler.

## Image descriptor

The first 0x108 bytes are unchanged. A jump at 0x108 skips eight little-endian
words at 0x10c: magic 0x4e414c43/0x0052454b (CLANKER plus NUL), descriptor ABI 1,
mailbox wire ABI 1, record ABI 4, capability mask 0, version 0x00020000 and a
reserved zero word. The linker asserts its offset and the software check
validates all fields. The descriptor lets the kernel choose the private
startup handshake without an experimental device-tree property. Capability
bits are intentionally zero until corresponding datapath support exists.

## Execution path

The build uses the normal root Makefile source list and its shared runtime
objects. Only the startup and linker script are selected for EN7523. The path
is core-start.S -> npu_init() -> en7523_core_init(), then core_dispatch() ->
en7523_core_loop(). Common npu_dbg_init(), mailbox_init(), mbox_isr(), dispatch
tables, debug records, globals and utility code are linked and used. This is
not a second assembly-only mailbox implementation.

The EN7523 platform hook intentionally bypasses the AN75xx reset pulses, port
power-down policy, TDMA/BME, tunnel/DBA, UART setup and WiFi paths. Interrupts
remain disabled; queue 0 is polled before calling the shared mailbox handler.
Timers, PLIC interrupt delivery and hardware mutexes are not initialized or
validated by this target. Some related functions remain linked through shared
error paths but are not invoked by supported core-only requests.
No raw debug command polling is enabled, avoiding unported DMA/debug commands.

The common mailbox transport only accepts blocking function-7 requests in
this target. Other function IDs, static buffers and asynchronous notifications
are rejected, including values outside the callback array. AN75xx behavior
is unchanged. No per-packet forwarding is claimed.

## Memory and synchronization

- Code: 0x84000000, below 0x840d0000; stock 0x108-byte MPIS/cache prefix retained.
- Stacks: four disjoint 16 KiB DRAM regions at 0x840d0000..0x840dffff.
- Shared initialized data/BSS: 0x1e900000 up to 0x1e906800 (linker checked).
- Common debug block: 0x1e906800..0x1e9077ff (layout from docs/debug.md).
- Progress records: 0x840f0000; packet: 0x840f1000, unchanged.

The existing reservation must cover at least the first MiB at 0x84000000.
The host loads initialized data before releasing any hart. A flag in .data is
reset by that load; hart 0 clears shared BSS, fences and releases secondaries.
Records are outside BSS and each hart owns its own slot. A second barrier
releases workers after the primary initializes shared debug/mailbox tables.
These barriers rely on the documented cache configuration and cold-boot
contract; warm restarts over old data are not supported.

A data-image marker is checked before shared runtime entry. Record state 3
with signature 0x45525204 indicates a missing/mismatched companion data image.
Do not mix the standalone diagnostic pair with the main runtime pair.
Progress records use ABI 4 and retain the previous offsets. They have the same
state values and terminal exception fields. Signature is now COR + hart, SP
fields reflect 16 KiB DRAM stacks, and hart 0's request counter is at offset 48.
Do not apply the old 4 KiB stack-address expectations to this image.
The common debug block exposes additional version, per-hart and mailbox
information; advanced raw debug commands remain inactive on this target.

## Validation

The main target builds without warnings. The supplied test-core.py executes
the linked RV32 image after the unchanged prologue with all four harts sharing
data memory. It checks BSS/init synchronization, stack separation, common debug
initialization, mailbox replies/rejections and a terminal worker trap followed
by a successful status request. MMIO writes are restricted in the model to
mailbox/boot-signature registers; no reset or datapath writes occur on these
paths. The model does not simulate MPIS, caches, hardware interrupts or the
interconnect. The preceding descriptor-less core runtime has passed the four-hart
heartbeat and status tests on EN7523 hardware. The new descriptor and kernel
startup handshake still require hardware validation.

All eleven existing AN75xx variants were rebuilt with fixed GITREV=x and their
22 firmware files compared byte-for-byte to the original build. They remained
identical. The previous EN7523 standalone diagnostic checks also passed.
