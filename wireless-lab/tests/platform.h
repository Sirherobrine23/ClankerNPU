#ifndef LAB_HOST_PLATFORM_H
#define LAB_HOST_PLATFORM_H
#define NPU_INTERNAL_H
#define NPU_TYPES_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef uint32_t u32;typedef uint16_t u16;typedef uint8_t u8;
typedef int32_t s32;typedef int16_t s16;typedef int8_t s8;
typedef uint64_t u64;typedef int64_t s64;
#define NPU_HOT
#define NPU_INLINE inline
#define MT7915 1
extern u32 wifi_pkt_buf_addr;
extern u8 wifi_debug_flags;
#define EN7523 1
#define NPU_ADDR_MASK 0x40000000
static inline u32 irq_save(void){return 0;}
static inline void irq_restore(u32 x){(void)x;}
int hw_mutex_lock(u32 *p);int hw_mutex_unlock(u32 *p);
void *npu_memcpy(void *d,const void *s,u32 n);
void *npu_memset(void *d,int v,u32 n);
u32 sram_buf_alloc(u32 type);
#include "../../npu_regs.h"
#include "../../npu_config.h"
extern u8 wifi_dbdc_mode,wifi_pcie_state[2];
extern u32 ba_table_a,ba_table_b,wifi_dram_ba_node_addr;
extern volatile u32 rxd_2g_init_done,rxd_5g_init_done;
extern u32 rxd_base_2g,rxd_base_5g;
extern u32 rxd_2g_flush_tick,rxd_5g_flush_tick;
extern volatile u32 timer_slow_tick;
extern u8 wifi_no_ba_test;
#define KITE_TICK timer_slow_tick
extern u32 wcid_counter_base_2g,wcid_counter_base_5g;
int npu_printf(const char *fmt,...);
u32 test_cycle(void);
#define csr_read(reg) test_cycle()
#define en7523_bseti(a,k) ((a)|(1u<<(k)))
#define en7523_bclri(a,k) ((a)&~(1u<<(k)))
#endif
