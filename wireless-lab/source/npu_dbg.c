/*
 * AN75XX NPU firmware - field debug block
 *
 * A fixed 4 KB window in the global SRAM that the host reads and writes
 * with plain 32-bit accesses: version, per-hart heartbeat and trap
 * record, counters, trace rings and a command mailbox. docs/debug.md
 * has the layout and the commands.
 */

#include "npu_internal.h"
#include "npu_wifi.h"

#define NDBG_XSTR(x)	#x
#define NDBG_STR(x)	NDBG_XSTR(x)

enum {
	NDBG_CMD_PING = 1, NDBG_CMD_READ, NDBG_CMD_WRITE, NDBG_CMD_COPY,
	NDBG_CMD_HEXDUMP, NDBG_CMD_STATUS, NDBG_CMD_CLEAR, NDBG_CMD_TRACE,
	NDBG_CMD_BRIDGE, NDBG_CMD_SRAM, NDBG_CMD_CSR, NDBG_CMD_PROF,
};

enum { NDBG_OK, NDBG_EUNKNOWN, NDBG_EARG, NDBG_ENOTSUP };

static const char *const ndbg_names[16] = {
	"mbox", "wifi", "tdma", "tunnel", "l4s", "ppe", "dba", "sram",
	"trap", "", "", "", "", "", "", "stats",
};

extern u16 sram_alloc_table[];
extern u32 sram_alloc_offset;
extern char __data_start[], __bss_end[];
extern u8 mbox_dispatch[][80];

/* EN7523 Lab debug occupies its own reserved packet-SRAM page. */
_Static_assert(sizeof(*ndbg)==NDBG_SIZE,"debug layout must fit its page");
__asm__(".globl __ndbg_base\n.set __ndbg_base, " NDBG_STR(NDBG_BASE));

static void ndbg_sym(u32 i, u32 tag, u32 addr)
{
	ndbg->sym[i].tag = tag;
	ndbg->sym[i].addr = addr;
}

/* hart 0, before the other harts are released */
void npu_dbg_init(void)
{
	volatile u32 *p = (volatile u32 *)NDBG_BASE;
	const char *v = NPU_VERSION;
	u32 i;

	for (i = 0; i < NDBG_SIZE / 4; i++)
		p[i] = 0;
	for (i = 0; i < 47 && v[i]; i++)
		ndbg->version[i / 4] |= (u32)(u8)v[i] << (24 - 8 * (i & 3));
	ndbg->layout = NDBG_LAYOUT;
	ndbg->size = NDBG_SIZE;
	ndbg->harts = MAX_CORE_NUM;
#ifdef NPU_DATAPATH_DBG
	ndbg->print_mask = 1u << NDBG_STATS | 1u << NDBG_WIFI;
#endif

	i = 0;
	ndbg_sym(i++, NDBG_TAG('G', 'L', 'O', 'B'), (u32)__data_start);
	ndbg_sym(i++, NDBG_TAG('B', 'S', 'S', 'E'), (u32)__bss_end);
	ndbg_sym(i++, NDBG_TAG('M', 'B', 'O', 'X'), (u32)mbox_dispatch);
	ndbg_sym(i++, NDBG_TAG('S', 'R', 'A', 'M'), (u32)sram_alloc_table);
	ndbg_sym(i++, NDBG_TAG('T', 'I', 'C', 'K'), (u32)&timer_raw_tick);
#ifdef HAS_TUNNEL
	ndbg_sym(i++, NDBG_TAG('B', 'R', 'D', 'G'), (u32)&npu_bridge_base);
	ndbg_sym(i++, NDBG_TAG('T', 'U', 'N', 'F'), (u32)tunnel_func_table);
	ndbg_sym(i++, NDBG_TAG('L', '4', 'S', 'E'), (u32)&tunnel_ecn_enabled);
#endif
#ifdef WIFI_EAGLE
	ndbg_sym(i++, NDBG_TAG('E', 'D', 'B', 'G'), (u32)&dbg);
#endif
#ifdef HAS_EAGLE_STA_QLIMIT
	ndbg_sym(i++, NDBG_TAG('S', 'Q', 'L', 'M'), (u32)&wifi_sta_q);
#endif
#ifdef NPU_PROFILE
	ndbg_sym(i++, NDBG_TAG('P', 'R', 'O', 'F'), (u32)&npu_prof);
#endif
#ifdef WIFI_KITE
	ndbg_sym(i++, NDBG_TAG('K', 'F', 'L', 'G'), (u32)&wifi_debug_flags);
	ndbg_sym(i++, NDBG_TAG('K', 'C', '2', 'G'), (u32)&counter_base_2g);
	ndbg_sym(i++, NDBG_TAG('K', 'C', '5', 'G'), (u32)&counter_base_5g);
#endif

	/* written last: a reader that sees the magic sees the rest */
	ndbg->magic = NDBG_MAGIC;
}

void npu_dbg_trace(u32 sub, u32 ev, u32 a, u32 b)
{
	u32 h = get_hartid();
	u32 i = ndbg->hart[h].trace_wr++ & 15;
	volatile struct ndbg_trace *t = &ndbg->trace[h][i];

	t->tick = timer_raw_tick;
	t->id = h << 28 | (sub & 0xFF) << 20 | (ev & 0xFFFFF);
	t->a = a;
	t->b = b;
	/* never from an ISR: the interrupted code may hold the printf mutex */
	if ((ndbg->print_mask & (1u << sub)) && !ndbg->hart[h].in_isr)
		npu_printf("[DBG] %s ev %x %x %x\n", ndbg_names[sub & 15],
			   ev, a, b);
}

/* oldest entry first, every hart */
void npu_dbg_service(u32 hart)
{
 ndbg->ret[0]=hart;ndbg->ret[1]=csr_read(mcycle);
 ndbg->status=(ndbg->cmd==1)?0:3;ndbg->done++;ndbg->cmd=0;
}
void npu_dbg_idle(void) { for(;;)npu_dbg_poll(); }
