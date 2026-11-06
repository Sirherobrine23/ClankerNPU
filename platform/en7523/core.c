/* EN7523 platform entry for the shared ClankerNPU runtime. */
#include "npu_internal.h"

extern void en7523_park(void);
extern volatile u32 en7523_core_data_magic;

#define CORE_RECORD(hart) ((volatile u32 *)(0x840f0000 + (hart) * 64))
#define CORE_PACKET ((volatile u32 *)0x840f1000)
#define CORE_TRAP 0x54524150
static volatile u32 runtime_ready;
static volatile u32 requests;

void en7523_core_init(void)
{
	u32 hart = get_hartid(), sp;
	volatile u32 *record = CORE_RECORD(hart);

	if (en7523_core_data_magic != 0x434f5245) {
		record[13] = 0x45525204;
		record[3] = 3;
		for (;;)
			__asm__ volatile("nop");
	}
	/* No AN75xx bus reset, port power-down, TDMA or BME initialization. */
	if (!hart) {
		npu_dbg_init();
		mailbox_init();
		__asm__ volatile("fence rw, rw" ::: "memory");
		runtime_ready = 1;
	} else {
		while (!runtime_ready)
			__asm__ volatile("nop");
		__asm__ volatile("fence rw, rw" ::: "memory");
	}
	__asm__ volatile("mv %0, sp" : "=r" (sp));
	record[14] = sp;
	record[13] = 0x434f5200 | hart; /* COR: common runtime C entry. */
	record[9] = csr_read(0x7c0);
	record[10] = csr_read(0x9c0);
	irq_disable();
	csr_write(mie, 0);
	csr_write(mtvec, (u32)trap_vector);
	npu_dbg_loop(NDBG_TAG('C', 'O', 'R', 'E'));
	__asm__ volatile("fence rw, rw" ::: "memory");
	record[3] = 1;
	REG32(NPU_MIB31) = INIT_COMPLETE;
}

void en7523_core_loop(void)
{
	u32 hart = get_hartid();
	volatile u32 *record = CORE_RECORD(hart);

	for (;;) {
		record[4]++;
		ndbg->hart[hart].beat++;
		if (hart && record[15] == CORE_TRAP) {
			record[15] = 0;
			__asm__ volatile("fence rw, rw\necall" ::: "memory");
		}
		/* Use the shared mailbox dispatcher, with a polling v1 transport. */
		if (!hart && (REG32(MBQ_CTRL3(0)) & 3) == 1) {
			__asm__ volatile("fence iorw, iorw" ::: "memory");
			mbox_isr(8);
		}
	}
}

int en7523_diag_mail(u32 base, u32 size)
{
	volatile u32 *p = CORE_PACKET;
	u32 i, command, hart;

	requests++;
	CORE_RECORD(0)[12] = requests;
	if (base != (u32)p || size != 64)
		return 0;
	if (p[0] != 0x4e50444d || p[1] != 1)
		return 0;
	command = p[3];
	if (command > 2)
		goto reject;
	if (command == 2) {
		hart = p[12];
		if (!hart || hart >= MAX_CORE_NUM || CORE_RECORD(hart)[3] != 1)
			goto reject;
		CORE_RECORD(hart)[15] = CORE_TRAP;
	}
	p[5] = 0x00020000;
	p[6] = MAX_CORE_NUM;
	for (i = 0; i < MAX_CORE_NUM; i++)
		p[7 + i] = CORE_RECORD(i)[3];
	p[11] = requests;
	p[13] = 0; /* Core-only capability mask; no WLAN/PPE commands. */
	p[4] = 0;
	__asm__ volatile("fence rw, rw" ::: "memory");
	return 1;
reject:
	p[4] = 1;
	return 0;
}

u32 en7523_terminal_trap(u32 cause, u32 pc, u32 *sp)
{
	u32 hart = get_hartid();
	volatile u32 *record = CORE_RECORD(hart);

	irq_disable();
	csr_write(mie, 0);
	csr_write(mtvec, (u32)en7523_park);
	record[5] = cause;
	record[6] = pc;
	record[7] = csr_read(mtval);
	record[8] = csr_read(mstatus);
	ndbg->hart[hart].mcause = cause;
	ndbg->hart[hart].mepc = pc;
	ndbg->hart[hart].sp = (u32)sp;
	ndbg->hart[hart].trap_cnt++;
	__asm__ volatile("fence rw, rw" ::: "memory");
	record[3] = 2;
	for (;;)
		__asm__ volatile("nop");
}
