/* SPDX-License-Identifier: GPL-2.0-only */
#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <sys/mman.h>
#include "../source/lab.h"
volatile u32 lab_started_mask,lab_pause_request;
volatile u32 rxd_2g_init_done,rxd_5g_init_done,timer_slow_tick;
u32 rxd_2g_flush_tick,rxd_5g_flush_tick;
u8 wifi_no_ba_test;
static u32 frames[2],polls[2],consumed[2],scans[2],partial[2],inject_pause,inject_error,total,checks;
#define CHECK(x) do{assert(x);checks++;}while(0)
void ba_timeout_scan(u32 band){CHECK(band<2);scans[band]++;}
int lab_rx_one(u32 band)
{
 CHECK(band<2);CHECK(lab_started_mask&(1u<<band));
 CHECK(band?rxd_5g_init_done:rxd_2g_init_done);
 polls[band]++;
 if(partial[band] || !frames[band])return 0;
 frames[band]--;consumed[band]++;total++;
 if(inject_pause)lab_pause_request=1;
 if(inject_error)lab_diag->error=1;
 return 1;
}
static void reset(void)
{
 lab_started_mask=0;lab_pause_request=0;lab_diag->error=0;lab_rx_poll();
 for(u32 b=0;b<2;b++)frames[b]=polls[b]=consumed[b]=scans[b]=partial[b]=0;
 rxd_2g_init_done=rxd_5g_init_done=0;timer_slow_tick=0;
 rxd_2g_flush_tick=rxd_5g_flush_tick=0;wifi_no_ba_test=0;
 inject_pause=inject_error=total=0;
}
int main(void)
{
 void *p=mmap((void *)0x1e85e000,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
 assert(p!=MAP_FAILED);
 reset();frames[0]=frames[1]=100;lab_rx_poll();CHECK(!total&&!polls[0]&&!polls[1]);
 lab_started_mask=3;lab_rx_poll();CHECK(!total); /* START without initialized rings */
 rxd_2g_init_done=rxd_5g_init_done=1;lab_rx_poll();CHECK(consumed[0]==4&&consumed[1]==4&&total==8);
 for(u32 i=0;i<4;i++){lab_rx_poll();}
 CHECK(consumed[0]==20&&consumed[1]==20);
 reset();lab_started_mask=3;rxd_2g_init_done=rxd_5g_init_done=1;frames[1]=1000;
 lab_rx_poll();CHECK(polls[0]==1&&consumed[1]==8);
 for(u32 i=0;i<7;i++){lab_rx_poll();}
 CHECK(polls[0]==1&&consumed[1]==64);
 lab_rx_poll();CHECK(polls[0]==2&&consumed[1]==72); /* empty ring rechecked every 8 calls */
 /* Arrival immediately after an empty poll has bounded delay, including with
  * a continuously busy peer band. */
 frames[0]=1;
 for(u32 i=0;i<8;i++){lab_rx_poll();}
 CHECK(consumed[0]==1);
 reset();lab_started_mask=3;rxd_2g_init_done=rxd_5g_init_done=1;
 lab_rx_poll();u32 p0=polls[0],p1=polls[1];timer_slow_tick=11;lab_rx_poll();
 CHECK(polls[0]==p0&&polls[1]==p1);CHECK(scans[0]==1&&scans[1]==1); /* timers survive backoff */
 CHECK(rxd_2g_flush_tick==11&&rxd_5g_flush_tick==11);
 timer_slow_tick=20;lab_rx_poll();CHECK(scans[0]==1&&scans[1]==1);
 timer_slow_tick=21;lab_rx_poll();CHECK(scans[0]==2&&scans[1]==2);
 wifi_no_ba_test=1;timer_slow_tick=100;lab_rx_poll();CHECK(scans[0]==2&&scans[1]==2);
 wifi_no_ba_test=0;rxd_2g_flush_tick=rxd_5g_flush_tick=0xfffffffc;timer_slow_tick=7;lab_rx_poll();
 CHECK(scans[0]==3&&scans[1]==3); /* unsigned tick wrap */
 reset();lab_started_mask=2;rxd_5g_init_done=1;frames[1]=32;inject_pause=1;lab_rx_poll();
 CHECK(total==1&&lab_pause_request);lab_rx_poll();CHECK(total==1);
 reset();lab_started_mask=2;rxd_5g_init_done=1;frames[1]=32;inject_error=1;lab_rx_poll();
 CHECK(total==1&&lab_diag->error);lab_rx_poll();CHECK(total==1);
 reset();lab_started_mask=3;rxd_2g_init_done=rxd_5g_init_done=1;partial[1]=1;frames[1]=10;
 lab_rx_poll();CHECK(!total&&polls[1]==1);partial[1]=0;
 for(u32 i=0;i<8;i++){lab_rx_poll();}
 CHECK(total>0&&frames[1]<10);
 /* Mixed arrival stream: each pass remains bounded and both streams drain. */
 reset();lab_started_mask=3;rxd_2g_init_done=rxd_5g_init_done=1;
 u32 offered[2]={0,0},rng=17;
 for(u32 i=0;i<2000;i++) {
  rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;
  for(u32 b=0;b<2;b++){u32 add=(rng>>(b*4))&3u;frames[b]+=add;offered[b]+=add;}
  u32 before=total,pc=polls[0]+polls[1];timer_slow_tick++;lab_rx_poll();
  CHECK(total-before<=8);CHECK(polls[0]+polls[1]-pc<=10);
 }
 for(u32 i=0;i<32;i++)lab_rx_poll();
 CHECK(!frames[0]&&!frames[1]);CHECK(consumed[0]==offered[0]&&consumed[1]==offered[1]);
 printf("RX scheduler: %u assertions passed; batch <=8, dual-band fairness, bounded empty/partial backoff, START/ring gates, BA deadlines/wrap, pause/error, mixed arrivals\n",checks);
 return 0;
}
