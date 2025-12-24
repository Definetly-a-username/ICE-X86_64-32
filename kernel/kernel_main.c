#include "fs/ext2.h"
/*
 * ICE Operating System - Kernel Main Entry
 * MPM Kernel initialization
 */

#include "core/mpm.h"
#include "drivers/vga.h"
#include "cpu/gdt.h"
#include "cpu/idt.h"
#include "drivers/pic.h"
#include "drivers/pit.h"
#include "drivers/keyboard.h"
#include "mm/pmm.h"
#include "tty/tty.h"

/* Multiboot1 magic value */
#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

/* Kernel entry point - called from boot.asm */
void kernel_main(uint32_t magic, void *mboot_info) {
    /* Initialize VGA first for output */
    vga_init();
    vga_puts("VGA Initialized.\n");
    
    /* Verify multiboot */
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        vga_puts("ERROR: Not booted by Multiboot loader!\n");
        // for (;;) __asm__ volatile ("hlt");
    }
    
    /* Initialize GDT */
    vga_puts("[BOOT] Loading GDT... ");
    gdt_init();
    vga_puts("OK\n");
    
    /* Initialize IDT */
    vga_puts("[BOOT] Loading IDT... ");
    idt_init();
    vga_puts("OK\n");
    
    /* Initialize PIC */
    vga_puts("[BOOT] Initializing PIC... ");
    pic_init();
    vga_puts("OK\n");
    
    /* Initialize physical memory manager */
    vga_puts("[BOOT] Initializing memory... ");
    pmm_init(mboot_info);
    vga_puts("OK\n");
    
    /* Initialize PIT timer */
    vga_puts("[BOOT] Initializing timer... ");
    pit_init(100);  /* 100 Hz */
    vga_puts("OK\n");
    
    /* Initialize keyboard */
    vga_puts("[BOOT] Initializing keyboard... ");
    keyboard_init();
    vga_puts("OK\n");
    
    /* Initialize TTY */
    vga_puts("[BOOT] Initializing TTY... ");
    tty_init();
    vga_puts("OK\n");

    /* Initialize filesystem */
    vga_puts("[BOOT] Mounting Filesystem (EXT2)... ");
    if (ext2_init() == 0) {
        vga_puts("OK\n");
    } else {
        vga_puts("FAILED (Check disk)\n");
    }
    
    /* Initialize applications */
    
    /* Enable interrupts */
    vga_puts("[BOOT] Enabling interrupts... ");
    __asm__ volatile ("sti");
    vga_puts("OK\n");
    
    vga_puts("\n");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts("MPM Kernel ready.\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("Type 'help' for available commands.\n\n");
    
    /* Initialize MPM and enter shell */
    mpm_init();
    mpm_shell();
    
    /* Should never reach here */
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
