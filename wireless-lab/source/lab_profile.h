/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LAB_PROFILE_H
#define LAB_PROFILE_H
#ifndef LAB_PROFILE
#define LAB_PROFILE 0
#endif
/* All instrumented stages and pressure counters have hart 1 as sole writer.
 * Timing is inclusive; CLASS/SUBMIT/ALLOC are nested inside RX. */
#if LAB_PROFILE
extern struct lab_profile_data lab_profile_live;
static inline u32 lab_prof_clock(void)
{
#ifdef LAB_HOST_TEST
 return csr_read(mcycle);
#else
 u32 value;
 __asm__ volatile("csrr %0, mcycle" : "=r"(value) :: "memory");
 return value;
#endif
}
static inline u32 lab_prof_begin(u32 id)
{
 struct lab_profile_counter *p=&lab_profile_live.stage[id];
 p->calls++;
 return (p->calls&63u)?0:lab_prof_clock();
}
static inline void lab_prof_end(u32 id,u32 start,u32 work)
{
 struct lab_profile_counter *p=&lab_profile_live.stage[id];
 if(p->calls&63u)return;
 u32 elapsed=lab_prof_clock()-start;
 p->samples++;p->cycles+=elapsed;
 if(elapsed>p->maximum)p->maximum=elapsed;
 if(work){p->busy_samples++;p->busy_cycles+=elapsed;p->sample_work+=work;}
}
#define LAB_PROF_MAX(field,v) do { u32 _v=(v); if(_v>lab_profile_live.field)lab_profile_live.field=_v; } while(0)
#define LAB_PROF_INC(field) (lab_profile_live.field++)
void lab_profile_rx_observe(u32 band,u32 ring,u32 idx,u32 ready);
void lab_profile_publish(void);
void lab_profile_report(void);
#else
static inline u32 lab_prof_begin(u32 id){(void)id;return 0;}
static inline void lab_prof_end(u32 id,u32 start,u32 work){(void)id;(void)start;(void)work;}
#define LAB_PROF_MAX(field,v) ((void)0)
#define LAB_PROF_INC(field) ((void)0)
#define lab_profile_rx_observe(b,r,i,n) ((void)0)
#define lab_profile_publish() ((void)0)
#define lab_profile_report() ((void)0)
#endif
#endif
