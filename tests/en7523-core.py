#!/usr/bin/env python3
"""Execute the EN7523 SDK mailbox transport and native IRQ runtime."""
from pathlib import Path
import hashlib
import re
import sys
import struct

root = Path(sys.argv[1])
quiet = '--quiet' in sys.argv[2:]
staggered = '--staggered' in sys.argv[2:]
wifi = root.name.endswith('_MT7915')
variant = root.name.lower().replace('_nowifi', '')
code = root.parent / (variant + '_npu_rv32.bin')
data = root.parent / (variant + '_npu_data.bin')
image = (code if code.exists() else root / 'npu_rv32.bin').read_bytes()
assert hashlib.sha256(image[:0x108]).hexdigest() == '1752af1bbce53493c6da3f36203afacb2c50edcaf79cc471cb5a9a9dd106bf98'
assert b'CLANKER\x00' not in image[0x108:0x130]
mem = {}
mutex_owner = {}
current_hart = None
uart = bytearray()
steps = 0
completed = [0] * 4
active_irq = [None] * 4
injected = [set() for _ in range(4)]
class BusWait(Exception):
    pass
def load(addr, data):
    for i, byte in enumerate(data): mem[addr+i] = byte
load(0x84000000, image)
load(0x1e900000, (data if data.exists() else root / 'npu_data.bin').read_bytes())
# Dirty shared BSS first so the test actually exercises primary initialization.
linkmap = (root / 'firmware.map').read_text()
def symbol(name):
    return int(re.search(r'0x([0-9a-f]+)\s+' + name + r'(?:\s+=|\s*\n)', linkmap).group(1), 16)
start, end = symbol('__bss_start'), symbol('__bss_end')
load(start, b'\xa5' * (end - start))
load(0x1ec0c194, int(quiet).to_bytes(4, 'little'))
load(0x1ec08004, (1 if staggered else 15).to_bytes(4, 'little'))

def read(addr, n=4):
    assert not 0x0c000000 <= addr < 0x0c300000, hex(addr)
    if addr == 0x1fbf0014:
        return 1 << 5
    if 0x1ec08800 <= addr < 0x1ec08c00 and (addr & 255) == 0:
        hart = (addr - 0x1ec08800) // 256
        assert read(0x1ec08004) == 15, 'cluster probe ran before secondary release'
        return harts[hart].pc if read(0x1ec08004) & (1 << hart) else 0
    if 0x1ec09800 <= addr < 0x1ec09900:
        owner = mutex_owner.get((addr - 0x1ec09800) // 4)
        return 0 if owner is None else 0x10000 | owner << 8
    return sum(mem.get(addr+i, 0) << (i*8) for i in range(n))
def write(addr, val, n=4):
    assert not 0x0c000000 <= addr < 0x0c300000, hex(addr)
    assert not 0x840f0000 <= addr < 0x840f3000, 'removed telemetry page accessed'
    if 0x1ec00000 <= addr < 0x20000000:
        assert ((wifi and (0x1fa90030 <= addr <= 0x1fa9003c or
                          0x1ec0d180 <= addr <= 0x1ec0d19c)) or 0x1ec09018 <= addr < 0x1ec09098 or
                0x1ec09098 <= addr < 0x1ec090d8 or
                0x1ec09118 <= addr < 0x1ec09128 or
                0x1ec09138 <= addr < 0x1ec09148 or
                0x1ec09b00 <= addr < 0x1ec09d00 or
                0x1ec0c000 <= addr <= 0x1ec0c014 or
                0x1ec0c030 <= addr <= 0x1ec0c06c or
                0x1ec0c0b0 <= addr <= 0x1ec0c0bc or
                addr in (0x1ec0c194, 0x1ec0c1bc, 0x1ec10100, 0x1ec10104,
                         0x1fbf0000)), hex(addr)
    if 0x1ec09b00 <= addr < 0x1ec09c00:
        ident = (addr - 0x1ec09b00) // 4
        owner = mutex_owner.get(ident)
        assert owner != current_hart, 'recursive hardware mutex acquisition'
        if owner is not None:
            raise BusWait()
        assert val == current_hart << 8 | 0x40
        mutex_owner[ident] = current_hart
    if 0x1ec09c00 <= addr < 0x1ec09d00:
        ident = (addr - 0x1ec09c00) // 4
        assert mutex_owner.get(ident) == current_hart
        assert val == current_hart << 8
        mutex_owner.pop(ident)
    if addr == 0x1fbf0000:
        assert mutex_owner.get(15) == current_hart, 'console must hold printf mutex 15'
        uart.append(val & 255)
    if addr == 0x1ec0c000: val = read(addr) & ~val
    if addr == 0x1ec10100:
        val = (read(addr) & 0xffff0000 & ~(val & 0xffff0000)) | (val & 0xffff)
    if addr in (0x1ec0c038, 0x1ec0c048, 0x1ec0c058, 0x1ec0c068):
        load(0x1ec0c000, (read(0x1ec0c000) | 1 << ((addr-0x1ec0c038)//16)).to_bytes(4,'little'))
    for i in range(n): mem[addr+i] = (val >> (i*8)) & 255
    # Model the host's normal queue-8 completion for asynchronous SDK requests.
    if addr == 0x1ec0c0b8:
        load(0x1ec0c0bc, (read(0x1ec0c0bc) | 2).to_bytes(4, 'little'))

def sx(n, bits): return n-(1<<bits) if n & (1<<(bits-1)) else n
MASK = 0xffffffff
class Hart:
    def __init__(self, hart):
        self.hart=hart; self.r=[0]*32; self.pc=0x84000108
        self.csr={0xf14:hart,0x7c0:0x139,0x9c0:0x7f,0x301:0x40001100,0xbc0:0}
    def step(self):
        global current_hart
        current_hart = self.hart
        # Virtual cycle samples exercise delay loops without claiming cycle accuracy.
        if self.csr[0xbc0] & 1:
            self.csr[0xb00] = (self.csr.get(0xb00, 0) + 1000) & MASK
        # Model external delivery only when both the CPU and source are enabled.
        if (self.csr.get(0x300, 0) & 8 and self.csr.get(0x304, 0) & 0x800 and
                self.csr.get(0x7c0, 0) & 0x40):
            sources = set(injected[self.hart])
            if read(0x1ec0c000) & (1 << self.hart) and read(0x1ec0c008 + self.hart * 4) & (1 << self.hart):
                sources.add(8 + self.hart)
            if read(0x1ec10100) & 0x10000:
                sources.add(18)
            enabled = [src for src in sources if read(0x1ec09098 + self.hart * 16 + (src // 32) * 4) & 1 << (src & 31)]
            if enabled:
                assert active_irq[self.hart] is None
                src = min(enabled)
                active_irq[self.hart] = src
                self.csr[0x5c0] = src
                self.csr[0x341] = self.pc
                self.csr[0x342] = 0x8000000b
                self.csr[0x300] = (self.csr.get(0x300, 0) & ~0x88) | 0x1880
                self.pc = self.csr[0x305]
        r=self.r; pc=self.pc; w=read(pc); op=w&127; rd=w>>7&31
        f=w>>12&7; a=w>>15&31; b=w>>20&31; imm=sx(w>>20,12); nxt=pc+4
        if op==0x37:r[rd]=w&0xfffff000
        elif op==0x17:r[rd]=pc+(w&0xfffff000)
        elif op==0x13:
            if f==0:r[rd]=r[a]+imm
            elif f==1:r[rd]=r[a]<<(b&31)
            elif f==2:r[rd]=int(sx(r[a],32)<imm)
            elif f==3:r[rd]=int(r[a]<(imm&MASK))
            elif f==4:r[rd]=r[a]^(imm&MASK)
            elif f==5:r[rd]=(sx(r[a],32) if w>>30&1 else r[a])>>(b&31)
            elif f==6:r[rd]=r[a]|(imm&MASK)
            elif f==7:r[rd]=r[a]&(imm&MASK)
        elif op==0x33:
            if w>>25==1:
                if f==0:r[rd]=r[a]*r[b]
                elif f==1:r[rd]=(sx(r[a],32)*sx(r[b],32))>>32
                elif f==2:r[rd]=(sx(r[a],32)*r[b])>>32
                elif f==3:r[rd]=(r[a]*r[b])>>32
                elif f==4:r[rd]=MASK if not r[b] else int(sx(r[a],32)/sx(r[b],32))
                elif f==5:r[rd]=MASK if not r[b] else r[a]//r[b]
                elif f==6:r[rd]=r[a] if not r[b] else sx(r[a],32)-int(sx(r[a],32)/sx(r[b],32))*sx(r[b],32)
                elif f==7:r[rd]=r[a] if not r[b] else r[a]%r[b]
                self.r=[v&MASK for v in r];self.r[0]=0;self.pc=nxt&MASK;return
            if f==0:r[rd]=r[a]-r[b] if w>>30&1 else r[a]+r[b]
            elif f==1:r[rd]=r[a]<<(r[b]&31)
            elif f==2:r[rd]=int(sx(r[a],32)<sx(r[b],32))
            elif f==3:r[rd]=int(r[a]<r[b])
            elif f==4:r[rd]=r[a]^r[b]
            elif f==5:r[rd]=(sx(r[a],32) if w>>30&1 else r[a])>>(r[b]&31)
            elif f==6:r[rd]=r[a]|r[b]
            elif f==7:r[rd]=r[a]&r[b]
        elif op==3:
            n={0:1,1:2,2:4,4:1,5:2}[f];v=read((r[a]+imm)&MASK,n)
            r[rd]=sx(v,n*8) if f<4 else v
        elif op==0x23:
            off=sx((w>>25<<5)|(w>>7&31),12);addr=(r[a]+off)&MASK
            if 0x840d0000<=addr<0x840e0000:
                assert 0x840d0000+self.hart*0x4000<=addr<0x840d0000+(self.hart+1)*0x4000
            write(addr,r[b],{0:1,1:2,2:4}[f])
        elif op==0x63:
            off=sx(((w>>31)<<12)|((w>>7&1)<<11)|((w>>25&63)<<5)|((w>>8&15)<<1),13)
            take={0:r[a]==r[b],1:r[a]!=r[b],4:sx(r[a],32)<sx(r[b],32),
                  5:sx(r[a],32)>=sx(r[b],32),6:r[a]<r[b],7:r[a]>=r[b]}[f]
            if take:nxt=pc+off
        elif op==0x6f:
            off=sx(((w>>31)<<20)|((w>>12&255)<<12)|((w>>20&1)<<11)|((w>>21&1023)<<1),21)
            r[rd]=pc+4;nxt=pc+off
        elif op==0x67:
            assert f==0;nxt=(r[a]+imm)&~1;r[rd]=pc+4
        elif op==0x73:
            c=w>>20;old=self.csr.get(c,0);v=r[a] if f<5 else a
            if c == 0xb00:
                assert self.csr[0xbc0] & 1, f'hart {self.hart}: mcycle read before counter enable'
            if f==0:
                if w == 0x30200073:
                    status = self.csr.get(0x300, 0)
                    self.csr[0x300] = (status & ~0x1888) | ((status >> 4) & 8) | 0x80
                    nxt = self.csr[0x341]
                else:
                    assert w==0x73
                    self.csr.update({0x341:pc,0x342:11,0x343:0})
                    self.csr[0x300] = (self.csr.get(0x300,0) & ~8) | 0x1800
                    nxt=self.csr[0x305]
            elif f in (1,5):self.csr[c]=v
            elif f in (2,6):self.csr[c]=old|v
            elif f in (3,7):self.csr[c]=old&~v
            if c == 0x5c2:
                assert v == active_irq[self.hart]
                active_irq[self.hart] = None
                completed[self.hart] += 1
                injected[self.hart].discard(v)
            r[rd]=old
        elif op==0xf:pass
        else:raise AssertionError((hex(pc),hex(w)))
        self.r=[v&MASK for v in r];self.r[0]=0;self.pc=nxt&MASK

if wifi:
    exec((Path(__file__).parent / 'en7523-memcpy.py').read_text())
harts=[Hart(i) for i in range(4)]
def tick(n):
    global steps
    for _ in range(n):
        steps += 1
        # Model the v1 host releasing secondary harts after the primary begins init.
        if staggered and steps in (50000, 100000, 150000):
            mask = {50000:3, 100000:7, 150000:15}[steps]
            load(0x1ec08004, mask.to_bytes(4, 'little'))
        for h in harts:
            if not read(0x1ec08004) & (1 << h.hart):
                continue
            try:
                h.step()
            except BusWait:
                pass
NDBG = 0x1e906800
def hart_word(hart, word):
    return read(NDBG + 0x80 + hart * 48 + word * 4)
for _ in range(2000000 if wifi else 400000):
    tick(1)
    if (read(NDBG) == 0x4e444247 and all(hart_word(i, 1) for i in range(4)) and
            (not wifi or read(symbol('en7523_wifi_ready')))):
        break
else:
    raise AssertionError('shared runtime startup did not complete')
assert read(0x1ec0c1bc) == 0xcccccccc
assert read(0x1ec10104) == 2500000, '500 MHz NPU / 2, ten millisecond tick'
for i in range(4):
    assert harts[i].csr[0xbc0] == 1, 'per-hart cycle counter enabled'
    assert harts[i].csr[0x304] == 0x800
    assert harts[i].csr[0x7c0] == 0x179
    if not wifi or i in (0, 3):
        assert hart_word(i, 0) == 0x49444c45, 'common idle loop'
assert read(0x1ec0c004) == 256, 'SDK notifications route to the host'
assert read(0x1ec0c194) == int(quiet), 'host print gate must be preserved'
assert not mutex_owner
console = uart.decode('ascii').replace('\r', '')
if quiet:
    assert not console, 'MIB21 must suppress SoC UART output'
else:
    assert 'bite my shiny metal NPU' in console
    assert 'core freq at 500 MHz' in console
    assert ('NPU Version: TLB7.' + ('9' if wifi else '8') + '.0.0_v003.' + ('MT7915.' if wifi else 'NOWIFI.')) in console
    assert 'cores enabled f, running f' in console
    assert 'Error:' not in console
    for hart in range(4):
        assert f'[C{hart}]core {hart}: RV32IM' in console
    assert console.count('self-test ok (1b972363,') == 4
    Path(root / 'boot-model.log').write_text(console)

def request(cmd=0, msg_type=3, func=0, ptr=0x85012000, arg=1, size=12,
            ifindex=0, value=0xffffffff):
    q=0x1ec0c030
    # The caller owns this dynamic buffer; no fixed diagnostic page is reserved.
    payload = ptr if 0x80000000 <= ptr <= 0xbfffffe0 else 0x85012000
    for i, value in enumerate(((msg_type << 4) | ifindex, cmd,
                               value, 0x12345678)):
        load(payload+i*4, value.to_bytes(4,'little'))
    write(q,ptr);write(q+4,size);write(q+12,(func<<11)|arg)
    before = completed[0]
    write(q+8,read(q+8)+1)
    for _ in range(300000 if wifi else 10000):
        tick(1)
        if completed[0] > before:
            return read(q+12), read(payload+8)
    raise AssertionError('SDK mailbox did not complete its interrupt')
for ptr in (0x85012000, 0x8a004000):
    control,info=request(ptr=ptr)
    assert control&0x1e==6 and info==0
    assert read(ptr)==0x30 and read(ptr+4)==0
    assert read(ptr+12)==0x12345678, 'response exceeded SDK payload'
    for ifindex in (0, 3, 15):
        control,version=request(cmd=10, ptr=ptr, ifindex=ifindex)
        assert control&0x1e==6 and version==(0x00070009 if wifi else 0x00070008)
        assert read(ptr)==0x30|ifindex and read(ptr+4)==10
        assert read(ptr+12)==0x12345678, 'version exceeded its response word'
# Wire values must match the host enum, not Linux's negative errno numbers.
EINVAL, EOPNOTSUPP, ENODEV = 2, 3, 4
failures = (
    (dict(func=7), ENODEV),
    (dict(func=8), EINVAL),
    (dict(func=15), EINVAL),
    (dict(ptr=0), EINVAL),
    (dict(ptr=0x85012001), EINVAL),
    (dict(ptr=0xbffffffc), EINVAL),
    (dict(ptr=0xc0000000), EINVAL),
    (dict(size=0), EINVAL),
    (dict(size=8), EINVAL),
    (dict(msg_type=0), EINVAL),
    (dict(msg_type=5), EINVAL),
    (dict(cmd=1), EOPNOTSUPP),
    (dict(cmd=5), EOPNOTSUPP),
    (dict(cmd=8), EOPNOTSUPP),
    (dict(cmd=9), EOPNOTSUPP),
    (dict(cmd=11), EOPNOTSUPP),
    (dict(cmd=0xffffffff), EOPNOTSUPP),
    (dict(msg_type=1), EOPNOTSUPP),
    (dict(msg_type=1, cmd=30), EOPNOTSUPP),
    (dict(msg_type=1, cmd=31), EOPNOTSUPP),
    (dict(msg_type=1, cmd=32), EOPNOTSUPP),
    (dict(msg_type=1, cmd=33), EOPNOTSUPP),
    (dict(msg_type=1, cmd=34), EOPNOTSUPP),
    (dict(msg_type=2, cmd=10), EOPNOTSUPP),
    (dict(msg_type=4, cmd=10), EOPNOTSUPP),
    (dict(cmd=10, size=8), EINVAL),
    (dict(arg=0x21), EOPNOTSUPP),
)
for kwargs, expected in failures:
    if wifi and kwargs == dict(msg_type=1):
        expected = EINVAL
    control,info=request(**kwargs)
    assert control&0x1e == (expected<<2)|2, (kwargs,control,expected)
    assert info==0xffffffff, 'rejection must leave the payload untouched'
# A missing callback must replace stale success bits with ENODEV.
control,_=request(func=7,arg=0x1d)
assert control&0x1e == (ENODEV<<2)|2
# Successful and failed asynchronous replies carry the same codes through queue 8.
for kwargs, expected in (({},1), (dict(cmd=10),1), (dict(ptr=0),EINVAL),
                         (dict(cmd=5),EOPNOTSUPP), (dict(func=7),ENODEV),
                         (dict(func=15),EINVAL)):
    before=read(0x1ec0c0b8)
    _,info=request(arg=0,**kwargs)
    if kwargs.get('cmd')==10:
        assert info==(0x00070009 if wifi else 0x00070008)
    assert read(0x1ec0c0b0)==0
    assert read(0x1ec0c0b4)==expected
    assert read(0x1ec0c0b8)==before+1 and not mutex_owner
for _ in range(3):
    load(0x1ec10100, (read(0x1ec10100) | 0x10000).to_bytes(4,'little'))
    tick(1500)
assert read(symbol('timer_int_count')) == 3
assert completed[2] == 3
assert read(0x1ec10100) == 1, 'timer enable survives W1C'
# Unregistered per-hart callbacks reject without using a different hart's table.
q = 0x1ec0c040
write(q+12,1);write(q+8,1);tick(1500)
assert read(q+12)&0x1e==(ENODEV<<2)|2 and completed[1]==1
for src in (0,127):
    addr=0x1ec09098+3*16+(src//32)*4
    write(addr,read(addr)|1<<(src&31))
    injected[3].add(src);tick(1000)
    assert not read(addr)&1<<(src&31)
assert completed[3]==2
if wifi:
    exec((Path(__file__).parent / 'en7523-wifi.py').read_text())
# Inject a real exception into the model; no firmware fault-injection opcode exists.
h=harts[1]
h.csr.update({0x341:h.pc,0x342:11,0x343:0})
h.csr[0x300]=(h.csr.get(0x300,0)&~8)|0x1800
h.pc=h.csr[0x305]
tick(1000)
assert hart_word(1,6)==1 and hart_word(1,7)==11
old=[hart_word(i,1) for i in range(4)];tick(20000)
new=[hart_word(i,1) for i in range(4)]
assert old[1]==new[1] and all(old[i]!=new[i] for i in (0,2,3))
control,info=request()
assert control&0x1e==6 and info==0
print('PASS: common init, host UART gate, four self-tests, four harts, SDK mailbox statuses 2/3/4, async notification, timer W1C, native IRQ completion, exception isolation' + (' (quiet)' if quiet else '') + (' (staggered boot)' if staggered else ''))
