#!/usr/bin/env python3
"""Execute the core-only image in a small RV32 model, not a silicon simulator."""
from pathlib import Path
import hashlib
import re
import sys
import struct

root = Path(sys.argv[1])
image = (root / 'npu_rv32.bin').read_bytes()
assert hashlib.sha256(image[:0x108]).hexdigest().startswith('1752af1bbce53493')
assert struct.unpack_from('<8I', image, 0x10c) == (
    0x4e414c43, 0x0052454b, 1, 1, 4, 0, 0x20000, 0)
mem = {}
def load(addr, data):
    for i, byte in enumerate(data): mem[addr+i] = byte
load(0x84000000, image)
load(0x1e900000, (root / 'npu_data.bin').read_bytes())
# Dirty shared BSS first so the test actually exercises primary initialization.
linkmap = (root / 'firmware.map').read_text()
def symbol(name):
    return int(re.search(r'0x([0-9a-f]+)\s+' + name + r'\s+=', linkmap).group(1), 16)
start, end = symbol('__bss_start'), symbol('__bss_end')
load(start, b'\xa5' * (end - start))

def read(addr, n=4):
    return sum(mem.get(addr+i, 0) << (i*8) for i in range(n))
def write(addr, val, n=4):
    if 0x1ec00000 <= addr < 0x20000000:
        assert addr in (0x1ec0c000, 0x1ec0c030, 0x1ec0c034, 0x1ec0c038, 0x1ec0c03c, 0x1ec0c1bc), hex(addr)
    if addr == 0x1ec0c000: val = read(addr) & ~val  # W1C model
    for i in range(n): mem[addr+i] = (val >> (i*8)) & 255

def sx(n, bits): return n-(1<<bits) if n & (1<<(bits-1)) else n
MASK = 0xffffffff
class Hart:
    def __init__(self, hart):
        self.hart=hart; self.r=[0]*32; self.pc=0x84000108
        self.csr={0xf14:hart,0x7c0:0x139,0x9c0:0x7f}
    def step(self):
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
            if f==0:
                assert w==0x73
                self.csr.update({0x341:pc,0x342:11,0x343:0});nxt=self.csr[0x305]
            elif f in (1,5):self.csr[c]=v
            elif f in (2,6):self.csr[c]=old|v
            elif f in (3,7):self.csr[c]=old&~v
            r[rd]=old
        elif op==0xf:pass
        else:raise AssertionError((hex(pc),hex(w)))
        self.r=[v&MASK for v in r];self.r[0]=0;self.pc=nxt&MASK

harts=[Hart(i) for i in range(4)]
def tick(n):
    for _ in range(n):
        for h in harts:h.step()
for _ in range(10000):
    tick(1)
    if all(read(0x840f000c+i*64)==1 for i in range(4)):break
else:raise AssertionError('shared runtime startup did not complete')
for i in range(4):
    assert read(0x840f0004+i*64)==4
    assert read(0x840f0034+i*64)==0x434f5200|i
assert read(0x1e906800)==0x4e444247, 'common debug subsystem did not initialize'
tick(100)

def request(cmd, target=0, func=7, ptr=0x840f1000):
    p=0x840f1000;q=0x1ec0c030
    for i in range(16):write(p+i*4,0)
    for i,v in enumerate([0x4e50444d,1,123,cmd,0xffffffff]):write(p+i*4,v)
    write(p+48,target);write(q,ptr);write(q+4,64);write(q+8,read(q+8)+1);write(q+12,(func<<11)|1)
    for _ in range(10000):
        tick(1)
        if read(q+12)&2:return read(q+12), read(p+16)
    raise AssertionError('shared mailbox did not reply')
for cmd in (0,1):
    control,status=request(cmd);assert control&0x1e==6 and status==0
    assert read(0x840f1014)==0x20000
    assert read(0x840f1034)==0, "unexpected datapath capabilities"
for kw in (dict(func=0),dict(func=15),dict(ptr=0),dict(cmd=3)):
    args=dict(cmd=0);args.update(kw);control,_=request(**args);assert control&0x1e==2
control,status=request(2,1);assert control&0x1e==6 and status==0
tick(300)
assert read(0x840f004c)==2 and read(0x840f0054)==11
old=[read(0x840f0010+i*64) for i in range(4)];tick(500)
new=[read(0x840f0010+i*64) for i in range(4)]
assert old[1]==new[1] and all(old[i]!=new[i] for i in (0,2,3))
control,status=request(1);assert control&0x1e==6 and status==0
print('PASS: shared npu_init/core_dispatch/debug/mailbox, isolated stacks, malformed requests and terminal worker trap')
