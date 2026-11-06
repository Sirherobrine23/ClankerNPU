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
	la t0, __diag_base
	slli t1, a0, 6
	add t0, t0, t1
	csrw mscratch, t0
	la t1, .Learly_trap
	csrw mtvec, t1
	/* Records are outside BSS; each hart owns its own 64-byte slot. */
	li t1, 0
.Lrecord:
	add t2, t0, t1
	sw zero, 0(t2)
	addi t1, t1, 4
	li t2, 64
	bltu t1, t2, .Lrecord
	li t1, 4
	sw t1, 4(t0)
	sw a0, 8(t0)
	sw sp, 44(t0)
	li t1, 0x454e5055
	sw t1, 0(t0)
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
	csrr t0, mscratch
	csrr t1, mcause
	sw t1, 20(t0)
	csrr t1, mepc
	sw t1, 24(t0)
	csrr t1, mtval
	sw t1, 28(t0)
	li t1, 2
	fence rw, rw
	sw t1, 12(t0)
	j .Lpark

	/* Reloaded by the host before every cold boot; not cleared as BSS. */
	.section .data, "aw", @progbits
	.balign 4
	.global __en7523_bss_ready
__en7523_bss_ready:
	.word 0
