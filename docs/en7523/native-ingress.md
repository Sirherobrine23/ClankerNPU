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
