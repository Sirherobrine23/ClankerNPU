/* SPDX-License-Identifier: MIT */
/*
 * EN7523 MT7915 vendor firmware: human-readable behavioral reconstruction.
 *
 * SOURCE: en7523_7915_npu_rv32.bin, load address 0x84000000.
 *
 * This file is NOT original vendor C and MUST NOT be executed or flashed.
 * It contains only recovered logic for a few selected routines, with
 * unresolved operations represented as external opaque callbacks. The
 * original binary includes extra locking, validation and queue maintenance.
 * Refer to recovered/en7523_7915.annotated.S for instruction-level proof.
 */
#include <stdbool.h>
#include <stdint.h>

#define BIT32(n)              (UINT32_C(1) << (n))
#define PPE_RETURN_INFO       UINT32_C(0x1fb50fdc)
#define PPE_RETURN_DATA       UINT32_C(0x1fb50fe0)
#define PPE_RETURN_COUNT      UINT32_C(0x1fb50fe4)
#define QDMA_TX_BASE          UINT32_C(0x1fb541c0)
#define QDMA_TX_CPU           UINT32_C(0x1fb541c8)
#define QDMA_IRQ_POP          UINT32_C(0x1fb54064)

/* These are study-only abstractions, not recovered vendor function types. */
extern uint32_t model_readl(uint32_t address);
extern void model_writel(uint32_t value, uint32_t address);
extern bool vendor_callback_840004cc(uint16_t id, uint16_t data_halfword,
                                      uint16_t foe_hash, uint8_t cpu_reason,
                                      uint32_t misc_flags);
extern void vendor_callback_8400552c(uint16_t id);
extern void model_unknown_qdma_ring_bookkeeping(uint32_t descriptor_addr,
                                                uint16_t descriptor_id,
                                                uint32_t table_entry_addr);
extern uint32_t model_allocate_bookkeeping(uint32_t bytes);
extern uint32_t model_qdma_irq_ring_get_entry(uint16_t index);
extern void model_qdma_irq_complete_entry(uint32_t value);

/*
 * Entry: 0x84003004
 * Evidence:
 *   0x8400301c: read PPE_RETURN_COUNT
 *   0x84003044: read PPE_RETURN_DATA
 *   0x84003048: test signed DATA < 0 (bit 31)
 *   0x8400304c: md.extui(DATA, 16)
 *   0x84003052: read PPE_RETURN_INFO only if bit 16 == 0
 *   0x84003072: md.maskl(INFO, 15)
 *   0x8400307e: md.bfextr16(INFO, 5)
 *   0x84003090: call 0x840004cc
 *   0x84003096/0x840030aa: completion call 0x8400552c
 *   0x840030b4: write 0x40000000 ACK
 *
 * CAUTION: The callback at 0x840004cc has more parameters than are
 * represented in this model. In particular, a4/a5/a6 are prepared by
 * the original routine and its return controls the completion callback.
 */
void model_vendor_ppe_return_poll(void)
{
    uint32_t count = model_readl(PPE_RETURN_COUNT) & UINT32_C(0xffff);
    uint32_t i;

    for (i = 0; i < count; i++) {
        uint32_t data = model_readl(PPE_RETURN_DATA);
        uint16_t id;

        if (data & BIT32(31))
            break;

        id = (uint16_t)data;
        if (!(data & BIT32(16))) {
            uint32_t info = model_readl(PPE_RETURN_INFO);
            uint8_t reason = (uint8_t)((info >> 16) & 0x1f);
            uint16_t hash = (uint16_t)(info & 0x7fff);
            uint32_t opaque_flags = info; /* Model placeholder. */

            if (vendor_callback_840004cc(id, id, hash, reason,
                                         opaque_flags))
                vendor_callback_8400552c(id);
        } else {
            vendor_callback_8400552c(id);
        }

        model_writel(BIT32(30), PPE_RETURN_DATA);
    }
}

/*
 * Entry: 0x84003180
 * Evidence:
 *   0x840031ae: read QDMA_TX_BASE
 *   0x840031ec: loop counter is initialized to 1024
 *   0x840031f0..0x84003244: zero/initialize structures and descriptors
 *   0x8400323e: bookkeeping record += 12 bytes
 *   0x84003240: hardware descriptor += 32 bytes
 *   0x84003244: loop until 1024 records are processed
 *
 * Precise descriptor initialization differs from the Clanker host-owned
 * ring contract. The model does not define correct hardware ownership.
 */
void model_vendor_qdma_ring_init(void)
{
    uint32_t desc_base = model_readl(QDMA_TX_BASE);
    uint32_t table_base = model_allocate_bookkeeping(1024u * 12u);
    uint32_t i;

    for (i = 0; i < 1024; i++) {
        uint32_t descriptor = desc_base + i * 32u;
        uint32_t record = table_base + i * 12u;

        /* The original updates descriptor word 1 bits 31 and 30. */
        uint32_t word1 = model_readl(descriptor + 4u);
        word1 |= BIT32(31);
        model_writel(word1, descriptor + 4u);
        word1 &= ~BIT32(30);
        model_writel(word1, descriptor + 4u);

        /* The original also initializes and chains 12-byte records. */
        model_unknown_qdma_ring_bookkeeping(descriptor, (uint16_t)i,
                                            record);
    }
}

/*
 * Entry: 0x840030c0
 * Evidence:
 *   0x84003114..0x84003146: descriptor fields are populated
 *   0x84003148: memory fence
 *   0x8400314e..0x8400315a: writes selected index to QDMA_TX_CPU
 *
 * This intentionally cannot model the descriptor layout, ownership
 * transfer or queue locking until the host ABI and descriptor format
 * have been reconstructed fully. Avoid treating it as production code.
 */
void model_vendor_qdma_tx_doorbell(uint16_t descriptor_id)
{
    /* Descriptor writeback and a memory fence must precede this write. */
    model_writel(descriptor_id & 0xfffu, QDMA_TX_CPU);
}

/*
 * Entry: 0x84002ecc
 * Evidence:
 *   0x84002f02..0x84002ff0: examines IRQ-ring entries in a bounded loop
 *   0x84002f30: md.maskl(entry, 12) extracts descriptor index
 *   0x84002f40..0x84002f44: tests type field against 0x00060000
 *   0x84002ff4..0x84002ffc: writes processed entry count to IRQ pop
 *
 * This is a simplified interpretation of the ring pop, not a complete
 * reconstruction of the vendor's free-list and queue callback logic.
 */
void model_vendor_qdma_irq_poll(void)
{
    uint16_t processed = 0;
    while (processed < 128) {
        uint32_t value = model_qdma_irq_ring_get_entry(processed);
        uint16_t id;

        if (value == UINT32_MAX)
            break;
        id = (uint16_t)(value & 0xfff);
        if ((value & UINT32_C(0x70000)) == UINT32_C(0x60000)) {
            if (id <= 0x3ff)
                model_qdma_irq_complete_entry(value);
        }
        processed++;
    }
    model_writel(processed, QDMA_IRQ_POP);
}
