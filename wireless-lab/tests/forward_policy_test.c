/* Native test of the REAL bridge decision, linked with GC for other BA code. */
#include <assert.h>
#include <stdio.h>
#include "../../npu_config.h"
/* Keep unrelated BA functions in separate sections for native link GC. */
#undef HAS_HOT_TEXT
#include "../../npu_internal.h"
#include "../../npu_wifi.h"
#include "../source/lab.h"
volatile u32 lab_hwnat_enabled;
u32 ratelimit_table[32],wifi_pkt_buf_addr=0x88000000;
u32 counter_base_2g,counter_base_5g,counter_base_tri;
u8 wifi_force_to_cpu,wifi_band_cap=1,wifi_debug_flags;
static unsigned host_count,ppe_count,frees;
int pkt_forward(u32 id,u32 len,s16 w,u8 a,u32 b,u8 f,u32 o,int c,u8 t)
{(void)id;(void)len;(void)w;(void)a;(void)b;(void)f;(void)o;(void)c;(void)t;host_count++;return 0;}
int tdma_tx_submit(u32 id,u32 len,u32 a,u32 b)
{(void)id;(void)len;(void)a;(void)b;ppe_count++;return 0;}
void buf_id_free(u32 t,u32 b,u32 id){(void)t;(void)b;(void)id;frees++;}
int main(void)
{
 assert(pkt_enqueue_bridge(3,100,20,0,0)==0);
 assert(host_count==1 && !ppe_count && !frees);
 lab_hwnat_enabled=1;
 assert(pkt_enqueue_bridge(3,100,20,0,0)==0);
 assert(host_count==1 && ppe_count==1 && !frees);
 lab_hwnat_enabled=0;
 assert(pkt_enqueue_bridge(3,100,20,0,0)==0);
 assert(host_count==2 && ppe_count==1 && !frees);
 puts("forward policy: disabled -> host, enabled -> PPE, disabled again -> host (actual bridge function)");
 return 0;
}

#include "../source/npu_wifi_ba.c"
