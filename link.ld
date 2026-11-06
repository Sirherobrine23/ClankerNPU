/*
 * ClankerNPU firmware linker script
 *
 * Memory layout:
 *   .text + .rodata  → DRAM at 0x84000000  (becomes npu_rv32.bin)
 *   .data            → SRAM: 0x1E900000 (EN7523), 0x3E900000 (AN75xx)
 *                      (becomes npu_data.bin)
 *   .bss             → NPU SRAM after .data (zeroed by crt0, not in any file)
 *   stacks           → DRAM: 0x840D0000 on EN7523, after .text on AN75xx
 *                      (not in any file, 16KB per hart)
 */

OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY
{
#ifdef EN7523
    DRAM (rx)   : ORIGIN = 0x84000000, LENGTH = 0xd0000
    STACK (rw)  : ORIGIN = 0x840d0000, LENGTH = 0x10000
    SRAM (rw)   : ORIGIN = 0x1e900000, LENGTH = 0x6800
#else
    DRAM (rwx)  : ORIGIN = 0x84000000, LENGTH = 2M
    SRAM (rw)   : ORIGIN = 0x3E900000, LENGTH = 64K
#endif
}

SECTIONS
{
    .text : {
        KEEP(*(.text.init))
        *(.text.hot)
        *(.text .text.*)
    } > DRAM

#ifdef EN7523
    .rodata : {
#else
    .rodata : ALIGN(4) {
#endif
        *(.rodata .rodata.*)
        *(.srodata .srodata.*)
    } > DRAM

    __image_end = .;

#ifdef EN7523
    .data : {
#else
    .data : ALIGN(4) {
#endif
        __data_start = .;
        *(.data .data.*)
        *(.sdata .sdata.*)
        __data_end = .;
    } > SRAM

    __global_pointer$ = __data_start + 0x800;

    .bss (NOLOAD) : ALIGN(4) {
        __bss_start = .;
        *(.bss .bss.*)
        *(.sbss .sbss.*)
        *(COMMON)
        . = ALIGN(4);
#ifdef EN7523
        __bss_end = .;
#else
        __bss_end = . - 4;
#endif
    } > SRAM

#ifdef EN7523
    __stack_base = ORIGIN(STACK);
#else
    /* the debug block sits at a fixed address above the globals */
    ASSERT(__bss_end < __ndbg_base, "globals overlap the debug block")

    .stack (NOLOAD) : ALIGN(16) {
        __stack_base = .;
    } > DRAM
#endif

    /DISCARD/ : {
        *(.eh_frame)
        *(.comment)
        *(.note.*)
    }
}

__STACK_SIZE = 0x4000;
__stack0 = __stack_base + 1 * __STACK_SIZE;
__stack1 = __stack_base + 2 * __STACK_SIZE;
__stack2 = __stack_base + 3 * __STACK_SIZE;
__stack3 = __stack_base + 4 * __STACK_SIZE;
__stack4 = __stack_base + 5 * __STACK_SIZE;
__stack5 = __stack_base + 6 * __STACK_SIZE;
__stack6 = __stack_base + 7 * __STACK_SIZE;
__stack7 = __stack_base + 8 * __STACK_SIZE;

#ifdef EN7523
ASSERT(en7523_prologue_end - _start == 0x108, "EN7523 boot prefix changed")
ASSERT(__bss_end <= 0x1e906800, "globals overlap common debug block")
ASSERT(ORIGIN(DRAM) + LENGTH(DRAM) <= ORIGIN(STACK), "code overlaps stacks")
#endif
