#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/mman.h>
#include "../source/lab.h"
#include "../../npu_wifi.h"
#define PACKET 0x88000000u
#define DESC 0x85000000u
#define IRQ 0x85010000u
#define H0 0x85020000u
#define H1 0x85023000u
#define DST 0x87000000u
u32 wifi_pkt_buf_addr=PACKET;
u8 wifi_debug_flags;
volatile u32 lab_started_mask,lab_hwnat_enabled;
static u32 bump=0x1e800000,locks[64],checks;
int hw_mutex_lock(u32 *p){assert(p[0]<16);assert(!locks[p[0]]);locks[p[0]]=1;return 0;}
int hw_mutex_unlock(u32 *p){assert(locks[p[0]]);locks[p[0]]=0;return 0;}
void *npu_memcpy(void *d,const void *s,u32 n){return memcpy(d,s,n);}
void *npu_memset(void *d,int v,u32 n){return memset(d,v,n);}
u32 sram_buf_alloc(u32 t){u32 n=0;switch(t){case 138:n=11200;break;case 139:n=5600;break;case 140:n=2048;break;case 141:n=1024;break;case 142:n=256*12;break;case 143:n=512*12;break;default:abort();}u32 p=(bump+31)&~31u;bump=p+n;assert(bump<0x1e85e000);memset((void *)(uintptr_t)p,0,n);return p;}
void lab_fault(u32 e,u32 c,u32 p,u32 v){(void)c;(void)p;(void)v;lab_diag->error=e;}
static void map(u32 addr,u32 size){void *p=mmap((void *)(uintptr_t)addr,size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);if(p==MAP_FAILED){perror("mmap");exit(1);}}
#define CHECK(x) do{assert(x);checks++;}while(0)
int main(void)
{
 map(0x1e800000,0x60000);map(0x1ec0d000,0x1000);map(0x1fb50000,0x10000);
 map(DESC,0x30000);map(DST,0x200000);map(PACKET,0x1600000);
 lab_buf_init();lab_host_init();
 /* An unmapped/unresponsive host block must never be touched before START. */
 CHECK(mprotect((void *)(uintptr_t)0x1ec0d000,4096,PROT_NONE)==0);
 CHECK(!lab_any_started());CHECK(!lab_band_started(2));
 CHECK(!lab_host_ready());lab_host_poll();
 CHECK(!hostadpt_tx_ring_ready);
 CHECK(mprotect((void *)(uintptr_t)0x1ec0d000,4096,PROT_READ|PROT_WRITE)==0);
 CHECK(lab_diag->free_ids==5600);
 int id=lab_buf_alloc();CHECK(id==0);lab_buf_free(id);lab_buf_free(id);CHECK(lab_diag->bad_id==1);CHECK(lab_diag->free_ids==5600);
 /* All 5600 IDs unique, no sentinel ambiguity on full or empty pool. */
 unsigned char seen[5600]={0};
 for(u32 i=0;i<5600;i++){id=lab_buf_alloc();CHECK(id>=0&&id<5600);CHECK(!seen[id]);seen[id]=1;}
 CHECK(lab_buf_alloc()==-1);for(u32 i=0;i<5600;i++)lab_buf_free(i);CHECK(lab_diag->free_ids==5600);
 /* Cold boot: no host QDMA allocation yet. Retrying must not claim/reset it. */
 CHECK(lab_qdma_init()==-1);CHECK(lab_diag->error==0);
 CHECK(REG32(0x1fb50190)==0);CHECK(REG32(0x1fb50fe8)==0);
 REG32(0x1fb541c0)=DESC;REG32(0x1fb54048)=IRQ;REG32(0x1fb5404c)=1024;
 REG32(0x1fb54068)=0;for(u32 i=0;i<1024;i++)REG32(IRQ+4*i)=~0u;
 /* A pending return queue is not silently discarded during a retry. */
 REG32(0x1fb50fe4)=1;CHECK(lab_qdma_init()==-1);CHECK(lab_diag->error==0);
 CHECK(REG32(0x1fb50190)==0);REG32(0x1fb50fe4)=0;
 CHECK(lab_qdma_init()==0);
 /* Without HWNAT permission, a new packet must not reach the QDMA ring. */
 id=lab_buf_alloc();
 CHECK(tdma_tx_submit(id,100,PACKET+id*4096,0)==-1);
 CHECK(lab_diag->qdma_submit==0);CHECK(REG32(0x1fb541c8)==0);
 lab_buf_free(id);lab_hwnat_enabled=1;
 u32 ids[1024];
 for(u32 i=0;i<1023;i++){ids[i]=lab_buf_alloc();CHECK(tdma_tx_submit(ids[i],100,PACKET+ids[i]*4096+42,0)==0);}
 CHECK(lab_diag->qdma_submit==1023);CHECK(tdma_tx_submit(0,100,PACKET,0)==-1);CHECK(lab_diag->qdma_full==1);
 CHECK(REG32(DESC+4)==100);CHECK(REG32(DESC+8)==PACKET+ids[0]*4096+42);
 CHECK(REG32(DESC+12)==1);CHECK(REG32(DESC+16)==(0x80000000u|ids[0]<<14));CHECK(REG32(DESC+20)==0x7f4087ff);
 CHECK(REG32(0x1fb541c8)==1023);
 for(u32 i=0;i<1023;i++){REG32(DESC+32*i+4)|=0x80000000;REG32(IRQ+4*i)=0x60000|i;}
 REG32(0x1fb54068)=1023u<<16;
 for(u32 i=0;i<8;i++)lab_qdma_poll();
 CHECK(lab_diag->error==0);CHECK(lab_diag->qdma_complete==1023);
 CHECK(lab_diag->free_ids==5600-1023); /* completion must NOT free tokens */
 REG32(0x1fb50fe4)=1;
 for(u32 i=0;i<1023;i++){REG32(0x1fb50fe0)=0x10000|ids[i];lab_ppe_poll();}
 CHECK(lab_diag->ppe_free==1023);CHECK(lab_diag->free_ids==5600);CHECK(REG32(0x1fb50fe0)==0x40000000);
 /* Reuse ring after wrap, with Ethernet minimum-length padding. */
 id=lab_buf_alloc();memset((void *)(uintptr_t)(PACKET+4096*id),0xa5,64);
 CHECK(tdma_tx_submit(id,40,PACKET+4096*id,1)==0);
 CHECK(REG32(DESC+1023*32+4)==60);for(u32 i=40;i<60;i++)CHECK(*(u8 *)(uintptr_t)(PACKET+4096*id+i)==0);
 /* Modern mt76 descriptor: addr at +12 and fixed 24-byte stride. */
 REG32(0x1ec0d180)=H0;REG32(0x1ec0d190)=H1;
 /* Ring presence alone does not authorize access to an inactive band. */
 CHECK(!lab_host_ready());
 lab_started_mask=1;CHECK(lab_band_started(0));CHECK(!lab_band_started(1));
 lab_started_mask=3;CHECK(lab_band_started(1));
 for(u32 b=0;b<2;b++)for(u32 i=0;i<512;i++){u32 a=(b?H1:H0)+i*24;REG32(a+12)=DST+(b*512+i)*2048;memset((void *)(uintptr_t)REG32(a+12),0xcc,2048);}
 id=lab_buf_alloc();for(u32 i=0;i<3500;i++)*(u8 *)(uintptr_t)(PACKET+id*4096+i)=(u8)i;
 CHECK(pkt_forward(id,3500,0,0,0,2,3500,0,0)==0);lab_host_poll();
 CHECK(lab_diag->host_frames==1);CHECK(REG32(HOSTADPT_RX_DMA_IDX(0))==3);
 u32 offset=0;
 for(u32 i=0;i<3;i++){u32 a=H0+i*24,len=i==2?300:1600;CHECK(((REG32(a)>>1)&0x3fff)==len);CHECK((REG32(a+4)>>29)==3);CHECK(((REG32(a+4)>>26)&7)==i+1);CHECK(REG32(a+12)==DST+i*2048);CHECK(memcmp((void *)(uintptr_t)(DST+i*2048),(void *)(uintptr_t)(PACKET+id*4096+offset),len)==0);CHECK(*(u8 *)(uintptr_t)(DST+i*2048+len)==0xcc);offset+=len;}
 /* Backpressure retains the packet and ownership; retry succeeds. */
 id=lab_buf_alloc();CHECK(pkt_forward(id,100,0,0,0,2,100,0,0)==0);REG32(H0+3*24)=1;u32 nf=lab_diag->free_ids;
 lab_host_poll();CHECK(lab_diag->free_ids==nf);CHECK(REG32(HOSTADPT_RX_DMA_IDX(0))==3);
 REG32(H0+3*24)=0;lab_host_poll();CHECK(lab_diag->free_ids==nf+1);CHECK(lab_diag->host_frames==2);
 /* Partial multi-descriptor frame must not be published before its tail. */
 u32 a=lab_buf_alloc(),b=lab_buf_alloc();
 CHECK(pkt_forward(a,3000,0,0,1,0x44,1500,0,0)==0);lab_host_poll();CHECK(REG32(H1)==0);
 CHECK(pkt_forward(b,3000,0,0,1,0x4a,1500,0,0)==0);lab_host_poll();CHECK((REG32(H1)&1)==1);CHECK((REG32(H1+24)&1)==1);CHECK((REG32(H1+4)>>29)==2);CHECK(REG32(HOSTADPT_RX_DMA_IDX(1))==2);
 /* PPE punt keeps the buffer allocated until the CPU copy is published. */
 id=lab_buf_alloc();u32 pp=PACKET+id*4096;memset((void *)(uintptr_t)pp,0x42,104);
 *(u16 *)(uintptr_t)pp=104;REG32(pp+12)=0;
 nf=lab_diag->free_ids;REG32(0x1fb50fdc)=(15u<<16)|123;
 REG32(0x1fb50fe0)=id;lab_ppe_poll();CHECK(lab_diag->free_ids==nf);
 CHECK(lab_diag->ppe_punt==1);lab_host_poll();CHECK(lab_diag->free_ids==nf+1);
 CHECK((REG32(H0+4*24+4)&0x1fffff)==((15u<<16)|123));
 CHECK(REG32(H0+4*24+8)==(LAB_HOST_RX_PPE|(15u<<16)|123));
 CHECK(lab_diag->host_ppe==1);CHECK(lab_diag->ppe_reason[15]==1);
 CHECK(lab_diag->ppe_last_meta==((15u<<16)|123));CHECK(!lab_diag->ppe_invalid_hash);
 /* Band 1 must not set bit 6 in a hash whose bit 6 is clear. A fragmented
  * PPE punt must carry the SAME vendor metadata through its final fragment. */
 id=lab_buf_alloc();nf=lab_diag->free_ids;
 CHECK(pkt_forward(id,3500,0x123,14,1,2,3500,0,1)==0);lab_host_poll();
 CHECK(lab_diag->free_ids==nf+1);CHECK(lab_diag->host_ppe==2);
 u32 ppe_vendor=LAB_HOST_RX_PPE|(14u<<16)|0x123;
 for(u32 i=0;i<3;i++) {
  u32 d=H1+(2+i)*24;
  CHECK(REG32(d+8)==ppe_vendor);
  CHECK((REG32(d+8)&0x7fff)==0x123);
  CHECK(((REG32(d+4)>>26)&7)==i+1);
  CHECK((REG32(d+4)>>29)==3);
 }
 /* Raw RX keeps its existing vendor hint and never claims PPE/PN/reorder. */
 CHECK(!(REG32(H0+8)&LAB_HOST_RX_PPE));
 /* Bad lengths and addresses fail before any padding or DMA doorbell. */
 id=lab_buf_alloc();u32 submits=lab_diag->qdma_submit;
 CHECK(tdma_tx_submit(id,4096,PACKET+id*4096+42,0)==-1);
 CHECK(tdma_tx_submit(id,1,PACKET+id*4096+4095,0)==-1);
 CHECK(lab_diag->qdma_submit==submits);lab_buf_free(id);
 id=lab_buf_alloc();*(u16 *)(uintptr_t)(PACKET+id*4096)=5000;
 REG32(0x1fb50fe0)=id;nf=lab_diag->free_ids;lab_ppe_poll();CHECK(lab_diag->free_ids==nf+1);
 /* Queue-full failure never takes ownership of the caller's ID. */
 for(u32 i=0;i<512;i++){id=lab_buf_alloc();CHECK(pkt_forward(id,100,0,0,1,2,100,0,0)==0);}
 CHECK(lab_diag->queue_depth[1]==512);id=lab_buf_alloc();nf=lab_diag->free_ids;
 CHECK(pkt_forward(id,100,0,0,1,2,100,0,0)==1);CHECK(lab_diag->free_ids==nf);lab_buf_free(id);
 /* Foreign completion is quarantined, never returned into our free list. */
 REG32(IRQ+1023*4)=0x10001;REG32(0x1fb54068)=1u<<16;lab_qdma_poll();CHECK(lab_diag->error==5);CHECK(REG32(IRQ+1023*4)==0x10001);
 printf("backend: %u assertions passed (host model, no silicon execution)\n",checks);return 0;
}
