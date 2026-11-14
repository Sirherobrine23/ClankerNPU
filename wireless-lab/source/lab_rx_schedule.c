/* SPDX-License-Identifier: GPL-2.0-only */
#include "lab.h"
#include "npu_wifi.h"
#define RX_BUDGET 8u
#define EMPTY_SKIP 7u
static u8 empty_skip[2],first_band;

/* BA timers must run even while a ring is empty or its polls are backed off. */
static void maintain(u32 enabled)
{
 u32 now=KITE_TICK;
 if(wifi_no_ba_test)return;
 if((enabled&1u) && (u32)(now-rxd_2g_flush_tick)>9u) {
  ba_timeout_scan(0);rxd_2g_flush_tick=KITE_TICK;
 }
 if((enabled&2u) && (u32)(now-rxd_5g_flush_tick)>9u) {
  ba_timeout_scan(1);rxd_5g_flush_tick=KITE_TICK;
 }
}

void lab_rx_poll(void)
{
 u32 enabled=0,due,band,budget=RX_BUDGET;
 if(lab_pause_request || lab_diag->error)return;
 if(lab_band_started(0) && rxd_2g_init_done)enabled|=1u;
 if(lab_band_started(1) && rxd_5g_init_done)enabled|=2u;
 if(!enabled){empty_skip[0]=empty_skip[1]=first_band=0;return;}
 maintain(enabled);due=enabled;
 for(u32 b=0;b<2;b++) {
  if(!(enabled&(1u<<b)))empty_skip[b]=0;
  else if(empty_skip[b]){empty_skip[b]--;due&=~(1u<<b);}
 }
 band=first_band;first_band^=1u;
 /* Alternate productive bands. Empty/partial rings lose only this turn and
  * are checked again within eight calls. QDMA/PPE run between bounded batches.
  * lab_rx_one returns whether descriptors were consumed, even on a drop. */
 while(due && budget) {
  if(lab_pause_request || lab_diag->error)break;
  if(due&(1u<<band)) {
   if(lab_rx_one(band)){budget--;empty_skip[band]=0;}
   else {empty_skip[band]=EMPTY_SKIP;due&=~(1u<<band);}
  }
  band^=1u;
 }
}
