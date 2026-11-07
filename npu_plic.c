/*
 * ClankerNPU firmware - interrupt controllers
 *
 * EN7523 has 128 zero-based sources and native claim/complete CSRs.
 * AN75xx has 192 sources, numbered from 1 by hardware and 0 by firmware.
 */

#include "npu_internal.h"

#ifdef EN7523

void plic_global_init(void)
{
	u32 src, hart, word;

	/* Run once, before any hart enables delivery or registers a handler. */
	for (hart = 0; hart < MAX_CORE_NUM; hart++) {
		for (word = 0; word < 4; word++)
			REG32(PLIC_HART_ENABLE(hart, word)) = 0;
		REG32(PLIC_THRESHOLD(hart)) = 0;
	}
	for (src = 0; src < PLIC_MAX_SOURCE; src += 4)
		REG32(PLIC_PRIORITY(src)) = 0x40404040;
	/* Match the vendor priority for the PPE return source. It stays off. */
	REG32(PLIC_PRIORITY(96)) = 0x40404041;
}

void plic_init(void)
{
	u32 word, hart = get_hartid();

	for (word = 0; word < 4; word++)
		REG32(PLIC_HART_ENABLE(hart, word)) = 0;
	REG32(PLIC_THRESHOLD(hart)) = 0;
	/* These local source masks are part of the v1
	 * vendor initialization, not SiFive PLIC contexts. */
	csr_write(0x5e8, ALL_FF);
	csr_write(0x5e9, ALL_FF);
	csr_write(0x5ea, ALL_FF);
	csr_write(0x5eb, ALL_FF);
	csr_write(0x5d8, ALL_FF);
	csr_write(0x5d9, ALL_FF);
	csr_write(0x5da, ALL_FF);
	csr_write(0x5db, ALL_FF);
}

void plic_enable(u32 src)
{
	if (src < PLIC_MAX_SOURCE)
		REG32(PLIC_HART_ENABLE(get_hartid(), src / 32)) |=
			1u << (src & 31);
}

void plic_disable(u32 src)
{
	if (src < PLIC_MAX_SOURCE)
		REG32(PLIC_HART_ENABLE(get_hartid(), src / 32)) &=
			~(1u << (src & 31));
}

void plic_enable_wrapper(u32 src)
{
	plic_enable(src);
}

void plic_register_isr(u32 src, isr_fn_t handler)
{
	if (src >= PLIC_MAX_SOURCE || !handler)
		return;
	if (plic_isr_table[src] && plic_isr_table[src] != handler)
		return;
	plic_isr_table[src] = handler;
	__asm__ volatile("fence iorw, iorw" ::: "memory");
	plic_enable(src);
}

void call_isr_by_src(u32 src)
{
	u32 hart = get_hartid();
	u32 active = REG32(PLIC_ACTIVE(hart));

	if (src < PLIC_MAX_SOURCE && plic_isr_table[src]) {
		plic_isr_table[src](src);
	} else {
		/* Do not print or acquire mutexes in an unexpected ISR. */
		plic_disable(src);
	}
	REG32(PLIC_ACTIVE(hart)) = active;
	__asm__ volatile("fence iorw, iorw" ::: "memory");
	csr_write(0x5c2, src);
}

void plic_handle_irq(void)
{
	u32 hart = get_hartid(), src = csr_read(0x5c0) & 0x7f;

	ndbg->hart[hart].in_isr = 1;
	call_isr_by_src(src);
	ndbg->hart[hart].in_isr = 0;
}

#else


/* ================================================================
 * PLIC (Platform-Level Interrupt Controller)
 * ================================================================ */

static void default_isr(int src)
{
	npu_printf("(%s) implement code to clear intrSrc:%d then execute ISR here\n",
		   "default_isr", src);
}

/* The claim has to be completed on every path or the PLIC never offers
 * that source again. */
void call_isr_by_src(u32 src)
{
	get_hartid();
	if (src > 191) {
		npu_printf("%s just return due to intSrc:%d > %d\n",
			   "call_isr_bySrc", src, 191);
		return;
	}
	if (plic_isr_table[src] != NULL)
		plic_isr_table[src](src);
	REG32(PLIC_CLAIM_REG) = src + 1;
}

static void plic_set_threshold(void)
{
	u32 hart = get_hartid();

	REG32(PLIC_THRESHOLD_REG) = plic_threshold_table[hart] & 0x1F;
}

static u32 plic_get_priority(u32 src)
{
	if (src > 191) {
		npu_printf("%s just return due to intSrc:%d > %d\n",
			   "PLIC_get_priority_bySrc", src, 191);
		return 16;
	}
	return REG32(PLIC_PRIORITY(src)) & 0x1F;
}

static void plic_set_priority(u32 src, u8 prio)
{
	if (src > 191) {
		npu_printf("%s just return due to intSrc:%d > %d\n",
			   "PLIC_set_priority_bySrc", src, 191);
		return;
	}
	REG32(PLIC_PRIORITY(src)) = prio & 0x1F;
}

void plic_enable(u32 src)
{
	u32 word_idx, bit;

	if (src > 191) {
		npu_printf("%s just return due to intSrc:%d > %d\n",
			   "PLIC_enable_intSrc_toOneCore", src, 191);
		return;
	}
	word_idx = (src + 1) >> 5;
	bit = 1u << ((src + 1) & 0x1F);
	REG32(PLIC_MASK_REG(word_idx)) &= ~bit;
	REG32(PLIC_ENABLE_REG(word_idx)) |= bit;
}

void plic_disable(u32 src)
{
	u32 word_idx, bit;

	if (src > 191) {
		npu_printf("%s just return due to intSrc:%d > %d\n",
			   "PLIC_DISABLE_intSrc_toOneCore", src, 191);
		return;
	}
	word_idx = (src + 1) >> 5;
	bit = 1u << ((src + 1) & 0x1F);
	REG32(PLIC_MASK_REG(word_idx)) |= bit;
	REG32(PLIC_ENABLE_REG(word_idx)) &= ~bit;
}

void plic_enable_wrapper(u32 src)
{
	plic_enable(src);
}

void plic_init(void)
{
	u32 i;

	plic_set_threshold();

	for (i = 0; i < 192; i++) {
		plic_disable(i);
		if (i == 95)
			REG32(PLIC_PRIORITY(95)) = 17;
		else
			REG32(PLIC_PRIORITY(i)) = 16;

		if (get_hartid() == 0 && plic_isr_init_done == 0)
			plic_isr_table[i] = default_isr;
	}

	if (get_hartid() == 0)
		plic_isr_init_done = 1;
}

/* A source keeps the first real handler it gets; enable either way */
void plic_register_isr(u32 src, isr_fn_t handler)
{
	isr_fn_t cur;

	if (src > 191) {
		npu_printf("%s just return due to intSrc:%d > %d\n",
			   "PLIC_register_ISR_bySrc", src, 191);
	} else {
		cur = plic_isr_table[src];
		if (handler != default_isr && handler != NULL &&
		    cur != default_isr && cur != NULL)
			npu_printf("Error: src:%d already registered ISR, just exit\n",
				   src);
		else
			plic_isr_table[src] = handler;
	}
	plic_enable(src);
}
#endif /* EN7523 */
