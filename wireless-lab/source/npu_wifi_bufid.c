/* Clanker counter ABI, EN7523 Lab software buffer pool. */
#include "lab.h"
#include "npu_wifi.h"
NPU_HOT s32 buf_id_alloc_hw(u32 type,u32 band) { (void)type;(void)band;return lab_buf_alloc(); }
NPU_HOT void buf_id_free(u32 type,u32 band,u32 id) { (void)type;(void)band;lab_buf_free(id); }
u32 counter_base_2g,counter_base_5g,counter_base_tri;
u32 wcid_counter_base_2g,wcid_counter_base_5g;
/* per-band counter words: 170 on AN7552 (types 9/10 are 0x2A8) */
#ifdef AN7552
#define BAND_COUNTER_WORDS	170
#else
#define BAND_COUNTER_WORDS	250
#endif

void counter_init(u32 band)
{
	u32 *base;
	u32 count;

	npu_printf("%s:%d\n", "counter_init", band);
	if (band == 1) {
		counter_base_5g = sram_buf_alloc(9);
		base = (u32 *)counter_base_5g;
		count = BAND_COUNTER_WORDS;
	} else if (band == 0) {
		counter_base_2g = sram_buf_alloc(10);
		base = (u32 *)counter_base_2g;
		count = BAND_COUNTER_WORDS;
	} else {
		counter_base_tri = sram_buf_alloc(11);
		base = (u32 *)counter_base_tri;
		count = 28;
	}

	npu_memset(base, 0, count * 4);
}

void wcid_counter_init(u32 band)
{
	u32 *base;

	npu_printf("%s:%d\n", "wcid_counter_init", band);

	if (band == 1) {
		wcid_counter_base_5g = sram_buf_alloc(19);
		base = (u32 *)wcid_counter_base_5g;
	} else if (band == 0) {
		wcid_counter_base_2g = sram_buf_alloc(20);
		base = (u32 *)wcid_counter_base_2g;
	} else {
		return;
	}

	npu_memset(base, 0, 256 * 4);
}
