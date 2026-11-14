/* SPDX-License-Identifier: GPL-2.0-only
 * EN7523 vendor 0x84002bfc/0x84002cbc: QDMA ring 6, reserved tail.
 * Completion recycles descriptors, PPE FIFO returns packet buffers.
 */
#include "lab.h"
#include "npu_wifi.h"
#if !defined(LAB_HOST_TEST) && defined(__riscv_xen7523ops)
#include <en7523_intrinsics.h>
#endif
#define QREG(o) REG32(LAB_QDMA_BASE+(o))
#define NDESC 1024u
struct qdesc { u32 r0, ctrl, addr, next, msg0, msg1, r6, r7; };
_Static_assert(sizeof(struct qdesc)==32,"QDMA ABI");
static volatile struct qdesc *ring;
static volatile u32 *irq_ring;
static u16 *free_idx;
static u8 *state;
static u16 head, tail, avail, active, completion_idx;
static u32 irq_size;
static u32 qlock[2]={4,0};
static u32 ready;
int lab_qdma_init(void)
{
 u32 base=QREG(0x1c0), ir=QREG(0x48), cfg=QREG(0x4c);
 lab_diag->qdma_base=base; lab_diag->irq_base=ir; lab_diag->irq_cfg=cfg;
 if (!base || base==~0u || base&31 || !ir || ir==~0u || ir&3) return -1;
 irq_size=cfg&0xfff;
 if (irq_size!=1024 || (QREG(0x68)&0xfff)>=irq_size) return -1;
 /* Host must reserve QDMA0 ring 6 and IRQ queue 1 before attachment. */
 if (QREG(0x1c8)!=QREG(0x1cc) || ((QREG(0x68)>>16)&0xfff) ||
     (REG32(0x1fb50fe4)&0xffff)) return -1;
 ring=(volatile struct qdesc *)base; irq_ring=(volatile u32 *)ir;
 free_idx=(u16 *)sram_buf_alloc(140); state=(u8 *)sram_buf_alloc(141);
 if (!free_idx || !state) return -1;
 for (u32 i=0;i<NDESC;i++) {
#if defined(__riscv_xen7523ops)
  ring[i].ctrl=en7523_bclri(en7523_bseti(ring[i].ctrl,31),30);
#else
  ring[i].ctrl=(ring[i].ctrl | (1u << 31)) & ~(1u << 30);
#endif
  free_idx[i]=i; state[i]=0;
 }
 /* Empty active tail is a hardware descriptor, never on the free list. */
 active=0; head=1; tail=0; avail=NDESC-1; state[0]=1;
 QREG(0x1c8)=active; QREG(0x1cc)=active;
 completion_idx=QREG(0x68)&0xfff;
 if (completion_idx>=irq_size) return -1;
 QREG(0x40)|=0x30; QREG(0x6c)=0;
 REG32(0x1fb50190)=0x40;
 REG32(0x1fb50fe8)=(REG32(0x1fb50fe8)&0xc0e200ffu)|0x1b190100u;
 lab_fence(); ready=1; return 0;
}
int tdma_tx_submit(u32 token,u32 len,u32 address,u32 band)
{
 u32 irq=irq_save(), idx; volatile struct qdesc *d;
 (void)band;
 u32 slot=wifi_pkt_va(token), padded=len<60?60:len;
 if (!ready || !lab_hwnat_enabled || lab_diag->error || token>=LAB_IDS || !len || padded>4096 ||
     address<slot || address-slot>4096-padded) { irq_restore(irq); return -1; }
 hw_mutex_lock(qlock);
 if (!avail) { lab_diag->qdma_full++; hw_mutex_unlock(qlock); irq_restore(irq); return -1; }
 idx=free_idx[head]; head=(head+1)&1023; avail--;
 LAB_PROF_MAX(q_peak,NDESC-1-avail);
 if (idx>=NDESC || state[idx] || state[active]!=1) {
  lab_fault(4,0,0,idx); hw_mutex_unlock(qlock); irq_restore(irq); return -1;
 }
 d=&ring[active];
 if (len<60) { npu_memset((void *)(address+len),0,60-len); len=60; }
 d->msg0=0x80000000u|((token&0xffff)<<14);
 d->msg1=0x7f4087ffu; d->addr=address; d->next=idx;
 state[active]=2; state[idx]=1;
 lab_fence(); d->ctrl=len; lab_fence();
 active=idx; QREG(0x1c8)=idx;
 lab_diag->qdma_submit++;lab_diag->state=4;
 hw_mutex_unlock(qlock); irq_restore(irq); return 0;
}
void lab_qdma_poll(void)
{
 u32 n,done=0,irq;
 if (!ready || lab_diag->error) return;
 n=(QREG(0x68)>>16)&0xfff; LAB_PROF_MAX(irq_peak,n);
 if(n>128){LAB_PROF_INC(irq_budget);n=128;}
 if(!n)return;
 irq=irq_save(); hw_mutex_lock(qlock);
 while(done<n) {
  u32 v=irq_ring[completion_idx],idx=v&0xfff;
  if(v==~0u)break;
  lab_fence();
  if((v&0x70000)!=0x60000 || idx>=NDESC || state[idx]!=2 || !(ring[idx].ctrl&0xc0000000u)) {
   lab_fault(5,0,0,v); break;
  }
  state[idx]=0; free_idx[tail]=idx; tail=(tail+1)&1023; avail++;
  if(avail>=NDESC) {lab_fault(6,0,0,avail);break;}
  irq_ring[completion_idx]=~0u; completion_idx=(completion_idx+1)&1023;
  done++; lab_diag->qdma_complete++;
 }
 lab_fence(); if(done)QREG(0x64)=done;
 hw_mutex_unlock(qlock); irq_restore(irq);
 lab_diag->qdma_cpu=QREG(0x1c8); lab_diag->qdma_dma=QREG(0x1cc);
}
void lab_ppe_poll(void)
{
 u32 n=REG32(0x1fb50fe4)&0xffff;
 LAB_PROF_MAX(fifo_peak,n);
 if(n>128){LAB_PROF_INC(fifo_budget);n=128;}
 while(n-- && !lab_diag->error) {
  u32 v=REG32(0x1fb50fe0),id=v&0xffff;
  if((s32)v<0)break;
  if(id>=LAB_IDS) { lab_fault(7,0,0,v);break; }
  if(v&0x10000) { lab_buf_free(id); lab_diag->ppe_free++; }
  else {
   u32 meta=REG32(0x1fb50fdc),buf=wifi_pkt_va(id);
   lab_diag->ppe_last_meta=meta;
   lab_diag->ppe_reason[(meta>>16)&31]++;
   if((meta&LAB_PPE_HASH_MASK)==LAB_PPE_HASH_MASK)lab_diag->ppe_invalid_hash++;
   u32 len=*(volatile u16 *)buf;
   u32 band=((*(volatile u32 *)(buf+12)&0xfc00u)>0x2300u);
   if(!len || len>4096 || pkt_forward(id,len,meta&0xffff,(meta>>16)&31,band,2,len,0,1))lab_buf_free(id);
   lab_diag->ppe_punt++;
  }
  lab_fence(); REG32(0x1fb50fe0)=0x40000000u;
 }
}
