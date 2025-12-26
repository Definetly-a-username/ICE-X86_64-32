 

#ifndef ICE_ATA_H
#define ICE_ATA_H

#include "../types.h"

 
#define ATA_PRIMARY_DATA        0x1F0
#define ATA_PRIMARY_ERROR       0x1F1
#define ATA_PRIMARY_SECCOUNT    0x1F2
#define ATA_PRIMARY_LBA_LO      0x1F3
#define ATA_PRIMARY_LBA_MID     0x1F4
#define ATA_PRIMARY_LBA_HI      0x1F5
#define ATA_PRIMARY_DRIVE       0x1F6
#define ATA_PRIMARY_STATUS      0x1F7
#define ATA_PRIMARY_COMMAND     0x1F7
#define ATA_PRIMARY_CONTROL     0x3F6

 
#define ATA_CMD_READ_PIO        0x20
#define ATA_CMD_WRITE_PIO       0x30
#define ATA_CMD_IDENTIFY        0xEC

 
#define ATA_STATUS_BSY          0x80
#define ATA_STATUS_DRDY         0x40
#define ATA_STATUS_DRQ          0x08
#define ATA_STATUS_DF           0x20
#define ATA_STATUS_ERR          0x01

 
#define ATA_SECTOR_SIZE         512

 
int ata_init(void);

 
int ata_read_sectors(u32 lba, u8 count, void *buffer);

 
int ata_write_sectors(u32 lba, u8 count, const void *buffer);

 
bool ata_is_present(void);

#endif  
