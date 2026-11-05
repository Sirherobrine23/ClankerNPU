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

	/* No stack access until the per-hart memory bank is validated on silicon. */
	li sp, 0x30003ff0
	la gp, __global_pointer$
	la t0, __diag_base
	slli t1, a0, 6
	add t0, t0, t1
	csrw mscratch, t0
	la t1, .Ltrap
	csrw mtvec, t1

	/* Each hart exclusively owns one 64-byte diagnostic record. */
	sw zero, 0(t0)
	sw zero, 4(t0)
	sw a0, 8(t0)
	li t1, 1
	sw t1, 12(t0)
	sw zero, 16(t0)
	sw zero, 20(t0)
	sw zero, 24(t0)
	sw zero, 28(t0)
	csrr t1, mstatus
	sw t1, 32(t0)
	csrr t1, 0x7c0
	sw t1, 36(t0)
	csrr t1, 0x9c0
	sw t1, 40(t0)
	sw sp, 44(t0)
	sw zero, 48(t0)
	sw zero, 52(t0)
	sw zero, 56(t0)
	sw zero, 60(t0)
	li t1, 1
	sw t1, 4(t0)
	fence rw, rw
	li t1, 0x454e5055 /* ENPU */
	sw t1, 0(t0)
	fence rw, rw

	/* Polling progress only: no peripheral interrupt, DMA or mailbox setup. */
	li t1, 0
.Lheartbeat:
	addi t1, t1, 1
	sw t1, 16(t0)
	fence w, w
	j .Lheartbeat

	.balign 4
.Ltrap:
	/* Stackless terminal handler. A second fault parks rather than printing. */
	csrci mstatus, 8
	csrw mie, zero
	la t1, .Lpark
	csrw mtvec, t1
	csrr t0, mscratch
	csrr t1, mcause
	sw t1, 20(t0)
	csrr t1, mepc
	sw t1, 24(t0)
	csrr t1, mtval
	sw t1, 28(t0)
	csrr t1, mstatus
	sw t1, 32(t0)
	fence rw, rw
	li t1, 2
	sw t1, 12(t0)
	fence rw, rw
	.balign 4
.Lpark:
	j .Lpark

	/* The existing host loader requires a non-empty companion data image. */
	.section .data, "aw", @progbits
	.word 0
