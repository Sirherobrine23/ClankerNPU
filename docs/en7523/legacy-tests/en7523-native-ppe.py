# Historical backend fixtures requiring the removed station/key wire protocol.
# Kept as reference alongside en7523-direct-rx.py, outside the active suite.
print("Checking native PPE lifetime and exception delivery", flush=True)

def wait_until(predicate, label, limit=80000):
    for _ in range(limit):
        tick(1)
        if predicate():
            return
    raise AssertionError(label)


def tx_complete(index):
    assert read(0x1fb54068) >> 16 == 0, 'previous completion still pending'
    head = read(0x1fb54068) & 1023
    load(txbase+index*32+4, (read(txbase+index*32+4) | 1<<31).to_bytes(4, 'little'))
    load(irqbase+head*4, (6<<16 | index).to_bytes(4, 'little'))
    load(0x1fb54068, (1<<16 | head).to_bytes(4, 'little'))
    wait_until(lambda: read(ids+index*2, 2) == 0xffff, 'TX descriptor not recycled')
    wait_until(lambda: read(0x1fb54068) >> 16 == 0, 'IRQ queue not acknowledged')


def packet_status(ident):
    return read(packet_state+ident*16+12)


# Retire earlier reorder/replay test packets before testing the native lifetime.
for index in range(1024):
    ident = read(ids+index*2, 2)
    if ident != 0xffff:
        ppe_returns.append((1<<16 | ident, 0))
        tx_complete(index)
wait_until(lambda: not ppe_returns and all(not packet_status(i) for i in range(5600)),
           'native packets leaked after both completion events')

control_data(34, struct.pack('<III8s8s', 1, 1, 3, mac, bytes.fromhex('020102030405')))
control_data(4, struct.pack('<IIII', 1, 0, 200, 64))
set_cmd(12, 0)


def submit_native(sn):
    index = read(0x1fb541c8)
    blob, header = rx_frame(sn)
    packet(0, blob)
    ident = read(ids+index*2, 2)
    assert ident != 0xffff and packet_status(ident) & 0x80000000
    return index, ident, blob[header:]


# PPE may return before QDMA writes its descriptor: do not publish or free yet.
head = read(0x1ec0d188)
index, ident, ethernet = submit_native(200)
metadata = 0xf << 16 | 0x4010
before_rate = read(symbol('ppe_cpu_reasons') + 0xf * 4)
ppe_returns.append((ident, metadata))
wait_until(lambda: not ppe_returns, 'PPE FIFO was not acknowledged')
assert packet_status(ident) & 0x20000000
assert read(symbol('ppe_cpu_reasons') + 0xf * 4) == before_rate + 1
assert read(0x1ec0d188) == head and packet_status(ident) & 0x80000000
# An outstanding host copy must retain the buffer and descriptor ownership.
dma_hold.add(0)
tx_complete(index)
wait_until(lambda: 0 in dma_pending, 'exception did not use the owning RX hart')
assert packet_status(ident) & 0x80000000
assert not read(host[0]+head*24) & 1
assert read(0x1ec0d188) == head
dma_hold.clear()
wait_until(lambda: read(0x1ec0d188) == (head+1)%512, 'exception not delivered')
check_host(0, head, ethernet, len(ethernet), 1, 1, True,
           vendor=0x10000000 | metadata)
assert read(host[0]+head*24+4) == 0  # check_host returned the descriptor
assert not packet_status(ident)

# QDMA may finish first: its descriptor is reusable, but its packet is not.
head = read(0x1ec0d188)
index, ident, _ = submit_native(201)
tx_complete(index)
assert packet_status(ident) & 0x80000000
ppe_returns.append((1<<16 | ident, 0))
wait_until(lambda: not packet_status(ident), 'forwarded packet not returned')
assert read(0x1ec0d188) == head, 'bound forwarding generated a host IRQ'

# A duplicate keepalive is not injected into the host as fresh traffic.
index, ident, _ = submit_native(202)
before_keepalive = read(symbol('ppe_cpu_reasons') + 0x15 * 4)
ppe_returns.append((ident, 0x15 << 16 | 0x4011))
tx_complete(index)
wait_until(lambda: not packet_status(ident), 'keepalive buffer leaked')
assert read(symbol('ppe_cpu_reasons') + 0x15 * 4) == before_keepalive + 1
assert read(0x1ec0d188) == head

# Backpressure cannot overwrite a host-owned descriptor or leak a PPE buffer.
load(host[0]+head*24, (1).to_bytes(4, 'little'))
index, ident, _ = submit_native(203)
ppe_returns.append((ident, metadata))
tx_complete(index)
wait_until(lambda: not packet_status(ident), 'full host ring leaked a packet')
assert read(host[0]+head*24) == 1 and read(0x1ec0d188) == head
load(host[0]+head*24, b'\0'*4)

# Delayed exceptions from an earlier authorization/key generation are discarded.
index, ident, _ = submit_native(204)
control_data(34, struct.pack('<III8s8s', 1, 0, 3, mac, bytes.fromhex('020102030405')))
control_data(34, struct.pack('<III8s8s', 1, 1, 3, mac, bytes.fromhex('020102030405')))
ppe_returns.append((ident, metadata))
tx_complete(index)
wait_until(lambda: not packet_status(ident), 'stale station packet leaked')
assert read(0x1ec0d188) == head, 'old authorization reached a reused station'

# Invalid FIFO tokens are acknowledged without freeing unrelated RX buffers.
before_invalid = read(state('ppe_invalid'))
ppe_returns.extend(((0x7fff, 0), (ident, metadata)))
wait_until(lambda: not ppe_returns, 'invalid FIFO entries wedged the return queue')
assert read(state('ppe_invalid')) == before_invalid+2

# A separate PCIe radio routes exceptions through its own host/GDMA queue.
control_data(34, struct.pack('<III8s8s', 150, 1, 4, mac,
                            bytes.fromhex('020102030405')), band=1)
control_data(4, struct.pack('<IIII', 150, 0, 300, 64), band=1)
set_cmd(12, 0, 1)
index = read(0x1fb541c8)
head1 = read(0x1ec0d198)
blob, header = rx_frame(300)
blob = bytearray(blob)
struct.pack_into('<I', blob, 4, 150 | 1<<14)
packet(1, blob)
ident = read(ids+index*2, 2)
assert ident != 0xffff
ppe_returns.append((ident, metadata))
tx_complete(index)
wait_until(lambda: read(0x1ec0d198) == (head1+1)%512,
           'queue-1 native exception not delivered')
assert read(host[1]+head1*24+4) & 0xffff == 4
check_host(1, head1, blob[header:], len(blob)-header, 1, 1, True,
           vendor=0x10000000 | metadata)
assert read(0x1ec0d188) == head, 'queue-1 return advanced queue 0'
control_data(34, struct.pack('<III8s8s', 150, 0, 4, mac,
                            bytes.fromhex('020102030405')), band=1)

# STOP refuses to release host storage while the PPE still owns a packet.
control_data(4, struct.pack('<IIII', 1, 0, 210, 64))
index, ident, _ = submit_native(210)
set_cmd(2, 0, expected=0)
assert packet_status(ident) & 0x80000000
ppe_returns.append((ident, metadata))
tx_complete(index)
wait_until(lambda: not packet_status(ident), 'STOP could not retire a late return')
set_cmd(2, 0)
set_cmd(2, 1)
# An unavailable/deinitialized PPE must fall back to raw host reception.
ppe_request([1, 2])
head = read(0x1ec0d188)
before_tail = read(0x1fb541c8)
blob, _ = rx_frame(211)
packet(0, blob)
check_host(0, head, blob, len(blob), 1, 1, True, vendor=0x80000000)
assert read(0x1fb541c8) == before_tail
ppe_request([1, 1, 0, 0, 3, 0, 0])
index, ident, _ = submit_native(212)
ppe_returns.append((1<<16 | ident, 0))
tx_complete(index)
wait_until(lambda: not packet_status(ident), 'PPE resume lost packet ownership')
assert read(symbol('sram_alloc_offset')) <= 0x60000
assert all(not packet_status(i) for i in range(5600))
print('PASS: native PPE FIFO, both completion orders, DMA ownership, no host copy for bound traffic, Ethernet/FoE exceptions, keepalive suppression, backpressure, stale STA and STOP lifetime')
