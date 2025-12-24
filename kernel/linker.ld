/*
 * ICE Operating System - Linker Script
 * Links kernel at 1MB (0x100000) for Multiboot
 */

ENTRY(_start)

SECTIONS
{
    . = 1M;

    /* Code section */
    .text ALIGN(4K) :
    {
        KEEP(*(.multiboot))
        *(.text)
    }

    /* Read-only data */
    .rodata ALIGN(4K) :
    {
        *(.rodata)
        *(.rodata.*)
    }

    /* Initialized data */
    .data ALIGN(4K) :
    {
        *(.data)
    }

    /* Uninitialized data */
    .bss ALIGN(4K) :
    {
        *(COMMON)
        *(.bss)
    }

    /* Kernel end marker */
    kernel_end = .;
}
