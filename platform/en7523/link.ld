OUTPUT_ARCH(riscv)
ENTRY(_start)
MEMORY
{
	DRAM (rx) : ORIGIN = 0x84000000, LENGTH = 0xf0000
	DATA (rw) : ORIGIN = 0x1e900000, LENGTH = 0x8000
	DIAG (rw) : ORIGIN = 0x840f0000, LENGTH = 0x100
}
SECTIONS
{
	.text : { KEEP(*(.text.init)) *(.text .text.*) } > DRAM
	.rodata : { *(.rodata .rodata.*) } > DRAM
	.data : { *(.data .data.*) } > DATA
	__global_pointer$ = ORIGIN(DATA) + 0x800;
	__diag_base = ORIGIN(DIAG);
	__diag_end = ORIGIN(DIAG) + LENGTH(DIAG);
	/DISCARD/ : { *(.comment) *(.note*) *(.riscv.attributes) }
}
ASSERT(en7523_prologue_end - _start == 0x108, "EN7523 prologue size changed")
ASSERT(SIZEOF(.text) + SIZEOF(.rodata) < LENGTH(DRAM), "code overlaps diagnostic records")
