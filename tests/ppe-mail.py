#!/usr/bin/env python3
"""Execute AN75xx PPE mailbox code with RV32IM test builds, without boot emulation."""
from pathlib import Path
import re
import sys

base = Path(sys.argv[1])
model = (Path(__file__).parent / 'en7523-core.py').read_text()
interpreter = model[model.index('class Hart:'):model.index('harts=[Hart(i)')]
for soc, family, entries in [('AN7552', 15, 512), ('AN7581', 14, 16384), ('AN7583', 16, 8192)]:
    directory = base / soc
    linkmap = (directory / 'firmware.map').read_text()
    def symbol(name):
        return int(re.search(r'0x([0-9a-f]+)\s+' + re.escape(name) + r'(?:\s+=|\s*\n)', linkmap).group(1), 16)
    mem, tables = {}, {}
    current_hart = None
    ppe_ack = True
    def physical(addr):
        return addr + 0x40000000 if 0x40000000 <= addr < 0x80000000 else addr
    def load(addr, data):
        addr = physical(addr)
        for i, byte in enumerate(data):
            mem[addr+i] = byte
    def read(addr, n=4):
        addr = physical(addr)
        return sum(mem.get(addr+i, 0) << (i*8) for i in range(n))
    def write(addr, value, n=4):
        addr = physical(addr)
        if addr in (0x1fb50f1c, 0x1fb51f1c) and value & 1 and ppe_ack:
            bank = int(addr == 0x1fb51f1c)
            index = value >> 8 & 0xffff
            assert index < entries and (not bank or soc == 'AN7581')
            tables[index] = tuple(read(addr+4+i*4) for i in range(20))
            value |= 0x80000000
        load(addr, (value & ((1 << (n*8))-1)).to_bytes(n, 'little'))
    MASK = 0xffffffff
    def sx(n, bits):
        return n-(1 << bits) if n & (1 << (bits-1)) else n
    active_irq = [None] * 4
    injected = [set() for _ in range(4)]
    completed = [0] * 4
    exec(interpreter)
    load(0x84000000, (base / (soc.lower() + '_npu_rv32.bin')).read_bytes())
    load(symbol('__data_start'), (base / (soc.lower() + '_npu_data.bin')).read_bytes())
    load(0x1fb00064, (family << 16).to_bytes(4, 'little'))
    if soc == 'AN7581':
        load(0x1fb51e00, (1).to_bytes(4, 'little'))
    printf_pc = symbol('npu_printf')
    def call(words, expected=1, length=None):
        addr = 0x85010000
        load(addr, b''.join(w.to_bytes(4, 'little') for w in words))
        h = Hart(0)
        h.pc = symbol('hwnat_mail_dispatch')
        h.r[1], h.r[2] = 0xdead0000, 0x841f0000
        h.r[3] = symbol('__global_pointer$')
        h.r[10:12] = [addr, len(words)*4 if length is None else length]
        for _ in range(3000000):
            if h.pc == printf_pc:
                h.r[10] = 0
                h.pc = h.r[1]
            else:
                h.step()
            if h.pc == 0xdead0000:
                assert h.r[10] == expected, (soc, words, h.r[10], expected)
                return
        raise AssertionError((soc, 'PPE function did not return', hex(h.pc)))
    call([1, 1, 0, 0, 3, 0, 0])
    assert read(symbol('hwnat_ready'), 1) == 1
    assert read(0x1fb50e1c) >> 24 & 7 == (7 if soc == 'AN7552' else 3)
    source = 0x85020000
    entry = tuple(0x13370000+i for i in range(20))
    load(source, b''.join(w.to_bytes(4, 'little') for w in entry))
    call([1, 3, 1, 80, source])
    call([1, 3, 2, 4, 0])
    assert tables[0] == entry
    call([1, 3, 0 if soc == 'AN7581' else 1, 80, source])
    call([1, 3, 2, 4, entries-1])
    assert tables[entries-1] == entry
    call([1, 3, 2, 4, entries], 2)
    call([1, 3, 1, 80, source])
    ppe_ack = False
    call([1, 3, 2, 4, 1], 0)
    ppe_ack = True
    call([1, 3, 3, entries, source])
    assert len(tables) == entries and all(not any(e) for e in tables.values())
    call([1, 2])
    assert read(symbol('hwnat_ready'), 1) == 0
    print(f'PASS: {soc}, {entries} SRAM entries, host aliases, bank selection, FoE commit/clear and ACK timeout')
