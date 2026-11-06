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

	/* Use disjoint reserved DRAM stacks, independent of local SRAM banking. */
	la sp, __stack_base
	addi t0, a0, 1
	slli t0, t0, 12
	add sp, sp, t0
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
	li t1, 3
	sw t1, 4(t0)
	/* All shared initialization is complete before entering freestanding C. */
	mv s4, a0
	mv s5, t0
	mv a1, t0
	call en7523_runtime_init
	mv t6, a0
	mv a0, s4
	mv t0, s5
	beqz t6, .Lc_ready
	li t1, 0x45525200
	or t1, t1, t6
	sw t1, 52(t0)
	li t1, 3
	sw t1, 12(t0)
	j .Lpublish
.Lc_ready:
	li t1, 1
	sw t1, 12(t0)
.Lpublish:
	fence rw, rw
	li t1, 0x454e5055 /* ENPU */
	sw t1, 0(t0)
	fence rw, rw

	beqz t6, .Lrun
	j .Lpark
.Lrun:
	/* Hart 0 owns queue 0; the other harts only publish progress. */
	li s0, 0x1ec0c030
	li s1, 0x840f1000
	li s2, 0
.Lheartbeat:
	lw t1, 16(t0)
	addi t1, t1, 1
	sw t1, 16(t0)
	fence w, w
	bnez a0, .Lworker_test
	lw t2, 12(s0)
	andi t3, t2, 3
	li t4, 1
	bne t3, t4, .Lheartbeat
	fence iorw, iorw
	li t3, 0x1ec0c000
	li t4, 1
	sw t4, 0(t3) /* Clear the queue 0 pending source, not host queue 8. */
	addi s2, s2, 1
	sw s2, 48(t0)
	li s3, 0 /* Transport error until the request has been validated. */
	srli t3, t2, 11
	andi t3, t3, 15
	li t4, 7
	bne t3, t4, .Lreply
	andi t3, t2, 32
	bnez t3, .Lreply
	lw t3, 0(s0)
	bne t3, s1, .Lreply
	lw t3, 4(s0)
	li t4, 64
	bne t3, t4, .Lreply
	lw t3, 0(s1)
	li t4, 0x4e50444d
	bne t3, t4, .Lreply
	lw t3, 4(s1)
	li t4, 1
	bne t3, t4, .Lreply
	lw t3, 12(s1)
	li t4, 2
	beq t3, t4, .Lrequest_trap
	bgeu t3, t4, .Lbad_command
	j .Lstatus
.Lrequest_trap:
	lw t3, 48(s1)
	beqz t3, .Lbad_command
	li t4, 4
	bgeu t3, t4, .Lbad_command
	slli t3, t3, 6
	la t4, __diag_base
	add t3, t3, t4
	lw t4, 12(t3)
	li t5, 1
	bne t4, t5, .Lbad_command
	li t4, 0x54524150
	sw t4, 60(t3)
	fence rw, rw
.Lstatus:
	li t3, 0x00010001
	sw t3, 20(s1) /* Diagnostic protocol version 1.0. */
	li t3, 4
	sw t3, 24(s1)
	la t3, __diag_base
	li t4, 0
.Lstates:
	lw t5, 12(t3)
	slli t6, t4, 2
	add t6, s1, t6
	sw t5, 28(t6)
	addi t3, t3, 64
	addi t4, t4, 1
	li t5, 4
	bltu t4, t5, .Lstates
	sw s2, 44(s1)
	sw zero, 16(s1)
	li s3, 1
	j .Lreply
.Lbad_command:
	li t3, 1
	sw t3, 16(s1)
.Lreply:
	/* Publish the complete response before asserting transport DONE. */
	fence iorw, iorw
	li t3, -31
	and t2, t2, t3
	slli s3, s3, 2
	or t2, t2, s3
	ori t2, t2, 2
	sw t2, 12(s0)
	fence iorw, iorw
	j .Lheartbeat

.Lworker_test:
	lw t3, 60(t0)
	li t4, 0x54524150
	bne t3, t4, .Lheartbeat
	sw zero, 60(t0)
	fence rw, rw
	ecall /* Intentional M-mode environment call; terminal trap cause 11. */
	j .Lpark

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
