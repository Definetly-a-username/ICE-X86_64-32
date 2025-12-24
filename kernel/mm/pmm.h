/*
 * ICE Operating System - Physical Memory Manager Header
 */

#ifndef ICE_PMM_H
#define ICE_PMM_H

#include "../types.h"

/* Page size */
#define PAGE_SIZE 4096

/* Initialize PMM from multiboot info */
void pmm_init(void *mboot_info);

/* Allocate a physical page */
phys_addr_t pmm_alloc_page(void);

/* Free a physical page */
void pmm_free_page(phys_addr_t addr);

/* Get total/free memory info */
u32 pmm_get_total_memory(void);
u32 pmm_get_free_memory(void);

#endif /* ICE_PMM_H */
