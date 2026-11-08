#ifndef NPU_WIFI_EN7523_H
#define NPU_WIFI_EN7523_H

#define EN7523_RX_BYPASS_BA	0x20000000u
#define EN7523_RX_REORDERED	0x80000000u
#define EN7523_RX_PN_VALID	0x40000000u
#define EN7523_RX_PPE		0x10000000u
#define EN7523_STA_MAX		150
#define EN7523_QDMA_COUNT	1024

struct kite_node {
	u32 next, msdu_head, msdu_tail;
	u16 msdu_cnt, pad0;
	u8 hdr, tid, wcid, bss;
	u16 buf_id, idx, sn, len;
	u8 amsdu, pool;
	u16 pad1;
	u32 tick;
};

#ifdef EN7523
int en7523_qdma_init(void);
int en7523_qdma_submit(u32 id, u32 addr, u32 len, u32 band,
		       u32 wcid, u32 vif, u32 generation);
void en7523_qdma_poll(void);
void en7523_ppe_rx_poll(u32 band);
u32 en7523_ppe_pending(u32 band);
int en7523_rx_exception(u32 id, u32 offset, u32 len, u32 band,
			u32 wcid, u32 generation, u32 vif, u32 metadata);
void en7523_qdma_loop(void);
void en7523_wifi_debug_poll(void);
int en7523_rx_parse(u32 id, struct kite_node *node, u32 len, u32 band);
int en7523_rx_release(u32 id, u32 len, u32 band, int reordered);
s32 en7523_rx_classify(s32 id, u32 len, u32 band);
void en7523_rx_state_init(void);
void en7523_rx_free(u32 id);
void en7523_rx_link(u32 id, u32 len, u32 next);
u32 en7523_rx_next(u32 id);
int en7523_rx_process(u32 id, u32 len, u32 band);
int en7523_rx_control(u32 *msg);
int en7523_rx_managed(u32 id, u32 len, u32 band);
void en7523_rx_poll(u32 band);
void en7523_wifi_discard(u32 band);
#endif
#endif
