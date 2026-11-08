# Execute the linked firmware through its RX rings and SDK mailbox.
import subprocess

symbols = {fields[2]: int(fields[0], 16) for line in subprocess.check_output(
    ['riscv64-unknown-elf-nm', '-n', str(root / 'firmware.elf')], text=True).splitlines()
    if len(fields := line.split()) == 3 and fields[1] not in ('U', 'u')}

def state(name):
    return symbols[name]

def control_data(cmd, data, band=0):
    ctrl, _ = request(cmd=cmd, msg_type=1, ifindex=band,
                      size=len(data)+8, blob=data)
    assert ctrl & 0x1e == 6, (cmd, hex(ctrl))

mac = bytes.fromhex('024455667788')
txbase, irqbase = 0x8a100000, 0x8a110000
load(0x1fb541c0, txbase.to_bytes(4, 'little'))
load(0x1fb54048, irqbase.to_bytes(4, 'little'))
load(0x1fb5404c, (1024).to_bytes(4, 'little'))
load(0x1fb54004, (1).to_bytes(4, 'little'))
load(irqbase, b'\xff' * 4096)
ppe_request([1, 1, 0, 0, 3, 0, 0])
set_cmd(30, 1)
assert read(symbol('sram_alloc_offset')) <= 0x60000, 'main SRAM exhausted'
control_data(34, struct.pack('<III8s8s', 1, 1, 3, mac, bytes.fromhex('020102030405')))
control_data(4, struct.pack('<IIII', 1, 0, 10, 64))
set_cmd(12, 0)

def rx_frame(sn, pn=None, ethertype=0x800, amsdu=0, bad=0, source=mac):
    header = 56 if pn is not None else 40
    payload = bytes.fromhex('020102030405')+source+ethertype.to_bytes(2, 'big')
    payload += b'\x45\0\0\x28'+b'\0'*36
    raw = bytearray(header+len(payload))
    struct.pack_into('<6I', raw, 0, 2<<27 | len(raw),
                     1 | 1<<14 | (1<<11 | 4<<16 if pn is not None else 0) | bad,
                     1<<13, 1<<16, amsdu, 0)
    struct.pack_into('<I', raw, 24, 0x188 | (0x4000 if pn is not None else 0))
    struct.pack_into('<I', raw, 32, sn<<4)
    if pn is not None:
        raw[40:46] = pn.to_bytes(6, 'little')
    raw[header:] = payload
    return bytes(raw), header

head = read(0x1ec0d188)
before_dma = dma_copies[:]
blob, hdr = rx_frame(10)
packet(0, blob)
assert read(0x1fb541c8) == 1
assert read(0x1ec0d188) == head and dma_copies == before_dma
assert read(txbase+4) == max(60, len(blob)-hdr)
assert read(read(txbase+8)+58, 2) == 0, 'nonzero Ethernet padding'
first_id = read(read(state('tx_id')), 2)
assert read(txbase+16) == 0x80000000 | first_id << 14
assert read(txbase+20) == 0x7f4087ff, 'native PPE return queue missing'
addr = read(txbase+8)
assert bytes(read(addr+i, 1) for i in range(12)) == blob[hdr:hdr+12]
assert bytes(read(addr+i, 1) for i in range(len(blob)-hdr)) == blob[hdr:]

# A gap is retained; the missing MPDU releases both frames in order.
blob12, _ = rx_frame(12)
packet(0, blob12)
assert read(0x1fb541c8) == 1
blob11, _ = rx_frame(11)
packet(0, blob11)
assert read(0x1fb541c8) == 3
assert read(0x1ec0d188) == head and dma_copies == before_dma
packet(0, blob11)
assert read(0x1fb541c8) == 3, 'old sequence number was forwarded'

# Completion, not RX refill, releases the DMA buffer.
ids = read(state('tx_id'))
assert read(ids, 2) != 0xffff
packet_state = read(state('packets'))
load(irqbase, (6<<16).to_bytes(4, 'little'))
load(0x1fb54068, (1<<16).to_bytes(4, 'little'))
tick(3000)
assert read(ids, 2) != 0xffff, 'buffer freed before descriptor writeback'
load(txbase+4, (read(txbase+4) | 1<<31).to_bytes(4, 'little'))
for _ in range(10000):
    tick(1)
    if read(ids, 2) == 0xffff:
        break
else:
    raise AssertionError('QDMA completion did not release the buffer')
assert read(packet_state+first_id*16+12) & 0x80000000, 'TX completion freed PPE-owned buffer'
ppe_returns.append((1 << 16 | first_id, 0))
for _ in range(10000):
    tick(1)
    if not read(packet_state+first_id*16+12) and not ppe_returns:
        break
else:
    raise AssertionError('PPE return did not release completed packet')
assert not ppe_returns
load(0x1fb54068, (0).to_bytes(4, 'little'))

# Key replacement brackets the chip operation; no old RX descriptor survives it.
control_data(35, struct.pack('<II96s', 1, 1<<31, b''))
assert read(symbol('en7523_wifi_paused')) == 1
control_data(35, struct.pack('<II96s', 1, 1, b''))
assert read(symbol('en7523_wifi_paused')) == 0
control_data(4, struct.pack('<IIII', 1, 0, 20, 64))
for sn, pn in ((20, 100), (22, 102), (21, 101)):
    blob, _ = rx_frame(sn, pn)
    packet(0, blob)
assert read(0x1fb541c8) == 6
before = read(symbol('en7523_wifi_stats')+8)
blob, _ = rx_frame(23, 100)
packet(0, blob)
assert read(0x1fb541c8) == 6
assert read(symbol('en7523_wifi_stats')+8) == before+1, 'replayed PN passed'

# ARP remains on the host, carrying completed reorder/replay checks.
blob, _ = rx_frame(24, 103, ethertype=0x806)
packet(0, blob)
assert read(0x1ec0d188) == (head+1)%512
assert read(host[0]+head*24+8) == 0xc0000000
assert read(0x1fb541c8) == 6
check_host(0, head, blob, len(blob), 1, 1, True, vendor=0xc0000000)

# A full TX ring returns the unmodified raw frame to mt76.
free_r, free_w = read(state('free_read'), 2), read(state('free_write'), 2)
load(state('free_read'), free_w.to_bytes(2, 'little'))
head = read(0x1ec0d188)
blob, _ = rx_frame(25, 104)
packet(0, blob)
check_host(0, head, blob, len(blob), 1, 1, True, vendor=0xc0000000)
load(state('free_read'), free_r.to_bytes(2, 'little'))

# A-MSDU members share one PN, including an ARP member returned to the host.
head = read(0x1ec0d188)
before = read(0x1fb541c8)
for fmt, proto in ((3, 0x800), (2, 0x806), (1, 0x800)):
    blob, _ = rx_frame(26, 105, ethertype=proto, amsdu=fmt)
    packet(0, blob)
    if proto == 0x806:
        check_host(0, head, blob, len(blob), 1, 1, True, vendor=0xc0000000)
assert read(0x1fb541c8) == before+2
blob, _ = rx_frame(27, 105)
packet(0, blob)
assert read(0x1fb541c8) == before+2, 'PN reuse after the last A-MSDU member'

# Neither an unknown Ethernet source nor bridged traffic bypasses the host.
for sn, pn, source in ((28, 106, bytes.fromhex('024455667789')), (29, 107, mac)):
    head = read(0x1ec0d188)
    blob, hdr = rx_frame(sn, pn, source=source)
    if sn == 29:
        blob = blob[:hdr] + bytes.fromhex('021111111111') + blob[hdr+6:]
    packet(0, blob)
    check_host(0, head, blob, len(blob), 1, 1, True, vendor=0xc0000000)
assert read(0x1fb541c8) == before+2

# Invalid hardware security status cannot enter the reorder/replay state.
head = read(0x1ec0d188)
blob, _ = rx_frame(30, 108, bad=1<<27)
packet(0, blob)
assert read(0x1ec0d188) == head and read(0x1fb541c8) == before+2

# A missing MPDU is released by the timer without requiring new radio traffic.
blob, _ = rx_frame(31, 109)
packet(0, blob)
assert read(0x1fb541c8) == before+2
now = read(symbol('timer_int_count'))
load(symbol('timer_int_count'), (now+5).to_bytes(4, 'little'))
for _ in range(100000):
    tick(1)
    if read(0x1fb541c8) == before+3:
        break
else:
    raise AssertionError('reorder timeout did not release a missing sequence gap')

# Fragmented MPDUs stay raw and advance the NPU window when complete.
for fragment in (0, 1):
    raw = bytearray(56+26+32)
    struct.pack_into('<6I', raw, 0, 2<<27 | len(raw),
                     1 | 1<<14 | 1<<11 | 4<<16, 1<<27, 1<<16, 0, 0)
    fc = 0x4188 | (0x400 if fragment == 0 else 0)
    struct.pack_into('<I', raw, 24, fc)
    struct.pack_into('<I', raw, 32, 32<<4 | fragment)
    raw[40:46] = (110+fragment).to_bytes(6, 'little')
    struct.pack_into('<H', raw, 56, fc)
    struct.pack_into('<H', raw, 78, 32<<4 | fragment)
    head = read(0x1ec0d188)
    packet(0, raw)
    check_host(0, head, raw, len(raw), 1, 1, True, vendor=0x60000000)
blob, _ = rx_frame(33, 112)
packet(0, blob)
assert read(0x1fb541c8) == before+4, 'fragment left the BA window stalled'

# Queue 1's highest supported WCID has separate bounded security storage.
control_data(34, struct.pack('<III8s8s', 150, 1, 4, mac,
                            bytes.fromhex('020102030405')), band=1)
control_data(4, struct.pack('<IIII', 150, 0, 100, 64), band=1)
set_cmd(12, 0, 1)
blob, _ = rx_frame(100)
blob = bytearray(blob)
struct.pack_into('<I', blob, 4, 150 | 1<<14)
packet(1, blob)
assert read(0x1fb541c8) == before+5
control_data(34, struct.pack('<III8s8s', 150, 0, 4, mac,
                            bytes.fromhex('020102030405')), band=1)

# Without negotiated BA, normal host duplicate filtering remains active.
control_data(4, struct.pack('<IIII', 1, 0, 34, 0))
head = read(0x1ec0d188)
blob, _ = rx_frame(34, 113)
packet(0, blob)
check_host(0, head, blob, len(blob), 1, 1, True, vendor=0x40000000)

# Disassociation disables direct forwarding and discards the station context.
control_data(34, struct.pack('<III8s8s', 1, 0, 3, mac, bytes.fromhex('020102030405')))
head = read(0x1ec0d188)
blob, _ = rx_frame(35, 114)
packet(0, blob)
check_host(0, head, blob, len(blob), 1, 1, True)
exec((Path(__file__).parent / 'en7523-native-ppe.py').read_text())
print('PASS: NPU RX -> QDMA TX6, no host copies, BA/timeout, PN replay, A-MSDU, fragments, completion ownership, key barrier, L2/ARP fallback, TX congestion, WCID bounds and teardown')
