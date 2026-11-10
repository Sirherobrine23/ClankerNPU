# EN7523 MT7915 native PPE ingress

Firmware 7.14.1.0 and the matching mt76 driver enable native RX ingress for
authorized AP stations on open networks. No station/key mailbox opcodes are
added: the existing BA setup and DEL_STA commands control the session lifetime.
The extended BA payload already used by this driver contains WCID, TID, SSN
and window size; this is not a claim of binary compatibility with vendor Linux.

## Packet path

The host sends BA setup only after station authorization, on an AP with no
configured cipher or pairwise hardware key. Firmware reorders eligible open
MPDUs, validates the translated Ethernet source against the RXD transmitter,
and submits unicast IPv4 TCP/UDP frames to the native QDMA ring. Fragments,
IPv4 options, unsupported translations, chained buffers and other protocols
stay on the host path. Encrypted traffic always retains host replay checks.

The PPE is armed by the normal kernel flow offload setup. Until its mailbox
configuration is ready, failed QDMA admission falls back to raw host RX.

QDMA completion and PPE return are independent. The packet buffer remains
owned until both events complete. Successful bound forwarding needs no host
copy. CPU exceptions return the original RXD and Ethernet frame to mt76,
including reason/hash metadata. mt76 reports them to the kernel PPE flow
learner and continues through the normal mac80211 receive path. Duplicate
old-header keepalives are recycled instead of reinjected.

DEL_STA pauses the radio worker, invalidates the station generation, and
discards held reorder nodes. Late exceptions from the previous generation are
discarded. BA replacement and radio STOP discard held nodes while the worker
is quiescent; the mailbox core never performs a host GDMA copy. STOP refuses
to release storage while QDMA/PPE still own a buffer. Key installation also
retires open-network sessions, including peer sessions when a group key is
installed.

## RX descriptor index batching

The EN7523 native receive worker handles up to 32 descriptors per pass and
publishes the last consumed descriptor once after the pass. The ring has 1536
descriptors, so a batch cannot exhaust it. A partial or single-descriptor
pass also publishes the index before returning to the polling loop. This
reduces PCIe MMIO writes during bursts without delaying the final refill.

This change reduces software overhead but does not establish that PCIe index
writes are the throughput bottleneck; compare the same client, RF conditions,
and upstream endpoint before and after applying it.

## RX cycle profiling

Build with `make SOC=EN7523 WIFI=MT7915 RXPROF=1` after the RX batching
change. The default image has no profiler. When the RX debug print mask is
set, hart 0 reports `[RX PROF]` summaries every 500 KITE ticks. Nothing is
printed by the radio harts or inside an interrupt handler.

The `desc`, `frames`, and `kicks` values are interval deltas; `batch_x10`
is descriptors per published PCIe index multiplied by ten. Each stage reports
`n` samples, `avg` cycles per sample, and `max` cycles since boot. The
firmware samples one in 64 completed frames and one in 64 nonempty batches.
With the EN7523 core at 250 MHz, 250 cycles are approximately one microsecond.

The sampled `rx-desc` interval encloses `buf-alloc` and `rx-process`;
`rx-process` encloses decode and QDMA submission. `qdma-submit` includes
`qdma-lock`, `qdma-descriptor`, and `qdma-kick`. Do not add the nested times together.
The `pcie-kick` timer measures the cost of issuing a posted MMIO write,
not PCIe transaction completion. `first-decode` is the initial RXWI parse,
`rx-managed` includes another RXWI parse, and `ba-classify` measures the
reorder/classification call (which also parses the RXWI). `rx-release`
includes the final RXWI parse, forwarding checks and `qdma-submit`. These
four stages are inside `rx-process`; the latter two are not disjoint from
`decode` and `qdma-submit`. All measurements are sampled, not per-packet logs. `ba-scan` and `host-flush` are sampled on
nonempty RX batches. These timings cannot directly measure GPON airtime,
802.11 retransmissions or time spent waiting for new RX descriptors.

Compare a 30-second Wi-Fi upload with a cable baseline and the same firmware
built with `RXPROF=0`. High `qdma-lock` cycles point to contention with the
completion hart; high `qdma-descriptor` cycles point to descriptor/fence/MMIO
cost. High `rx-process` with low `qdma-submit` suggests parsing or BA work.
A low `rx-desc` average with low throughput means the next suspect is RX
arrival rate, aggregation, or a downstream shaper rather than CPU execution.

## Validation and target checks

Build and run the instruction-level firmware model:

```sh
make SOC=EN7523 WIFI=MT7915
python3 -u tests/en7523-core.py build/EN7523_MT7915
```

The suite exercises both RX channels, BA ordering, both completion orders,
raw/FoE exceptions, encrypted fallback, station invalidation, held-node cleanup
and STOP with outstanding PPE ownership. It models register behavior; it does
not establish real hardware descriptor or radio correctness.

Install the matching firmware and mt76 packages together, then reboot. On an
open AP, generate routed IPv4 TCP/UDP traffic from a client that negotiates RX
BA, with OpenWrt hardware flow offload enabled. Check the serial counters:

- `direct=1` means native ingress is enabled by policy.
- Increasing RX `ppe` and QDMA `done` prove submissions and DMA completions.
- Increasing PPE `returned` proves hardware returns; `exceptions` alone are
  normal for new/unbound flows and do not prove acceleration.
- Bound kernel PPE entries and forwarded hardware counters are needed to
  establish actual acceleration. Throughput alone is insufficient.

WED error status `00080000` is a separate issue and is not fixed by this change.

### RXWI reuse (0005)

EN7523 keeps a decoded RXWI frame context only while a radio hart is
processing the matching `(band, buffer ID, length)` tuple. `rx-managed`,
`en7523_rx_parse()` and the forwarding callback reuse that context. When BA
releases older buffered MPDUs, their descriptors are decoded independently.
The context is invalidated on every return from `en7523_rx_process()`,
including duplicate/drop and host fallback paths. No replay, BA or QDMA
state changes are made by this optimization.

With `RXPROF=1`, the `[RX PROF]` header includes `cache_hits`, the delta in
reused RXWI decodes over that reporting interval. `ba-forward` measures
`pkt_enqueue_bridge()` → `en7523_rx_release()` and includes QDMA submission;
`ba-classify` encloses the forwarding call. Subtracting two independently
sampled averages is only an estimate, not an exact exclusive BA cost.

Compare equal-duration A/B `iperf3` tests before and after this change and
check that PPE exception, fallback and TCP retransmit counts do not regress.
