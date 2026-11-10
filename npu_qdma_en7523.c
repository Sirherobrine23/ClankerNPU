/* EN7523 native WiFi ingress: QDMA completion and PPE buffer return. */
#include "npu_internal.h"
#include "npu_wifi.h"

#if defined(EN7523) && defined(HAS_WIFI)
#define QDMA_BASE	0x1fb54000
#define QDMA_TX_BASE	(QDMA_BASE + 0x1c0)
#define QDMA_TX_CPU	(QDMA_BASE + 0x1c8)
#define QDMA_IRQ_BASE	(QDMA_BASE + 0x48)
#define QDMA_IRQ_CFG	(QDMA_BASE + 0x4c)
#define QDMA_IRQ_POP	(QDMA_BASE + 0x64)
#define QDMA_IRQ_STATUS	(QDMA_BASE + 0x68)
#define PPE_RETURN_INFO	0x1fb50fdc
#define PPE_RETURN_DATA	0x1fb50fe0
#define PPE_RETURN_COUNT 0x1fb50fe4
#define PPE_RETURN_ACK	0x40000000u
#define PACKET_OWNED	0x80000000u
#define PACKET_TX_DONE	0x40000000u
#define PACKET_PPE_DONE	0x20000000u
#define PACKET_EXCEPTION 0x10000000u
#define PACKET_QUEUED	0x08000000u
#define PACKET_COMPLETE (PACKET_TX_DONE | PACKET_PPE_DONE)

struct en7523_ppe_packet {
	u16 offset, len, wcid, generation;
	u16 next;
	u8 band, vif;
	u32 status;
};
_Static_assert(sizeof(struct en7523_ppe_packet) == 16, "PPE ownership size");

static u32 tx_base, irq_base;
static u16 *free_ring, *tx_id;
static u16 free_read, free_write, tx_tail;
static volatile u32 qdma_ready;
static struct en7523_ppe_packet *packets;
static u16 return_head[2], return_tail[2];
static u32 pending[2];
static u32 ppe_returned, ppe_exceptions, ppe_invalid, ppe_host_drops;
static u32 ppe_last_data, ppe_last_info;
u32 ppe_cpu_reasons[32];
static u32 ppe_logged_reasons[32];

static const char *const en7523_rx_fallback_name[] = {
	[EN7523_RX_FALLBACK_DECODE] = "decode",
	[EN7523_RX_FALLBACK_BAD] = "bad-rxd",
	[EN7523_RX_FALLBACK_CRYPTO] = "protected",
	[EN7523_RX_FALLBACK_NO_SESSION] = "no-session",
	[EN7523_RX_FALLBACK_NOT_REORDERED] = "no-ba-reorder",
	[EN7523_RX_FALLBACK_DIRECT_DISABLED] = "direct-disabled",
	[EN7523_RX_FALLBACK_CHAINED] = "chained",
	[EN7523_RX_FALLBACK_FRAME_FORMAT] = "frame-format",
	[EN7523_RX_FALLBACK_ETH_FORMAT] = "ethernet",
	[EN7523_RX_FALLBACK_IPV4_FORMAT] = "ipv4",
	[EN7523_RX_FALLBACK_L4_PROTO] = "l4-proto",
	[EN7523_RX_FALLBACK_SOURCE] = "source-mac",
	[EN7523_RX_FALLBACK_TRANSLATION] = "translation",
	[EN7523_RX_FALLBACK_QDMA] = "qdma-admission",
};

static u32 en7523_rx_fallback_logged[2][EN7523_RX_FALLBACK_MAX];

static const char *ppe_reason_name(u32 reason)
{
	switch (reason) {
	case 0x0c:
		return "TCP_FIN_SYN_RST";
	case 0x0d:
		return "UN_HIT";
	case 0x0e:
		return "HIT_UNBIND";
	case 0x0f:
		return "HIT_UNBIND_RATE_REACH";
	case 0x15:
		return "KEEPALIVE_DUP_OLD_HDR";
	default:
		return "OTHER";
	}
}
extern u32 en7523_qdma_mutex[2];
extern u32 en7523_log_tick, en7523_log_stats[2][4];
extern volatile u32 en7523_qdma_done, en7523_qdma_wb_wait;
extern volatile u32 en7523_wifi_enabled[2], en7523_wifi_direct[2];
extern u32 en7523_wifi_stats[2][4];
#define qdma_mutex en7523_qdma_mutex

static void qdma_fence(void)
{
	__asm__ volatile("fence iorw, iorw" ::: "memory");
}

int en7523_qdma_init(void)
{
	u32 i, base = REG32(QDMA_TX_BASE), irq = REG32(QDMA_IRQ_BASE);
	u32 state, packet_state;

	if (qdma_ready)
		return NPU_MBOX_SUCCESS;
	/* A reset retires outstanding buffers until the next firmware boot. */
	if (tx_base)
		return NPU_MBOX_ENODEV;
	if ((base & 31) || base < 0x80000000 || base > 0xbfff8000 ||
	    (irq & 3) || irq < 0x80000000 || irq > 0xbffff000 ||
	    (REG32(QDMA_IRQ_CFG) & 0xfff) != EN7523_QDMA_COUNT ||
	    REG32(QDMA_TX_CPU) != REG32(QDMA_TX_CPU + 4))
		return NPU_MBOX_ENODEV;

	state = sram_buf_alloc(43);
	packet_state = sram_buf_alloc(46);
	if (!state || state >= 0x1e860000 || !packet_state ||
	    packet_state > 0x1e860000 - 5600 * sizeof(*packets))
		return NPU_MBOX_ERROR;
	packets = (struct en7523_ppe_packet *)packet_state;
	for (i = 0; i < 5600; i++)
		packets[i].status = 0;
	for (i = 0; i < 2; i++) {
		return_head[i] = return_tail[i] = 0xffff;
		pending[i] = 0;
	}
	free_ring = (u16 *)state;
	tx_id = free_ring + EN7523_QDMA_COUNT;
	for (i = 0; i < EN7523_QDMA_COUNT; i++) {
		free_ring[i] = i;
		tx_id[i] = 0xffff;
	}
	tx_base = base;
	irq_base = irq;
	tx_tail = REG32(QDMA_TX_CPU) & 0x3ff;
	free_ring[0] = tx_tail;
	free_ring[tx_tail] = 0;
	free_read = 1;
	free_write = 0;
	REG32(QDMA_BASE + 0x1c4) |= 0x10;
	qdma_fence();
	qdma_ready = 1;
	return NPU_MBOX_SUCCESS;
}

/* Caller holds qdma_mutex. Descriptor and packet lifetimes are independent. */
static void packet_complete(u32 id)
{
	struct en7523_ppe_packet *p = &packets[id];

	if ((p->status & PACKET_COMPLETE) != PACKET_COMPLETE ||
	    p->status & PACKET_QUEUED)
		return;
	if (p->status & PACKET_EXCEPTION) {
		p->next = 0xffff;
		if (return_head[p->band] == 0xffff)
			return_head[p->band] = id;
		else
			packets[return_tail[p->band]].next = id;
		return_tail[p->band] = id;
		p->status |= PACKET_QUEUED;
	} else {
		pending[p->band]--;
		p->status = 0;
		buf_mgr_free(id);
	}
}

int en7523_qdma_submit(u32 id, u32 addr, u32 len, u32 band,
		       u32 wcid, u32 vif, u32 generation)
{
	volatile u32 *desc;
	struct en7523_ppe_packet *p;
	u32 base = wifi_pkt_buf_addr + (id << 12);
	u16 next, tail;
#ifdef EN7523_RX_PROFILE
	u32 stamp;
#endif

	/* The first TC flow setup publishes the PPE configuration over mailbox 5. */
	if (!hwnat_ready || !qdma_ready || REG32(QDMA_TX_BASE) != tx_base ||
	    REG32(QDMA_IRQ_BASE) != irq_base || !(REG32(QDMA_BASE + 4) & 1) ||
	    id >= 5600 || len < 14 || len > 3500 || band > 1 || vif >= 16 ||
	    addr < base || addr - base > 255 || addr - base + len > 4096)
		return -1;
#ifdef EN7523_RX_PROFILE
	stamp = en7523_rx_prof_begin(band);
#endif
	hw_mutex_lock(qdma_mutex);
#ifdef EN7523_RX_PROFILE
	en7523_rx_prof_end(band, EN7523_PROF_QDMA_LOCK, stamp);
#endif
	if (REG32(QDMA_TX_CPU) != tx_tail) {
		qdma_ready = 0;
		hw_mutex_unlock(qdma_mutex);
		return -1;
	}
	p = &packets[id];
	if (free_read == free_write || p->status & PACKET_OWNED) {
		hw_mutex_unlock(qdma_mutex);
		return -1;
	}
#ifdef EN7523_RX_PROFILE
	stamp = en7523_rx_prof_begin(band);
#endif
	tail = tx_tail;
	next = free_ring[free_read];
	free_read = (free_read + 1) & (EN7523_QDMA_COUNT - 1);
	p->offset = addr - base;
	p->len = len;
	p->band = band;
	p->vif = vif;
	p->wcid = wcid;
	p->generation = generation;
	p->status = PACKET_OWNED;
	pending[band]++;
	/* Keep the original Ethernet frame, without a private Linux VLAN tag. */
	while (len < 60)
		*(u8 *)(addr + len++) = 0;
	desc = (volatile u32 *)(tx_base + tail * 32);
	tx_id[tail] = id;
	desc[2] = addr;
	desc[3] = next;
	desc[4] = 0x80000000u | id << 14;
	desc[5] = 0x7f4087ff;
	desc[6] = 0;
	desc[7] = 0;
	qdma_fence();
	desc[1] = len;
	qdma_fence();
	tx_tail = next;
#ifdef EN7523_RX_PROFILE
	en7523_rx_prof_end(band, EN7523_PROF_QDMA_DESC, stamp);
	stamp = en7523_rx_prof_begin(band);
#endif
	REG32(QDMA_TX_CPU) = next;
#ifdef EN7523_RX_PROFILE
	en7523_rx_prof_end(band, EN7523_PROF_QDMA_KICK, stamp);
#endif
	hw_mutex_unlock(qdma_mutex);
	return 0;
}

static void ppe_return_poll(void)
{
	u32 n = REG32(PPE_RETURN_COUNT) & 0xffff;
	u32 data, metadata, id, reason;
	struct en7523_ppe_packet *p;

	if (n > 64)
		n = 64;
	while (n--) {
		data = REG32(PPE_RETURN_DATA);
		if (data & 0x80000000u)
			break;
		/* INFO belongs to the current FIFO head; ACK advances both words. */
		metadata = (data & (1 << 16)) ? 0 : REG32(PPE_RETURN_INFO);
		ppe_last_data = data;
		ppe_last_info = metadata;
		id = data & 0xffff;
		hw_mutex_lock(qdma_mutex);
		if (id < 5600 && packets[id].status & PACKET_OWNED &&
		    !(packets[id].status & PACKET_PPE_DONE)) {
			p = &packets[id];
			p->status |= PACKET_PPE_DONE;
			if (!(data & (1 << 16))) {
				reason = (metadata >> 16) & 0x1f;
				ppe_cpu_reasons[reason]++;
				/* Duplicate old-header keepalives have already been forwarded. */
				if (reason != 0x15) {
					p->status |= PACKET_EXCEPTION | (metadata & 0x1fffff);
					ppe_exceptions++;
				}
			}
			ppe_returned++;
			packet_complete(id);
		} else {
			ppe_invalid++;
		}
		hw_mutex_unlock(qdma_mutex);
		qdma_fence();
		REG32(PPE_RETURN_DATA) = PPE_RETURN_ACK;
	}
}

void en7523_qdma_poll(void)
{
	volatile u32 *entry;
	u32 status, head, count, done = 0, val, idx, id;

	if (!qdma_ready)
		return;
	if (REG32(QDMA_TX_BASE) != tx_base || REG32(QDMA_IRQ_BASE) != irq_base) {
		qdma_ready = 0;
		return;
	}
	status = REG32(QDMA_IRQ_STATUS);
	head = status & 0x3ff;
	count = (status >> 16) & 0xfff;
	if (count)
		REG32(QDMA_BASE + 0x20) = 0x10;
	while (count-- && done < 64) {
		entry = (volatile u32 *)(irq_base + head * 4);
		val = *entry;
		if (val == 0xffffffff)
			break;
		qdma_fence();
		idx = val & 0xfff;
		if (((val >> 16) & 0x1f) == 6 && idx < EN7523_QDMA_COUNT) {
			if (!(REG32(tx_base + idx * 32 + 4) & 0xc0000000)) {
				en7523_qdma_wb_wait++;
				break;
			}
			hw_mutex_lock(qdma_mutex);
			id = tx_id[idx];
			if (id < 5600) {
				packets[id].status |= PACKET_TX_DONE;
				tx_id[idx] = 0xffff;
				en7523_qdma_done++;
				free_ring[free_write] = idx;
				free_write = (free_write + 1) & 0x3ff;
				packet_complete(id);
			}
			hw_mutex_unlock(qdma_mutex);
		}
		*entry = 0xffffffff;
		head = (head + 1) & 0x3ff;
		done++;
	}
	if (done) {
		qdma_fence();
		REG32(QDMA_IRQ_POP) = done;
	}
	ppe_return_poll();
}

u32 en7523_ppe_pending(u32 band)
{
	return *(volatile u32 *)&pending[band];
}

/* The radio hart alone owns its host ring and reserved GDMA copy channel. */
void en7523_ppe_rx_poll(u32 band)
{
	struct en7523_ppe_packet saved;
	u32 id, i;

	if (!qdma_ready || *(volatile u16 *)&return_head[band] == 0xffff)
		return;
	for (i = 0; i < 32; i++) {
		hw_mutex_lock(qdma_mutex);
		id = return_head[band];
		if (id == 0xffff) {
			hw_mutex_unlock(qdma_mutex);
			break;
		}
		saved = packets[id];
		return_head[band] = saved.next;
		if (saved.next == 0xffff)
			return_tail[band] = 0xffff;
		hw_mutex_unlock(qdma_mutex);
		if (en7523_rx_exception(id, saved.offset, saved.len, band,
					 saved.wcid, saved.generation, saved.vif,
					 saved.status))
			ppe_host_drops++;
		hw_mutex_lock(qdma_mutex);
		packets[id].status = 0;
		pending[band]--;
		buf_mgr_free(id);
		hw_mutex_unlock(qdma_mutex);
	}
}

#ifdef EN7523_RX_PROFILE
static const char *const en7523_rx_prof_names[] = {
	[EN7523_PROF_RX_DESC] = "rx-desc",
	[EN7523_PROF_BUF_ALLOC] = "buf-alloc",
	[EN7523_PROF_RX_PROCESS] = "rx-process",
	[EN7523_PROF_RX_FIRST_DECODE] = "first-decode",
	[EN7523_PROF_RX_MANAGED] = "rx-managed",
	[EN7523_PROF_BA_CLASSIFY] = "ba-classify",
	[EN7523_PROF_BA_FORWARD] = "ba-forward",
	[EN7523_PROF_RX_RELEASE] = "rx-release",
	[EN7523_PROF_RX_DECODE] = "decode",
	[EN7523_PROF_QDMA_SUBMIT] = "qdma-submit",
	[EN7523_PROF_QDMA_LOCK] = "qdma-lock",
	[EN7523_PROF_QDMA_DESC] = "qdma-descriptor",
	[EN7523_PROF_QDMA_KICK] = "qdma-kick",
	[EN7523_PROF_PCIE_KICK] = "pcie-kick",
	[EN7523_PROF_BA_SCAN] = "ba-scan",
	[EN7523_PROF_HOST_FLUSH] = "host-flush",
};

static void en7523_rx_prof_dump(void)
{
	static u32 prev_samples[2][EN7523_PROF_STAGE_MAX];
	static u32 prev_cycles[2][EN7523_PROF_STAGE_MAX];
	static u32 prev_desc[2], prev_frames[2], prev_kicks[2];
	static u32 prev_cache_hits[2];
	u32 band, stage;

	for (band = 0; band < 2; band++) {
		u32 desc = en7523_rx_prof_desc[band];
		u32 frames = en7523_rx_prof_frames[band];
		u32 kicks = en7523_rx_prof_kicks[band];
		u32 cache_hits = en7523_rx_cache_hits[band];
		u32 dh = cache_hits - prev_cache_hits[band];
		u32 dd = desc - prev_desc[band];
		u32 df = frames - prev_frames[band];
		u32 dk = kicks - prev_kicks[band];

		prev_desc[band] = desc;
		prev_frames[band] = frames;
		prev_kicks[band] = kicks;
		prev_cache_hits[band] = cache_hits;
		if (!dd)
			continue;
		npu_printf("[RX PROF] band=%u desc=%u frames=%u kicks=%u batch_x10=%u cache_hits=%u\n",
			   band, dd, df, dk,
			   dk ? dd * 10 / dk : 0, dh);
		for (stage = 0; stage < EN7523_PROF_STAGE_MAX; stage++) {
			volatile struct en7523_rx_prof_stage_stats *s =
				&en7523_rx_prof[band][stage];
			u32 count = s->samples;
			u32 cycles = s->cycles;
			u32 n = count - prev_samples[band][stage];
			u32 delta = cycles - prev_cycles[band][stage];

			prev_samples[band][stage] = count;
			prev_cycles[band][stage] = cycles;
			if (n)
				npu_printf("[RX PROF] band=%u stage=%s n=%u avg=%u max=%u\n",
					   band, en7523_rx_prof_names[stage], n,
					   delta / n, s->max_cycles);
		}
	}
}
#endif

/* Hart 0 prints outside the RX, completion and interrupt paths. */
void en7523_wifi_debug_poll(void)
{
	u32 now = KITE_TICK, band, i, stats[4], reasons[32];
	u32 flags, free = 0, tail = 0, status;

	if (now - en7523_log_tick < 500 || !NDBG_PRINTING(NDBG_RX_LOG) ||
	    REG32(NPU_MIB(21)))
		return;
	en7523_log_tick = now;
	for (band = 0; band < 2; band++) {
		for (i = 0; i < 4; i++)
			stats[i] = *(volatile u32 *)&en7523_wifi_stats[band][i];
		npu_printf("[RX%u] on=%u direct=%u ppe=%u(+%u) host=%u(+%u) pn_drop=%u full=%u(+%u)\n",
			   band, en7523_wifi_enabled[band], en7523_wifi_direct[band],
			   stats[0], stats[0] - en7523_log_stats[band][0],
			   stats[1], stats[1] - en7523_log_stats[band][1],
			   stats[2], stats[3], stats[3] - en7523_log_stats[band][3]);
		for (i = 0; i < 4; i++)
			en7523_log_stats[band][i] = stats[i];
	}
	flags = irq_save();
	hw_mutex_lock(qdma_mutex);
	if (qdma_ready) {
		free = (free_write - free_read) & (EN7523_QDMA_COUNT - 1);
		tail = tx_tail;
	}
	for (i = 0; i < 32; i++)
		reasons[i] = ppe_cpu_reasons[i];
	hw_mutex_unlock(qdma_mutex);
	irq_restore(flags);
	status = REG32(QDMA_IRQ_STATUS);
	npu_printf("[QDMA] ready=%u free=%u tail=%u dma=%u irq=%u/%u done=%u wb_wait=%u ppe_ret=%u mails=%u last=%x\n",
		   qdma_ready, free, tail, REG32(QDMA_TX_CPU + 4) & 0x3ff,
		   status & 0x3ff, (status >> 16) & 0xfff,
		   en7523_qdma_done, en7523_qdma_wb_wait,
		   REG32(0x1fb50fe4) & 0xffff,
		   ndbg->cnt[NC_PPE_MAILS], ndbg->cnt[NC_PPE_LAST]);
	npu_printf("[PPE] returned=%u exceptions=%u pending=%u/%u invalid=%u host_drop=%u last=%x/%x\n",
		   ppe_returned, ppe_exceptions, pending[0], pending[1],
		   ppe_invalid, ppe_host_drops, ppe_last_data, ppe_last_info);
	for (band = 0; band < 2; band++) {
		for (i = 0; i < EN7523_RX_FALLBACK_MAX; i++) {
			u32 total = en7523_rx_fallback[band][i];
			u32 delta = total - en7523_rx_fallback_logged[band][i];

			if (!delta)
				continue;
			npu_printf("[RX FALLBACK] band=%u cause=%s total=%u delta=%u\n",
				   band, en7523_rx_fallback_name[i], total, delta);
			en7523_rx_fallback_logged[band][i] = total;
		}
	}
#ifdef EN7523_RX_PROFILE
	en7523_rx_prof_dump();
#endif
	for (i = 0; i < 32; i++) {
		u32 delta = reasons[i] - ppe_logged_reasons[i];

		if (!delta)
			continue;
		npu_printf("[PPE REASON] code=%02x name=%s total=%u delta=%u\n",
			   i, ppe_reason_name(i), reasons[i], delta);
		ppe_logged_reasons[i] = reasons[i];
	}
}

void en7523_qdma_loop(void)
{
	npu_dbg_loop(NDBG_TAG('Q', 'D', 'M', 'A'));
	for (;;) {
		npu_dbg_poll();
		en7523_qdma_poll();
	}
}
#endif
