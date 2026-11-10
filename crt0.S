/*
 * AN75XX NPU firmware startup
 *
 * Reset vector at 0x84000000. Sets GP, clears BSS (cold boot only),
 * assigns per-hart stack, calls npu_init() then core_dispatch().
 *
 * Hart count: AN7552=2, AN7583=6, AN7581=8.
 * Unmatched harts fall through to the hart-0 path.
 */

#include "npu_config.h"

#define NPU_MIB0   0x1EC0C140

    .section .text.init, "ax", @progbits
    .globl _start
    .type  _start, @function
_start:
    /* Vendor reset vector is eight 32-bit NOPs (0x20 bytes). */
    .rept 8
    .4byte 0x00000013
    .endr

#ifdef EN7523
    /* EN7523 vendor entry 0x84000020..0x84000104 (all 7 images).
     * Private CSR address translators / cache regions are required BEFORE
     * any load from the 0x1e8/0x1e9 local SRAM windows. Preserve the
     * vendor constants; cache op 0x0005400f is not in base RISC-V ISA.
     * See docs/en7523-vendor.md for the extracted instruction sequence.
     */
    li      a0, 0x103
    csrc    0x7c0, a0
    li      a0, 3
    csrs    0x7c0, a0
    li      a0, 0x18
    .4byte  0x0005400f
    li      a0, 8
    .4byte  0x0005400f
    li      a0, 0x360
    csrw    0x9e0, a0
    li      a0, 0x400
    csrw    0x9f0, a0
    li      a0, 0x1c0000e0
    csrw    0x9e1, a0
    li      a0, 0x1e800000
    csrw    0x9f1, a0
    li      a0, 0x1e8003e0
    csrw    0x9e2, a0
    li      a0, 0x1e860000
    csrw    0x9f2, a0
    li      a0, 0x1e8600e0
    csrw    0x9e3, a0
    li      a0, 0x20000000
    csrw    0x9f3, a0
    li      a0, 0x200000e0
    csrw    0x9e4, a0
    li      a0, 0x30000000
    csrw    0x9f4, a0
    li      a0, 0x300000c0
    csrw    0x9e5, a0
    li      a0, 0x30004000
    csrw    0x9f5, a0
    li      a0, 0x30000000
    csrw    0x9dd, a0
    li      a0, 0x30004000
    csrw    0x9df, a0
    li      a0, 0x800003e0
    csrw    0x9e6, a0
    li      a0, 0xc0000000
    csrw    0x9f6, a0
    li      a0, 0x7f
    csrw    0x9c0, a0
    li      a0, 0x103
    csrs    0x7c0, a0
    fence.i
    li      a0, 0x103
    csrc    0x7c0, a0
    fence.i
    li      a0, 0x139
    csrs    0x7c0, a0
    fence.i
#else
    fence.i
    fence
#endif

    .option push
    .option norelax
    la      gp, __global_pointer$
    .option pop

    csrrci  t0, mstatus, 8

    /* ---- hart dispatch ---- */
    csrr    t0, mhartid
    li      t1, 0
    beq     t0, t1, .L_hart0

    csrr    t0, mhartid
    li      t1, 1
    beq     t0, t1, .L_hart1

#if MAX_CORE_NUM > 2
    csrr    t0, mhartid
    li      t1, 2
    beq     t0, t1, .L_hart2

    csrr    t0, mhartid
    li      t1, 3
    beq     t0, t1, .L_hart3

    csrr    t0, mhartid
    li      t1, 4
    beq     t0, t1, .L_hart4

    csrr    t0, mhartid
    li      t1, 5
    beq     t0, t1, .L_hart5
#endif

#if MAX_CORE_NUM > 6
    csrr    t0, mhartid
    li      t1, 6
    beq     t0, t1, .L_hart6

    csrr    t0, mhartid
    li      t1, 7
    beq     t0, t1, .L_hart7
#endif

    /* ---- hart 0 path (and fall-through for unmatched harts) ---- */
.L_hart0:
    lui     t0, %hi(NPU_MIB0)
    addi    t0, t0, %lo(NPU_MIB0)
    lw      t1, 0(t0)
    .option push
    .option norvc
    li      t2, -1
    .option pop
    beq     t1, t2, .L_skip_bss

    la      t0, __bss_start
    la      t1, __bss_end
    addi    t1, t1, 4
.L_bss_loop:
    sw      zero, 0(t0)
    addi    t0, t0, 4
    bne     t0, t1, .L_bss_loop

.L_skip_bss:
    la      sp, __stack0
    j       .L_common

.L_hart1:
    la      sp, __stack1
    j       .L_common

#if MAX_CORE_NUM > 2
.L_hart2:
    la      sp, __stack2
    j       .L_common

.L_hart3:
    la      sp, __stack3
    j       .L_common

.L_hart4:
    la      sp, __stack4
    j       .L_common

.L_hart5:
    la      sp, __stack5
    j       .L_common
#endif

#if MAX_CORE_NUM > 6
.L_hart6:
    la      sp, __stack6
    j       .L_common

.L_hart7:
    la      sp, __stack7
    j       .L_common
#endif

    /* ---- common entry ---- */
.L_common:
    auipc   ra, 0
    addi    sp, sp, -16
    sw      ra, 8(sp)
    call    npu_init
    li      a0, 0
    li      a1, 0
    call    core_dispatch
    /* a hart whose main returns serves debug commands */
    call    npu_dbg_idle
.L_hang:
    j       .L_hang

    .size _start, . - _start
