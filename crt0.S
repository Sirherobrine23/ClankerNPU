/*
 * ClankerNPU firmware startup
 *
 * Reset vector at 0x84000000. Sets GP, clears BSS (cold boot only),
 * assigns per-hart stack, calls npu_init() then core_dispatch().
 * EN7523 retains its md32rv cache/MPIS prefix and cold-boot BSS barrier.
 *
 * Hart count: AN7552=2, AN7583=6, AN7581=8.
 * Unmatched harts fall through to the hart-0 path.
 */

#include "npu_config.h"

#ifdef EN7523
/* EN7523 md32rv startup, reconstructed from the documented boot sequence. */
	.option norvc
	.option norelax
	.section .text.init, "ax", @progbits
	.global _start
	.global en7523_prologue_end
_start:
/* Keep this prefix byte-identical to the five observed EN7523 profiles. */
	addi x0, x0, 0
	addi x0, x0, 0
	addi x0, x0, 0
	addi x0, x0, 0
	addi x0, x0, 0
	addi x0, x0, 0
	addi x0, x0, 0
	addi x0, x0, 0
	addi x10, x0, 259
	csrrc x0, 0x7c0, x10
	addi x10, x0, 3
	csrrs x0, 0x7c0, x10
	addi x10, x0, 24
	.insn i 0x0f, 4, x0, a0, 0 /* md.cache.op a0 */
	addi x10, x0, 8
	.insn i 0x0f, 4, x0, a0, 0 /* md.cache.op a0 */
	addi x10, x0, 0
	ori x10, x10, 864
	csrrw x0, 0x9e0, x10
	addi x10, x0, 1024
	csrrw x0, 0x9f0, x10
	lui x10, 0x1c000
	ori x10, x10, 224
	csrrw x0, 0x9e1, x10
	lui x10, 0x1e800
	csrrw x0, 0x9f1, x10
	lui x10, 0x1e800
	ori x10, x10, 992
	csrrw x0, 0x9e2, x10
	lui x10, 0x1e860
	csrrw x0, 0x9f2, x10
	lui x10, 0x1e860
	ori x10, x10, 224
	csrrw x0, 0x9e3, x10
	lui x10, 0x20000
	csrrw x0, 0x9f3, x10
	lui x10, 0x20000
	ori x10, x10, 224
	csrrw x0, 0x9e4, x10
	lui x10, 0x30000
	csrrw x0, 0x9f4, x10
	lui x10, 0x30000
	ori x10, x10, 192
	csrrw x0, 0x9e5, x10
	lui x10, 0x30004
	csrrw x0, 0x9f5, x10
	lui x10, 0x30000
	csrrw x0, 0x9dd, x10
	lui x10, 0x30004
	csrrw x0, 0x9df, x10
	lui x10, 0x80000
	ori x10, x10, 992
	csrrw x0, 0x9e6, x10
	lui x10, 0xc0000
	csrrw x0, 0x9f6, x10
	addi x10, x0, 127
	csrrw x0, 0x9c0, x10
	addi x10, x0, 259
	csrrs x0, 0x7c0, x10
	fence.i
	addi x10, x0, 259
	csrrc x0, 0x7c0, x10
	fence.i
	addi x10, x0, 313
	csrrs x0, 0x7c0, x10
	fence.i
en7523_prologue_end:
	csrci mstatus, 8
	csrw mie, zero
	csrr a0, mhartid
	li t0, 4
	bgeu a0, t0, .Lpark
	la sp, __stack_base
	addi t0, a0, 1
	slli t0, t0, 14
	add sp, sp, t0
	la gp, __global_pointer$
	la t1, .Learly_trap
	csrw mtvec, t1
	beqz a0, .Lprimary
.Lwait_bss:
	la t1, __en7523_bss_ready
	lw t2, 0(t1)
	beqz t2, .Lwait_bss
	fence rw, rw
	j .Lmain
.Lprimary:
	la t1, __bss_start
	la t2, __bss_end
.Lbss:
	bgeu t1, t2, .Lbss_done
	sw zero, 0(t1)
	addi t1, t1, 4
	j .Lbss
.Lbss_done:
	fence rw, rw
	la t1, __en7523_bss_ready
	li t2, 1
	sw t2, 0(t1)
	fence rw, rw
.Lmain:
	call npu_init
	li a0, 0
	li a1, 0
	call core_dispatch
	.global en7523_park
en7523_park:
.Lpark:
	j .Lpark

	.balign 4
.Learly_trap:
	csrci mstatus, 8
	csrw mie, zero
	la t1, .Lpark
	csrw mtvec, t1
	j .Lpark

	/* Reloaded by the host before every cold boot; not cleared as BSS. */
	.section .data, "aw", @progbits
	.balign 4
	.global __en7523_bss_ready
__en7523_bss_ready:
	.word 0

#else

#define NPU_MIB0   0x1EC0C140

    .section .text.init, "ax", @progbits
    .globl _start
    .type  _start, @function
_start:
    .rept 8
    nop
    .endr

    fence.i
    fence

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

#endif /* EN7523 */
