/* SPDX-License-Identifier: GPL-2.0-only */
#include "lab.h"
#if LAB_PROFILE
struct lab_profile_data lab_profile_live;
static u32 publish_loops;
static struct lab_profile_snapshot previous, current;
static u32 previous_packets[6], report_id, have_previous;

void lab_profile_rx_observe(u32 band,u32 ring,u32 idx,u32 ready)
{
 u32 n=0;
 if(band>1)return;
 lab_profile_live.rx_polls[band]++;
 if(ready)lab_profile_live.rx_ready[band]++;
 /* A bounded lookahead of DDONE descriptors, not total ring occupancy. */
 if(lab_profile_live.rx_polls[band]&63u)return;
 lab_profile_live.peek_samples[band]++;
 while(n<16) {
  if(!(*(volatile u32 *)(ring+idx*16+4)&0x80000000u))break;
  n++;idx=idx==1535?0:idx+1;
 }
 if(n==16)lab_profile_live.peek_full[band]++;
 if(n>lab_profile_live.peek_max[band])lab_profile_live.peek_max[band]=n;
}

void lab_profile_publish(void)
{
 volatile struct lab_profile_snapshot *out=&lab_diag->profile;
 const u32 *src=(const u32 *)&lab_profile_live;
 volatile u32 *dst=(volatile u32 *)&out->data;
 if(++publish_loops&4095u)return;
 out->seq++;lab_fence();
 out->clock=lab_prof_clock();out->loops=publish_loops;
 for(u32 i=0;i<sizeof(lab_profile_live)/sizeof(u32);i++)dst[i]=src[i];
 lab_fence();out->seq++;
}

/* Never wait for hart 1 from the control hart; a busy snapshot is skipped. */
static int snapshot_read(void)
{
 volatile struct lab_profile_snapshot *in=&lab_diag->profile;
 u32 *dst=(u32 *)&current.data;
 const volatile u32 *src=(const volatile u32 *)&in->data;
 for(u32 tries=0;tries<3;tries++) {
  u32 seq=in->seq;
  if(!seq || (seq&1u))continue;
  lab_fence();current.clock=in->clock;current.loops=in->loops;
  for(u32 i=0;i<sizeof(current.data)/sizeof(u32);i++)dst[i]=src[i];
  lab_fence();
  if(seq==in->seq){current.seq=seq;return 1;}
 }
 return 0;
}

void lab_profile_report(void)
{
 static const char * const names[LP_STAGES]={"qdone","ppe","rx","class","submit","alloc"};
 u32 packets[6]={lab_diag->rx[0]+lab_diag->rx[1],lab_diag->qdma_submit,
                lab_diag->qdma_complete,lab_diag->ppe_free,lab_diag->ppe_punt,
                lab_diag->host_frames};
 if(!snapshot_read() || current.loops==previous.loops)return;
 if(have_previous && packets[0]!=previous_packets[0]) {
  u32 id=++report_id;
  npu_printf("[PROF] id=%u dt=%u loops=%u rx=%u submit=%u done=%u free=%u punt=%u host=%u ids=%u\n",
             id,current.clock-previous.clock,current.loops-previous.loops,
             packets[0]-previous_packets[0],packets[1]-previous_packets[1],
             packets[2]-previous_packets[2],packets[3]-previous_packets[3],
             packets[4]-previous_packets[4],packets[5]-previous_packets[5],lab_diag->free_ids);
  for(u32 i=0;i<LP_STAGES;i++) {
   const struct lab_profile_counter *a=&current.data.stage[i],*b=&previous.data.stage[i];
   npu_printf("[PROF] id=%u stage=%s calls=%u n=%u cyc=%u max=%u busy=%u bcyc=%u work=%u\n",
              id,names[i],a->calls-b->calls,a->samples-b->samples,a->cycles-b->cycles,
              a->maximum,a->busy_samples-b->busy_samples,a->busy_cycles-b->busy_cycles,
              a->sample_work-b->sample_work);
  }
  npu_printf("[PROF] id=%u qpeak=%u irqpeak=%u fifopeak=%u irqcap=%u fifocap=%u poll=%u/%u ready=%u/%u nobuf=%u/%u peek=%u/%u full=%u/%u probes=%u/%u\n",
             id,current.data.q_peak,current.data.irq_peak,current.data.fifo_peak,
             current.data.irq_budget-previous.data.irq_budget,current.data.fifo_budget-previous.data.fifo_budget,
             current.data.rx_polls[0]-previous.data.rx_polls[0],current.data.rx_polls[1]-previous.data.rx_polls[1],
             current.data.rx_ready[0]-previous.data.rx_ready[0],current.data.rx_ready[1]-previous.data.rx_ready[1],
             current.data.rx_no_buf[0]-previous.data.rx_no_buf[0],current.data.rx_no_buf[1]-previous.data.rx_no_buf[1],
             current.data.peek_max[0],current.data.peek_max[1],
             current.data.peek_full[0]-previous.data.peek_full[0],current.data.peek_full[1]-previous.data.peek_full[1],
             current.data.peek_samples[0]-previous.data.peek_samples[0],current.data.peek_samples[1]-previous.data.peek_samples[1]);
 }
 npu_memcpy(&previous,&current,sizeof(previous));for(u32 i=0;i<6;i++)previous_packets[i]=packets[i];have_previous=1;
}
#endif
