/*
 * ICE Operating System - ATA/IDE PIO Driver
 */

#include "ata.h"
#include "vga.h"

/* I/O helpers */
static inline void outb(u16 port, u8 value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline u8 inb(u16 port) {
    u8 ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void insl(u16 port, void *addr, u32 count) {
    __asm__ volatile ("rep insl" : "+D"(addr), "+c"(count) : "d"(port) : "memory");
}

static inline void outsl(u16 port, const void *addr, u32 count) {
    __asm__ volatile ("rep outsl" : "+S"(addr), "+c"(count) : "d"(port));
}

/* State */
static bool drive_present = false;

/* Wait for drive ready */
static int ata_wait_ready(void) {
    int timeout = 100000;
    while (timeout--) {
        u8 status = inb(ATA_PRIMARY_STATUS);
        if (!(status & ATA_STATUS_BSY)) {
            return 0;
        }
    }
    return -1;  /* Timeout */
}

/* Wait for data request */
static int ata_wait_drq(void) {
    int timeout = 100000;
    while (timeout--) {
        u8 status = inb(ATA_PRIMARY_STATUS);
        if (status & ATA_STATUS_ERR) return -1;
        if (status & ATA_STATUS_DRQ) return 0;
    }
    return -1;
}

int ata_init(void) {
    /* Reset drive */
    outb(ATA_PRIMARY_CONTROL, 0x04);  /* Set SRST */
    for (int i = 0; i < 10000; i++) inb(ATA_PRIMARY_STATUS);  /* Delay */
    outb(ATA_PRIMARY_CONTROL, 0x00);  /* Clear SRST */
    
    /* Wait for not busy */
    if (ata_wait_ready() < 0) {
        drive_present = false;
        return -1;
    }
    
    /* Select master drive */
    outb(ATA_PRIMARY_DRIVE, 0xA0);
    
    /* Check for drive presence */
    outb(ATA_PRIMARY_SECCOUNT, 0);
    outb(ATA_PRIMARY_LBA_LO, 0);
    outb(ATA_PRIMARY_LBA_MID, 0);
    outb(ATA_PRIMARY_LBA_HI, 0);
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_IDENTIFY);
    
    u8 status = inb(ATA_PRIMARY_STATUS);
    if (status == 0) {
        drive_present = false;
        return -1;  /* No drive */
    }
    
    /* Wait and check for errors */
    if (ata_wait_ready() < 0 || ata_wait_drq() < 0) {
        /* Check for ATAPI */
        u8 mid = inb(ATA_PRIMARY_LBA_MID);
        u8 hi = inb(ATA_PRIMARY_LBA_HI);
        if (mid || hi) {
            drive_present = false;
            return -1;  /* ATAPI or no drive */
        }
    }
    
    /* Read and discard identify data */
    u16 tmp;
    for (int i = 0; i < 256; i++) {
        __asm__ volatile ("inw %1, %0" : "=a"(tmp) : "Nd"((u16)ATA_PRIMARY_DATA));
    }
    (void)tmp;
    
    drive_present = true;
    return 0;
}

int ata_read_sectors(u32 lba, u8 count, void *buffer) {
    if (!drive_present) return -1;
    if (count == 0) return 0;
    
    /* Wait for drive ready */
    if (ata_wait_ready() < 0) return -1;
    
    /* Select drive with LBA bit set */
    outb(ATA_PRIMARY_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    
    /* Set up transfer */
    outb(ATA_PRIMARY_ERROR, 0);
    outb(ATA_PRIMARY_SECCOUNT, count);
    outb(ATA_PRIMARY_LBA_LO, lba & 0xFF);
    outb(ATA_PRIMARY_LBA_MID, (lba >> 8) & 0xFF);
    outb(ATA_PRIMARY_LBA_HI, (lba >> 16) & 0xFF);
    
    /* Send read command */
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_READ_PIO);
    
    /* Read sectors */
    u16 *buf = (u16*)buffer;
    for (int s = 0; s < count; s++) {
        if (ata_wait_drq() < 0) return -1;
        
        for (int i = 0; i < 256; i++) {
            __asm__ volatile ("inw %1, %0" : "=a"(buf[i]) : "Nd"((u16)ATA_PRIMARY_DATA));
        }
        buf += 256;
    }
    
    return count;
}

int ata_write_sectors(u32 lba, u8 count, const void *buffer) {
    if (!drive_present) return -1;
    if (count == 0) return 0;
    
    /* Wait for drive ready */
    if (ata_wait_ready() < 0) return -1;
    
    /* Select drive with LBA bit set */
    outb(ATA_PRIMARY_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    
    /* Set up transfer */
    outb(ATA_PRIMARY_ERROR, 0);
    outb(ATA_PRIMARY_SECCOUNT, count);
    outb(ATA_PRIMARY_LBA_LO, lba & 0xFF);
    outb(ATA_PRIMARY_LBA_MID, (lba >> 8) & 0xFF);
    outb(ATA_PRIMARY_LBA_HI, (lba >> 16) & 0xFF);
    
    /* Send write command */
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_WRITE_PIO);
    
    /* Write sectors */
    const u16 *buf = (const u16*)buffer;
    for (int s = 0; s < count; s++) {
        if (ata_wait_drq() < 0) return -1;
        
        for (int i = 0; i < 256; i++) {
            __asm__ volatile ("outw %0, %1" : : "a"(buf[i]), "Nd"((u16)ATA_PRIMARY_DATA));
        }
        buf += 256;
    }
    
    /* Flush cache */
    outb(ATA_PRIMARY_COMMAND, 0xE7);
    ata_wait_ready();
    
    return count;
}

bool ata_is_present(void) {
    return drive_present;
}
