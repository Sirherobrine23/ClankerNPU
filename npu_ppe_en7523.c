/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * EN7523 NPU v1: PPE mailbox slot 5.
 *
 * The ARM airoha_ppe driver owns FE/PPE initialization and the DRAM FoE
 * table.  The NPU handles mailbox synchronization and the PPE SRAM window;
 * never call the AN758x tunnel/HWNAT register setup on this SoC.
 *
 * Host ABI (struct ppe_mbox_data in airoha_npu.c):
 *   +0  u32 func_type = NPU_OP_SET (1)
 *   +4  u32 func_id   = 1 init, 2 deinit, 3 SRAM API, 4 stats
 *   +8  payload union
 */
#include "npu_internal.h"

#ifdef EN7523

#define EN7523_PPE_FLOW_CFG       0x1FB50E04u
#define EN7523_PPE_SRAM_CTRL      0x1FB50F1Cu
#define EN7523_PPE_SRAM_DATA      0x1FB50F20u
#define EN7523_PPE_FLOW_PROFILE   0x06BBF7C0u
#define EN7523_PPE_SRAM_ENTRIES   512u
#define EN7523_PPE_ENTRY_SIZE     80u
#define EN7523_PPE_SRAM_REQ       3u
#define EN7523_PPE_SRAM_ACK       (1u << 31)
#define EN7523_PPE_SRAM_POLLS     10000u
#define EN7523_MBOX_MAX_LEN      256u

/* Matches the host's PPE_FUNC_SET_WAIT_* and PPE_*_SRAM enums. */
#define PPE_CMD_INIT             1u
#define PPE_CMD_DEINIT           2u
#define PPE_CMD_API              3u
#define PPE_CMD_STATS_SETUP      4u
#define PPE_CMD_SRAM_PPE2        0u
#define PPE_CMD_SRAM_STAGE       1u
#define PPE_CMD_SRAM_COMMIT      2u
#define PPE_CMD_SRAM_CLEAR       3u

static u32 en7523_ppe_ready;
static u32 en7523_ppe_staged;

/* EN7523 NPU v1 can dereference physical host DRAM addresses directly. */
static int en7523_ppe_host_buffer(u32 base, u32 cnt)
{
	return base >= 0x80000000u && base < 0xC0000000u &&
	       !(base & 3u) && cnt >= 8u && cnt <= EN7523_MBOX_MAX_LEN &&
	       cnt <= 0xC0000000u - base;
}

static int en7523_ppe_sram_command(u32 hash)
{
	u32 i;

	if (hash >= EN7523_PPE_SRAM_ENTRIES)
		return NPU_MBOX_EINVAL;

	__asm__ volatile("fence iorw, iorw" ::: "memory");
	REG32(EN7523_PPE_SRAM_CTRL) =
		(hash << 8) | EN7523_PPE_SRAM_REQ;

	for (i = 0; i < EN7523_PPE_SRAM_POLLS; i++) {
		if (REG32(EN7523_PPE_SRAM_CTRL) & EN7523_PPE_SRAM_ACK)
			return NPU_MBOX_SUCCESS;
	}
	return NPU_MBOX_ERROR;
}

static int en7523_ppe_sram_stage(u32 src, u32 size)
{
	u32 i;

	en7523_ppe_staged = 0;
	if (size != EN7523_PPE_ENTRY_SIZE ||
	    !en7523_ppe_host_buffer(src, size))
		return NPU_MBOX_EINVAL;

	/* Host publishes all 80 bytes before the mailbox doorbell. */
	__asm__ volatile("fence iorw, iorw" ::: "memory");
	for (i = 0; i < EN7523_PPE_ENTRY_SIZE; i += sizeof(u32))
		REG32(EN7523_PPE_SRAM_DATA + i) = REG32(src + i);
	__asm__ volatile("fence iorw, iorw" ::: "memory");
	en7523_ppe_staged = 1;
	return NPU_MBOX_SUCCESS;
}

static int en7523_ppe_sram_clear(u32 entries)
{
	u32 i, word;
	int ret;

	if (entries != EN7523_PPE_SRAM_ENTRIES)
		return NPU_MBOX_EINVAL;

	en7523_ppe_staged = 0;
	for (i = 0; i < entries; i++) {
		for (word = 0; word < EN7523_PPE_ENTRY_SIZE; word += sizeof(u32))
			REG32(EN7523_PPE_SRAM_DATA + word) = 0;
		ret = en7523_ppe_sram_command(i);
		if (ret != NPU_MBOX_SUCCESS)
			return ret;
	}
	return NPU_MBOX_SUCCESS;
}

static int en7523_ppe_api(const volatile u32 *msg, u32 cnt)
{
	u32 cmd, size, data;
	int ret;

	if (cnt < 5u * sizeof(u32))
		return NPU_MBOX_EINVAL;
	if (!en7523_ppe_ready)
		return NPU_MBOX_ENODEV;

	cmd = msg[2];
	size = msg[3];
	data = msg[4];

	switch (cmd) {
	case PPE_CMD_SRAM_STAGE:
		return en7523_ppe_sram_stage(data, size);
	case PPE_CMD_SRAM_COMMIT:
		if (size != sizeof(u32) || !en7523_ppe_staged)
			return NPU_MBOX_EINVAL;
		/* Consume the staged window, including on a hardware timeout. */
		en7523_ppe_staged = 0;
		ret = en7523_ppe_sram_command(data);
		return ret;
	case PPE_CMD_SRAM_CLEAR:
		return en7523_ppe_sram_clear(size);
	case PPE_CMD_SRAM_PPE2:
		/* EN7523 has a single PPE. */
		return NPU_MBOX_EOPNOTSUPP;
	default:
		return NPU_MBOX_EOPNOTSUPP;
	}
}

int hwnat_mail_dispatch(u32 base, u32 cnt)
{
	const volatile u32 *msg = (const volatile u32 *)base;
	u32 func_id = 0;
	int ret;

	if (!en7523_ppe_host_buffer(base, cnt))
		return NPU_MBOX_EINVAL;
	if (msg[0] != NPU_OP_SET)
		return NPU_MBOX_EOPNOTSUPP;

	func_id = msg[1];
	switch (func_id) {
	case PPE_CMD_INIT:
		/* Full PPE register setup is owned by the ARM driver.  This
		 * known-good EN7523 flow profile is the nonzero readiness value
		 * that airoha_ppe_wait_for_npu_init() checks before it runs
		 * airoha_ppe_hw_init().  Do not enable GLO_CFG here. */
		if (cnt < 28u)
			ret = NPU_MBOX_EINVAL;
		else if (msg[4] != 3u || msg[5] != 1u)
			ret = NPU_MBOX_EOPNOTSUPP;
		else {
			en7523_ppe_staged = 0;
			if (!REG32(EN7523_PPE_FLOW_CFG))
				REG32(EN7523_PPE_FLOW_CFG) = EN7523_PPE_FLOW_PROFILE;
			__asm__ volatile("fence iorw, iorw" ::: "memory");
			en7523_ppe_ready = 1;
			ret = NPU_MBOX_SUCCESS;
		}
		break;
	case PPE_CMD_DEINIT:
		en7523_ppe_ready = 0;
		en7523_ppe_staged = 0;
		/* Linux tears down the FE.  Do not reset shared registers. */
		ret = NPU_MBOX_SUCCESS;
		break;
	case PPE_CMD_API:
		ret = en7523_ppe_api(msg, cnt);
		break;
	case PPE_CMD_STATS_SETUP:
		/* EN7523 has ppe_stats_entries=0; never invent an NPU
		 * statistics buffer address for a host devm_ioremap(). */
		ret = NPU_MBOX_EOPNOTSUPP;
		break;
	default:
		ret = NPU_MBOX_EOPNOTSUPP;
		break;
	}

	NDBG_CNT(NC_PPE_MAILS);
	NDBG_SET(NC_PPE_LAST, func_id << 8 | (ret & 0xff));
	NDBG_TRACE(NDBG_PPE, func_id, ret, cnt);
	return ret;
}

#endif /* EN7523 */
