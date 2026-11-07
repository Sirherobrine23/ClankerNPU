# Executed by en7523-core.py after boot, using the linked RV32 image.

def set_cmd(cmd, value, band=0, expected=1):
    ctrl,_=request(cmd=cmd, msg_type=1, ifindex=band, value=value)
    assert ctrl&0x1e==(expected<<2)|2,(cmd,value,band,hex(ctrl))

set_cmd(8,0x84d00000)
set_cmd(5,1)
set_cmd(14,1)
set_cmd(12,0,expected=EOPNOTSUPP)
set_cmd(12,1)
set_cmd(4,1,expected=EOPNOTSUPP)
set_cmd(1,1537,expected=EINVAL)
set_cmd(2,1,expected=ENODEV)
rings=[]
for band in range(2):
    set_cmd(0,0x20004500+band*16,band)
    set_cmd(1,1536,band)
    ctrl,ring=request(cmd=4,ifindex=band)
    assert ctrl&0x1e==6 and 0x1e800000<=ring<0x1e860000
    rings.append(ring)
assert rings[0]-rings[1]==0x6020
ids=[read(ring+i*16) for ring in rings for i in range(1536)]
assert len(set(ids))==3072
assert all(0x84d00000<=addr<0x86300000 and not addr&4095 for addr in ids)
assert read(0x1fa90038)==rings[1] and read(0x1fa9003c)==rings[0]+1536*16

host=[0x88000000,0x88010000]
buffers=[0x89000000,0x89400000]
for band in range(2):
    for i in range(512):
        load(host[band]+i*24+12,(buffers[band]+i*4096).to_bytes(4,'little'))
    load(0x1ec0d180+band*16,host[band].to_bytes(4,'little'))
    load(0x1ec0d184+band*16,(512).to_bytes(4,'little'))
for band in range(2):
    set_cmd(2,1,band)

def packet(band, blob, last=True):
    index=read(symbol('en7523_wifi_index')+band*4)
    desc=rings[band]+index*16
    addr=read(desc)
    load(addr,blob)
    load(desc+4,(0x80000000 | (0x40000000 if last else 0) |
                 len(blob)<<16).to_bytes(4,'little'))
    for _ in range(60000):
        tick(1)
        if (not read(desc+4)&0x80000000 and
                read(symbol('en7523_wifi_index')+band*4) == (index+1)%1536):
            break
    else:
        raise AssertionError('WiFi descriptor was not returned')
    assert read(0x20004508+band*16)==index
    assert read(desc+4)==3500<<16
    return desc

def body(length):
    return length.to_bytes(4,'little')+bytes((i*13+7)&255 for i in range(length-4))

def check_host(band,index,blob,total,part,parts,last):
    d=host[band]+index*24
    ctrl=read(d)
    assert ctrl&1 and (ctrl>>1)&0x3fff==total
    assert (ctrl>>15)&0x3fff==len(blob)
    assert bool(ctrl&1<<29)==last
    info=read(d+4)
    assert (info>>26)&7==part and (info>>29)&7==parts
    addr=read(d+12)
    assert bytes(read(addr+i,1) for i in range(len(blob)))==blob
    assert read(d+8)==0 and read(d+16)==0 and read(d+20)==0
    load(d,b'\0'*12)

for band in range(2):
    blob=body(1536+band*64)
    packet(band,blob)
    check_host(band,0,blob,len(blob),1,1,True)
    assert read(0x1ec0d188+band*16)==1

# Large frames use multiple host descriptors, preserving every raw RX byte.
blob=body(7400)
for i,offset in enumerate(range(0,len(blob),3500)):
    segment=blob[offset:offset+3500]
    last=offset+len(segment)==len(blob)
    packet(0,segment,last)
    check_host(0,1+i,segment,len(blob),1+i,3,last)

# A host-owned descriptor must never be overwritten.
head=read(0x1ec0d188)
load(host[0]+head*24,(1).to_bytes(4,'little'))
packet(0,body(100))
assert read(0x1ec0d188)==head and read(host[0]+head*24)==1
load(host[0]+head*24,b'\0'*4)

# Both producer indices wrap at their actual hardware ring sizes.
load(symbol('en7523_wifi_index')+4,(1535).to_bytes(4,'little'))
load(0x1ec0d198,(511).to_bytes(4,'little'))
blob=body(128)
packet(1,blob)
check_host(1,511,blob,len(blob),1,1,True)
assert read(symbol('en7523_wifi_index')+4)==0 and read(0x1ec0d198)==0

# STOP must also wait for a copy that was already in flight.
band=0
index=read(symbol('en7523_wifi_index'))
desc=rings[0]+index*16
blob=body(3500)
head=read(0x1ec0d188)
load(read(desc),blob)
load(desc+4,(0xc0000000 | len(blob)<<16).to_bytes(4,'little'))
tick(1500)
assert read(symbol('en7523_wifi_busy'))==1 and read(desc+4)&0x80000000
set_cmd(2,0)
check_host(0,head,blob,len(blob),1,1,True)
assert read(symbol('en7523_wifi_enabled'))==0

# After STOP acknowledges, a pending radio descriptor cannot reach freed host memory.
for band in range(2):
    set_cmd(2,0,band)
    index=read(symbol('en7523_wifi_index')+band*4)
    desc=rings[band]+index*16
    load(read(desc),body(64))
    load(desc+4,(0xc0000000 | 64<<16).to_bytes(4,'little'))
    host_index=read(0x1ec0d188+band*16)
    tick(1000)
    assert read(desc+4)&0x80000000
    assert read(0x1ec0d188+band*16)==host_index
    assert read(symbol('en7523_wifi_busy')+band*4) in (0,1)
print('PASS: MT7915 dual-band RX, 24-byte host ABI, raw bytes, fragments, backpressure, index wrap, STOP quiescence')
