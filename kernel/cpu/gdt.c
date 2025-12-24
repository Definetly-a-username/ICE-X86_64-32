/*
 * ICE Operating System - GDT Implementation
 */

#include "gdt.h"

/* GDT Table: 6 entries (Null, KCode, KData, UCode, UData, TSS) */
/* Initialized to force .data section placement instead of .bss */
static gdt_entry_t gdt_entries[6] = {
    {0, 0, 0, 0, 0, 0},      /* 0: Null */
    {0xFFFF, 0, 0, 0x9A, 0xCF, 0}, /* 1: Kernel Code placeholder */
    {0xFFFF, 0, 0, 0x92, 0xCF, 0}, /* 2: Kernel Data placeholder */
    {0xFFFF, 0, 0, 0xFA, 0xCF, 0}, /* 3: User Code placeholder */
    {0xFFFF, 0, 0, 0xF2, 0xCF, 0}, /* 4: User Data placeholder */
    {0, 0, 0, 0, 0, 0}       /* 5: TSS placeholder */
};
static gdt_ptr_t   gdt_ptr = {0, 0};
static tss_t       tss_entry = {0};

/* Helper to set GDT gate */
static void gdt_set_gate(int num, u32 base, u32 limit, u8 access, u8 gran) {
    gdt_entries[num].base_low    = (base & 0xFFFF);
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].base_high   = (base >> 24) & 0xFF;

    gdt_entries[num].limit_low   = (limit & 0xFFFF);
    gdt_entries[num].granularity = (limit >> 16) & 0x0F;

    gdt_entries[num].granularity |= (gran & 0xF0);
    gdt_entries[num].access      = access;
}

void gdt_init(void) {
    gdt_ptr.limit = (sizeof(gdt_entry_t) * 6) - 1;
    gdt_ptr.base  = (u32)&gdt_entries;

    /* 0: Null descriptor */
    gdt_set_gate(0, 0, 0, 0, 0);

    /* 1: Kernel Code (0x08) */
    /* Base=0, Limit=4GB, Access=0x9A (Ring 0 Code), Gran=0xCF (4KB blocks) */
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);

    /* 2: Kernel Data (0x10) */
    /* Base=0, Limit=4GB, Access=0x92 (Ring 0 Data), Gran=0xCF */
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    /* 3: User Code (0x18) */
    /* Access=0xFA (Ring 3 Code) */
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);

    /* 4: User Data (0x20) */
    /* Access=0xF2 (Ring 3 Data) */
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    /* 5: TSS (0x28) */
    /* Clean TSS first */
    for (u32 i = 0; i < sizeof(tss_entry); i++) {
        ((u8*)&tss_entry)[i] = 0;
    }
    tss_entry.ss0 = 0x10;  /* Kernel Data */
    tss_entry.esp0 = 0;
    
    /* TSS descriptor base/limit */
    u32 tss_base = (u32)&tss_entry;
    u32 tss_limit = sizeof(tss_entry);
    
    /* Access=0x89 (Available TSS), Gran=0x00 (Byte blocks) */
    gdt_set_gate(5, tss_base, tss_limit, 0x89, 0x00);

    /* Load GDT */
    gdt_flush((u32)&gdt_ptr);
    
    /* Load TSS */
    tss_flush();
}

void gdt_set_kernel_stack(u32 stack) {
    tss_entry.esp0 = stack;
}
