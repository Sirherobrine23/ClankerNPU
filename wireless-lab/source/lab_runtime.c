/* SPDX-License-Identifier: GPL-2.0-only */
#include "lab.h"
#include "npu_wifi.h"
static volatile u32 boot_ok,backend_ready;
static u32 last_ppe_report;
extern volatile u32 lab_pause_request,lab_pause_ack[4];
void lab_fault(u32 err,u32 cause,u32 pc,u32 val)
{
 if(!lab_diag->error) {
  lab_diag->error=err;lab_diag->fatal_cause=cause;
  lab_diag->fatal_pc=pc;lab_diag->fatal_val=val;lab_diag->state=0xee;
 }
 REG32(NPU_MIB(26))=lab_diag->error;
}
void lab_stats_publish(void)
{
 REG32(NPU_MIB(0))=lab_diag->last_reject;
 REG32(NPU_MIB(1))=lab_diag->mail_count;
 REG32(NPU_MIB(2))=lab_diag->last_cmd;REG32(NPU_MIB(3))=lab_diag->last_arg;
 REG32(NPU_MIB(4))=lab_diag->last_base;
 REG32(NPU_MIB(5))=lab_diag->rx[0];REG32(NPU_MIB(6))=lab_diag->rx[1];
 REG32(NPU_MIB(7))=lab_diag->host_full;REG32(NPU_MIB(8))=lab_diag->qdma_full;
 REG32(NPU_MIB(9))=lab_diag->bad_id;
 REG32(NPU_MIB(11))=lab_diag->fatal_cause;REG32(NPU_MIB(12))=lab_diag->fatal_pc;
 REG32(NPU_MIB(13))=lab_diag->fatal_val;
 REG32(NPU_MIB(14))=lab_diag->queue_depth[0];REG32(NPU_MIB(15))=lab_diag->queue_depth[1];
 REG32(NPU_MIB(16))=wifi_pkt_buf_addr;
 REG32(NPU_MIB(17))=lab_diag->host_drop;
 lab_diag->packet_base=wifi_pkt_buf_addr;
 lab_diag->rx_base[0]=rxd_base_2g;lab_diag->rx_base[1]=rxd_base_5g;
 REG32(NPU_MIB(18))=boot_ok|(backend_ready?2u:0)|(lab_hwnat_enabled?4u:0);
 REG32(NPU_MIB(19))=LAB_MAGIC;REG32(NPU_MIB(20))=LAB_DIAG_ADDR;
 REG32(NPU_MIB(22))=lab_diag->rx[0]+lab_diag->rx[1];
 REG32(NPU_MIB(23))=lab_diag->qdma_submit;
 REG32(NPU_MIB(24))=lab_diag->ppe_free;
 REG32(NPU_MIB(25))=lab_diag->host_frames;
 REG32(NPU_MIB(26))=lab_diag->error;
 REG32(NPU_MIB(27))=lab_diag->free_ids;
 REG32(NPU_MIB(28))=lab_diag->qdma_complete;
 REG32(NPU_MIB(29))=lab_diag->ppe_punt;
 REG32(NPU_MIB(30))=lab_diag->state;
 /* At most once per status tick and only after another 2048 punts. */
 if((u32)(lab_diag->ppe_punt-last_ppe_report)>=2048u) {
  last_ppe_report=lab_diag->ppe_punt;
  npu_printf("[WLAB] PPE punt=%u free=%u host_ppe=%u reasons=0d:%u/0e:%u/0f:%u invalid_hash=%u last=%x\n",
             lab_diag->ppe_punt,lab_diag->ppe_free,lab_diag->host_ppe,
             lab_diag->ppe_reason[13],lab_diag->ppe_reason[14],lab_diag->ppe_reason[15],
             lab_diag->ppe_invalid_hash,lab_diag->ppe_last_meta);
 }
}
void lab_init(void)
{
 npu_memset((void *)LAB_DIAG_ADDR,0,sizeof(*lab_diag));
 lab_started_mask=0;lab_hwnat_enabled=0;
 lab_diag->magic=LAB_MAGIC;lab_diag->abi=1;lab_diag->state=1;
 sram_buf_init();
 lab_buf_init();lab_host_init();wifi_bridge_init();
 ba_table_a=sram_buf_alloc(8);ba_table_b=sram_buf_alloc(7);
 if(!ba_table_a || !ba_table_b)lab_fault(14,0,0,0);
 extern char __bss_end[];
 npu_printf("[WLAB] local_end=%x ndbg=%x rx_ready=%x/%x\n",
            (u32)__bss_end,(u32)NDBG_BASE,rxd_2g_init_done,rxd_5g_init_done);
 npu_printf("[WLAB] v7.13.22: data workers wait for WLAN START\n");
 /* Kernel may start the NPU before Ethernet allocates its QDMA rings.
  * Keep the mailbox available and claim QDMA later from the RX worker. */
 if(!lab_diag->error){lab_diag->state=2;boot_ok=1;}
 lab_stats_publish();
}
void lab_run(u32 hart)
{
 u32 last=0,retry_tick=0,wait_start=0,waiting=0;
 npu_dbg_loop(NDBG_TAG('W','L','A','B'));
 for(;;) {
  lab_diag->cycles[hart]++;
  if(hart && lab_pause_request) {
   lab_fence();lab_pause_ack[hart]=1;
   while(lab_pause_request) __asm__ volatile("nop");
   lab_pause_ack[hart]=0;lab_fence();
  }
  if(hart==0) {
   u32 now=csr_read(mcycle);
   if((u32)(now-last)>EN7523_CORE_CLK_MHZ*1000000u){last=now;lab_stats_publish();}
  } else if(hart==1 && boot_ok && !lab_diag->error) {
   /* Wireless setup follows PPE acquisition in the host driver. Wait for
    * that handshake, not just partially published Ethernet registers. */
   if(!backend_ready && lab_any_started()) {
    u32 now=csr_read(mcycle);
    if((u32)(now-retry_tick)>=EN7523_CORE_CLK_MHZ*1000u) {
     retry_tick=now;
     if(!lab_qdma_init()){lab_fence();backend_ready=1;}
    }
    /* No global boot timeout: Ethernet can load late. Bound the wait only
     * once the wireless has supplied the packet pool and an RX ring. */
    if(!backend_ready && lab_any_started()) {
     if(!waiting){wait_start=now;waiting=1;}
     else if((u32)(now-wait_start)>EN7523_CORE_CLK_MHZ*10000000u)
      lab_fault(13,0,0,lab_diag->qdma_base);
    }
   }
   if(backend_ready && lab_any_started()) {
    lab_qdma_poll();
    if(wifi_pkt_buf_addr){lab_ppe_poll();lab_rx_poll();}
   }
  } else if(hart==3 && boot_ok && !lab_diag->error && lab_any_started())lab_host_poll();
  if(lab_diag->error) __asm__ volatile("nop");
 }
}
