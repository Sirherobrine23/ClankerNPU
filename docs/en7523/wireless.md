# EN7523 MT7915 receive transport

The XX230v's MT7905DEN/MT7975DN combination exposes PCI ID `14c3:7915`.
It uses the MT7915 path in mt76. Build its NPU image with:

```sh
make SOC=EN7523 WIFI=MT7915
# Optional MT7916 image (not validated on hardware):
make SOC=EN7523 WIFI=MT7916
```

Install the matching `build/en7523_mt7915_npu_rv32.bin` and
`build/en7523_mt7915_npu_data.bin` under `/lib/firmware/airoha/`. The board's
`firmware-name` selects these two files. The NOWIFI image cannot provide RX
rings, and neither the stock EN7523 16-byte ABI nor the old experimental
`sdk-24` mt76 profile matches this transport.

The OpenWrt integration adapts patches 173/174 from `airoha_npu_v2`. It builds
`CONFIG_MT7915_NPU` on EN7523 and selects the profile on the XX230v PCI node:

```dts
airoha,npu-rx-abi = "clanker-kite-24";
airoha,npu-wed-tx;
airoha,npu-driver-model = <1>;
airoha,npu-port-type = <1>;
```

`npu_enable=0` keeps mt7915e on its ordinary receive path. When selected, the
transport requires a little-endian EN7523 with single-HIF
MT7915 devices. It checks the normal version query and firmware ring
availability before publishing DMA storage. It uses the normal NPU provider probe, without demand-start or
an Ethernet startup call.

### Separate PCIe radios

The Askey RTF8225VW board identified as MT7975N/MT7975AN exposes two
independent `14c3:7915` functions. The DTS assigns queue 0 to PCIe0 and
queue 1 to PCIe1; queue selection does not depend on the configured RF band.
Each function uses one NPU queue, independently of its local hardware band.
Its PCI node adds `airoha,npu-rx-queue = <0>` or `<1>` to the profile above,
with `airoha,npu-port-type` set to its own PCIe port. DBDC nodes omit the
queue property and claim both queues. Overlapping claims are rejected.

Firmware 7.9 is required for separate radios. It configures the PCIe port
per queue and allows either host ring to start before the other exists.
The shared packet-pool address may be repeated unchanged by a second
consumer; changing it after ring preparation is rejected. A DBDC driver
still accepts firmware 7.8 and publishes both rings before starting them.
Firmware 7.9 also supports the XX230v DBDC configuration.

## Kite BA and protected RX (incremental integration)

The EN7523 MT7915 RX transport feeds the shared Kite classifier in
`npu_wifi_rx.c` and the common BA engine in `npu_wifi_ba.c`. The
EN7523-specific adapter parses the MT7915 RXD, owns its PCIe buffers
and submits eligible **unprotected** IPv4 packets to QDMA/PPE.

A protected QoS frame with an active BA session now follows Kite's
sequence/reordering rules. The NPU still copies the complete RXD and
payload to mt76/mac80211: it does **not** set `REORDERED`,
`PN_VALID` or `BYPASS_BA` in the host descriptor. This avoids
asserting that mac80211 duplicate/PN checks have already run.

Only metadata validated by the radio and replay/key state synchronized
with mt76 can authorize protected direct PPE forwarding. The current
firmware does not implement this contract. `SEC_MODE` or the 802.11
Protected bit alone is not a replay-validation indication.

`SET_KEY` still retires the NPU BA session; if a key changes while
RX BA is active, the session remains host-managed until re-established.
This is a correctness-first shared-Kite integration, not yet WPA direct
PPE offload.

## Receive path

Hart 0 initializes the common SRAM allocator and software buffer-ID pool.
Two 1536-entry, 16-byte radio RX rings occupy SRAM-A at `0x1e800000` and up,
with the second band offset by `0x6020`. The common allocator determines the
actual base. The packet reservation must hold 5600 buffers of 4096 bytes.
PCIe windows use the v1 SCU registers, not AN75xx aliases.

Hart 1 drains queue 0; hart 2 drains queue 1 and services the common timer.
A worker detaches each completed packet buffer and refills the radio descriptor
with a different ID. Buffers remain owned until host copying finishes, or both
QDMA completion and the native PPE buffer return have arrived. Scattered radio buffers are chained;
only a complete frame enters parsing or the host transport.

The native direct path accepts eligible routed IPv4 ingress for authorized
AP stations with open security, WCID 1..150, TID 0..7 and an active BA
session. Without BA, duplicate filtering remains with the host. Source MAC,
header translation and protocol validation also gate native forwarding.
Multicast, ARP, EAPOL, unsupported translations and multi-buffer frames
retain the complete RXD and use the normal host path. The EN7523/MT7916 image builds, but its RXD and PCIe transport still
require hardware validation before use.

The Kite BA engine sorts MPDUs by sequence number and drops duplicates.
It does not validate CCMP/GCMP packet numbers. For protected frames, the
NPU never sets the host `PN_VALID` or `REORDERED` claims and never submits
them to PPE; mt76/mac80211 retains replay and key-generation validation.
BA timeouts use the configured tick-based flush interval. Key installation
and station removal invalidate the NPU BA state before WCID reuse.

Firmware 7.14 keeps eligible frames in their original packet buffer, without
inserting a private VLAN. TX ring 6 uses the vendor NPU message words:
`0x80000000 | buf_id << 14` and `0x7f4087ff`. Hart 3 drains completion queue 1
and the PPE return FIFO (`INFO=0x1fb50fdc`, `DATA=0x1fb50fe0`,
`COUNT=0x1fb50fe4`). It acknowledges the current FIFO head with bit 30 of
DATA only after reading its metadata. A packet remains owned until both its
QDMA descriptor has completed and the PPE has returned its buffer ID.
Descriptor indices can be recycled independently of packet buffers.

A successful forwarding return frees the buffer without notifying the ARM.
An exception is queued to its original RX hart, which copies the Ethernet
frame to its host ring and carries the FoE hash and CPU reason. mt76 hands
these frames to `airoha_npu_rx_skb()` on the original AP netdev; the PPE can
bind the learned tuple before GRO. These exceptions bypass the mt76 ingress
hook to avoid re-injection into the PPE. Raw control, unsupported and
unvalidated frames still take the original mac80211 path. A duplicate
old-header keepalive is discarded because the original was already forwarded.

STA/key generations reject delayed exceptions across rekey or station reuse.
STOP also waits for native PPE ownership to retire; a failed STOP keeps host
DMA storage pinned. Full host rings drop exceptions without overwriting an
owned descriptor. An Ethernet/QDMA resource reset requires a reboot.

The slot-5 control path remains a Clanker extension. Until the first TC flow
setup has initialized the PPE, admitted frames retain the raw host fallback. EN7523 uses 80-byte FoE
entries with 256 SRAM entries and a 16K DRAM table. WED TX is a separate
WiFi egress path; the native RX path does not remove host work on WiFi TX.

Raw exceptions use coherent GDMA channel 0 or 1 for their queue. Firmware 7.11's
32-descriptor notification batching is retained, with immediate flush when the
radio empties and no idle doorbells. STOP waits through final publication.
CPU descriptor accesses retain the v1 uncached boot mapping. EN7523 does not
use TDMA, BME, hardware BMGR or the AN75xx bridge.

## Host descriptor and control

Each host ring has 512 entries of 24 bytes. Words 3..5 belong to the host:
word 3 is the 4096-byte destination buffer address; words 4/5 are its cookie.
The firmware preserves them. Word 0 is published last, before the DMA index:

| Field | Bits |
| --- | --- |
| Done | word 0, bit 0 |
| Total raw frame length | word 0, bits 14:1 |
| Bytes in this descriptor | word 0, bits 28:15 |
| Last fragment | word 0, bit 29 |
| Fragment number, starting at 1 | word 1, bits 28:26 |
| Fragment count | word 1, bits 31:29 |

Word 2 carries NPU checks: bit 31 means BA processing is complete, bit 30
means CCMP PN validation is complete, and bit 29 bypasses host BA processing
for a fragment. Bit 28 identifies a validated Ethernet PPE exception: word 1
bits 15:0 carry the vif index, while word 2 bits 14:0 carry the hardware FoE
hash and bits 20:16 the CPU reason. The host handles that format before
attempting to decode a radio RX descriptor. These frames are never submitted
to mac80211 a second time. Other raw frames have zero. mt76 consumes these bits only on its
NPU queues and avoids repeating completed checks. Translated A-MSDU exceptions
are delivered as individual Ethernet frames because other members may have
taken the direct path. Frames up to 16383 bytes can span multiple host entries.
An owned/full entry drops the incoming frame without overwriting host data.
The driver copies completed fragments into an skb before clearing Done.

The supported SET IDs are PCIE_ADDR (0), DESC (1), NPU_INIT_DONE (2),
BA_WIN_SIZE (4, extended EN7523 payload), DRIVER_MODEL (5), PKT_BUF_ADDR (8),
IS_FORCE_TO_CPU (12), PCIE_PORT_TYPE (14), HWNAT_INIT (30, QDMA resource check),
EN7523_STA (34) and EN7523_KEY (35). Value 0 of IS_FORCE_TO_CPU enables the
admission checks; it cannot bypass authorization or replay validation.
Supported GETs include NPU_INFO (0), DBG_COUNTER (3, four per-queue counters),
RXDESC_BASE (4) and NPU_VERSION (10). The counters are QDMA submissions, raw
host frames, rejected PNs and full/unavailable QDMA fallbacks. They are shown
in mt76's `npu_rx` debugfs file. See [mailbox.md](../mailbox.md) for payloads.

The host prepares its radio rings, publishes all host rings owned by that
device and only then sends NPU_INIT_DONE=1 for each queue. Separate radios
can probe in either order. Value 0 disables only that worker and waits
until its in-flight copy has finished. The worker publishes Busy before
checking Enable, so a completed STOP prevents later writes to freed host
buffers. The driver retains storage if STOP fails. Radio recovery or rebinding
a prepared radio still requires a reboot; it does not reuse a retired ring.
Cleanup masks, stops and clears only the device's queues. Retired queues
remain unavailable until reboot, including after a failed probe.

## Validation

Firmware 7.14 was RAM-booted on the Askey RTF8225VW on 2026-11-07, with
the preceding configuration restored, GPON in O5 and the notebook associated
with PCIe1's 5 GHz AP. TCP tests explicitly routed `172.16.0.1/32` through
the WiFi interface and the Askey, avoiding the notebook's separate wired
default route. The firmware, kernel helper and mt76 came from the same
verified initramfs image. Ten-second runs before and after that boot gave:

| Direction | Previous image | Native PPE 7.14 | ARM CPU busy before / after |
| --- | --- | --- | --- |
| WiFi to GPON | 147.3 Mbit/s | 146.6 Mbit/s | 35.7% / 30.2% |
| GPON to WiFi | 402.3 Mbit/s | 520.4 Mbit/s | 55.6% / 76.1% |

CPU busy is the aggregate `/proc/stat` delta across both ARM cores during
each run. Upload softirq time fell from 8.6% to 6.5%. The native upload
submitted 128227 packets and delivered 1041 packets through the NPU host
ring, including control traffic and exceptions. The final idle UART snapshot
showed all 150616 submissions completed and returned, 1156 exceptions,
zero pending buffers, zero invalid returns and zero exception host drops.
PPE dumps during traffic showed a bound upstream NAT tuple. No panic was
observed during these runs.

These short RF measurements demonstrate native ingress and lower upload
CPU usage, not a general bidirectional CPU reduction. Download consumed more
CPU at its higher throughput; downstream WiFi TX still needs separate work.
MT7916, XX230v hardware, dual-radio load and long-duration recovery are not
validated by this Askey run.

```sh
make all-variants GITREV=x
python3 tests/en7523-core.py build/EN7523_MT7915
python3 tests/en7523-core.py build/EN7523_MT7915 --split
python3 tests/en7523-core.py build/EN7523_MT7915 --split --reverse --quiet --staggered
python3 tests/en7523-core.py build/EN7523_NOWIFI --quiet --staggered
```

The direct RX tests execute the linked firmware against modeled RX, QDMA and
completion rings. They check zero host copies for eligible traffic, sequence
gaps, old sequence rejection, PN replay rejection, A-MSDU members, fragments,
key barriers, full TX fallback, source/destination admission and station teardown.
Both independent-radio probe orders and the highest queue-1 WCID are covered.
These are firmware execution checks, not evidence of a CPU or throughput gain
on hardware. Firmware 7.13 has been RAM-booted on the Askey with station association,
DHCP and GPON traffic. QDMA completions recycle the packet buffers without
filling the vendor buffer-return FIFO. An earlier 7.12 Askey test exposed
a host WCID first-member layout regression and exhaustion of the vendor
buffer-return queue; both were corrected before performance comparison.

The linked-image model delays GDMA completion, checks channel ownership
and descriptor publication, and verifies that a failed STOP retains its
in-flight buffers. It also checks PPE initialization, the 256-entry, 80-byte SRAM
and 16K DRAM geometry, FoE staging, bounds, ACK timeout and table clear.
It checks two-band RX, raw byte preservation, fragmented
frames, descriptor ownership, ring wrapping and STOP, in addition to the
common boot/IRQ tests. A 37-frame burst must use two producer doorbells,
with no idle notifications. The model does not validate PCIe window routing,
interrupt timing or throughput on silicon.
Split-radio runs start each queue with the other host ring absent, repeat
the shared pool configuration without reallocating IDs and verify that
stopping one worker leaves the other receiving. Both probe orders are tested.

An Askey RTF8225VW RAM boot on 2026-11-07 verified firmware 7.9, four hart
self-tests and both independent mt76 consumers. PCIe0 used queue 0/IRQ 37;
PCIe1 used queue 1/IRQ 38. Both queues delivered packets. A phone associated
with `OpenWrt2` on PCIe1 at 5 GHz/channel 36/80 MHz, obtained a DHCP lease
and exchanged traffic through the GPON WAN. User-provided ten-second TCP
tests to `172.16.0.1` measured 114 Mbit/s upload and 251 Mbit/s download at
the receivers, with 262 and 331 sender retransmissions respectively.

A subsequent computer test on the same 5 GHz AP measured 135 Mbit/s upload
and 317 Mbit/s download at the receivers, with 21 and 55 sender TCP
retransmissions respectively. DHCP and hostapd records confirmed that the
computer used PCIe1's AP; it had disconnected before station-rate inspection.

The first radio was temporarily enabled in its existing 5 GHz configuration
during that validation and then restored to its original disabled state.
The throughput runs are initial measurements with configuration activity,
not controlled RF benchmarks. Association and traffic at 2.4 GHz, sustained
dual-radio load and hardware recovery remain unvalidated.

An XX230v RAM boot on 2026-11-06 verified all four hart self-tests and RX
interrupts, descriptors and packets on both bands. A phone associated on
channel 36 at 80 MHz, obtained a DHCP lease and exchanged data in both
directions. These XX230v hardware measurements used firmware 7.8; the 7.9
DBDC path is covered by the linked-image regression test. The cycle-counter
enable must precede secondary-hart startup delays; enabling it later leaves the RX workers stuck before their loops.
Long-duration throughput, stress and recovery testing remain open.

The earlier CPU-copy transport copied aligned buffers with unrolled 32-bit loads and stores,
followed by word and byte tails. Unaligned callers use byte copies. The earlier
linked-image checks covered all source/destination alignments, zero lengths,
block boundaries and guards around packet-sized buffers.

On the same XX230v and Galaxy A15, a 30-second phone-to-router TCP test
improved from 27.5 to 146 Mbit/s at the receiver after this copy change,
with no sender retransmissions. Both runs used VHT 80 MHz; hostapd selected
primary channel 36 before the reboot and 40 afterward within the same block.
Before the change, the phone-to-WAN upload also measured 27.5 Mbit/s,
the reverse download measured 277 Mbit/s, and a router-to-WAN test reached
655 Mbit/s. During ten seconds of the original local upload, 99 of 100
hart-2 PC samples landed in `npu_memcpy`.

Subsequent ten-second tests through the GPON WAN to `172.16.0.1` measured
147 Mbit/s phone-to-server upload and 281 Mbit/s server-to-phone download
at the receivers with the optimized firmware. Sender TCP retransmissions
were 34 and 99 respectively. The WAN upload improvement is about 5.3 times
the original result; the download remained close to its earlier measurement.
These are single hardware trials, not controlled RF benchmarks.

Earlier CPU-copy measurements describe firmware 7.8/7.9.
The later 7.11 Askey follow-up below confirms boot and RX operation,
but does not establish accelerated forwarding or its CPU benefit.

## Hybrid NPU RX and WED TX

The `airoha,npu-wed-tx` boolean requests the EN7523 transmit-only WED provider
alongside NPU RX. The Askey and XX230v profiles enable it. Without the flag,
NPU RX retains host TX. `npu_enable=0` disables NPU RX and its DT request. WED is enabled by
default for supported mt7615, mt7915 and mt7996 devices when a WED provider
is available. `wed_enable=0` explicitly disables WED, including this hybrid
profile, while preserving NPU RX. WED owns transmit and
TX-free rings; the NPU owns data receive rings, which are excluded from the
WED interrupt mask. Host BA reordering remains enabled for this transport
when WED is attached.

OpenWrt enables `CONFIG_NET_AIROHA_WHNAT` on EN7523 for the vif registry,
native exception delivery and the CPU-assisted fallback below. Firmware 7.14
uses the native receive path described above for admitted frames. After host reorder,
PN validation and mac80211 processing, the WHNAT hook offers eligible IPv4
frames to LAN QDMA/PPE with a private vif tag. Unbound frames return to the
original vif below that hook, so they do not loop. The tag is removed before
PPE flow learning. GPON QDMA1 packets cannot enter this return path.
Existing flowtable policy decides which connections become hardware flows.
Bound upstream traffic leaves through its programmed GDM port. Bound
WiFi egress uses the WED path, including PCIe index, band, BSS and WCID.
EN7523 does not use the EN7580 CPU-egress metadata fallback.

This is a CPU-assisted RX path: it retains the host copy, reorder and security
checks, plus QDMA submission/completion. It removes routing/NAT and host TX
work for eligible bound forwarding traffic; it does not eliminate per-packet
CPU work. Router-local traffic, IPv6 on the WHNAT ingress hook, GSO and tagged
input keep their software path. Higher throughput or lower CPU/IRQ load must
be measured on hardware with matching firmware, kernel and mt76 images.

For a later RAM test, enable the OpenWrt firewall hardware flowtable, run a
forwarded LAN/GPON iperf test, and compare IRQ deltas, CPU softirq and mt76 TX
worker load. Check `/sys/module/airoha_eth/parameters/whnat_handed` and
`whnat_returned`, WED debugfs state, and `/sys/kernel/debug/ppe/flows`.
A software `BND` entry with `hash=ffff` is still pending; a valid hardware hash
and hardware packet counters are required to claim acceleration.

### Askey 7.11 follow-up: PPE readiness gate

A notebook test reported 267 Mbit/s upload and 486 Mbit/s download at the
receivers through `172.16.0.1`. The attached one-second `top` capture had two
traffic phases in that order. Their mean idle percentages were 30.9 and
27.3; softirq was 42.2% in the first phase, while system time was 44.4% in
the second. The capture lacks timestamps for exact iperf synchronization.

A read-only port1 inspection confirmed firmware 7.11, two NPU RX consumers,
WED0/WED1 registration and enabled firewall hardware flow offload. However,
PPE debugfs reported `offload_setup_done=0`; WHNAT counters were
`handed=0`, `returned=0` and `fallback=1012607`. Thus the intended ingress
acceleration was blocked despite successful initialization messages.

The host kernel now publishes PPE readiness after successful NPU setup,
hardware initialization and provider publication. Error paths retain the
blocked state. A regression harness executing the actual setup function
reproduced the old failure and checked NPU success, init/wait/stats errors,
unsupported-NPU fallback and repeated CPU-direct initialization.
The RAM boot with the readiness fix set that flag correctly but exposed
the CPU-origin RX lookup failure described below.

That boot also logged `PPE0 SRAM size mismatch: wrote 7, read 6` and a
startup WED status `0x00080000` on each device. These observations remain
open for hardware validation; the readiness fix does not resolve or
reinterpret them. Hardware-bound flow hashes and traffic counters must be
checked during the next test before claiming PPE/WED acceleration.

### CPU-origin return and DHCP recovery

On the next Askey boot the phone authenticated and associated, but could
not obtain an address. WHNAT handed and completed 23 frames while returning
none. Temporarily disabling `whnat_enable` restored DHCP without a radio
restart; dnsmasq acknowledged `192.168.1.110` to the phone.

The modern QDMA RX path resolved a physical GDM before examining the WHNAT
return tag. EN7523 source port 0 (CPU/CDM1) is not a physical ingress and
was rejected at that stage. The corrected lookup borrows the GDM1 netdev
only for CPU-origin frames on LAN QDMA0 while WHNAT is active, then the
existing return handler removes the private tag and restores the vif.
WAN QDMA1 and the existing physical-source mapping retain their behavior.
Broadcast and multicast frames now bypass the WHNAT ingress hook, keeping
DHCP discovery and other control traffic on the ordinary host path.

Regression harnesses executing the real lookup and WHNAT functions reproduce
the old lookup failure and check LAN/WAN isolation, disabled state, missing
GDM/vif, physical port mapping, tag removal, packet preservation and
broadcast/multicast exclusion. The follow-up below validates this return path with WHNAT enabled on the
Askey. It does not validate accelerated WiFi egress.

### Askey return-path validation and remaining WiFi egress work

The user booted the return-path fix and repeated the notebook test on
2026-11-07. Receiver rates were 373 Mbit/s upload and 516 Mbit/s download,
compared with 267 and 486 Mbit/s previously. These are individual RF trials.
In the stable traffic phases of the 33-sample `top -d1` capture, mean CPU
busy time was approximately 65.6% and 70.9%. Softirq averaged 46.7% in the
first phase; system time averaged 47.0% in the second. The capture lacks
precise timestamps for synchronization with iperf.

Read-only port1 inspection found WHNAT enabled, PPE readiness set, 864878
handed/completed frames, 19269 host returns and no submission fallbacks.
Notebook-origin flows now had valid hardware indices. This confirms the
CPU-origin return lookup and control-traffic recovery on the running board.
These cumulative counters include background traffic, not just iperf.

A further 15-second reverse iperf was run from the same notebook route
(`192.168.1.153` over `wlan0`). During that transfer, the notebook-to-server
flow had hardware slot `00b4` in BIND state. Its reverse data flow from
`172.16.0.1:5201` to `172.16.0.11:39922` remained UNBIND at slot `00e8`,
with no corresponding reverse flow in the software offload listing.
The WiFi TX worker remained active. This establishes a remaining failure
to install/bind the tested PPE-to-WED egress flow; WED registration and
ring movement alone are not proof of direct hardware forwarding.
The next investigation is reverse-flow creation and the WiFi forwarding
path. No new driver change or router restart was made for this measurement.

### WED defaults and PR #89

The OpenWrt mt76 package enables WED by default in mt7615, mt7915 and
mt7996. Existing hardware/provider checks remain in force, and the boolean
`wed_enable=0` module parameter remains an explicit opt-out. The EN7523
NPU hybrid profile still requires `airoha,npu-wed-tx` to allow shared WED
TX and NPU RX ownership; that property does not override `wed_enable=0`.

Patch 180 from idiazraz's OpenWrt PR #89 is included with its attribution.
It configures MT7916 CTXD for WED rings 18/19 and enables TX flow control
inside the WED startup branch. It does not program these MT7916 registers
on the Askey/XX230v MT7915 devices. The patch was reviewed at PR head
`6eab09890c801d6010d84d3c694475abc13bb0d1`.

PR patch 181 is a different receive transport: it targets MT7916 and stock
EN7523 V1 firmware with 16-byte host descriptors, no host reorder for frames
forwarded by the NPU, and different mailbox/ring ownership. It cannot be
stacked over this MT7915 ClankerNPU 24-byte transport. The existing Clanker
RX, PN checks, reordering and reserved GDMA channels remain in use.

These changes do not establish a fix for the Askey reverse flow remaining
UNBIND. The Askey already attached WED through its hybrid DT profile.
New MT7916 behavior and the new default policy require hardware validation.

### Askey WED-default image measurement

The user booted `askey_rtf8225vw-initramfs-kernel-wed-default.bin`; the
port1 boot log `2026/11/07/10-30-55_1794058255.log` confirms the filename,
firmware 7.11, both NPU consumers and WED providers. Runtime inspection
confirmed `mt7915e.wed_enable=Y`, `offload_setup_done=1`, 301113 handed
and completed frames, 3212 returns and no submission fallback. The existing
startup WED status `0x00080000` and SRAM size readback warning persisted.

User iperf receiver rates were 314/391 Mbit/s upload/download, compared
with 373/516 Mbit/s in the preceding trial. The supplied 29-sample top
capture averaged approximately 52.1% and 53.7% aggregate CPU busy time in
its two traffic phases. These are lower throughputs and lower CPU load;
they do not demonstrate an efficiency gain or identify the cause of the
throughput drop. Capture timestamps do not align the phases precisely.

A repeat from the same notebook route measured 329.5/403.0 Mbit/s over
15 seconds per direction. During the reverse transfer, the upstream ACK
flow was BIND at hardware slot `0025`, while the reverse data flow to
`172.16.0.11:46302` remained UNBIND at slot `0138`. It still had no reverse
software offload entry. Another ten-second download measured 406.8 Mbit/s;
per-core `/proc/stat` deltas averaged 65.5% busy on CPU0 and 36.2% on CPU1.
These samples do not establish sustained single-core saturation.

The notebook reported HE 2-stream, 80 MHz, 1200.9 Mbit/s PHY rates and
-36 dBm in the contemporaneous host snapshot; those are not payload rates.
The MT7916-only CTXD branch is not run on this MT7915 board. WED default
activation is now verified on Askey, but the reverse flow creation/binding
issue remains open. No offload settings, driver code or router boot state
were changed during this measurement.


### Wireless SDK cross-check and WDMA forward-path repair

The supplied `sdk_wireless/mt_wifi/os/linux/plug_in` directory was used as
read-only reference. `whnat/woe_main.c` separates ring initialization from
TX tuple registration (`whnat_wlan_tx` calls `wifi_tx_tuple_add`).
`whnat/woe_hw.c` documents the WED/WDMA enable sequence and preservation of
RX INFO1/2/3 metadata. `warp_proxy/warp_proxy.c` supplies BSS index, local
DBDC ring and WCID; its MT7915 chip adapter initializes CT TXD/TXP buffers.
The current WED/mt76 implementation already covers these basic mechanisms.
The supplied directory declares and calls `wifi_tx_tuple_add` but does not
contain its implementation; it is not a complete Airoha PPE binding source.

On the running Askey, firewall rules had been generated before the AP was
added: the active flowtable contained LAN ports and `pon0`, but omitted
`phy1-ap0`. Regenerating the existing rules included that interface. After
a firewall reload, a 15-second reverse transfer still measured 388.4 Mbit/s
and remained UNBIND, with only the upstream ACK flow present in the software
PPE listing. Fixing flowtable membership alone did not establish WED egress.
Temporary dynamic-debug tracing confirmed WED flow-block bind calls and was
disabled again after the check. No reboot or flash was performed.

Source inspection then found a separate missing integration in
`net/netfilter/nf_flow_table_path.c`: `nft_dev_path_info` treated
`DEV_PATH_MTK_WDMA` as an unknown path and cleared `info.indev`. This prevents
Wi-Fi paths from acquiring direct TX parameters even though mt76 returns a
valid WDMA path. The published patch by Eric Woudstra was rebased onto this
6.18 tree and included in OpenWrt's generic pending patches with attribution:
https://lists.infradead.org/pipermail/linux-mediatek/2026-March/105004.html

A harness compiling the actual path helper reproduces the lost bridge-to-
WDMA destination before the change and passes after it. Additional cases
cover a native WDMA path, hardware offload disabled, VLAN/Ethernet, and an
unknown path. This validates path discovery only. The repaired image still
requires a hardware traffic test to establish reverse-flow BIND, working
PPE-to-WED forwarding, connectivity and CPU load under sustained traffic.


The kernel, kernel packages, root filesystem installation and image builds
passed. The Askey and XX230v `-initramfs-kernel-wed-path.bin` images were
checked for FIT hashes, the final kernel/root archive, the corrected
`nf_flow_table.ko`, mt76 modules, firmware 7.11, WED TX flags and reserved
GDMA channels. Their SHA256 values are recorded in
`bin/targets/airoha/en7523/clankernpu-7.11-wed-path.sha256` in OpenWrt.
These images were prepared without restarting either router.

## Periodic UART receive log

The EN7523 MT7915 build enables `print_mask` bit 9 (`0x200`) by default,
independently of `NPUDBG`. Hart 0 prints a summary every 500 timer ticks
(configured as five seconds), outside the radio RX and QDMA completion loops.
No packet-by-packet UART trace is enabled. The host's MIB21 UART-silence gate
still takes precedence.

`[RX0]` and `[RX1]` show queue enable, direct-mode enable, PPE submissions,
raw host frames, PN drops and full/unavailable-QDMA fallbacks. Parenthesized
values are increments since the previous summary. `[QDMA]` shows resource
readiness, free descriptors (1023 when idle), software tail, hardware DMA index,
completion head/count, reclaimed buffers, descriptor-writeback polling waits,
the vendor PPE buffer-return FIFO occupancy, and the last PPE mailbox result.
The snapshot releases the QDMA ownership mutex before formatting or printing.

Clear bit 9 at `0x1e906844` to silence this summary; preserve other print-mask
bits when changing it. Other SoCs keep their existing print-mask defaults.
The kernel flowtable must also be enabled: a growing PPE submission counter
does not prove forwarding acceleration when every packet returns to the ARM.

## Native PPE receive lifetime (7.14)

`[PPE]` reports consumed return tokens, exceptions, per-band pending buffer
ownership, invalid tokens and host drops. A bound forwarding workload should
increase submissions, QDMA completions and PPE returns while exceptions and
host interrupts stay low. QDMA completion alone is not evidence of forwarding.
The native path requires the matching mt76 exception decoder and kernel
`airoha_npu_rx_skb` export in the same image.

The `[PPE REASON]` lines show the PPE CPU reason extracted from bits 20:16
of the return `INFO` word, counted only when the upper half of `DATA` is
zero and the returned packet ID is valid. Each reason reports a cumulative
total and the delta since the previous five-second report. Only reasons
whose counters changed are printed. `0x0c` is TCP FIN/SYN/RST, `0x0d`
is UN_HIT, `0x0e` is HIT_UNBIND, and `0x0f` is
HIT_UNBIND_RATE_REACH. The `0x15` keepalive reason is counted but is
not treated as an exception. Other reason codes retain their numeric ID;
the log does not interpret `INFO` for successfully forwarded packets.
