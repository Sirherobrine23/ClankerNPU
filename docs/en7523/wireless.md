# EN7523 MT7915 receive transport

The XX230v's MT7905DEN/MT7975DN combination exposes PCI ID `14c3:7915`.
It uses the MT7915 path in mt76. Build its NPU image with:

```sh
make SOC=EN7523 WIFI=MT7915
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
airoha,npu-driver-model = <1>;
airoha,npu-port-type = <1>;
```

`npu_enable=0` keeps mt7915e on its ordinary receive path. When selected, the
transport excludes WED and requires a little-endian EN7523 with single-HIF
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

## Receive path

Hart 0 initializes the common SRAM allocator and software buffer-ID pool.
Two 1536-entry, 16-byte radio RX rings occupy SRAM-A at `0x1e800000` and up,
with the second band offset by `0x6020`. The common allocator determines the
actual base. The packet reservation must hold 5600 buffers of 4096 bytes.
PCIe windows use the v1 SCU registers, not AN75xx aliases.

Hart 1 drains band 0; hart 2 drains band 1 and services the common timer.
Each worker copies received bytes to its host ring, then returns the radio
descriptor with a 3500-byte buffer and publishes its CPU index. Hart 3 stays
in the common idle loop. This path does not use TDMA, BME, hardware BMGR or
the AN75xx bridge. Host copies use ordinary uncached memory accesses with
the existing v1 boot mapping.

The host receives raw MT7915 RX headers and payloads. The radio TX queues,
TX completion, firmware control, BA reorder, PN checks and checksum validation
remain with mt76/mac80211. This is an RX transport, not PPE acceleration.

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

Word 2 is zero. Frames up to 16383 bytes can span multiple host entries.
An owned/full entry drops the incoming frame without overwriting host data.
The driver copies completed fragments into an skb before clearing Done.

The supported SET IDs are PCIE_ADDR (0), DESC (1), NPU_INIT_DONE (2),
DRIVER_MODEL (5), PKT_BUF_ADDR (8), IS_FORCE_TO_CPU (12, value 1 only) and
PCIE_PORT_TYPE (14). Supported GETs are NPU_INFO (0), RXDESC_BASE (4) and
NPU_VERSION (10). BA, TX and PPE commands return EOPNOTSUPP.

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

```sh
make all-variants GITREV=x
python3 tests/en7523-core.py build/EN7523_MT7915
python3 tests/en7523-core.py build/EN7523_MT7915 --split
python3 tests/en7523-core.py build/EN7523_MT7915 --split --reverse --quiet --staggered
python3 tests/en7523-core.py build/EN7523_NOWIFI --quiet --staggered
```

The linked-image model checks two-band RX, raw byte preservation, fragmented
frames, descriptor ownership, ring wrapping and STOP, in addition to the
common boot/IRQ tests. The model does not validate PCIe window routing,
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

EN7523 copies aligned packet buffers with unrolled 32-bit loads and stores,
followed by word and byte tails. Unaligned callers use byte copies. The
linked-image tests cover all source/destination alignments, zero lengths,
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
