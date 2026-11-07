# Executed by en7523-core.py with the linked RV32 image and table MMIO model.

def ppe_request(words, expected=1, ptr=0x85018000, size=None):
    payload = b''.join((word & 0xffffffff).to_bytes(4, 'little') for word in words)
    load(ptr, payload)
    q = 0x1ec0c030
    write(q, ptr)
    write(q+4, len(payload) if size is None else size)
    write(q+12, 5 << 11 | 1)
    before = completed[0]
    write(q+8, read(q+8)+1)
    for _ in range(200000):
        tick(1)
        if completed[0] > before:
            assert read(q+12)&0x1e == expected << 2 | 2, (words, hex(read(q+12)))
            return
    raise AssertionError('PPE mailbox did not complete')

load(0x1fb00064, (12 << 16).to_bytes(4, 'little'))
ppe_request([1, 3, 2, 4, 0], ENODEV)
ppe_request([1, 1], EINVAL)
ppe_request([0, 2], EINVAL)
ppe_request([1, 1, 0, 0, 4, 0, 0], EINVAL)
ppe_request([1, 1, 0, 0, 3, 0, 0])
assert read(symbol('hwnat_ready')) & 255 == 1
assert read(0x1fb50e04) and read(0x1fb50e00)&1
assert read(0x1fb50e1c)>>24 & 7 == 7, 'EN7523 has 512 SRAM entries'
assert read(0x1fb50e1c)&7 == 4, 'EN7523 keeps a 16K DRAM table'
ppe_request([1, 3, 2, 4, 0], ENODEV)
entry = tuple(0x11220000+i for i in range(20))
source = 0x85020000
load(source, b''.join(word.to_bytes(4, 'little') for word in entry))
ppe_request([1, 3, 1, 79, source], EINVAL)
ppe_request([1, 3, 1, 80, source+1], EINVAL)
ppe_request([1, 3, 1, 80, 0], EINVAL)
ppe_request([1, 3, 0, 80, source], EOPNOTSUPP)
ppe_request([1, 3, 1, 80, source])
assert not ppe_entries, 'staging must not commit an entry'
# An independent host window access cannot corrupt the software-staged entry.
for i in range(20):
    load(0x1fb50f20+i*4, (0xa5a5a5a5).to_bytes(4, 'little'))
ppe_request([1, 3, 2, 4, 511])
assert ppe_entries[511] == entry
ppe_request([1, 3, 2, 4, 511], ENODEV)
ppe_request([1, 3, 1, 80, source])
ppe_request([1, 3, 2, 4, 512], EINVAL)
ppe_request([1, 3, 3, 8192, source], EINVAL)
ppe_ack = False
ppe_request([1, 3, 2, 4, 1], 0)
assert 1 not in ppe_entries, 'timeout must not report a completed FoE commit'
ppe_ack = True
ppe_request([1, 3, 3, 512, source])
assert len(ppe_entries) == 512 and all(not any(e) for e in ppe_entries.values())
for words in ([1, 3, 99, 4, 0], [1, 4, 0, 0], [1, 5, 0, 0], [1, 99]):
    ppe_request(words, EOPNOTSUPP)
ppe_request([1, 2])
assert not read(symbol('hwnat_ready')) & 255
assert not read(0x1fb50e00)&1
ppe_request([1, 3, 1, 80, source], ENODEV)
print('PASS: PPE init/deinit, 512-entry SRAM / 16K DRAM, staged FoE commit, bounds, ACK timeout, complete table clear')
