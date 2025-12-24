/*
 * ICE Operating System - Physical Memory Manager
 * Bitmap-based page allocator for Multiboot1
 */

#include "pmm.h"
#include "../drivers/vga.h"

/* Multiboot1 info structure */
typedef struct {
    u32 flags;
    u32 mem_lower;
    u32 mem_upper;
    u32 boot_device;
    u32 cmdline;
    u32 mods_count;
    u32 mods_addr;
    u32 syms[4];
    u32 mmap_length;
    u32 mmap_addr;
} multiboot_info_t;

/* Multiboot memory map entry */
typedef struct __attribute__((packed)) {
    u32 size;
    u64 addr;
    u64 len;
    u32 type;
} mboot_mmap_entry_t;

#define MMAP_TYPE_AVAILABLE 1
#define MBOOT_FLAG_MMAP     (1 << 6)
#define MBOOT_FLAG_MEM      (1 << 0)

/* Memory bitmap */
#define MAX_PAGES (256 * 1024)  /* Support up to 1GB */
static u8 page_bitmap[MAX_PAGES / 8];

static u32 total_pages = 0;
static u32 used_pages = 0;

/* Bitmap operations */
static inline void bitmap_set(u32 page) {
    page_bitmap[page / 8] |= (1 << (page % 8));
}

static inline void bitmap_clear(u32 page) {
    page_bitmap[page / 8] &= ~(1 << (page % 8));
}

static inline bool bitmap_test(u32 page) {
    return page_bitmap[page / 8] & (1 << (page % 8));
}

/* Mark range as used */
static void mark_range_used(phys_addr_t start, u32 size) {
    u32 start_page = start / PAGE_SIZE;
    u32 end_page = (start + size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    for (u32 p = start_page; p < end_page && p < MAX_PAGES; p++) {
        if (!bitmap_test(p)) {
            bitmap_set(p);
            used_pages++;
        }
    }
}

/* Mark range as free */
static void mark_range_free(phys_addr_t start, u32 size) {
    u32 start_page = start / PAGE_SIZE;
    u32 end_page = (start + size) / PAGE_SIZE;
    
    for (u32 p = start_page; p < end_page && p < MAX_PAGES; p++) {
        if (bitmap_test(p)) {
            bitmap_clear(p);
            used_pages--;
        }
        total_pages++;
    }
}

void pmm_init(void *mboot_info) {
    multiboot_info_t *mbi = (multiboot_info_t*)mboot_info;
    
    /* Start with all memory marked as used */
    for (u32 i = 0; i < sizeof(page_bitmap); i++) {
        page_bitmap[i] = 0xFF;
    }
    used_pages = MAX_PAGES;
    total_pages = 0;
    
    /* Use memory map if available */
    if (mbi->flags & MBOOT_FLAG_MMAP) {
        mboot_mmap_entry_t *entry = (mboot_mmap_entry_t*)mbi->mmap_addr;
        u32 end = mbi->mmap_addr + mbi->mmap_length;
        
        while ((u32)entry < end) {
            if (entry->type == MMAP_TYPE_AVAILABLE) {
                u32 addr = (u32)entry->addr;
                u32 len = (u32)entry->len;
                mark_range_free(addr, len);
            }
            entry = (mboot_mmap_entry_t*)((u32)entry + entry->size + 4);
        }
    } else if (mbi->flags & MBOOT_FLAG_MEM) {
        /* Fallback to basic memory info */
        u32 upper_kb = mbi->mem_upper;
        mark_range_free(0x100000, upper_kb * 1024);
    }
    
    /* Reserve low memory (0-1MB) for BIOS/kernel */
    mark_range_used(0, 0x100000);
    
    /* Reserve kernel space (1MB-2MB) */
    mark_range_used(0x100000, 0x100000);
}

phys_addr_t pmm_alloc_page(void) {
    for (u32 i = 256; i < MAX_PAGES; i++) {  /* Start after 1MB */
        if (!bitmap_test(i)) {
            bitmap_set(i);
            used_pages++;
            return i * PAGE_SIZE;
        }
    }
    return 0;  /* Out of memory */
}

void pmm_free_page(phys_addr_t addr) {
    u32 page = addr / PAGE_SIZE;
    if (page < MAX_PAGES && bitmap_test(page)) {
        bitmap_clear(page);
        used_pages--;
    }
}

u32 pmm_get_total_memory(void) {
    return total_pages * PAGE_SIZE;
}

u32 pmm_get_free_memory(void) {
    return (total_pages > used_pages) ? (total_pages - used_pages) * PAGE_SIZE : 0;
}
