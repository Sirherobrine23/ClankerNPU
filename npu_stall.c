#include "npu_internal.h"
#ifdef NPU_STALL_PROFILE
/* Each hart writes only its own slots. The host should read snapshots;
 * a snapshot during a write may contain a partially updated sum. */
volatile struct npu_stall_data npu_stall = {
 .magic = 0x5354414cu, .threshold_cycles = 64,
 .nharts = MAX_CORE_NUM, .nevents = ST_MAX,
};

void npu_stall_record(u32 id, u32 cycles, int failure, u32 pc)
{
 u32 hart = get_hartid();
 volatile struct npu_stall_stat *s;
 u32 lo;
 if (hart >= MAX_CORE_NUM || id >= ST_MAX)
  return;
 s = &npu_stall.hart[hart][id];
 s->calls++;
 if (cycles >= npu_stall.threshold_cycles)
  s->slow++;
 if (failure)
  s->failures++;
 lo = s->sum_lo + cycles;
 if (lo < s->sum_lo)
  s->sum_hi++;
 s->sum_lo = lo;
 if (cycles > s->max_cycles) {
  s->max_cycles = cycles;
  s->last_max_pc = pc;
 }
}

/* Only invoked by an explicit NDBG STATUS command; never by RX/TX paths. */
void npu_stall_dump(void)
{
 u32 hart, id;
 for (hart = 0; hart < MAX_CORE_NUM; hart++) {
  for (id = 0; id < ST_MAX; id++) {
   volatile struct npu_stall_stat *s = &npu_stall.hart[hart][id];
   if (!s->calls) continue;
   npu_printf("[STAL] h=%u ev=%u calls=%u slow=%u fail=%u max=%u sum=%x:%x pc=%x\n",
    hart, id, s->calls, s->slow, s->failures, s->max_cycles,
    s->sum_hi, s->sum_lo, s->last_max_pc);
  }
 }
}
#endif
