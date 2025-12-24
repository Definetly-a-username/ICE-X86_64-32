/*
 * ICE Operating System - IDT Header
 */

#ifndef ICE_IDT_H
#define ICE_IDT_H

#include "../types.h"

/* IDT entry structure */
typedef struct __attribute__((packed)) {
    u16 offset_low;
    u16 selector;
    u8  zero;
    u8  type_attr;
    u16 offset_high;
} idt_entry_t;

/* IDT pointer structure */
typedef struct __attribute__((packed)) {
    u16 limit;
    u32 base;
} idt_ptr_t;

/* Interrupt frame pushed by CPU */
typedef struct {
    u32 ds;
    u32 edi, esi, ebp, esp, ebx, edx, ecx, eax;
    u32 int_no, err_code;
    u32 eip, cs, eflags, useresp, ss;
} interrupt_frame_t;

/* Interrupt handler type */
typedef void (*interrupt_handler_t)(interrupt_frame_t *frame);

/* Initialize IDT */
void idt_init(void);

/* Register an interrupt handler */
void idt_register_handler(u8 n, interrupt_handler_t handler);

#endif /* ICE_IDT_H */
