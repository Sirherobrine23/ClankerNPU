/* EN7523 MT7915 receive transport and direct QDMA ingress. */
#include "npu_internal.h"
#include "npu_wifi.h"

#if defined(EN7523) && defined(HAS_WIFI)

#define RX_DESCS	1536
#define RX_BYTES	3500
#define RX_DONE		0x80000000
#define RX_LAST		0x40000000
#define RX_BATCH	32

extern volatile u32 en7523_wifi_ready;
extern volatile u32 en7523_wifi_enabled[2];
extern volatile u32 en7523_wifi_busy[2];
extern u32 en7523_wifi_prepared[2];
extern u32 en7523_wifi_regs[2];
extern u32 en7523_wifi_ring[2];
extern u32 en7523_wifi_index[2];
extern u32 en7523_wifi_port[2];
extern u32 en7523_wifi_total[2];
extern u32 en7523_wifi_part[2];
extern u32 en7523_wifi_drop[2];
extern volatile u32 en7523_wifi_paused[2];
extern volatile u32 en7523_wifi_direct[2];
extern u32 en7523_wifi_stats[2][4];
static u32 chain_first[2], chain_last[2], chain_bytes[2];

static void wifi_fence(void)
{
	__asm__ volatile("fence iorw, iorw" ::: "memory");
}

void en7523_wifi_init(void)
{
	u32 base;

	sram_buf_init();
	buf_mgr_init();
	base = sram_buf_alloc(1);
	if (!base)
		return;
	en7523_wifi_ring[0] = base + 0x6020;
	en7523_wifi_ring[1] = base;
	chain_first[0] = chain_first[1] = 0xffff;
	en7523_rx_state_init();
	wifi_fence();
	en7523_wifi_ready = 1;
}

static int en7523_wifi_prepare(u32 band, u32 count)
{
	volatile u32 *desc = (volatile u32 *)en7523_wifi_ring[band];
	u32 i, window;
	s32 id;

	if (count != RX_DESCS || !wifi_pkt_buf_addr ||
	    !en7523_wifi_regs[band] || en7523_wifi_enabled[band])
		return NPU_MBOX_EINVAL;
	if (en7523_wifi_prepared[band])
		return NPU_MBOX_SUCCESS;

	/* The SDK's P1 window is at SCU +0x38, P0 at +0x30. */
	window = en7523_wifi_port[band] ? PCIE0_WIN_BASE : PCIE1_WIN_BASE;
	REG32(window) = en7523_wifi_ring[1];
	REG32(window + 4) = en7523_wifi_ring[0] + RX_DESCS * 16;
	for (i = 0; i < count; i++, desc += 4) {
		id = buf_mgr_alloc();
		if (id < 0)
			return NPU_MBOX_ERROR;
		desc[0] = wifi_pkt_buf_addr + ((u32)id << 12);
		desc[1] = RX_BYTES << 16;
		desc[2] = 0;
		desc[3] = 0;
	}
	wifi_fence();
	en7523_wifi_prepared[band] = 1;
	return NPU_MBOX_SUCCESS;
}

static int en7523_wifi_run(u32 band, u32 enable)
{
	u32 i, base;

	if (enable > 1)
		return NPU_MBOX_EINVAL;
	if (!enable) {
		en7523_wifi_enabled[band] = 0;
		wifi_fence();
		for (i = 0; i < 100; i++) {
			if (!en7523_wifi_busy[band] && !en7523_ppe_pending(band)) {
				wifi_fence();
				ba_flush_all(band);
				en7523_rx_poll(band);
			hostadpt_rx_flush(band);
				en7523_rx_free(chain_first[band]);
				chain_first[band] = 0xffff;
				return NPU_MBOX_SUCCESS;
			}
			delay_ms_mcycle(1);
		}
		return NPU_MBOX_ERROR;
	}
	if (!en7523_wifi_prepared[band])
		return NPU_MBOX_ENODEV;
	base = REG32(HOSTADPT_RX_BASE_PTR(band));
	if ((base & 3) || base < 0x80000000 || base > 0xbfffd000 ||
	    REG32(HOSTADPT_RX_MAX_CNT(band)) != 512)
		return NPU_MBOX_EINVAL;
	if (en7523_wifi_enabled[band])
		return NPU_MBOX_SUCCESS;
	hostadpt_rx_init(band);
	en7523_wifi_total[band] = 0;
	en7523_wifi_part[band] = 0;
	en7523_wifi_drop[band] = 0;
	wifi_fence();
	en7523_wifi_enabled[band] = 1;
	return NPU_MBOX_SUCCESS;
}

int en7523_wifi_set(u32 *msg)
{
	u32 band = msg[0] & 15, val = msg[2];

	if (band > 1)
		return NPU_MBOX_EINVAL;
	if (!en7523_wifi_ready)
		return NPU_MBOX_ENODEV;
	switch (msg[1]) {
	case WLAN_FUNC_SET_WAIT_NPU_INIT_DONE:
		return en7523_wifi_run(band, val);
	case WLAN_FUNC_SET_WAIT_PCIE_ADDR:
		if (!val || (val & 3) || val >= 0x80000000 ||
		    en7523_wifi_prepared[band])
			return NPU_MBOX_EINVAL;
		en7523_wifi_regs[band] = val;
		break;
	case WLAN_FUNC_SET_WAIT_DESC:
		return en7523_wifi_prepare(band, val);
	case WLAN_FUNC_SET_WAIT_DRIVER_MODEL:
		if (val > 1)
			return NPU_MBOX_EOPNOTSUPP;
		break;
	case WLAN_FUNC_SET_WAIT_PKT_BUF_ADDR:
		if (val < 0x80000000 || val > 0xc0000000 - (5600u << 12) ||
		    (val & 0xfff))
			return NPU_MBOX_EINVAL;
		/* Both PCIe consumers share one pool without resetting its IDs. */
		if (val != wifi_pkt_buf_addr &&
		    (en7523_wifi_prepared[0] || en7523_wifi_prepared[1]))
			return NPU_MBOX_EINVAL;
		wifi_pkt_buf_addr = val;
		break;
	case WLAN_FUNC_SET_WAIT_PCIE_PORT_TYPE:
		if (val > 1 || en7523_wifi_prepared[band])
			return NPU_MBOX_EINVAL;
		en7523_wifi_port[band] = val;
		break;
	case WLAN_FUNC_SET_WAIT_IS_FORCE_TO_CPU:
		if (val > 1)
			return NPU_MBOX_EINVAL;
		en7523_wifi_direct[band] = !val;
		break;
	default:
		return NPU_MBOX_EOPNOTSUPP;
	}
	return NPU_MBOX_SUCCESS;
}

int en7523_wifi_get(u32 *msg)
{
	u32 band = msg[0] & 15;

	if (band > 1)
		return NPU_MBOX_EINVAL;
	if (!en7523_wifi_ready)
		return NPU_MBOX_ENODEV;
	if (msg[1] == WLAN_FUNC_GET_WAIT_DBG_COUNTER) {
		u32 i;

		for (i = 0; i < 4; i++)
			msg[2 + i] = en7523_wifi_stats[band][i];
	} else {
		msg[2] = msg[1] == WLAN_FUNC_GET_WAIT_RXDESC_BASE ?
			 en7523_wifi_ring[band] : 0;
	}
	return NPU_MBOX_SUCCESS;
}

static int en7523_wifi_rx(u32 band)
{
	u32 idx = en7523_wifi_index[band];
	volatile u32 *desc = (volatile u32 *)(en7523_wifi_ring[band] + idx * 16);
	u32 ctrl = desc[1], buf, len, id, total;
	s32 replacement;

	if (!(ctrl & RX_DONE))
		return 0;
	wifi_fence();
	buf = desc[0];
	len = (ctrl >> 16) & 0x3fff;
	id = (buf - wifi_pkt_buf_addr) >> 12;
	if (!en7523_wifi_part[band]) {
		en7523_wifi_drop[band] = 0;
		chain_bytes[band] = 0;
		en7523_wifi_total[band] = ctrl & RX_LAST ? len : REG32(buf) & 0xffff;
	}
	total = en7523_wifi_total[band];
	replacement = buf_mgr_alloc();
	if (!len || len > RX_BYTES || !total || total > 16383 ||
	    id >= 5600 || replacement < 0 || en7523_wifi_part[band] >= 7 ||
	    en7523_wifi_drop[band]) {
		if (replacement >= 0)
			buf_mgr_free(replacement);
		en7523_rx_free(chain_first[band]);
		chain_first[band] = 0xffff;
		en7523_wifi_drop[band] = 1;
	} else {
		en7523_rx_link(id, len, 0xffff);
		if (chain_first[band] == 0xffff)
			chain_first[band] = id;
		else
			en7523_rx_link(chain_last[band],
				      chain_bytes[band] & 0xffff, id);
		chain_last[band] = id;
		chain_bytes[band] = len;
		desc[0] = wifi_pkt_buf_addr + ((u32)replacement << 12);
		if (ctrl & RX_LAST) {
			en7523_rx_process(chain_first[band], total, band);
			chain_first[band] = 0xffff;
		}
	}
	desc[1] = RX_BYTES << 16;
	wifi_fence();
	REG32(en7523_wifi_regs[band] + 8) = idx;
	en7523_wifi_index[band] = idx + 1 == RX_DESCS ? 0 : idx + 1;
	en7523_wifi_part[band] = ctrl & RX_LAST ? 0 :
		(en7523_wifi_part[band] < 8 ? en7523_wifi_part[band] + 1 : 8);
	if (ctrl & RX_LAST)
		en7523_wifi_drop[band] = 0;
	return 1;
}

void en7523_wifi_discard(u32 band)
{
	u32 n, idx = en7523_wifi_index[band];
	volatile u32 *desc;

	en7523_rx_free(chain_first[band]);
	chain_first[band] = 0xffff;
	en7523_wifi_part[band] = 0;
	for (n = 0; n < RX_DESCS; n++) {
		desc = (volatile u32 *)(en7523_wifi_ring[band] + idx * 16);
		if (!(desc[1] & RX_DONE))
			break;
		desc[1] = RX_BYTES << 16;
		wifi_fence();
		REG32(en7523_wifi_regs[band] + 8) = idx;
		idx = idx + 1 == RX_DESCS ? 0 : idx + 1;
	}
	en7523_wifi_index[band] = idx;
}

void en7523_wifi_rx_loop(u32 band)
{
	u32 i;

	npu_dbg_loop(NDBG_TAG('W', 'R', 'X', '0') + band);
	while (1) {
		npu_dbg_poll();
		en7523_wifi_busy[band] = 1;
		wifi_fence();
		en7523_ppe_rx_poll(band);
		if (en7523_wifi_enabled[band] && !en7523_wifi_paused[band]) {
			for (i = 0; i < RX_BATCH; i++)
				if (!en7523_wifi_rx(band))
					break;
			en7523_rx_poll(band);
			hostadpt_rx_flush(band);
		}
		wifi_fence();
		en7523_wifi_busy[band] = 0;
	}
}

#endif
