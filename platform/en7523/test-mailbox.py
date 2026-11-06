from pathlib import Path
import struct
import sys
code=Path(sys.argv[1]).read_bytes()
MASK=0xffffffff
B=0x84000000; D=0x840f0000; P=0x840f1000; Q=0x1ec0c030

def sx(n,k):return n-(1<<k) if n&(1<<(k-1)) else n

def run_case(hart=0,func=7,ptr=P,length=64,magic=0x4e50444d,abi=1,cmd=0,target=0,bad_data=False,trap_worker=False):
 r=[0]*32;mem={};csr={0xf14:hart,0x7c0:0x139,0x9c0:0x7f,0x300:0,0x304:0};pc=B+0x108
 for i in range(0,len(code),4):mem[B+i]=struct.unpack_from('<I',code,i)[0]
 data=Path(sys.argv[1]).with_name('npu_data.bin').read_bytes()
 for i in range(0,len(data),4):mem[0x1e900000+i]=struct.unpack_from('<I',data,i)[0]
 if bad_data:mem[0x1e900004]=0
 for i in range(4):mem[D+i*64+12]=1
 injected=False
 for step in range(6000):
  if not injected and mem.get(D+hart*64)==0x454e5055:
   assert mem[D+hart*64+4]==3
   if mem[D+hart*64+12]==3:return mem
   assert mem[D+hart*64+52]==0x43525400|hart
   assert 0x840e0000+hart*4096 < mem[D+hart*64+56] < 0x840e0000+(hart+1)*4096
   if trap_worker:mem[D+hart*64+60]=0x54524150
   mem.update({P:magic,P+4:abi,P+8:123,P+12:cmd,P+16:0xffffffff,Q:ptr,Q+4:length,Q+8:1,P+48:target,Q+12:(func<<11)|1});injected=True
  w=mem[pc];op=w&127;rd=w>>7&31;f=w>>12&7;a=w>>15&31;b=w>>20&31;imm=sx(w>>20,12);n=pc+4
  if op==0x37:r[rd]=w&0xfffff000
  elif op==0x17:r[rd]=pc+(w&0xfffff000)
  elif op==0x13:
   if f==0:r[rd]=r[a]+imm
   elif f==1:r[rd]=r[a]<<(b&31)
   elif f==5:r[rd]=r[a]>>(b&31)
   elif f==6:r[rd]=r[a]|(imm&MASK)
   elif f==7:r[rd]=r[a]&(imm&MASK)
   else:raise AssertionError(hex(w))
  elif op==0x33:
   if f==0:r[rd]=r[a]+r[b]
   elif f==4:r[rd]=r[a]^r[b]
   elif f==6:r[rd]=r[a]|r[b]
   elif f==7:r[rd]=r[a]&r[b]
   else:raise AssertionError(hex(w))
  elif op==3:assert f==2;r[rd]=mem.get((r[a]+imm)&MASK,0)
  elif op==0x23:
   off=sx((w>>25<<5)|(w>>7&31),12);assert f==2
   address=(r[a]+off)&MASK
   if 0x840e0000<=address<0x840e4000:
    assert 0x840e0000+hart*4096<=address<0x840e0000+(hart+1)*4096
   mem[address]=r[b]
  elif op==0x63:
   off=sx(((w>>31)<<12)|((w>>7&1)<<11)|((w>>25&63)<<5)|((w>>8&15)<<1),13)
   cond={0:r[a]==r[b],1:r[a]!=r[b],6:r[a]<r[b],7:r[a]>=r[b]}[f]
   if cond:n=pc+off
  elif op==0x6f:
   off=sx(((w>>31)<<20)|((w>>12&255)<<12)|((w>>20&1)<<11)|((w>>21&1023)<<1),21);r[rd]=pc+4;n=pc+off
  elif op==0x67:
   assert f==0
   n=(r[a]+imm)&~1;r[rd]=pc+4
  elif op==0x73:
   c=w>>20;old=csr.get(c,0);v=r[a] if f<5 else a
   if f==0:
    assert w==0x73
    csr[0x341]=pc;csr[0x342]=11;csr[0x343]=0;n=csr[0x305]
   elif f in (1,5):csr[c]=v
   elif f in (2,6):csr[c]=old|v
   elif f in (3,7):csr[c]=old&~v
   r[rd]=old
  elif op==0xf:pass
  else:raise AssertionError(hex(w))
  r[:]=[v&MASK for v in r];r[0]=0;pc=n&MASK
  if trap_worker and mem.get(D+hart*64+12)==2:return mem
  if injected and mem.get(Q+12,0)&2:return mem
 if hart:return mem
 raise AssertionError('request did not complete')

for command in [0,1]:
 m=run_case(cmd=command);assert m[Q+12]&0x1e==6;assert m[P+16]==0;assert m[P+20]==0x10001;assert m[P+24]==4;assert m[P+8]==123
for kw in [dict(func=0),dict(ptr=P+4),dict(length=12),dict(magic=0),dict(abi=2),dict(cmd=2),dict(cmd=0xffffffff)]:
 m=run_case(**kw);assert m[Q+12]&0x1e==2,kw
for hart in [1,2,3]:
 m=run_case(hart=hart);assert m[D+hart*64+16]>10;assert m[Q+12]&2==0
print('PASS: executed built RV32 image: ping, status, invalid requests and worker isolation')

for hart in range(4):
 m=run_case(hart=hart,bad_data=True);assert m[D+hart*64+12]==3;assert m[D+hart*64+52]==0x45525201
for hart in [1,2,3]:
 m=run_case(cmd=2,target=hart);assert m[Q+12]&0x1e==6;assert m[D+hart*64+60]==0x54524150
 m=run_case(hart=hart,trap_worker=True);assert m[D+hart*64+12]==2;assert m[D+hart*64+20]==11;assert m[D+hart*64+24]>=B
print('PASS: C entry/stack/data tests on four harts, fail-closed bad data and terminal worker ECALL capture')

m=run_case(cmd=2,target=4);assert m[Q+12]&0x1e==2
