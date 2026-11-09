# Executed by en7523-core.py after boot, using the linked RV32 image.

split = '--split' in sys.argv[2:]
order = (1, 0) if '--reverse' in sys.argv[2:] else (0, 1)
ports = (0, 1) if split else (1, 1)
radio_regs = (0x20004500, 0x22004500) if split else (0x20004500, 0x20004510)

def set_cmd(cmd, value, band=0, expected=1):
    ctrl,_=request(cmd=cmd, msg_type=1, ifindex=band, value=value)
    assert ctrl&0x1e==(expected<<2)|2,(cmd,value,band,hex(ctrl))

set_cmd(12,0)
set_cmd(4,1,expected=EINVAL)
set_cmd(1,1537,expected=EINVAL)
set_cmd(2,1,expected=ENODEV)
rings=[]
for band in range(2):
    ctrl,ring=request(cmd=4,ifindex=band)
    assert ctrl&0x1e==6 and 0x1e800000<=ring<0x1e860000
    rings.append(ring)
assert rings[0]-rings[1]==0x6020
host=[0x88000000,0x88010000]
buffers=[0x89000000,0x89400000]

def configure(band):
    set_cmd(8,0x84d00000,band)
    set_cmd(5,1,band)
    set_cmd(14,ports[band],band)
    set_cmd(12,1,band)
    set_cmd(0,radio_regs[band],band)
    set_cmd(1,1536,band)
    window=0x1fa90030+ports[band]*8
    assert read(window)==rings[1] and read(window+4)==rings[0]+1536*16
    set_cmd(8,0x84d00000,band)
    set_cmd(8,0x84d01000,band,expected=EINVAL)
    set_cmd(14,1-ports[band],band,expected=EINVAL)
    assert read(symbol('en7523_wifi_port')+band*4)==ports[band]

def publish(band):
    for i in range(512):
        load(host[band]+i*24+12,(buffers[band]+i*4096).to_bytes(4,'little'))
    load(0x1ec0d180+band*16,host[band].to_bytes(4,'little'))
    load(0x1ec0d184+band*16,(512).to_bytes(4,'little'))

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
                read(symbol('en7523_wifi_index')+band*4) == (index+1)%1536 and
                read(symbol('en7523_host_index')+band*4) == read(0x1ec0d188+band*16)):
            break
    else:
        raise AssertionError('WiFi descriptor was not returned')
    assert read(radio_regs[band]+8)==index
    assert read(desc+4)==3500<<16
    return desc

def body(length):
    return length.to_bytes(4,'little')+bytes((i*13+7)&255 for i in range(length-4))

def check_host(band,index,blob,total,part,parts,last,vendor=0):
    d=host[band]+index*24
    ctrl=read(d)
    assert ctrl&1 and (ctrl>>1)&0x3fff==total
    assert (ctrl>>15)&0x3fff==len(blob)
    assert bool(ctrl&1<<29)==last
    info=read(d+4)
    assert (info>>26)&7==part and (info>>29)&7==parts
    addr=read(d+12)
    assert bytes(read(addr+i,1) for i in range(len(blob)))==blob
    assert read(d+8)==vendor and read(d+16)==0 and read(d+20)==0
    load(d,b'\0'*12)

# Split consumers may probe in either order, without the other host ring.
if split:
    for n,band in enumerate(order):
        configure(band)
        set_cmd(2,1,band,expected=EINVAL)
        publish(band)
        set_cmd(2,1,band)
        blob=body(1536+band*65)
        packet(band,blob)
        check_host(band,0,blob,len(blob),1,1,True)
        other=1-band
        assert read(symbol('en7523_wifi_enabled')+band*4)==1
        if not n:
            assert read(0x1ec0d180+other*16)==0
            assert read(symbol('en7523_wifi_enabled')+other*4)==0
        else:
            assert read(symbol('en7523_wifi_enabled')+other*4)==1
else:
    for band in range(2):
        configure(band)
        publish(band)
    for band in range(2):
        set_cmd(2,1,band)
        blob=body(1536+band*65)
        packet(band,blob)
        check_host(band,0,blob,len(blob),1,1,True)
    assert read(0x1fa90030)==0 and read(0x1fa90034)==0

ids=[read(ring+i*16) for ring in rings for i in range(1536)]
assert len(set(ids))==3072
assert all(0x84d00000<=addr<0x86300000 and not addr&4095 for addr in ids)
for band in range(2):
    assert read(0x1ec0d188+band*16)==1

# Large frames use multiple host descriptors, preserving every raw RX byte.
blob=body(7400)
for i,offset in enumerate(range(0,len(blob),3500)):
    segment=blob[offset:offset+3500]
    last=offset+len(segment)==len(blob)
    packet(0,segment,last)
    if not last:
        assert read(0x1ec0d188)==1, 'incomplete frame reached the host'
for i,offset in enumerate(range(0,len(blob),3500)):
    segment=blob[offset:offset+3500]
    check_host(0,1+i,segment,len(blob),1+i,3,i==2)

# A host-owned descriptor must never be overwritten.
head=read(0x1ec0d188)
load(host[0]+head*24,(1).to_bytes(4,'little'))
packet(0,body(100))
assert read(0x1ec0d188)==head and read(host[0]+head*24)==1
load(host[0]+head*24,b'\0'*4)

# Both producer indices wrap at their actual hardware ring sizes.
load(symbol('en7523_wifi_index')+4,(1535).to_bytes(4,'little'))
load(0x1ec0d198,(511).to_bytes(4,'little'))
load(symbol('en7523_host_index')+4,(511).to_bytes(4,'little'))
blob=body(128)
packet(1,blob)
check_host(1,511,blob,len(blob),1,1,True)
assert read(symbol('en7523_wifi_index')+4)==0 and read(0x1ec0d198)==0

# Both radio workers can keep independent reserved channels in flight.
dma_hold.update((0, 1))
simultaneous = []
for band in range(2):
    index = read(symbol('en7523_wifi_index')+band*4)
    desc = rings[band]+index*16
    head = read(0x1ec0d188+band*16)
    blob = body(127+band*64)
    load(read(desc), blob)
    load(desc+4, (0xc0000000 | len(blob)<<16).to_bytes(4, 'little'))
    simultaneous.append((desc, head, blob, index))
for _ in range(10000):
    tick(1)
    if set(dma_pending) == {0, 1}:
        break
else:
    raise AssertionError('RX workers failed to acquire distinct GDMA channels')
assert dma_peak == 2
dma_hold.clear()
for _ in range(10000):
    tick(1)
    if all(not read(desc+4)&0x80000000 and
           read(symbol('en7523_wifi_index')+band*4) == (index+1)%1536 and
           read(symbol('en7523_host_index')+band*4) == read(0x1ec0d188+band*16)
           for band, (desc, _, _, index) in enumerate(simultaneous)):
        break
else:
    raise AssertionError('parallel GDMA copies did not finish')
for band, (_, head, blob, _) in enumerate(simultaneous):
    check_host(band, head, blob, len(blob), 1, 1, True)

# A continuous burst publishes complete batches, then flushes its short tail.
band = 1
set_cmd(2, 0, band)
index = read(symbol('en7523_wifi_index')+4)
head = read(0x1ec0d198)
blobs = [body(96+i) for i in range(37)]
for i, blob in enumerate(blobs):
    desc = rings[band]+((index+i)%1536)*16
    load(read(desc), blob)
    load(desc+4, (0xc0000000 | len(blob)<<16).to_bytes(4, 'little'))
before = host_doorbells[band]
set_cmd(2, 1, band)
for _ in range(200000):
    tick(1)
    if read(0x1ec0d198) == (head+len(blobs))%512:
        break
else:
    raise AssertionError('RX burst failed to flush its short tail')
assert host_doorbells[band]-before == 2, host_doorbells
for i, blob in enumerate(blobs):
    check_host(band, (head+i)%512, blob, len(blob), 1, 1, True)
before = host_doorbells[:]
tick(1000)
assert host_doorbells == before, 'idle workers generated host interrupts'
print('PASS: 37 RX frames use two host doorbells; idle workers do not notify')

# STOP must also wait for a copy that was already in flight.
band=0
index=read(symbol('en7523_wifi_index'))
desc=rings[0]+index*16
blob=body(3500)
head=read(0x1ec0d188)
load(read(desc),blob)
load(desc+4,(0xc0000000 | len(blob)<<16).to_bytes(4,'little'))
dma_hold.add(0)
tick(1500)
assert 0 in dma_pending
assert read(symbol('en7523_wifi_busy'))==1 and read(desc+4)&0x80000000
set_cmd(2,0,expected=0)
assert read(symbol('en7523_wifi_busy'))==1 and read(desc+4)&0x80000000
assert not read(host[0]+head*24)&1, 'stalled DMA published a host descriptor'
dma_hold.remove(0)
for _ in range(60000):
    tick(1)
    if not read(desc+4)&0x80000000:
        break
else:
    raise AssertionError('released GDMA did not return the radio buffer')
set_cmd(2,0)
check_host(0,head,blob,len(blob),1,1,True)
assert read(symbol('en7523_wifi_enabled'))==0
# Stopping one radio must leave the other worker and host ring operational.
head=read(0x1ec0d198)
blob=body(193)
packet(1,blob)
check_host(1,head,blob,len(blob),1,1,True)
assert read(symbol('en7523_wifi_enabled')+4)==1

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
assert all(dma_copies) and not dma_pending
assert read(0x1fb30204)&0xfc == 0xfc, 'firmware cleared Linux completion bits'
print('PASS: coherent GDMA channels 0/1, delayed completion, failed STOP pins in-flight storage')
print('PASS: MT7915 ' + ('split radios ' + str(order) if split else 'DBDC') + ', independent PCIe/host rings, shared pool, raw bytes, fragments, backpressure, index wrap, isolated STOP')
