# Execute native ingress through the existing SDK BA/DEL_STA mailbox commands.
import subprocess

symbols = {fields[2]: int(fields[0], 16) for line in subprocess.check_output(
    ['riscv64-unknown-elf-nm', '-n', str(root / 'firmware.elf')], text=True).splitlines()
    if len(fields := line.split()) == 3 and fields[1] not in ('U', 'u')}

def control_data(cmd, data, band=0):
    ctrl, _ = request(cmd=cmd, msg_type=1, ifindex=band,
                      size=len(data)+8, blob=data)
    assert ctrl & 0x1e == 6, (cmd, hex(ctrl))

def wait_until(predicate, label):
    for _ in range(80000):
        tick(1)
        if predicate():
            return
    raise AssertionError(label)

txbase, irqbase = 0x8a100000, 0x8a110000
load(0x1fb541c0, txbase.to_bytes(4, 'little'))
load(0x1fb54048, irqbase.to_bytes(4, 'little'))
load(0x1fb5404c, (1024).to_bytes(4, 'little'))
load(0x1fb54004, (1).to_bytes(4, 'little'))
load(irqbase, b'\xff' * 4096)
ppe_request([1, 1, 0, 0, 3, 0, 0])
set_cmd(30, 1)
ids = read(symbols['tx_id'])
packet_state = read(symbols['packets'])
native_wcid = (1, 150)

def rx_frame(sn, wcid=1, encrypted=False, proto=0x800, source=None):
    mac = bytes.fromhex('024455667788')
    payload = bytes.fromhex('020102030405') + (source or mac) + proto.to_bytes(2, 'big')
    payload += bytes.fromhex('450000280000000040060000c0000201c6336401') + bytes(20)
    raw = bytearray(40 + len(payload))
    struct.pack_into('<6I', raw, 0, 2 << 27 | len(raw),
                     wcid | 1 << 14 | ((4 << 16) if encrypted else 0),
                     1 << 13, 1 << 16, 0, 0)
    struct.pack_into('<H', raw, 24, 0x188 | (0x4000 if encrypted else 0))
    raw[26:32] = mac
    struct.pack_into('<I', raw, 32, sn << 4)
    raw[40:] = payload
    return bytes(raw)

def tx_complete(index):
    head = read(0x1fb54068) & 1023
    assert not read(0x1fb54068) >> 16
    load(txbase+index*32+4, (read(txbase+index*32+4) | 1 << 31).to_bytes(4, 'little'))
    load(irqbase+head*4, (6 << 16 | index).to_bytes(4, 'little'))
    load(0x1fb54068, (1 << 16 | head).to_bytes(4, 'little'))
    wait_until(lambda: read(ids+index*2, 2) == 0xffff, 'QDMA descriptor leaked')

def status(ident):
    return read(packet_state+ident*16+12)

def submit(sn, band=0, wcid=None):
    if wcid is None:
        wcid = native_wcid[band]
    index = read(0x1fb541c8)
    blob = rx_frame(sn, wcid)
    packet(band, blob)
    ident = read(ids+index*2, 2)
    assert ident != 0xffff and status(ident) & 0x80000000
    assert read(txbase+index*32+20) == 0x7f4087ff
    return index, ident, blob

for band in range(2):
    set_cmd(12, 0, band)
    set_cmd(6, native_wcid[band], band)
    set_cmd(6, 151, band, expected=EINVAL)
    head = read(0x1ec0d188+band*16)
    blob = rx_frame(9, native_wcid[band])
    packet(band, blob)
    check_host(band, head, blob, len(blob), 1, 1, True)
    control_data(4, struct.pack('<IIII', native_wcid[band], 0, 10, 64), band)

# Bound frames cause no host copy; completion order must not change ownership.
for band in range(2):
    head = read(0x1ec0d188+band*16)
    index, ident, _ = submit(10, band)
    ppe_returns.append((1 << 16 | ident, 0))
    wait_until(lambda: not ppe_returns, 'PPE completion was not acknowledged')
    assert status(ident) & 0x80000000
    tx_complete(index)
    wait_until(lambda: not status(ident), 'completed native packet leaked')
    assert read(0x1ec0d188+band*16) == head

# A missing MPDU holds the following packet and releases both in sequence.
before = read(0x1fb541c8)
packet(0, rx_frame(12))
assert read(0x1fb541c8) == before
index, ident, _ = submit(11)
wait_until(lambda: read(0x1fb541c8) != before, 'reorder gap did not close')
for index in range(1024):
    ident = read(ids+index*2, 2)
    if ident == 0xffff:
        continue
    tx_complete(index)
    assert status(ident) & 0x80000000
    ppe_returns.append((1 << 16 | ident, 0))
    wait_until(lambda: not status(ident), 'PPE-owned buffer was not returned')

# Exceptions retain their RXD and FoE metadata for the normal mt76 path.
for band, sn in ((0, 13), (1, 11)):
    head = read(0x1ec0d188+band*16)
    index, ident, blob = submit(sn, band)
    metadata = 0xf << 16 | 0x4010
    # Vendor selects the return path using bit 16 alone.
    ppe_returns.append((1 << 17 | ident, metadata))
    wait_until(lambda: not ppe_returns, 'exception FIFO stalled')
    dma_hold.add(band)
    tx_complete(index)
    wait_until(lambda: band in dma_pending, 'exception used the wrong RX hart')
    assert status(ident) & 0x80000000
    assert read(0x1ec0d188+band*16) == head
    dma_hold.remove(band)
    wait_until(lambda: not status(ident), 'exception buffer leaked')
    check_host(band, head, blob, len(blob), 1, 1, True,
               vendor=0x90000000 | metadata)

# Kite orders secured frames, but raw host metadata must not claim PN checks.
# ARP and forged translations also retain the complete host RXD.
control_data(4, struct.pack('<IIII', 1, 0, 14, 64))
for sn, kwargs, flags in ((14, dict(encrypted=True), 0),
                          (15, dict(proto=0x806), 0x80000000),
                          (16, dict(source=b'\x02'*6), 0x80000000)):
    head = read(0x1ec0d188)
    before = read(0x1fb541c8)
    blob = rx_frame(sn, **kwargs)
    packet(0, blob)
    check_host(0, head, blob, len(blob), 1, 1, True, vendor=flags)
    assert read(0x1fb541c8) == before

# Protected frames enter the shared Kite window and leave in sequence.
head = read(0x1ec0d188)
before = read(0x1fb541c8)
protected_hi = rx_frame(18, encrypted=True)
protected_lo = rx_frame(17, encrypted=True)
packet(0, protected_hi)
assert read(0x1ec0d188) == head
packet(0, protected_lo)
for i, blob in enumerate((protected_lo, protected_hi)):
    check_host(0, (head+i) % 512, blob, len(blob), 1, 1, True, vendor=0)
assert read(0x1fb541c8) == before, 'protected frame entered the PPE'
# A repeated MPDU from the same BA window must not be forwarded twice.
packet(0, rx_frame(18, encrypted=True))
assert read(0x1ec0d188) == (head + 2) % 512
assert read(0x1fb541c8) == before

# DEL_STA invalidates late exceptions and flushes held frames before WCID reuse.
index, ident, _ = submit(19)
set_cmd(6, 1)
head = read(0x1ec0d188)
ppe_returns.append((ident, 0xe << 16))
tx_complete(index)
wait_until(lambda: not status(ident), 'stale exception leaked a buffer')
assert read(0x1ec0d188) == head
blob = rx_frame(18)
packet(0, blob)
check_host(0, head, blob, len(blob), 1, 1, True)

# Mailbox teardown discards held MPDUs without using a radio hart's GDMA.
control_data(4, struct.pack('<IIII', 1, 0, 20, 64))
head = read(0x1ec0d188)
packet(0, rx_frame(21))
entry = read(symbols['ba_table_a'])
assert read(entry) != 0
set_cmd(6, 1)
assert read(entry) == 0 and read(0x1ec0d188) == head

# STOP retains storage while PPE owns a packet, then discards its late return.
control_data(4, struct.pack('<IIII', 1, 0, 30, 64))
index, ident, _ = submit(30)
set_cmd(2, 0, expected=0)
assert status(ident) & 0x80000000
ppe_returns.append((ident, 0xe << 16))
tx_complete(index)
wait_until(lambda: not status(ident), 'STOP leaked a late PPE return')
assert read(0x1ec0d188) == head
set_cmd(2, 0)
set_cmd(2, 1)
set_cmd(6, 1)
set_cmd(6, native_wcid[1], 1)
for band in range(2):
    set_cmd(12, 1, band)
assert not ppe_returns and all(not status(i) for i in range(5600))
print('PASS: shared Kite BA, protected in-order host delivery without PN flags, QDMA/PPE ownership, DEL_STA and STOP')
