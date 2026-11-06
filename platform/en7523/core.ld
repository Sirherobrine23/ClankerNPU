OUTPUT_ARCH(riscv)
ENTRY(_start)
MEMORY
{
	DRAM (rx) : ORIGIN = 0x84000000, LENGTH = 0xd0000
	STACK (rw) : ORIGIN = 0x840d0000, LENGTH = 0x10000
	DATA (rw) : ORIGIN = 0x1e900000, LENGTH = 0x6800
	DIAG (rw) : ORIGIN = 0x840f0000, LENGTH = 0x2000
}
SECTIONS
{
	.text : { KEEP(*(.text.init)) *(.text.hot) *(.text .text.*) } > DRAM
	.rodata : { *(.rodata .rodata.*) *(.srodata .srodata.*) } > DRAM
	.data : {
		__data_start = .;
		*(.data .data.*) *(.sdata .sdata.*)
		__data_end = .;
	} > DATA
	__global_pointer$ = __data_start + 0x800;
	.bss (NOLOAD) : ALIGN(4) {
		__bss_start = .;
		*(.bss .bss.*) *(.sbss .sbss.*) *(COMMON)
		. = ALIGN(4);
		__bss_end = .;
	} > DATA
	__stack_base = ORIGIN(STACK);
	__diag_base = ORIGIN(DIAG);
	/DISCARD/ : { *(.eh_frame) *(.comment) *(.note*) }
}
ASSERT(en7523_prologue_end - _start == 0x108, "EN7523 boot prefix changed")
ASSERT(__bss_end <= 0x1e906800, "globals overlap common debug block")
ASSERT(ORIGIN(STACK) + LENGTH(STACK) <= ORIGIN(DIAG), "stack overlaps records")

ASSERT(en7523_image_header - _start == 0x10c, "EN7523 descriptor moved")
