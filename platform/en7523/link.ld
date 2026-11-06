OUTPUT_ARCH(riscv)
ENTRY(_start)
MEMORY
{
	DRAM (rx) : ORIGIN = 0x84000000, LENGTH = 0xe0000
	DATA (rw) : ORIGIN = 0x1e900000, LENGTH = 0x8000
	STACK (rw) : ORIGIN = 0x840e0000, LENGTH = 0x4000
	DIAG (rw) : ORIGIN = 0x840f0000, LENGTH = 0x2000
}
SECTIONS
{
	.text : { KEEP(*(.text.init)) *(.text .text.*) } > DRAM
	.rodata : { *(.rodata .rodata.*) } > DRAM
	.data : { *(.data .data.*) } > DATA
	.bss (NOLOAD) : { *(.bss .bss.*) *(.sbss .sbss.*) *(COMMON) } > DATA
	__stack_base = ORIGIN(STACK);
	__stack_end = ORIGIN(STACK) + LENGTH(STACK);
	__global_pointer$ = ORIGIN(DATA) + 0x800;
	__diag_base = ORIGIN(DIAG);
	__diag_end = ORIGIN(DIAG) + LENGTH(DIAG);
	/DISCARD/ : { *(.comment) *(.note*) *(.riscv.attributes) }
}
ASSERT(en7523_prologue_end - _start == 0x108, "EN7523 prologue size changed")
ASSERT(SIZEOF(.text) + SIZEOF(.rodata) < LENGTH(DRAM), "code overlaps diagnostic records")

ASSERT(SIZEOF(.bss) == 0, "shared BSS needs a synchronized startup implementation")
ASSERT(__stack_end <= __diag_base, "stacks overlap diagnostic records")
