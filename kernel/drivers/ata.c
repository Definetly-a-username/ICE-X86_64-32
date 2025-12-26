 

#include "ata.h"
#include "vga.h"
#include "pit.h"

 
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

 
static bool drive_present = false;

 
static int ata_wait_ready(void) {
    // Use iteration count to be safe even if interrupts are disabled (PIT won't update)
    // 100,000 iterations * ~1us (pause + inb) = ~100ms
    // Increase to 1,000,000 for generous timeout (~1s)
    u32 retries = 1000000;
    while (retries-- > 0) {
        u8 status = inb(ATA_PRIMARY_STATUS);
        if (!(status & ATA_STATUS_BSY)) {
            return 0;
        }
        // Small delay
        __asm__ volatile("pause");
        
        // Also check error
        if (status & ATA_STATUS_ERR) return -1;
        if (status & ATA_STATUS_DF) return -1;
    }
    return -1;   
}

 
static int ata_wait_drq(void) {
    u32 retries = 1000000;
    while (retries-- > 0) {
        u8 status = inb(ATA_PRIMARY_STATUS);
        if (status & ATA_STATUS_ERR) return -1;
        if (status & ATA_STATUS_DF) return -1; // Drive Fault
        if (status & ATA_STATUS_DRQ) return 0;
        __asm__ volatile("pause");
    }
    return -1;
}

int ata_init(void) {
     
    outb(ATA_PRIMARY_CONTROL, 0x04);   
    // Reduced delay - hardware should stabilize faster
    // Use a smaller, more efficient delay loop
    for (volatile int i = 0; i < 1000; i++) {
        inb(ATA_PRIMARY_STATUS);
    }
    outb(ATA_PRIMARY_CONTROL, 0x00);   
    
     
    if (ata_wait_ready() < 0) {
        drive_present = false;
        return -1;
    }
    
     
    outb(ATA_PRIMARY_DRIVE, 0xA0);
    
     
    outb(ATA_PRIMARY_SECCOUNT, 0);
    outb(ATA_PRIMARY_LBA_LO, 0);
    outb(ATA_PRIMARY_LBA_MID, 0);
    outb(ATA_PRIMARY_LBA_HI, 0);
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_IDENTIFY);
    
    u8 status = inb(ATA_PRIMARY_STATUS);
    if (status == 0) {
        drive_present = false;
        return -1;   
    }
    
     
    if (ata_wait_ready() < 0 || ata_wait_drq() < 0) {
         
        u8 mid = inb(ATA_PRIMARY_LBA_MID);
        u8 hi = inb(ATA_PRIMARY_LBA_HI);
        if (mid || hi) {
            drive_present = false;
            return -1;   
        }
    }
    
     
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
    
     
    if (ata_wait_ready() < 0) return -1;
    
     
    outb(ATA_PRIMARY_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    
     
    outb(ATA_PRIMARY_ERROR, 0);
    outb(ATA_PRIMARY_SECCOUNT, count);
    outb(ATA_PRIMARY_LBA_LO, lba & 0xFF);
    outb(ATA_PRIMARY_LBA_MID, (lba >> 8) & 0xFF);
    outb(ATA_PRIMARY_LBA_HI, (lba >> 16) & 0xFF);
    
     
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_READ_PIO);
    
     
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
    
     
    if (ata_wait_ready() < 0) return -1;
    
     
    outb(ATA_PRIMARY_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    
     
    outb(ATA_PRIMARY_ERROR, 0);
    outb(ATA_PRIMARY_SECCOUNT, count);
    outb(ATA_PRIMARY_LBA_LO, lba & 0xFF);
    outb(ATA_PRIMARY_LBA_MID, (lba >> 8) & 0xFF);
    outb(ATA_PRIMARY_LBA_HI, (lba >> 16) & 0xFF);
    
     
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_WRITE_PIO);
    
     
    const u16 *buf = (const u16*)buffer;
    for (int s = 0; s < count; s++) {
        if (ata_wait_drq() < 0) return -1;
        
        for (int i = 0; i < 256; i++) {
            __asm__ volatile ("outw %0, %1" : : "a"(buf[i]), "Nd"((u16)ATA_PRIMARY_DATA));
        }
        buf += 256;
    }
    
     
    outb(ATA_PRIMARY_COMMAND, 0xE7);
    ata_wait_ready();
    
    return count;
}

bool ata_is_present(void) {
    return drive_present;
}
