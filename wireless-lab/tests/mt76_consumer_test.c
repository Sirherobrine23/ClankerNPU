/* SPDX-License-Identifier: GPL-2.0-only
 * Consumer function copied from airoha/mt76 mt7915/npu.c at
 * 983ef6983102461f6daf6995a7819eed8c6f8496, with native skb/NAPI shims.
 * This verifies the firmware's fragment convention against the driver,
 * independently of the firmware producer implementation. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint32_t u32;
#define FIELD_GET(mask,val) (((val)&(mask))>>__builtin_ctz(mask))
#define MT7915_NPU_RX_LEN 0x1fff8000u
#define MT7915_NPU_RX_CUR_LEN 0x00007ffeu
#define MT7915_NPU_RX_PART 0x1c000000u
#define MT7915_NPU_RX_PARTS 0xe0000000u
#define MT7915_NPU_RX_LAST (1u<<29)
#define MT7915_NPU_BUF_SIZE 4096u
struct sk_buff { u32 len,capacity; unsigned char data[4096]; };
struct mt7915_npu_queue { int napi; struct sk_buff *partial; u32 total,part; };
static struct sk_buff *napi_alloc_skb(int *napi,u32 len)
{(void)napi;assert(len<=4096);struct sk_buff *s=calloc(1,sizeof(*s));assert(s);s->capacity=len;return s;}
static void skb_put_data(struct sk_buff *s,const void *p,u32 n)
{assert(n<=s->capacity-s->len);memcpy(s->data+s->len,p,n);s->len+=n;}
static u32 skb_tailroom(struct sk_buff *s){return s->capacity-s->len;}
static void mt7915_npu_drop_partial(struct mt7915_npu_queue *q)
{free(q->partial);q->partial=NULL;q->part=q->total=0;}
static struct sk_buff *mt7915_npu_rx(struct mt7915_npu_queue *q, u32 ctrl,
				      u32 info, void *data)
{
	unsigned int len = FIELD_GET(MT7915_NPU_RX_LEN, ctrl);
	unsigned int cur = FIELD_GET(MT7915_NPU_RX_CUR_LEN, ctrl);
	unsigned int part = FIELD_GET(MT7915_NPU_RX_PART, info);
	unsigned int parts = FIELD_GET(MT7915_NPU_RX_PARTS, info);
	struct sk_buff *skb;

	if (!cur || cur > MT7915_NPU_BUF_SIZE || cur > len)
		goto invalid;

	if (cur == len) {
		skb = napi_alloc_skb(&q->napi, len);
		if (skb)
			skb_put_data(skb, data, len);
		return skb;
	}

	if (part == 1) {
		mt7915_npu_drop_partial(q);
		q->partial = napi_alloc_skb(&q->napi, len);
		q->total = len;
	}

	if (!q->partial || part != q->part + 1U || q->total != len ||
	    cur > len - q->partial->len || cur > skb_tailroom(q->partial))
		goto invalid;

	skb_put_data(q->partial, data, cur);
	q->part = part;
	if (!(ctrl & MT7915_NPU_RX_LAST))
		return NULL;

	if (q->partial->len != len || part != parts)
		goto invalid;

	skb = q->partial;
	q->partial = NULL;
	q->part = 0;
	q->total = 0;
	return skb;

invalid:
	mt7915_npu_drop_partial(q);
	return NULL;
}

static u32 ctrl(u32 total,u32 current,int last)
{return 1|(current<<1)|(total<<15)|(last?MT7915_NPU_RX_LAST:0);}
int main(void)
{
 struct mt7915_npu_queue q={0};struct sk_buff *skb;
 unsigned char frame[3500];for(unsigned i=0;i<sizeof(frame);i++)frame[i]=(unsigned char)i;
 /* v21 used first part=0: the actual driver rejects that start. */
 skb=mt7915_npu_rx(&q,ctrl(3500,1600,0),3u<<29,frame);
 assert(!skb && !q.partial);
 unsigned offset=0;
 for(unsigned i=0;i<3;i++) {
  unsigned bytes=i<2?1600:300;
  skb=mt7915_npu_rx(&q,ctrl(3500,bytes,i==2),(3u<<29)|((i+1)<<26),frame+offset);
  if(i<2)assert(!skb && q.partial);
  else {assert(skb && skb->len==3500);assert(!memcmp(skb->data,frame,3500));free(skb);}
  offset+=bytes;
 }
 assert(!q.partial);
 skb=mt7915_npu_rx(&q,ctrl(1500,1500,1),0,frame);
 assert(skb && skb->len==1500 && !memcmp(skb->data,frame,1500));free(skb);
 puts("mt76 consumer: old zero-based fragments rejected; v22 1-based fragments reassemble; whole frames preserved");
 return 0;
}
