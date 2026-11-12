/* EN7523 firmware bottleneck profiling; no logging in packet path. */
#ifndef NPU_STALL_H
#define NPU_STALL_H

enum npu_stall_id {
 ST_GDMA_WAIT, ST_GDMA_COPY, ST_MUTEX, ST_MUTEX_PRI,
 ST_HOST_RING, ST_PINODE, ST_RXNODE, ST_MBOX_NOTIFY,
 ST_PRINTF, ST_PLIC_ISR, ST_BUFID_ALLOC, ST_BA_TIMEOUT,
 ST_BA_SEQ, ST_TDMA_TX, ST_FORWARD, ST_MAX
};

#ifdef NPU_STALL_PROFILE
/* All numbers are cycles of the *calling* hart. 32-bit wrap is
 * handled by unsigned subtraction for measurements shorter than 12 s. */
struct npu_stall_stat {
 u32 calls;
 u32 slow;
 u32 failures;
 u32 max_cycles;
 u32 sum_lo;
 u32 sum_hi;
 u32 last_max_pc;
};
struct npu_stall_data {
 u32 magic;             /* 'STAL' */
 u32 threshold_cycles;  /* 64 cycles (~0.2us at 333MHz) */
 u32 nharts;
 u32 nevents;
 struct npu_stall_stat hart[MAX_CORE_NUM][ST_MAX];
};
extern volatile struct npu_stall_data npu_stall;
void npu_stall_record(u32 id, u32 cycles, int failure, u32 pc);
void npu_stall_dump(void);
static inline u32 npu_stall_cycles(void)
{
 u32 value;
 __asm__ volatile ("csrr %0, 0xC00" : "=r"(value) :: "memory");
 return value;
}
#define NPU_STALL_RECORD(_id, _from, _fail) \
 npu_stall_record((_id), npu_stall_cycles() - (_from), (_fail), \
                  (u32)__builtin_return_address(0))
#else
static inline u32 npu_stall_cycles(void) { return 0; }
#define NPU_STALL_RECORD(_id, _from, _fail) do { (void)(_from); } while (0)
#endif
#endif
