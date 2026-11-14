#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <sys/mman.h>
#include "../source/lab.h"
#include "../../npu_wifi.h"
u32 wifi_pkt_buf_addr,ba_table_a,ba_table_b,wifi_dram_ba_node_addr;
u32 rxd_base_2g,rxd_base_5g,counter_base_2g,counter_base_5g;
u32 wcid_counter_base_2g,wcid_counter_base_5g;
u8 wifi_dbdc_mode,wifi_debug_flags,wifi_pcie_state[2];
volatile u32 rxd_2g_init_done,rxd_5g_init_done;
extern volatile u32 lab_pause_request,lab_pause_ack[4];
static u32 enables,cycles,cycle_reads;
static int workers_respond;
int npu_printf(const char *fmt,...){(void)fmt;return 0;}
u32 test_cycle(void)
{
 cycle_reads++;
 cycles+=workers_respond?1000:EN7523_CORE_CLK_MHZ*500000u;
 if(workers_respond)lab_pause_ack[1]=lab_pause_ack[3]=!!lab_pause_request;
 return cycles;
}
void npu_set_band_enable(u32 band){assert(band<2);enables|=1u<<band;}
void npu_set_pcie_base(u32 a,u32 b){(void)a;(void)b;}
void wifi_npu_init(u32 b){(void)b;}
void npu_set_rxd_init(u32 v,u32 b){(void)v;if(b)rxd_5g_init_done=1;else rxd_2g_init_done=1;}
void npu_set_wait_state(u32 p,u8 s){(void)p;(void)s;}
void npu_set_ba_entry(u32 b,u32 v){(void)b;(void)v;}
void wifi_reset_ba_entry(u32 b,u32 v){(void)b;(void)v;}
void npu_set_driver_model(u32 v){(void)v;}
void npu_set_pkt_buf_addr(u32 v){wifi_pkt_buf_addr=v;}
void npu_set_no_ba_test(u8 v){(void)v;}
void npu_set_flushone_timeout(u32 v){(void)v;}
void npu_set_flushall_timeout(u32 v){(void)v;}
void npu_set_force_to_cpu(u8 v){(void)v;}
void npu_set_pcie_port_type(u32 v){(void)v;}
void npu_set_retry_limit(u32 v){(void)v;}
void npu_set_bar_info(u32 b,u32 v){(void)b;(void)v;}
static int call_arg(u32 type,u32 band,u32 cmd,u32 bytes,u32 arg)
{
 u32 *m=(u32 *)(uintptr_t)0x85000000;
 m[0]=(type<<4)|band;m[1]=cmd;m[2]=arg;
 /* Emulate workers returning to their polling loop between transactions. */
 lab_pause_ack[1]=lab_pause_ack[3]=0;
 int rc=lab_wifi_dispatch(0x85000000,bytes);
 assert(!lab_pause_request);
 return rc;
}
static int call(u32 type,u32 band,u32 cmd,u32 bytes)
{
 return call_arg(type,band,cmd,bytes,1);
}
int main(void)
{
 assert(mmap((void *)(uintptr_t)0x1e85e000,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0)!=MAP_FAILED);
 assert(mmap((void *)(uintptr_t)0x85000000,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0)!=MAP_FAILED);
 assert(!lab_any_started());
 /* Reproduce the actual host order, with NO worker acknowledgements.
  * In v18 these commands require pause_workers and fail on missing acks. */
 assert(!workers_respond);
 assert(!lab_hwnat_enabled);
 assert(call_arg(1,0,WLAN_FUNC_SET_WAIT_HWNAT_INIT,12,2)==NPU_MBOX_EINVAL);
 assert(!lab_hwnat_enabled);
 assert(call_arg(1,2,WLAN_FUNC_SET_WAIT_HWNAT_INIT,12,1)==NPU_MBOX_EINVAL);
 assert(!lab_hwnat_enabled);
 /* Link notification must not release RX workers. */
 assert(call_arg(1,0,WLAN_FUNC_SET_WAIT_PCIE_STATE,12,1)==NPU_MBOX_SUCCESS);
 assert(!lab_started_mask);enables=0;
 assert(call_arg(1,1,WLAN_FUNC_SET_WAIT_NPU_BAND0_ONCPU,12,0)==NPU_MBOX_SUCCESS);
 assert(call_arg(1,0,WLAN_FUNC_SET_WAIT_PKT_BUF_ADDR,12,0x84d00000)==NPU_MBOX_SUCCESS);
 assert(wifi_pkt_buf_addr==0x84d00000);
 assert(call_arg(1,0,WLAN_FUNC_SET_WAIT_HWNAT_INIT,12,1)==NPU_MBOX_SUCCESS);
 assert(lab_hwnat_enabled && !lab_started_mask);
 assert(call_arg(1,0,WLAN_FUNC_SET_WAIT_HWNAT_INIT,12,0)==NPU_MBOX_SUCCESS);
 assert(!lab_hwnat_enabled);
 assert(call_arg(1,0,WLAN_FUNC_SET_WAIT_HWNAT_INIT,12,1)==NPU_MBOX_SUCCESS);
 assert(!cycle_reads && !lab_pause_request && !lab_started_mask);
 assert(call_arg(1,0,WLAN_FUNC_SET_WAIT_PKT_BUF_ADDR,12,0)==NPU_MBOX_EINVAL);
 assert(lab_diag->last_reject==LAB_REJECT_POOL_ZERO);
 assert(call_arg(1,0,WLAN_FUNC_SET_WAIT_PKT_BUF_ADDR,12,0x84d00004)==NPU_MBOX_EINVAL);
 assert(lab_diag->last_reject==LAB_REJECT_POOL_ALIGN);
 rxd_2g_init_done=1;
 assert(call_arg(1,0,WLAN_FUNC_SET_WAIT_PKT_BUF_ADDR,12,0x84d00000)==NPU_MBOX_EINVAL);
 assert(lab_diag->last_reject==LAB_REJECT_POOL_BUSY);
 assert(lab_diag->reject_rxd0==1 && !lab_diag->reject_rxd1);
 assert(lab_diag->reject_cmd==WLAN_FUNC_SET_WAIT_PKT_BUF_ADDR);
 assert(lab_diag->reject_arg==0x84d00000);
 rxd_2g_init_done=0;wifi_pkt_buf_addr=0;
 assert(call(1,0,WLAN_FUNC_SET_WAIT_NPU_INIT_DONE,12)==NPU_MBOX_EINVAL);
 assert(!enables&&!lab_started_mask);
 wifi_pkt_buf_addr=0x88000000;
 assert(call(1,0,WLAN_FUNC_SET_WAIT_NPU_INIT_DONE,12)==NPU_MBOX_EINVAL);
 rxd_2g_init_done=1;
 assert(call(1,0,WLAN_FUNC_SET_WAIT_NPU_INIT_DONE,8)==NPU_MBOX_EINVAL);
 assert(call(1,2,WLAN_FUNC_SET_WAIT_NPU_INIT_DONE,12)==NPU_MBOX_EINVAL);
 assert(!enables&&!lab_started_mask);
 assert(call(1,0,WLAN_FUNC_SET_WAIT_NPU_INIT_DONE,12)==NPU_MBOX_SUCCESS);
 assert(enables==1&&lab_started_mask==1&&lab_band_started(0)&&!lab_band_started(1));
 /* After the first START, a missing worker acknowledgement must fail. */
 assert(call_arg(1,0,WLAN_FUNC_SET_WAIT_IS_FORCE_TO_CPU,12,0)==NPU_MBOX_EINVAL);
 assert(lab_diag->last_reject==LAB_REJECT_PAUSE_ACK);
 assert(lab_started_mask==1 && !lab_pause_request);
 workers_respond=1;
 assert(call(1,1,WLAN_FUNC_SET_WAIT_NPU_INIT_DONE,12)==NPU_MBOX_EINVAL);
 assert(lab_started_mask==1);
 rxd_5g_init_done=1;
 assert(call(1,1,WLAN_FUNC_SET_WAIT_NPU_INIT_DONE,12)==NPU_MBOX_SUCCESS);
 assert(enables==3&&lab_started_mask==3);
 assert(call(3,0,WLAN_FUNC_GET_WAIT_NPU_VERSION,12)==NPU_MBOX_SUCCESS);
 assert(*(u32 *)(uintptr_t)0x85000008==NPU_VERSION_CODE(7,13,22));
 assert(cycle_reads>0);
 assert(call_arg(1,0,WLAN_FUNC_SET_WAIT_NPU_INIT_DONE,12,0)==NPU_MBOX_SUCCESS);
 assert(lab_started_mask==2 && !lab_band_started(0) && lab_band_started(1));
 assert(call_arg(1,1,WLAN_FUNC_SET_WAIT_NPU_INIT_DONE,12,0)==NPU_MBOX_SUCCESS);
 assert(!lab_started_mask && !lab_any_started());
 puts("HWNAT_INIT: valid enable/disable accepted; invalid values rejected; link notification never starts workers");
 puts("runner ABI: command 2 starts and stops each band independently");
 puts("pre-START: host order and pool accepted without worker acks; invalid pool reasons preserved");
 puts("post-START: missing ack rejected, synchronization retained for active band");
 puts("START: missing pool/ring, invalid band/length, per-band publication and version passed (host model)");
 return 0;
}
