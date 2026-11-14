/* SPDX-License-Identifier: GPL-2.0-only
 * Internal queue is ours (12 bytes). Host descriptor matches the supplied
 * mt76/airoha_offload.h ABI (24 bytes), not the vendor's 16-byte host ABI.
 */
#include "lab.h"
#include "npu_wifi.h"
#define HOST_CHUNK 1600u
/* Bit 7 is free for whole packets; grouped packets use bits 5..7 as count. */
#define HOST_ITEM_PPE 0x80u
struct host_item { u32 id; u16 length, segment, wcid; u8 flags, ready; };
_Static_assert(sizeof(struct host_item)==12,"internal queue ABI");
struct host_desc { volatile u32 ctrl,info,data,addr,rsv[2]; };
_Static_assert(sizeof(struct host_desc)==24,"host ABI");
static struct host_item *queue[2];
static u16 prod[2],cons[2],depth[2];
static u32 qlock[2]={7,0};
volatile u8 hostadpt_tx_ring_ready;
void lab_host_init(void)
{
 queue[0]=(struct host_item *)sram_buf_alloc(142);
 queue[1]=(struct host_item *)sram_buf_alloc(143);
 if(!queue[0]||!queue[1])lab_fault(8,0,0,0);
}
static int host_band_ready(u32 band)
{
 u32 base;
 if(!lab_band_started(band))return 0;
 base=REG32(band?0x1ec0d190:0x1ec0d180);
 return base && base!=~0u && !(base&3);
}
int lab_host_ready(void)
{
 return host_band_ready(0) || host_band_ready(1);
}
int hostadpt_init(void) { return lab_host_ready()?0:-1; }
void wifi_pkt_queue_init(u32 band) { (void)band; /* allocated once before mailbox */ }
int pkt_forward(u32 id,u32 len,s16 wcid,u8 amsdu,u32 band,u8 flags,u32 frag,int result,u8 tunnel)
{
 struct host_item *p;u32 irq=irq_save();
 (void)result;
 if(band>1 || id>=LAB_IDS || !len || !frag || frag>4096 || !queue[band]) { irq_restore(irq);return 1; }
 hw_mutex_lock(qlock);p=&queue[band][prod[band]];
 if(p->ready) { lab_diag->host_full++;hw_mutex_unlock(qlock);irq_restore(irq);return 1; }
 p->id=id;p->length=len;p->segment=frag;p->wcid=(u16)wcid;
 p->flags=(frag!=len)?flags:2;
 /* For whole packets the 5-bit value is the PPE reason/AMSDU metadata. */
 if(frag==len) {
  p->flags|=(amsdu&31)<<2;
  if(tunnel)p->flags|=HOST_ITEM_PPE;
 }
 lab_fence();p->ready=1;
 prod[band]=(prod[band]+1)&(band?511:255);
 lab_diag->queue_depth[band]=++depth[band];
 hw_mutex_unlock(qlock);irq_restore(irq);return 0;
}
int pkt_forward_bme(s32 id,u32 wcid,u32 info)
{
 u32 p=wifi_pkt_va(id),len=*(u16 *)p;
 u32 band=((*(u32 *)(p+12)&0xfc00u)>0x2300u);
 return pkt_forward(id,len,wcid,info,band,2,len,0,1);
}
static void drain(u32 band)
{
 struct host_item *first;struct host_desc *d[7];
 u32 flags,n,slots=0,idx,base,sw,addr[7],group,irq;
 u32 ctl[7],infos[7],data[7],s=0;
 if(!host_band_ready(band)||lab_diag->error)return;
 base=REG32(band?0x1ec0d190:0x1ec0d180);
 sw=REG32(HOSTADPT_RX_DMA_IDX(band));if(sw>=512){lab_fault(9,0,0,sw);return;}
 irq=irq_save();hw_mutex_lock(qlock);first=&queue[band][cons[band]];
 if(!first->ready)goto out;
 flags=first->flags;
 group=(first->segment!=first->length);
 n=group?((flags>>5)&7):1;
 if(!n){lab_fault(10,0,0,flags);goto out;}
 for(u32 i=0;i<n;i++) {
  struct host_item *p=&queue[band][(cons[band]+i)&(band?511:255)];
  if(!p->ready)goto out;
  if(!p->segment || p->segment>4096 || p->id>=LAB_IDS){lab_fault(11,0,0,p->id);goto out;}
  slots+=(p->segment+HOST_CHUNK-1)/HOST_CHUNK;
 }
 if(slots>7 || first->length>4096){lab_diag->host_drop++;goto release;}
 for(u32 i=0;i<slots;i++) {
  idx=(sw+i)&511;d[i]=(struct host_desc *)(base+idx*sizeof(*d[i]));
  if((d[i]->ctrl&1)||!d[i]->addr){lab_diag->host_full++;goto out;}
 }
 /* Copy complete batch before publishing its head: mt76 must never see a
  * completed first segment while a later segment still lacks ownership. */
 for(u32 i=0;i<n;i++) {
  struct host_item *p=&queue[band][(cons[band]+i)&(band?511:255)];
  u32 off=0,len=p->segment;
  while(len) {
   u32 bytes=len<HOST_CHUNK?len:HOST_CHUNK;
   addr[s]=d[s]->addr;
   if(addr[s]+bytes<addr[s]){lab_fault(12,0,0,addr[s]);goto out;}
   npu_memcpy((void *)addr[s],(void *)(wifi_pkt_va(p->id)+off),bytes);
   ctl[s]=1|(bytes<<1)|((u32)first->length<<15)|((s+1==slots)?1u<<29:0);
   infos[s]=((slots>1?slots:0)<<29)|((slots>1?s+1:0)<<26)|p->wcid;
   if(!group)infos[s]|=(u32)((p->flags>>2)&31)<<16;
   /* The driver passes vendor (word 2) to mt7915_npu_rx_status.
    * Never OR the legacy band hint into the FoE hash's low 15 bits.
    * Do not claim PN_VALIDATED or REORDERED: this is PPE provenance only. */
   if(!group && (p->flags&HOST_ITEM_PPE))
    data[s]=LAB_HOST_RX_PPE|((u32)p->wcid&LAB_PPE_HASH_MASK)|
            ((u32)((p->flags>>2)&31)<<16);
   else data[s]=band<<6;
   off+=bytes;len-=bytes;s++;
  }
 }
 lab_fence();
 for(u32 i=slots;i--;) {
  d[i]->info=infos[i];d[i]->data=data[i];lab_fence();d[i]->ctrl=ctl[i];
 }
 lab_fence();REG32(HOSTADPT_RX_DMA_IDX(band))=(sw+slots)&511;
 lab_diag->host_frames++;
 if(!group && (first->flags&HOST_ITEM_PPE))lab_diag->host_ppe++;
release:
 for(u32 i=0;i<n;i++) {
  struct host_item *p=&queue[band][cons[band]];
  lab_buf_free(p->id);p->ready=0;depth[band]--;cons[band]=(cons[band]+1)&(band?511:255);
 }
 lab_diag->queue_depth[band]=depth[band];
out:
 hw_mutex_unlock(qlock);irq_restore(irq);
}
void lab_host_poll(void)
{
 if(!lab_any_started())return;
 if(lab_host_ready())hostadpt_tx_ring_ready=1;
 drain(0);drain(1);
}
void pinode_drain(u32 band){drain(band);}
void rxnode_drain(u32 band){(void)band;}

u32 stats_bytes_2g[32],stats_pkts_2g[32],stats_bytes_5g[32],stats_pkts_5g[32];
