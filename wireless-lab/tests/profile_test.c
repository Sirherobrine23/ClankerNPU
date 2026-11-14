/* SPDX-License-Identifier: GPL-2.0-only */
#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/mman.h>
#include "../source/lab.h"
static u32 clock_value, reads;
static char output[16384];static size_t used;
u32 test_cycle(void){reads++;return clock_value;}
void *npu_memcpy(void *d,const void *s,u32 n){return memcpy(d,s,n);}
int npu_printf(const char *fmt,...)
{
 va_list ap;va_start(ap,fmt);int n=vsnprintf(output+used,sizeof(output)-used,fmt,ap);va_end(ap);
 assert(n>=0&&(size_t)n<sizeof(output)-used);used+=n;return n;
}
static void map(u32 address,u32 size)
{
 void *p=mmap((void *)(uintptr_t)address,size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
 if(p==MAP_FAILED){perror("mmap");exit(1);}
}
static void publish(void){for(u32 i=0;i<4096;i++)lab_profile_publish();}
int main(void)
{
 map(0x1e85e000,4096);map(0x85000000,32768);
 for(u32 i=0;i<63;i++){u32 t=lab_prof_begin(LP_CLASS);lab_prof_end(LP_CLASS,t,1);}
 assert(!reads&&!lab_profile_live.stage[LP_CLASS].samples);
 clock_value=0;u32 t=lab_prof_begin(LP_CLASS);clock_value=100;lab_prof_end(LP_CLASS,t,2);
 struct lab_profile_counter *p=&lab_profile_live.stage[LP_CLASS];
 assert(reads==2&&p->samples==1&&p->cycles==100&&p->maximum==100&&p->sample_work==2&&p->busy_samples==1);
 /* Both CSR wrap and cumulative sum wrap use unsigned subtraction. */
 p->calls=127;p->cycles=0xfffffff0;clock_value=0xfffffff0;t=lab_prof_begin(LP_CLASS);clock_value=0x20;lab_prof_end(LP_CLASS,t,0);
 assert(p->samples==2&&p->cycles==0x20&&p->maximum==100&&p->busy_samples==1);
 p->calls=0xffffffff;clock_value=10;t=lab_prof_begin(LP_CLASS);clock_value=20;lab_prof_end(LP_CLASS,t,1);
 assert(p->calls==0&&p->samples==3&&p->cycles==0x2a);
 for(u32 i=0;i<16;i++)REG32(0x85000000+i*16+4)=0x80000000;
 for(u32 i=0;i<64;i++)lab_profile_rx_observe(1,0x85000000,0,1);
 assert(lab_profile_live.peek_samples[1]==1&&lab_profile_live.peek_full[1]==1&&lab_profile_live.peek_max[1]==16);
 REG32(0x85000000+16+4)=0;
 for(u32 i=0;i<64;i++)lab_profile_rx_observe(1,0x85000000,0,1);
 assert(lab_profile_live.peek_samples[1]==2&&lab_profile_live.peek_full[1]==1);
 /* Ring-tail wrap: 1535 and 0 are ready, 1 is not. */
 for(u32 i=0;i<16;i++)REG32(0x85000000+i*16+4)=0;
 REG32(0x85000000+1535*16+4)=0x80000000;REG32(0x85000000+4)=0x80000000;
 for(u32 i=0;i<64;i++)lab_profile_rx_observe(0,0x85000000,1535,1);
 assert(lab_profile_live.peek_max[0]==2);
 publish();assert(lab_diag->profile.seq==2&&lab_diag->profile.loops==4096);
 assert(lab_diag->profile.data.stage[LP_CLASS].cycles==p->cycles);
 lab_profile_report();assert(!used); /* baseline only */
 lab_diag->profile.seq|=1;lab_diag->rx[1]=1;
 lab_profile_report();assert(!used); /* bounded skip of in-progress publication */
 lab_diag->profile.seq++;clock_value+=666000000;lab_diag->qdma_submit=1;lab_diag->ppe_free=1;
 for(u32 i=0;i<64;i++){t=lab_prof_begin(LP_RX);clock_value+=20;lab_prof_end(LP_RX,t,1);}
 publish();lab_profile_report();
 assert(strstr(output,"[PROF] id=1")&&strstr(output,"stage=rx calls=64 n=1 cyc=20"));
 assert(strstr(output,"free=1")&&strstr(output,"peek=2/16"));
 size_t before=used;publish();lab_profile_report();assert(used==before); /* no idle UART output */
 puts("profile: sampling 1/64; zero timestamp; CSR/counter wrap; bounded RX lookahead and ring wrap; sequence publication; busy-snapshot skip; delta report and idle suppression passed");
 fputs(output,stdout);
 return 0;
}
