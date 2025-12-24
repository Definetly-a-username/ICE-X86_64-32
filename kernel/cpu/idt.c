/*
 * ICE Operating System - IDT Implementation
 */

#include "idt.h"
#include "gdt.h"
#include "../drivers/vga.h"

/* IDT with 256 entries */
static idt_entry_t idt[256];
static idt_ptr_t idt_ptr;

/* Interrupt handlers */
static interrupt_handler_t handlers[256] = {0};

/* Exception messages */
static const char *exception_messages[] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 Floating Point",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating Point",
    "Virtualization",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Security Exception",
    "Reserved"
};

/* Set an IDT entry */
static void idt_set_gate(u8 num, u32 base, u16 sel, u8 flags) {
    idt[num].offset_low = base & 0xFFFF;
    idt[num].offset_high = (base >> 16) & 0xFFFF;
    idt[num].selector = sel;
    idt[num].zero = 0;
    idt[num].type_attr = flags;
}

/* ISR stubs - defined in isr.asm */
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);

/* IRQ stubs */
extern void irq0(void);
extern void irq1(void);
extern void irq2(void);
extern void irq3(void);
extern void irq4(void);
extern void irq5(void);
extern void irq6(void);
extern void irq7(void);
extern void irq8(void);
extern void irq9(void);
extern void irq10(void);
extern void irq11(void);
extern void irq12(void);
extern void irq13(void);
extern void irq14(void);
extern void irq15(void);

/* Syscall stub */
extern void isr128(void);

void idt_init(void) {
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (u32)&idt;
    
    /* Clear IDT */
    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, 0, 0, 0);
    }
    
    /* Set exception handlers (0-31) */
    idt_set_gate(0, (u32)isr0, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(1, (u32)isr1, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(2, (u32)isr2, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(3, (u32)isr3, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(4, (u32)isr4, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(5, (u32)isr5, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(6, (u32)isr6, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(7, (u32)isr7, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(8, (u32)isr8, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(9, (u32)isr9, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(10, (u32)isr10, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(11, (u32)isr11, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(12, (u32)isr12, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(13, (u32)isr13, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(14, (u32)isr14, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(15, (u32)isr15, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(16, (u32)isr16, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(17, (u32)isr17, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(18, (u32)isr18, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(19, (u32)isr19, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(20, (u32)isr20, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(21, (u32)isr21, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(22, (u32)isr22, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(23, (u32)isr23, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(24, (u32)isr24, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(25, (u32)isr25, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(26, (u32)isr26, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(27, (u32)isr27, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(28, (u32)isr28, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(29, (u32)isr29, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(30, (u32)isr30, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(31, (u32)isr31, GDT_KERNEL_CODE, 0x8E);
    
    /* Set IRQ handlers (32-47) */
    idt_set_gate(32, (u32)irq0, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(33, (u32)irq1, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(34, (u32)irq2, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(35, (u32)irq3, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(36, (u32)irq4, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(37, (u32)irq5, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(38, (u32)irq6, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(39, (u32)irq7, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(40, (u32)irq8, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(41, (u32)irq9, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(42, (u32)irq10, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(43, (u32)irq11, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(44, (u32)irq12, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(45, (u32)irq13, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(46, (u32)irq14, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(47, (u32)irq15, GDT_KERNEL_CODE, 0x8E);
    
    /* Syscall handler (int 0x80) - callable from ring 3 */
    idt_set_gate(128, (u32)isr128, GDT_KERNEL_CODE, 0xEE);
    
    /* Load IDT */
    __asm__ volatile ("lidt %0" : : "m"(idt_ptr));
}

void idt_register_handler(u8 n, interrupt_handler_t handler) {
    handlers[n] = handler;
}

/* Common interrupt handler - called from assembly */
void isr_handler(interrupt_frame_t *frame) {
    if (handlers[frame->int_no]) {
        handlers[frame->int_no](frame);
    } else if (frame->int_no < 32) {
        /* Unhandled exception */
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
        vga_puts("\n KERNEL PANIC: ");
        vga_puts(exception_messages[frame->int_no]);
        vga_puts("\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        
        /* Halt */
        __asm__ volatile ("cli; hlt");
    }
}

/* IRQ handler - called from assembly */
void irq_handler(interrupt_frame_t *frame) {
    /* Send EOI to PIC */
    if (frame->int_no >= 40) {
        /* Send to slave PIC */
        __asm__ volatile ("outb %0, %1" : : "a"((u8)0x20), "Nd"((u16)0xA0));
    }
    /* Send to master PIC */
    __asm__ volatile ("outb %0, %1" : : "a"((u8)0x20), "Nd"((u16)0x20));
    
    if (handlers[frame->int_no]) {
        handlers[frame->int_no](frame);
    }
}
