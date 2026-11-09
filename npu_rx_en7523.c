/* EN7523 MT7915 open-network RX reordering and PPE handoff. */
#include "npu_internal.h"
#include "npu_wifi.h"

#if defined(EN7523) && defined(HAS_WIFI)
struct en7523_sta {
	u8 enabled;
	u16 generation;
};

struct en7523_frame {
	struct kite_node node;
	u16 fc;
	u8 translated, sec, fragment, bad;
	u8 ta[6];
};

_Static_assert(sizeof(struct en7523_sta) * 2 * (EN7523_STA_MAX + 1) <= 0xc000,
	       "EN7523 station state exceeds its SRAM allocation");

static struct en7523_sta *stations;
static u32 *rx_chain;

void en7523_rx_link(u32 id, u32 len, u32 next)
{
	rx_chain[id] = (len << 16) | next;
}

u32 en7523_rx_next(u32 id)
{
	return rx_chain[id] & 0xffff;
}

void en7523_rx_free(u32 id)
{
	u32 next, n = 0;

	while (id < 5600 && n++ < 8) {
		next = en7523_rx_next(id);
		buf_mgr_free(id);
		id = next;
	}
}
extern volatile u32 en7523_wifi_busy[2];
extern volatile u32 en7523_wifi_paused[2];
extern volatile u32 en7523_wifi_direct[2];
extern volatile u32 en7523_wifi_enabled[2];
extern u32 en7523_wifi_stats[2][4];

static u32 rx_word(u32 addr)
{
	u8 *p = (u8 *)addr;

	return p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24;
}

static int rx_decode(u32 id, u32 len, struct en7523_frame *f)
{
	u32 buf = wifi_pkt_buf_addr + (id << 12), off = 24;
	u32 w0, w1, w2, w3, w4, i, seq = 0, fc = 0;

	if (id >= 5600 || len < 24 || len > 16383)
		return -1;
	w0 = rx_word(buf);
	w1 = rx_word(buf + 4);
	w2 = rx_word(buf + 8);
	w3 = rx_word(buf + 12);
	w4 = rx_word(buf + 16);
	/* Normal, individually addressed frames only; errors stay with mt76. */
	if ((w0 >> 27) != 2 || ((w3 >> 16) & 3) != 1)
		return -1;
	f->bad = !!((w1 & 0x0e000000) || (w2 & 0x01800000));
	f->node.wcid = w1 & 0x3ff;
	if (!(w1 & 0x3ff) || (w1 & 0x3ff) > EN7523_STA_MAX)
		return -1;
	f->node.buf_id = id;
	f->node.len = len;
	f->node.tid = (w2 >> 16) & 15;
	f->node.bss = w2 & 63;
	f->node.amsdu = w4 & 3;
	f->translated = !!(w2 & 0x2000);
	f->fragment = !!(w2 & 0x08000000);
	f->sec = (w1 >> 16) & 31;
	if (w1 & 0x4000) {
		if (off + 16 > len)
			return -1;
		fc = rx_word(buf + off) & 0xffff;
		seq = rx_word(buf + off + 8) & 0xffff;
		for (i = 0; i < 6; i++)
			f->ta[i] = *(u8 *)(buf + off + 2 + i);
		off += 16;
	} else if (f->translated) {
		return -1;
	}
	if (w1 & 0x800) {
		off += 16;
	}
	if (w1 & 0x1000)
		off += 8;
	if (w1 & 0x2000) {
		off += 8;
		if (w1 & 0x8000)
			off += 72;
	}
	off += 2 * ((w2 >> 14) & 3);
	if (off + (f->translated ? 14 : 2) > len || off > 255)
		return -1;
	if (!f->translated) {
		fc = rx_word(buf + off) & 0xffff;
		if ((fc & 0xfc) == 0x84) {
			if (off + 20 > len)
				return -1;
			seq = rx_word(buf + off + 18) & 0xffff;
			f->node.tid = (rx_word(buf + off + 16) >> 12) & 15;
		} else {
			if (off + 24 > len)
				return -1;
			seq = rx_word(buf + off + 22) & 0xffff;
		}
	}
	if ((fc & 0x8c) == 8)
		f->node.tid = 0;
	f->fc = fc;
	f->node.sn = seq >> 4;
	f->fragment |= !!(seq & 15);
	f->node.hdr = off;
	return 0;
}

void en7523_rx_state_init(void)
{
	u32 base = sram_buf_alloc(44), i;

	stations = (struct en7523_sta *)base;
	rx_chain = (u32 *)sram_buf_alloc(45);
	npu_memset(stations, 0, 2 * (EN7523_STA_MAX + 1) * sizeof(*stations));
	ba_table_a = sram_buf_alloc(8);
	ba_table_b = sram_buf_alloc(7);
	npu_memset((void *)ba_table_a, 0, 0x8340);
	npu_memset((void *)ba_table_b, 0, 0x8340);
	wifi_flushone_timeout = 3;
	wifi_queue_mutex_init();
	wifi_ba_node_init();
	/* EN7523 uses the SRAM nodes, with no secondary DRAM pool. */
	reorder_sec_ridx = 1;
	reorder_sec_widx = 0;
	for (i = 0; i < 2; i++) {
		counter_init(i);
		wcid_counter_init(i);
	}
}

int en7523_rx_managed(u32 id, u32 len, u32 band)
{
	struct en7523_frame f;
	struct en7523_sta *sta;

	if (rx_decode(id, len, &f) || f.bad || f.node.tid >= 8 ||
	    (f.fc & 0x0c) != 8)
		return 0;
	sta = &stations[band * (EN7523_STA_MAX + 1) + f.node.wcid];
	/* Encrypted traffic keeps the host's key and replay validation. */
	if (f.sec || (f.fc & 0x4000))
		return 0;
	return !!(sta->enabled & (1 << f.node.tid));
}

int en7523_rx_parse(u32 id, struct kite_node *node, u32 len, u32 band)
{
	struct en7523_frame f;

	(void)band;
	if (rx_decode(id, len, &f) || f.bad || f.fragment ||
	    (f.fc & 0x0400) || (f.fc & 0xfc) != 0x88)
		return -1;
	npu_memcpy(node, &f.node, sizeof(*node));
	return 0;
}

int en7523_rx_release(u32 id, u32 len, u32 band, int reordered)
{
	struct en7523_frame f;
	struct en7523_sta *sta;
	u32 info = (reordered & 1) ? EN7523_RX_REORDERED : 0;
	u32 addr, i, next, cur, part = 1, parts, sum = 0;
	int ret = 0;

	if (reordered & 4)
		info |= EN7523_RX_BYPASS_BA;

	if (rx_decode(id, len, &f))
		goto host;
	if (f.bad)
		info = 0;
	sta = &stations[band * (EN7523_STA_MAX + 1) + f.node.wcid];
	/* DEL_STA invalidates queued MPDUs before their WCID can be reused. */
	if ((reordered & 1) && (!sta->enabled || en7523_wifi_paused[band] ||
				!en7523_wifi_enabled[band])) {
		en7523_rx_free(id);
		return 0;
	}
	addr = wifi_pkt_buf_addr + (id << 12) + f.node.hdr;
	if (!en7523_wifi_direct[band] || !sta->enabled || !(reordered & 1) ||
	    en7523_rx_next(id) != 0xffff ||
	    !f.translated || f.bad || f.fragment || (f.fc & 0x0400) ||
	    (f.fc & 0x0300) != 0x0100 || (f.fc & 0x0c) != 8 ||
	    f.sec || (f.fc & 0x4000) || f.node.bss >= 16 ||
	    (*(u8 *)addr & 1) || (rx_word(addr + 12) & 0xffff) != 8 ||
	    len - f.node.hdr < 34 || (*(u8 *)(addr + 14) >> 4) != 4 ||
	    (*(u8 *)(addr + 14) & 15) != 5 ||
	    (rx_word(addr + 20) & 0xff3f) ||
	    (*(u8 *)(addr + 23) != 6 && *(u8 *)(addr + 23) != 17))
		goto host;
	/* Header translation must retain the associated transmitter as SA. */
	for (i = 0; i < 6; i++)
		if (*(u8 *)(addr + 6 + i) != f.ta[i])
			goto host;
	/* Translation errors require the host's LLC/VLAN repair path. */
	if (rx_word(wifi_pkt_buf_addr + (id << 12) + 8) & 0x02000000)
		goto host;
	if (!en7523_qdma_submit(id, addr, len - f.node.hdr, band,
			       f.node.wcid, f.node.bss, sta->generation)) {
		en7523_wifi_stats[band][0]++;
		return 0;
	}
	en7523_wifi_stats[band][3]++;
host:
	next = id;
	parts = 0;
	while (next < 5600 && parts++ < 8) {
		sum += rx_chain[next] >> 16;
		next = en7523_rx_next(next);
	}
	if (sum != len || parts > 7 || next != 0xffff) {
		en7523_rx_free(id);
		return -1;
	}
	while (id < 5600) {
		cur = rx_chain[id] >> 16;
		next = en7523_rx_next(id);
		if (!ret)
			ret = host_ring_submit(wifi_pkt_buf_addr + (id << 12),
					       cur, band, 0, 0,
					       (part << 2) | (parts << 5),
					       len, next == 0xffff, info);
		buf_mgr_free(id);
		id = next;
		part++;
	}
	en7523_wifi_stats[band][1]++;
	return ret;
}

/* Keep the RX descriptor so exceptions use the normal mac80211 receive path. */
int en7523_rx_exception(u32 id, u32 offset, u32 len, u32 band,
			u32 wcid, u32 generation, u32 vif, u32 metadata)
{
	struct en7523_sta *sta;
	int ret;

	if (band > 1 || !wcid || wcid > EN7523_STA_MAX ||
	    !en7523_wifi_enabled[band] || en7523_wifi_paused[band])
		return -1;
	sta = &stations[band * (EN7523_STA_MAX + 1) + wcid];
	if (!sta->enabled || sta->generation != generation || offset > 255 ||
	    offset + len > 3500)
		return -1;
	ret = host_ring_submit(wifi_pkt_buf_addr + (id << 12),
			       offset + len, band, vif, 0, (1 << 2) | (1 << 5),
			       offset + len, 1, EN7523_RX_PPE | EN7523_RX_REORDERED |
			       (metadata & 0x1fffff));
	if (!ret)
		en7523_wifi_stats[band][1]++;
	return ret;
}

static int rx_pause(u32 band)
{
	u32 n;

	en7523_wifi_paused[band] = 1;
	__asm__ volatile("fence iorw, iorw" ::: "memory");
	for (n = 0; n < 100; n++) {
		if (!en7523_wifi_busy[band])
			return 0;
		delay_ms_mcycle(1);
	}
	/* Keep RX paused when the ownership handoff cannot be acknowledged. */
	return -1;
}

int en7523_rx_control(u32 *msg)
{
	u32 band = msg[0] & 15, cmd = msg[1], wcid = msg[2], tid, val;
	u32 *entry;
	struct en7523_sta *sta;

	if (band > 1 || !stations)
		return NPU_MBOX_ENODEV;
	if (cmd == WLAN_FUNC_SET_WAIT_HWNAT_INIT)
		return en7523_qdma_init();
	if (cmd != WLAN_FUNC_SET_WAIT_BA_WIN_SIZE &&
	    cmd != WLAN_FUNC_SET_WAIT_DEL_STA)
		return NPU_MBOX_EOPNOTSUPP;
	if (!wcid || wcid > EN7523_STA_MAX)
		return NPU_MBOX_EINVAL;
	if (rx_pause(band)) {
		npu_printf("[RX CONTROL] pause timeout band=%u cmd=%u wcid=%u busy=%u\n",
			   band, cmd, wcid, en7523_wifi_busy[band]);
		return NPU_MBOX_ERROR;
	}
	sta = &stations[band * (EN7523_STA_MAX + 1) + wcid];
	if (cmd == WLAN_FUNC_SET_WAIT_DEL_STA) {
		sta->enabled = 0;
		sta->generation++;
		for (tid = 0; tid < 8; tid++) {
			entry = (u32 *)((band ? ba_table_b : ba_table_a) +
				       28 * (8 * (wcid - 1) + tid));
			ba_flush_entry(entry);
			npu_memset(entry, 0, 28);
		}
		goto resume;
	}
	/* The extended BA request supplies WCID, TID, SSN and window size. */
	tid = msg[3];
	val = msg[5];
	if (tid > 7 || val > 256) {
		en7523_wifi_paused[band] = 0;
		return NPU_MBOX_EINVAL;
	}
	entry = (u32 *)((band ? ba_table_b : ba_table_a) +
		       28 * (8 * (wcid - 1) + tid));
	ba_flush_entry(entry);
	*(u16 *)((u32)entry + 16) = val;
	*(u16 *)((u32)entry + 18) = (msg[4] - 1) & 0xfff;
	*(u8 *)((u32)entry + 25) = band;
	*(u8 *)((u32)entry + 24) = val ? 4 : 0;
	if (val)
		sta->enabled |= 1 << tid;
	else
		sta->enabled &= ~(1 << tid);
resume:
	__asm__ volatile("fence iorw, iorw" ::: "memory");
	en7523_wifi_paused[band] = 0;
	return NPU_MBOX_SUCCESS;
}

void en7523_rx_poll(u32 band)
{
	static u32 prev[2];
	u32 now = KITE_TICK;

	if (now == prev[band])
		return;
	prev[band] = now;
	ba_timeout_scan(band);
}
#endif

#if defined(EN7523) && defined(HAS_WIFI)
int en7523_rx_process(u32 id, u32 len, u32 band)
{
	u32 next = id, sum = 0, count = 0;
	struct en7523_frame f;
	struct en7523_sta *sta;
	u32 *entry;
	int managed;

	while (next < 5600 && count++ < 7) {
		sum += rx_chain[next] >> 16;
		next = en7523_rx_next(next);
	}
	if (next != 0xffff || sum != len) {
		en7523_rx_free(id);
		return -1;
	}
	if (!rx_decode(id, len, &f)) {
		sta = &stations[band * (EN7523_STA_MAX + 1) + f.node.wcid];
		if (sta->enabled && (f.fc & 0xfc) == 0x84 && f.node.tid < 8) {
			entry = (u32 *)((band ? ba_table_b : ba_table_a) +
				       28 * (8 * (f.node.wcid - 1) + f.node.tid));
			if (*(u8 *)((u32)entry + 24) == 4 &&
			    ((f.node.sn - *(u16 *)((u32)entry + 18)) & 0xfff) < 0x800) {
				ba_indicate_le_seq(entry, (f.node.sn - 1) & 0xfff);
				*(u16 *)((u32)entry + 18) = (f.node.sn - 1) & 0xfff;
			}
		}
	}
	managed = en7523_rx_managed(id, len, band);
	if (managed) {
		if (!f.fragment && !(f.fc & 0x0400) && (f.fc & 0xfc) == 0x88) {
			entry = (u32 *)((band ? ba_table_b : ba_table_a) +
				       28 * (8 * (f.node.wcid - 1) + f.node.tid));
			/* Without BA, duplicate filtering still belongs to the host. */
			if (*(u8 *)((u32)entry + 24) != 4)
				return en7523_rx_release(id, len, band, 2);
			if (en7523_rx_classify(id, len, band) < 0)
				return 0;
			return en7523_rx_release(id, len, band, 3);
		}
		/* Fragments keep host defragmentation and advance the BA window. */
		if (f.fragment || (f.fc & 0x0400)) {
			u32 last;
			int ret;

			entry = (u32 *)((band ? ba_table_b : ba_table_a) +
				       28 * (8 * (f.node.wcid - 1) + f.node.tid));
			if (*(u8 *)((u32)entry + 24) == 4) {
				last = *(u16 *)((u32)entry + 18);
				if (!((f.node.sn - last) & 0xfff) ||
				    ((f.node.sn - last) & 0x800)) {
					en7523_rx_free(id);
					return 0;
				}
				ba_indicate_le_seq(entry, (f.node.sn - 1) & 0xfff);
				*(u16 *)((u32)entry + 18) = (f.node.sn - 1) & 0xfff;
			}
			ret = en7523_rx_release(id, len, band, 6);
			if (*(u8 *)((u32)entry + 24) == 4 && !(f.fc & 0x0400)) {
				*(u16 *)((u32)entry + 18) = f.node.sn;
				last = ba_seq_scan(entry, f.node.sn);
				if (last != 0xffff)
					*(u16 *)((u32)entry + 18) = last;
			}
			return ret;
		}
		return en7523_rx_release(id, len, band, 2);
	}
	return en7523_rx_release(id, len, band, 0);
}
#endif
