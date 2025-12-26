/**
 * Block Device Abstraction Layer Implementation
 * 
 * Provides unified block-level I/O operations for filesystem drivers
 */

#include "blockdev.h"
#include "../errno.h"
#include "../drivers/ata.h"

#define MAX_BLOCKDEVS 8
static blockdev_t devices[MAX_BLOCKDEVS];
static u32 num_devices = 0;

/**
 * ATA block device operations
 */
static int ata_read_blocks(u32 dev_id, u32 block_num, u32 num_blocks, void *buffer) {
    (void)dev_id; // Primary ATA device
    
    if (!ata_is_present()) {
        return E_ATA_NO_DEV;
    }
    
    // Get block size from device
    blockdev_t *dev = blockdev_get(dev_id);
    if (!dev) {
        return E_ATA_NO_DEV;
    }
    
    u32 block_size = dev->block_size;
    if (block_size == 0) {
        block_size = 1024; // Default
    }
    
    // Convert block number to LBA (assuming 512-byte sectors)
    // block_size bytes = block_size / 512 sectors
    u32 sectors_per_block = block_size / 512;
    u32 lba = block_num * sectors_per_block;
    u8 sector_count = num_blocks * sectors_per_block;
    
    if (ata_read_sectors(lba, sector_count, buffer) < 0) {
        return E_ATA_READ_ERR;
    }
    
    return E_OK;
}

static int ata_write_blocks(u32 dev_id, u32 block_num, u32 num_blocks, const void *buffer) {
    (void)dev_id;
    
    if (!ata_is_present()) {
        return E_ATA_NO_DEV;
    }
    
    // Get block size from device
    blockdev_t *dev = blockdev_get(dev_id);
    if (!dev) {
        return E_ATA_NO_DEV;
    }
    
    u32 block_size = dev->block_size;
    if (block_size == 0) {
        block_size = 1024; // Default
    }
    
    u32 sectors_per_block = block_size / 512;
    u32 lba = block_num * sectors_per_block;
    u8 sector_count = num_blocks * sectors_per_block;
    
    if (ata_write_sectors(lba, sector_count, buffer) < 0) {
        return E_ATA_WRITE_ERR;
    }
    
    return E_OK;
}

static u32 ata_get_block_size(u32 dev_id) {
    (void)dev_id;
    return 1024; // Standard block size for filesystems
}

static u64 ata_get_block_count(u32 dev_id) {
    (void)dev_id;
    // For now, assume a reasonable default
    // In a real implementation, this would query the device
    return 65536; // 64MB disk / 1024 bytes per block
}

static bool ata_is_ready(u32 dev_id) {
    (void)dev_id;
    return ata_is_present();
}

static const blockdev_ops_t ata_ops = {
    .read_blocks = ata_read_blocks,
    .write_blocks = ata_write_blocks,
    .get_block_size = ata_get_block_size,
    .get_block_count = ata_get_block_count,
    .is_ready = ata_is_ready
};

void blockdev_init(void) {
    // Initialize ATA and register primary device
    if (ata_init() == 0) {
        blockdev_t *dev = &devices[0];
        dev->dev_id = BLOCKDEV_PRIMARY;
        dev->block_size = 1024;
        dev->block_count = ata_get_block_count(BLOCKDEV_PRIMARY);
        dev->ops = &ata_ops;
        dev->private_data = NULL;
        dev->initialized = true;
        num_devices = 1;
    }
}

int blockdev_register(blockdev_t *dev) {
    if (num_devices >= MAX_BLOCKDEVS) {
        return E_BUSY;
    }
    
    if (!dev || !dev->ops) {
        return E_INVALID_ARG;
    }
    
    // Check if device ID already exists
    for (u32 i = 0; i < num_devices; i++) {
        if (devices[i].dev_id == dev->dev_id) {
            return E_EXISTS;
        }
    }
    
    devices[num_devices] = *dev;
    num_devices++;
    
    return E_OK;
}

int blockdev_unregister(u32 dev_id) {
    for (u32 i = 0; i < num_devices; i++) {
        if (devices[i].dev_id == dev_id) {
            // Shift remaining devices
            for (u32 j = i; j < num_devices - 1; j++) {
                devices[j] = devices[j + 1];
            }
            num_devices--;
            return E_OK;
        }
    }
    return E_NOT_FOUND;
}

blockdev_t *blockdev_get(u32 dev_id) {
    for (u32 i = 0; i < num_devices; i++) {
        if (devices[i].dev_id == dev_id) {
            return &devices[i];
        }
    }
    return NULL;
}

int blockdev_read(u32 dev_id, u32 block_num, u32 num_blocks, void *buffer) {
    blockdev_t *dev = blockdev_get(dev_id);
    if (!dev || !dev->initialized || !dev->ops || !dev->ops->read_blocks) {
        return E_INVALID_ARG;
    }
    
    return dev->ops->read_blocks(dev_id, block_num, num_blocks, buffer);
}

int blockdev_write(u32 dev_id, u32 block_num, u32 num_blocks, const void *buffer) {
    blockdev_t *dev = blockdev_get(dev_id);
    if (!dev || !dev->initialized || !dev->ops || !dev->ops->write_blocks) {
        return E_INVALID_ARG;
    }
    
    return dev->ops->write_blocks(dev_id, block_num, num_blocks, buffer);
}

u32 blockdev_get_block_size(u32 dev_id) {
    blockdev_t *dev = blockdev_get(dev_id);
    if (!dev || !dev->initialized || !dev->ops || !dev->ops->get_block_size) {
        return 0;
    }
    
    return dev->ops->get_block_size(dev_id);
}

u64 blockdev_get_block_count(u32 dev_id) {
    blockdev_t *dev = blockdev_get(dev_id);
    if (!dev || !dev->initialized || !dev->ops || !dev->ops->get_block_count) {
        return 0;
    }
    
    return dev->ops->get_block_count(dev_id);
}

