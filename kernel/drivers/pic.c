 

#include "pic.h"

 
static inline void outb(u16 port, u8 value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline u8 inb(u16 port) {
    u8 ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void io_wait(void) {
    outb(0x80, 0);
}

void pic_init(void) {
    // ICW1: Initialize + ICW4 needed
    outb(PIC1_COMMAND, 0x11);    
    io_wait();
    outb(PIC2_COMMAND, 0x11);
    io_wait();
    
    // ICW2: Vector offsets (IRQ0 = int 32, IRQ8 = int 40)
    outb(PIC1_DATA, 0x20);       
    io_wait();
    outb(PIC2_DATA, 0x28);       
    io_wait();
    
    // ICW3: Cascade identity
    outb(PIC1_DATA, 0x04);       
    io_wait();
    outb(PIC2_DATA, 0x02);       
    io_wait();
    
    // ICW4: 8086 mode
    outb(PIC1_DATA, 0x01);
    io_wait();
    outb(PIC2_DATA, 0x01);
    io_wait();
    
    // Mask all IRQs initially (will be unmasked as needed)
    outb(PIC1_DATA, 0xFB);  // All masked except cascade (IRQ2)
    outb(PIC2_DATA, 0xFF);  // All masked
}

void pic_send_eoi(u8 irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, 0x20);
    }
    outb(PIC1_COMMAND, 0x20);
}

void pic_mask_irq(u8 irq) {
    u16 port;
    u8 value;
    
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    
    value = inb(port) | (1 << irq);
    outb(port, value);
}

void pic_unmask_irq(u8 irq) {
    u16 port;
    u8 value;
    
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    
    value = inb(port) & ~(1 << irq);
    outb(port, value);
}
