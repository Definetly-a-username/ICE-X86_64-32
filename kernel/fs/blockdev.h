/**
 * Block Device Abstraction Layer
 * 
 * Provides a unified interface for block-level I/O operations,
 * abstracting away the underlying hardware (ATA, IDE, etc.)
 * 
 * This layer enables filesystem code to work with any block device
 * without being tied to specific hardware implementations.
 */

#ifndef ICE_BLOCKDEV_H
#define ICE_BLOCKDEV_H

#include "../types.h"

/**
 * Block device operations structure
 * Each block device driver implements these operations
 */
typedef struct blockdev_ops {
    /**
     * Read one or more blocks from the device
     * @param dev_id Device identifier
     * @param block_num Starting block number (0-based)
     * @param num_blocks Number of blocks to read
     * @param buffer Destination buffer (must be at least num_blocks * block_size)
     * @return 0 on success, negative error code on failure
     */
    int (*read_blocks)(u32 dev_id, u32 block_num, u32 num_blocks, void *buffer);
    
    /**
     * Write one or more blocks to the device
     * @param dev_id Device identifier
     * @param block_num Starting block number (0-based)
     * @param num_blocks Number of blocks to write
     * @param buffer Source buffer (must contain num_blocks * block_size bytes)
     * @return 0 on success, negative error code on failure
     */
    int (*write_blocks)(u32 dev_id, u32 block_num, u32 num_blocks, const void *buffer);
    
    /**
     * Get block size for this device
     * @param dev_id Device identifier
     * @return Block size in bytes, or 0 on error
     */
    u32 (*get_block_size)(u32 dev_id);
    
    /**
     * Get total number of blocks on this device
     * @param dev_id Device identifier
     * @return Total blocks, or 0 on error
     */
    u64 (*get_block_count)(u32 dev_id);
    
    /**
     * Check if device is ready
     * @param dev_id Device identifier
     * @return true if ready, false otherwise
     */
    bool (*is_ready)(u32 dev_id);
} blockdev_ops_t;

/**
 * Block device structure
 */
typedef struct blockdev {
    u32 dev_id;                    ///< Device identifier
    u32 block_size;                ///< Block size in bytes
    u64 block_count;               ///< Total number of blocks
    const blockdev_ops_t *ops;     ///< Device operations
    void *private_data;            ///< Driver-specific data
    bool initialized;              ///< Initialization status
} blockdev_t;

/**
 * Register a block device
 * @param dev Device structure to register
 * @return 0 on success, negative error code on failure
 */
int blockdev_register(blockdev_t *dev);

/**
 * Unregister a block device
 * @param dev_id Device identifier
 * @return 0 on success, negative error code on failure
 */
int blockdev_unregister(u32 dev_id);

/**
 * Get block device by ID
 * @param dev_id Device identifier
 * @return Pointer to device structure, or NULL if not found
 */
blockdev_t *blockdev_get(u32 dev_id);

/**
 * Read blocks from a registered device
 * @param dev_id Device identifier
 * @param block_num Starting block number
 * @param num_blocks Number of blocks to read
 * @param buffer Destination buffer
 * @return 0 on success, negative error code on failure
 */
int blockdev_read(u32 dev_id, u32 block_num, u32 num_blocks, void *buffer);

/**
 * Write blocks to a registered device
 * @param dev_id Device identifier
 * @param block_num Starting block number
 * @param num_blocks Number of blocks to write
 * @param buffer Source buffer
 * @return 0 on success, negative error code on failure
 */
int blockdev_write(u32 dev_id, u32 block_num, u32 num_blocks, const void *buffer);

/**
 * Get block size for a device
 * @param dev_id Device identifier
 * @return Block size in bytes, or 0 on error
 */
u32 blockdev_get_block_size(u32 dev_id);

/**
 * Get total block count for a device
 * @param dev_id Device identifier
 * @return Total blocks, or 0 on error
 */
u64 blockdev_get_block_count(u32 dev_id);

/**
 * Initialize the block device subsystem
 * This should be called during kernel initialization
 */
void blockdev_init(void);

/**
 * Default device ID for the primary disk
 */
#define BLOCKDEV_PRIMARY 0

#endif // ICE_BLOCKDEV_H

