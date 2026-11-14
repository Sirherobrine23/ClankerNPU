#ifndef EN7523_WIRELESS_LAB_H
#define EN7523_WIRELESS_LAB_H
#ifdef LAB_HOST_TEST
#include "../tests/platform.h"
#else
#include "npu_internal.h"
#endif
#define LAB_MAGIC 0x574c3232u
enum lab_reject {
 LAB_REJECT_NONE, LAB_REJECT_LENGTH, LAB_REJECT_CMD,
 LAB_REJECT_PAUSE_IDLE, LAB_REJECT_PAUSE_ACK,
 LAB_REJECT_POOL_ZERO, LAB_REJECT_POOL_ALIGN, LAB_REJECT_POOL_BUSY,
 LAB_REJECT_ARGUMENT, LAB_REJECT_UNSUPPORTED
};
/* mt7915_npu_rx_status consumes this bit and these metadata fields from
 * the 24-byte descriptor's vendor word, not its fragment/info word. */
#define LAB_HOST_RX_PPE (1u << 28)
#define LAB_PPE_HASH_MASK 0x7fffu
#define LAB_IDS 5600u
#define LAB_DIAG_ADDR 0x1e85e000u
struct lab_diag {
 u32 magic, abi, state, error;
 u32 cycles[4], mail_count, last_cmd, last_arg, last_base;
 u32 rx[2], qdma_submit, qdma_complete, qdma_full, ppe_free, ppe_punt;
 u32 host_frames, host_full, host_drop, bad_id, free_ids, fatal_cause, fatal_pc, fatal_val;
 u32 qdma_base, irq_base, irq_cfg, qdma_cpu, qdma_dma, queue_depth[2];
 u32 packet_base, rx_base[2];
 /* Same size/offsets as the former eight reserved diagnostic words. */
 u32 last_reject, reject_cmd, reject_arg, reject_rxd0, reject_rxd1;
 u32 reject_pause, reject_ack1, reject_ack3;
 u32 ppe_reason[32], ppe_last_meta, ppe_invalid_hash, host_ppe;
};
_Static_assert(sizeof(struct lab_diag)<=0x1000,"Lab diagnostics exceed reserved page");
#define lab_diag ((volatile struct lab_diag *)LAB_DIAG_ADDR)
extern volatile u32 lab_started_mask;
extern volatile u32 lab_hwnat_enabled;
void lab_init(void);
void lab_run(u32 hart) __attribute__((noreturn));
void lab_fault(u32 error, u32 cause, u32 pc, u32 val);
void lab_rx_poll(void);
s32 lab_buf_alloc(void);
void lab_buf_free(u32 id);
void lab_buf_init(void);
int lab_qdma_init(void);
void lab_qdma_poll(void);
void lab_ppe_poll(void);
int lab_host_ready(void);
void lab_host_init(void);
void lab_host_poll(void);
void lab_stats_publish(void);
int lab_wifi_dispatch(u32 base, u32 len);
#define LAB_QDMA_BASE 0x1fb54000u
#ifdef LAB_HOST_TEST
static inline void lab_fence(void) { __sync_synchronize(); }
#else
static inline void lab_fence(void) { __asm__ volatile("fence iorw,iorw" ::: "memory"); }
#endif
/* START is published by hart 0 after descriptor/pool setup; no MMIO here. */
static inline int lab_band_started(u32 band)
{
 if(band>1 || !(lab_started_mask & (1u << band)))return 0;
 lab_fence();return 1;
}
static inline int lab_any_started(void)
{
 return lab_band_started(0) || lab_band_started(1);
}
#endif
