/*
 * ICE Operating System - PIT Timer Implementation
 */

#include "pit.h"
#include "pic.h"
#include "../cpu/idt.h"

/* I/O helper */
static inline void outb(u16 port, u8 value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

/* Timer state */
static volatile u64 tick_count = 0;
static u32 tick_frequency = 0;

/* Timer interrupt handler */
static void pit_handler(interrupt_frame_t *frame) {
    (void)frame;
    tick_count++;
    
    /* TODO: Trigger scheduler here */
}

void pit_init(u32 frequency) {
    tick_frequency = frequency;
    
    /* Calculate divisor */
    u32 divisor = PIT_FREQUENCY / frequency;
    
    /* Send command byte */
    outb(PIT_COMMAND, 0x36);  /* Channel 0, lobyte/hibyte, square wave */
    
    /* Send divisor */
    outb(PIT_CHANNEL0, divisor & 0xFF);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);
    
    /* Register handler */
    idt_register_handler(32, pit_handler);
    
    /* Unmask IRQ0 */
    pic_unmask_irq(IRQ_TIMER);
}

u64 pit_get_ticks(void) {
    return tick_count;
}

void pit_sleep_ms(u32 ms) {
    u64 target = tick_count + (ms * tick_frequency / 1000);
    while (tick_count < target) {
        __asm__ volatile ("hlt");
    }
}
