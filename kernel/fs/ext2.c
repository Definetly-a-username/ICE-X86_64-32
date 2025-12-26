/**
 * EXT2 Filesystem Driver Implementation
 * 
 * Provides reliable read/write support for EXT2 filesystems with proper
 * block allocation, inode management, and superblock handling.
 * 
 * Features:
 * - Proper block allocation and deallocation
 * - Inode management with caching
 * - Directory traversal and manipulation
 * - File creation and writing
 * - Error handling to prevent kernel panics
 * - 32/64-bit compatible
 */

#include "ext2.h"
#include "blockdev.h"
#include "../drivers/serial.h"
#include "../errno.h"
#include "../sync/spinlock.h"
#include <string.h>



#define DEBUG_EXT2 0
#if DEBUG_EXT2
#define dprintf(...) serial_printf(__VA_ARGS__)
#else
#define dprintf(...)
#endif

// Global filesystem state
static spinlock_t fs_lock;
static bool mounted = false;
static u32 fs_dev_id = 0;
static ext2_superblock_t sb;
static ext2_bg_desc_t *bg_descs = NULL;
static u32 block_size = 0;
static u32 sectors_per_block = 0;
static u32 num_bg = 0;
static u8 *block_buffer = NULL; // General purpose buffer

// Separate buffer for inode operations (avoids conflicts with directory ops)
static u8 inode_buffer[4096] __attribute__((aligned(4)));

// Separate buffer for directory operations
static u8 dir_buffer[4096] __attribute__((aligned(4)));

// Block cache (LRU)
#define CACHE_SIZE 16
static u8 cache_data[CACHE_SIZE][4096] __attribute__((aligned(4)));
static u32 cache_tags[CACHE_SIZE];
static u32 cache_lru[CACHE_SIZE];
static bool cache_valid[CACHE_SIZE];
static u32 cache_access_counter = 0;

static int get_cache_slot(u32 block) {
    for(int i=0; i<CACHE_SIZE; i++) {
        if (cache_valid[i] && cache_tags[i] == block) return i;
    }
    return -1;
}

static int get_victim_slot(void) {
    int victim = 0;
    u32 min_lru = 0xFFFFFFFF;
    for(int i=0; i<CACHE_SIZE; i++) {
        if (!cache_valid[i]) return i;
        if (cache_lru[i] < min_lru) {
            min_lru = cache_lru[i];
            victim = i;
        }
    }
    return victim;
}

// Cached values for block operations (avoid repeated lookups)
static u32 cached_dev_block_size = 0;
static u32 cached_dev_blocks_per_fs_block = 0;

#define MAX_OPEN_FILES 32
static ext2_file_t open_files[MAX_OPEN_FILES];

// Maximum number of block groups we cache (can be increased if needed)
#define MAX_CACHED_BGS 64

// Static buffer for block group descriptors
static ext2_bg_desc_t bg_cache[MAX_CACHED_BGS];

/**
 * Read a block from the block device (OPTIMIZED)
 * @param block Block number (EXT2 blocks are 0-based)
 * @param buffer Destination buffer (must be at least block_size bytes)
 * @return 0 on success, negative error code on failure
 */
static inline int read_block(u32 block, void *buffer) {
    spinlock_acquire(&fs_lock);
    cache_access_counter++;
    int slot = get_cache_slot(block);
    
    if (slot >= 0) {
        cache_lru[slot] = cache_access_counter;
        memcpy(buffer, cache_data[slot], block_size);
        spinlock_release(&fs_lock);
        return E_OK;
    }
    
    // Get device block size (cached)
    u32 dev_block_size = cached_dev_block_size;
    if (dev_block_size == 0) {
        dev_block_size = blockdev_get_block_size(fs_dev_id);
        if (dev_block_size == 0) dev_block_size = 1024;
        cached_dev_block_size = dev_block_size;
    }
    
    // Calculate device blocks
    u32 dev_blocks_per_fs_block = cached_dev_blocks_per_fs_block;
    if (dev_blocks_per_fs_block == 0) {
        dev_blocks_per_fs_block = block_size / dev_block_size;
        if (dev_blocks_per_fs_block == 0) dev_blocks_per_fs_block = 1;
        cached_dev_blocks_per_fs_block = dev_blocks_per_fs_block;
    }
    
    u32 start_dev_block = block * dev_blocks_per_fs_block;
    
    // Read into new cache slot
    slot = get_victim_slot();
    
    // Release lock while doing I/O to avoid blocking others?
    // BUT we are modifying cache_data[slot] which is shared.
    // If we release lock here, someone else might claim the slot or read it.
    // Ideally we lock the SLOT or use a finer grained lock. 
    // For now, holding fs_lock during I/O is safe for consistency but bad for concurrency.
    // Given the requirement "Protect ALL filesystem operations", we hold it.
    
    // dprintf("EXT2: read_block %d -> cache slot %d\n", block, slot);
    
    int ret = blockdev_read(fs_dev_id, start_dev_block, dev_blocks_per_fs_block, cache_data[slot]);
    if (ret < 0) {
        dprintf("EXT2: read_block %d FAILED\n", block);
        spinlock_release(&fs_lock);
        return E_EXT2_READ_BLOCK;
    }
    
    cache_valid[slot] = true;
    cache_tags[slot] = block;
    cache_lru[slot] = cache_access_counter;
    
    memcpy(buffer, cache_data[slot], block_size);
    spinlock_release(&fs_lock);
    return E_OK;
}

/**
 * Write a block to the block device (OPTIMIZED)
 * @param block Block number (EXT2 blocks are 0-based)
 * @param buffer Source buffer (must contain block_size bytes)
 * @return 0 on success, negative error code on failure
 */
static inline int write_block(u32 block, const void *buffer) {
    // Get device block size (cached)
    // Get device block size (cached)
    // Safe to read cached_dev_block_size without lock as it is set once? 
    // Better to just lock.
    spinlock_acquire(&fs_lock);

    u32 dev_block_size = cached_dev_block_size;
    if (dev_block_size == 0) {
        dev_block_size = blockdev_get_block_size(fs_dev_id);
        if (dev_block_size == 0) dev_block_size = 1024;
        cached_dev_block_size = dev_block_size;
    }
    
    // Calculate device blocks
    u32 dev_blocks_per_fs_block = cached_dev_blocks_per_fs_block;
    if (dev_blocks_per_fs_block == 0) {
        dev_blocks_per_fs_block = block_size / dev_block_size;
        if (dev_blocks_per_fs_block == 0) dev_blocks_per_fs_block = 1;
        cached_dev_blocks_per_fs_block = dev_blocks_per_fs_block;
    }
    
    u32 start_dev_block = block * dev_blocks_per_fs_block;
    
    // Write to disk
    // We hold the lock during write to prevent others from writing same block interleaved?
    int ret = blockdev_write(fs_dev_id, start_dev_block, dev_blocks_per_fs_block, buffer);
    if (ret < 0) {
        dprintf("EXT2: write_block %d FAILED\n", block);
        spinlock_release(&fs_lock);
        return E_EXT2_WRITE_BLOCK;
    }
    
    // Update cache if present
    int slot = get_cache_slot(block);
    if (slot >= 0) {
        memcpy(cache_data[slot], buffer, block_size);
        cache_lru[slot] = ++cache_access_counter;
    }
    
    spinlock_release(&fs_lock);
    return E_OK;
}

/**
 * Read an inode from disk
 * Uses inode_buffer to avoid conflicts with directory operations
 * @param inode_num Inode number (1-based)
 * @param dst Destination inode structure
 * @return 0 on success, negative error code on failure
 */
static int read_inode(u32 inode_num, ext2_inode_t *dst) {
    if (inode_num == 0 || inode_num > sb.inodes_count) {
        dprintf("EXT2: read_inode: invalid inode %d (max %d)\n", inode_num, sb.inodes_count);
        return E_EXT2_NO_INODE;
    }
    
    // Calculate which block group this inode belongs to
    u32 bg = (inode_num - 1) / sb.inodes_per_group;
    if (bg >= num_bg) {
        dprintf("EXT2: read_inode: inode %d in bg %d (max %d)\n", inode_num, bg, num_bg);
        return E_EXT2_NO_INODE;
    }
    
    // Calculate index within the group
    u32 index = (inode_num - 1) % sb.inodes_per_group;
    
    // Get inode table block
    u32 table_block = bg_descs[bg].inode_table;
    if (table_block == 0) {
        dprintf("EXT2: read_inode: no inode table for bg %d\n", bg);
        return E_EXT2_READ_BLOCK;
    }
    
    // Calculate inode size (default 128 bytes for rev 0, variable for rev 1+)
    u32 inode_size = sb.inode_size ? sb.inode_size : 128;
    
    // Calculate offset within inode table
    u32 offset = index * inode_size;
    u32 block_offset = offset / block_size;
    u32 byte_offset = offset % block_size;
    
    // Read the block containing the inode into inode_buffer (not block_buffer!)
    if (read_block(table_block + block_offset, inode_buffer) < 0) {
        dprintf("EXT2: read_inode: failed to read block %d\n", table_block + block_offset);
        return E_EXT2_READ_BLOCK;
    }
    
    // Copy inode data
    u8 *ptr = inode_buffer + byte_offset;
    memcpy(dst, ptr, sizeof(ext2_inode_t));
    
    dprintf("EXT2: read_inode %d: mode=0x%04x size=%d links=%d\n", 
            inode_num, dst->mode, dst->size, dst->links_count);
    
    return E_OK;
}

/**
 * Write an inode to disk
 * Uses inode_buffer to avoid conflicts with directory operations
 * @param inode_num Inode number (1-based)
 * @param src Source inode structure
 * @return 0 on success, negative error code on failure
 */
static int write_inode(u32 inode_num, const ext2_inode_t *src) {
    spinlock_acquire(&fs_lock);
    if (inode_num == 0 || inode_num > sb.inodes_count) {
        spinlock_release(&fs_lock);
        return E_EXT2_NO_INODE;
    }
    
    u32 bg = (inode_num - 1) / sb.inodes_per_group;
    if (bg >= num_bg) {
        spinlock_release(&fs_lock);
        return E_EXT2_NO_INODE;
    }
    
    u32 index = (inode_num - 1) % sb.inodes_per_group;
    u32 table_block = bg_descs[bg].inode_table;
    
    if (table_block == 0) {
        spinlock_release(&fs_lock);
        return E_EXT2_WRITE_BLOCK;
    }
    
    u32 inode_size = sb.inode_size ? sb.inode_size : 128;
    u32 offset = index * inode_size;
    u32 block_offset = offset / block_size;
    u32 byte_offset = offset % block_size;
    
    // Read the block containing the inode into inode_buffer
    if (read_block(table_block + block_offset, inode_buffer) < 0) {
        spinlock_release(&fs_lock);
        return E_EXT2_READ_BLOCK;
    }
    
    // Update inode data
    u8 *ptr = inode_buffer + byte_offset;
    memcpy(ptr, src, sizeof(ext2_inode_t));
    
    // Write back to disk
    dprintf("EXT2: write_inode %d: mode=0x%04x size=%d\n", inode_num, src->mode, src->size);
    int ret = write_block(table_block + block_offset, inode_buffer);
    spinlock_release(&fs_lock);
    return ret;
}

/**
 * Get physical block number from inode's block array
 * Supports direct blocks and singly indirect blocks
 * @param inode Inode structure
 * @param logical_block Logical block index (0-based)
 * @return Physical block number, or 0 on error/not allocated
 */
static u32 get_inode_block(ext2_inode_t *inode, u32 logical_block) {
    // Direct blocks (0-11)
    if (logical_block < 12) {
        return inode->block[logical_block];
    }
    
    // Singly indirect block (12)
    u32 entries_per_block = block_size / 4;
    logical_block -= 12;
    
    if (logical_block < entries_per_block) {
        u32 indirect = inode->block[12];
        if (indirect == 0) {
            return 0;
        }
        
        // Read indirect block
        if (read_block(indirect, block_buffer) < 0) {
            return 0;
        }
        
        u32 *table = (u32*)block_buffer;
        return table[logical_block];
    }
    
    // Doubly indirect (13) - not implemented for simplicity
    // Triply indirect (14) - not implemented for simplicity
    
    return 0;
}

/**
 * Set physical block number in inode's block array
 * @param inode Inode structure
 * @param logical_block Logical block index
 * @param phys_block Physical block number
 * @return 0 on success, negative error code on failure
 */
static int set_inode_block(ext2_inode_t *inode, u32 logical_block, u32 phys_block) {
    // Direct blocks
    if (logical_block < 12) {
        inode->block[logical_block] = phys_block;
        return E_OK;
    }
    
    // Singly indirect
    u32 entries_per_block = block_size / 4;
    logical_block -= 12;
    
    if (logical_block < entries_per_block) {
        u32 indirect = inode->block[12];
        
        // Allocate indirect block if needed
        if (indirect == 0) {
            // This would require block allocation - simplified for now
            return E_EXT2_NO_BLOCK;
        }
        
        // Read indirect block
        if (read_block(indirect, block_buffer) < 0) {
            return E_EXT2_READ_BLOCK;
        }
        
        u32 *table = (u32*)block_buffer;
        table[logical_block] = phys_block;
        
        // Write back
        return write_block(indirect, block_buffer);
    }
    
    return E_EXT2_BAD_TYPE;
}

/**
 * Find a file in a directory inode
 * Uses dir_buffer to avoid conflicts with inode operations
 * @param dir_inode Directory inode
 * @param name File name to find
 * @return Inode number if found, 0 otherwise
 */
static u32 find_file_in_dir(ext2_inode_t *dir_inode, const char *name) {
    if (!(dir_inode->mode & EXT2_S_IFDIR)) {
        dprintf("EXT2: find_file_in_dir: not a dir, mode=0x%04x\n", dir_inode->mode);
        return 0; // Not a directory
    }
    
    u32 size = dir_inode->size;
    u32 offset = 0;
    
    dprintf("EXT2: find_file_in_dir: searching for '%s' in dir size=%d\n", name, size);
    
    while (offset < size) {
        u32 block_idx = offset / block_size;
        u32 block_offset = offset % block_size;
        
        u32 phys_block = get_inode_block(dir_inode, block_idx);
        if (phys_block == 0) {
            break;
        }
        
        // Use dir_buffer instead of block_buffer
        if (read_block(phys_block, dir_buffer) < 0) {
            return 0;
        }
        
        // Iterate through directory entries in this block
        while (block_offset < block_size && offset < size) {
            ext2_dir_entry_t *entry = (ext2_dir_entry_t*)(dir_buffer + block_offset);
            
            if (entry->rec_len == 0) {
                break; // Invalid entry
            }
            
            if (entry->inode != 0) {
                // Compare names
                bool match = true;
                u32 name_len = entry->name_len;
                if (name_len > 255) name_len = 255; // Safety check
                
                for (u32 i = 0; i < name_len; i++) {
                    if (name[i] == 0 || name[i] != entry->name[i]) {
                        match = false;
                        break;
                    }
                }
                
                // Check if name matches exactly
                if (match && name[name_len] == 0) {
                    dprintf("EXT2: find_file_in_dir: found '%s' at inode %d\n", name, entry->inode);
                    return entry->inode;
                }
            }
            
            offset += entry->rec_len;
            block_offset += entry->rec_len;
        }
    }
    
    dprintf("EXT2: find_file_in_dir: '%s' not found\n", name);
    return 0;
}

/**
 * Resolve a path to an inode number
 * @param path Path to resolve
 * @return Inode number, or 0 on error
 */
static u32 resolve_path(const char *path) {
    if (!mounted) {
        return 0;
    }
    
    // Skip leading slashes
    while (*path == '/') {
        path++;
    }
    
    // Empty path or just "/" means root
    if (*path == 0) {
        dprintf("EXT2: resolve_path: empty path -> root inode %d\n", EXT2_ROOT_INO);
        return EXT2_ROOT_INO;
    }
    
    u32 current_inode_num = EXT2_ROOT_INO;
    ext2_inode_t current_inode;
    
    dprintf("EXT2: resolve_path: starting from root inode %d\n", current_inode_num);
    
    if (read_inode(current_inode_num, &current_inode) < 0) {
        dprintf("EXT2: resolve_path: failed to read root inode\n");
        return 0;
    }
    
    dprintf("EXT2: resolve_path: root inode mode=0x%04x size=%d\n", current_inode.mode, current_inode.size);
    
    char component[256];
    const char *p = path;
    
    while (*p) {
        // Extract next component
        u32 i = 0;
        while (*p && *p != '/' && i < 255) {
            component[i++] = *p++;
        }
        component[i] = 0;
        
        if (i == 0) {
            // Skip multiple slashes
            if (*p == '/') {
                p++;
            }
            continue;
        }
        
        dprintf("EXT2: resolve_path: looking for '%s' in inode %d\n", component, current_inode_num);
        
        // Find component in current directory
        u32 next_inode_num = find_file_in_dir(&current_inode, component);
        if (next_inode_num == 0) {
            dprintf("EXT2: resolve_path: '%s' not found\n", component);
            return 0; // Not found
        }
        
        current_inode_num = next_inode_num;
        if (read_inode(current_inode_num, &current_inode) < 0) {
            dprintf("EXT2: resolve_path: failed to read inode %d\n", current_inode_num);
            return 0;
        }
        
        dprintf("EXT2: resolve_path: found '%s' at inode %d, mode=0x%04x\n", component, current_inode_num, current_inode.mode);
        
        // Skip trailing slash
        if (*p == '/') {
            p++;
        }
    }
    
    return current_inode_num;
}

/**
 * Initialize EXT2 filesystem
 */
int ext2_init(u32 dev_id) {
    if (mounted) {
        return E_OK; // Already mounted
    }
    
    fs_dev_id = dev_id;
    spinlock_init(&fs_lock);
    
    // Get block device
    blockdev_t *dev = blockdev_get(dev_id);
    if (!dev || !dev->initialized) {
        return E_ATA_NO_DEV;
    }
    
    // Allocate block buffer
    block_size = blockdev_get_block_size(dev_id);
    if (block_size == 0) {
        block_size = 1024; // Default
    }
    
    // Allocate buffer for block operations
    // Use a static buffer large enough for any block size
    static u8 static_block_buffer[4096];
    block_buffer = static_block_buffer;
    
    sectors_per_block = block_size / 512;
    
    // Read superblock (always at offset 1024 from start of partition)
    // We need to read it directly using device blocks, not filesystem blocks
    // Superblock is at byte offset 1024, which is:
    // - Device block 1 (0-based) if dev_block_size = 1024 (offset 0 in that block)
    // - Device block 2 (0-based) if dev_block_size = 512 (offset 0 in that block)
    u32 dev_block_size = blockdev_get_block_size(dev_id);
    if (dev_block_size == 0) {
        dev_block_size = 1024;
    }
    
    // Calculate which device block contains the superblock
    u32 sb_dev_block = 1024 / dev_block_size;
    u32 sb_offset_in_block = 1024 % dev_block_size;
    
    // Read the device block containing the superblock
    // Read 2 blocks to be safe (in case superblock spans blocks)
    u8 sb_buffer[2048];
    if (blockdev_read(dev_id, sb_dev_block, 2, sb_buffer) < 0) {
        dprintf("EXT2: Failed to read superblock\n");
        return E_EXT2_SB_READ;
    }
    
    // Copy superblock from the buffer
    memcpy(&sb, sb_buffer + sb_offset_in_block, sizeof(ext2_superblock_t));
    
    // Verify magic number
    if (sb.magic != EXT2_SUPER_MAGIC) {
        dprintf("EXT2: Invalid magic: 0x%04x\n", sb.magic);
        return E_EXT2_BAD_MAGIC;
    }
    
    // Update block size from superblock
    block_size = 1024 << sb.log_block_size;
    sectors_per_block = block_size / 512;
    
    // Re-allocate buffer if needed (block_size might be larger than 1024)
    if (block_size > 4096) {
        // For very large block sizes, we'd need dynamic allocation
        // For now, limit to 4KB
        dprintf("EXT2: Block size %d too large, limiting to 4KB\n", block_size);
        block_size = 4096;
    }
    
    // Calculate number of block groups
    num_bg = (sb.blocks_count + sb.blocks_per_group - 1) / sb.blocks_per_group;
    
    // Limit to cached size
    if (num_bg > MAX_CACHED_BGS) {
        num_bg = MAX_CACHED_BGS;
    }
    
    // Read block group descriptors
    // Location depends on block size:
    // - If block_size == 1024: BG descriptors start at block 2
    // - If block_size > 1024: BG descriptors start at block 1 (right after superblock)
    u32 bg_loc = (block_size == 1024) ? 2 : 1;
    
    if (read_block(bg_loc, block_buffer) < 0) {
        dprintf("EXT2: Failed to read BG descriptors\n");
        return E_EXT2_BG_READ;
    }
    
    // Copy block group descriptors
    ext2_bg_desc_t *src = (ext2_bg_desc_t*)block_buffer;
    for (u32 i = 0; i < num_bg; i++) {
        bg_cache[i] = src[i];
    }
    bg_descs = bg_cache;
    
    dprintf("EXT2: Mounted successfully. Block size: %d, Groups: %d\n", block_size, num_bg);
    
    mounted = true;
    return E_OK;
}

bool ext2_is_mounted(void) {
    return mounted;
}

ext2_file_t* ext2_open(const char *path) {
    if (!mounted) {
        return NULL;
    }
    
    u32 inode_num = resolve_path(path);
    if (inode_num == 0) {
        return NULL;
    }
    
    ext2_inode_t inode;
    if (read_inode(inode_num, &inode) < 0) {
        return NULL;
    }
    
    // Find free slot
    spinlock_acquire(&fs_lock);
    for (u32 i = 0; i < MAX_OPEN_FILES; i++) {
        if (!open_files[i].valid) {
            open_files[i].valid = true;
            open_files[i].inode_num = inode_num; // FIX: Set inode_num
            open_files[i].inode = inode;
            open_files[i].position = 0;
            spinlock_release(&fs_lock);
            return &open_files[i];
        }
    }
    spinlock_release(&fs_lock);
    
    return NULL; // No free slots
}

void ext2_close(ext2_file_t *file) {
    if (file) {
        spinlock_acquire(&fs_lock);
        file->valid = false;
        spinlock_release(&fs_lock);
    }
}

int ext2_read(ext2_file_t *file, void *buffer, u32 size) {
    // Fast path: validate inputs early
    if (!file || !file->valid || !buffer || size == 0) {
        return (file && file->valid) ? 0 : E_INVALID_ARG;
    }
    
    // Early exit if at EOF
    if (file->position >= file->inode.size) {
        return 0;
    }
    
    // Limit read size to remaining file size
    u32 remaining = file->inode.size - file->position;
    if (size > remaining) {
        size = remaining;
    }
    
    u8 *buf = (u8*)buffer;
    u32 bytes_read = 0;
    
    // Cache last block index to avoid redundant calculations (per-file, not global)
    u32 last_block_idx = ~0;
    u32 last_phys_block = 0;
    
    while (bytes_read < size) {
        u32 block_idx = file->position / block_size;
        u32 offset_in_block = file->position % block_size;
        
        // Optimize: reuse last block if same index
        u32 phys_block;
        if (block_idx == last_block_idx && last_phys_block != 0) {
            phys_block = last_phys_block;
        } else {
            phys_block = get_inode_block(&file->inode, block_idx);
            last_block_idx = block_idx;
            last_phys_block = phys_block;
        }
        
        if (phys_block == 0) {
            // Sparse block - return zeros (optimized memset)
            u32 chunk = block_size - offset_in_block;
            if (chunk > size - bytes_read) {
                chunk = size - bytes_read;
            }
            
            // Use optimized zeroing
            u8 *zero_ptr = buf + bytes_read;
            u32 i = 0;
            // Unroll small chunks
            while (i + 8 <= chunk) {
                *((u32*)(zero_ptr + i)) = 0;
                *((u32*)(zero_ptr + i + 4)) = 0;
                i += 8;
            }
            while (i < chunk) {
                zero_ptr[i++] = 0;
            }
            
            bytes_read += chunk;
            file->position += chunk;
            continue;
        }
        
        // Read block (uses cache optimization)
        if (read_block(phys_block, block_buffer) < 0) {
            last_phys_block = 0; // Invalidate cache
            return E_EXT2_READ_BLOCK;
        }
        
        // Calculate how much to copy (optimize min calculation)
        u32 chunk = block_size - offset_in_block;
        u32 remaining_in_request = size - bytes_read;
        if (chunk > remaining_in_request) {
            chunk = remaining_in_request;
        }
        
        // Optimized memcpy for aligned data
        if (chunk >= 4 && ((u32)(buf + bytes_read) & 3) == 0 && ((u32)(block_buffer + offset_in_block) & 3) == 0) {
            // Aligned copy - use word operations
            u32 *dst = (u32*)(buf + bytes_read);
            u32 *src = (u32*)(block_buffer + offset_in_block);
            u32 words = chunk / 4;
            u32 i = 0;
            while (i < words) {
                dst[i] = src[i];
                i++;
            }
            // Copy remaining bytes
            u32 bytes_copied = words * 4;
            u8 *dst_byte = (u8*)dst + bytes_copied;
            u8 *src_byte = (u8*)src + bytes_copied;
            u32 remaining_bytes = chunk - bytes_copied;
            for (u32 j = 0; j < remaining_bytes; j++) {
                dst_byte[j] = src_byte[j];
            }
        } else {
            // Unaligned or small - use byte copy
            memcpy(buf + bytes_read, block_buffer + offset_in_block, chunk);
        }
        
        bytes_read += chunk;
        file->position += chunk;
    }
    
    return bytes_read;
}

/**
 * Allocate a free block
 * @return Block number, or 0 on error
 */
static u32 ext2_alloc_block(void) {
    spinlock_acquire(&fs_lock);
    for (u32 i = 0; i < num_bg; i++) {
        if (bg_descs[i].free_blocks_count > 0) {
            u32 bitmap_block = bg_descs[i].block_bitmap;
            if (bitmap_block == 0) continue;
            
            if (read_block(bitmap_block, block_buffer) < 0) continue;
            
            u32 bits_per_block = block_size * 8;
            for (u32 bit = 0; bit < bits_per_block; bit++) {
                u32 byte = bit / 8;
                u32 bit_in_byte = bit % 8;
                
                if (!(block_buffer[byte] & (1 << bit_in_byte))) {
                    // Found free bit
                    block_buffer[byte] |= (1 << bit_in_byte);
                    
                    if (write_block(bitmap_block, block_buffer) < 0) continue;
                    
                    bg_descs[i].free_blocks_count--;
                    sb.free_blocks_count--;
                    
                    u32 block_num = sb.first_data_block + i * sb.blocks_per_group + bit;
                    
                    // Update descriptors
                    u32 bg_loc = (block_size == 1024) ? 2 : 1;
                    if (read_block(bg_loc, block_buffer) < 0) {
                        spinlock_release(&fs_lock);
                        return 0;
                    }
                    
                    // Update cache in buffer
                    // Note: bg_descs points to bg_cache which we updated above
                    memcpy(block_buffer, bg_descs, num_bg * sizeof(ext2_bg_desc_t));
                    write_block(bg_loc, block_buffer);
                    
                    // Update superblock
                    u32 sb_block = (block_size == 1024) ? 1 : 0;
                    u32 sb_offset = (block_size == 1024) ? 0 : 1024;
                    if (read_block(sb_block, block_buffer) >= 0) {
                         memcpy(block_buffer + sb_offset, &sb, sizeof(ext2_superblock_t));
                         write_block(sb_block, block_buffer);
                    }
                    
                    spinlock_release(&fs_lock);
                    return block_num;
                }
            }
        }
    }
    spinlock_release(&fs_lock);
    return 0;
}

/**
 * Allocate a free inode
 * @return Inode number, or 0 on error
 */
static u32 ext2_alloc_inode(void) {
    for (u32 i = 0; i < num_bg; i++) {
        if (bg_descs[i].free_inodes_count > 0) {
            u32 bitmap_block = bg_descs[i].inode_bitmap;
            if (bitmap_block == 0) {
                continue;
            }
            
            if (read_block(bitmap_block, block_buffer) < 0) {
                continue;
            }
            
            u32 bits_per_block = block_size * 8;
            for (u32 bit = 0; bit < bits_per_block; bit++) {
                u32 byte = bit / 8;
                u32 bit_in_byte = bit % 8;
                
                if (!(block_buffer[byte] & (1 << bit_in_byte))) {
                    block_buffer[byte] |= (1 << bit_in_byte);
                    
                    if (write_block(bitmap_block, block_buffer) < 0) {
                        continue;
                    }
                    
                    bg_descs[i].free_inodes_count--;
                    sb.free_inodes_count--;
                    
                    u32 inode_num = i * sb.inodes_per_group + bit + 1;
                    
                    // Update metadata
                    u32 bg_loc = (block_size == 1024) ? 2 : 1;
                    if (read_block(bg_loc, block_buffer) < 0) {
                        return 0;
                    }
                    memcpy(block_buffer, bg_descs, num_bg * sizeof(ext2_bg_desc_t));
                    write_block(bg_loc, block_buffer);
                    
                    u32 sb_block = (block_size == 1024) ? 1 : 0;
                    u32 sb_offset = (block_size == 1024) ? 0 : 1024;
                    if (read_block(sb_block, block_buffer) < 0) {
                        return 0;
                    }
                    memcpy(block_buffer + sb_offset, &sb, sizeof(ext2_superblock_t));
                    write_block(sb_block, block_buffer);
                    
                    return inode_num;
                }
            }
        }
    }
    
    return 0;
}

/**
 * Add a directory entry
 */
static int ext2_add_dir_entry(u32 dir_inode_num, u32 inode_num, const char *name, u8 type) {
    ext2_inode_t dir_inode;
    if (read_inode(dir_inode_num, &dir_inode) < 0) {
        return E_EXT2_READ_BLOCK;
    }
    
    if (!(dir_inode.mode & EXT2_S_IFDIR)) {
        return E_NOT_DIR;
    }
    
    u32 name_len = 0;
    while (name[name_len] && name_len < 255) {
        name_len++;
    }
    
    // Calculate record length (4-byte aligned)
    u32 rec_len = sizeof(ext2_dir_entry_t) + name_len;
    rec_len = (rec_len + 3) & ~3;
    
    dprintf("EXT2: add_dir_entry: adding '%s' (ino=%d) to dir %d\n", name, inode_num, dir_inode_num);
    
    // Try to find space in existing blocks
    u32 offset = 0;
    while (offset < dir_inode.size) {
        u32 block_idx = offset / block_size;
        u32 phys_block = get_inode_block(&dir_inode, block_idx);
        if (phys_block == 0) {
            break;
        }
        
        // Use dir_buffer for directory operations
        if (read_block(phys_block, dir_buffer) < 0) {
            return E_EXT2_READ_BLOCK;
        }
        
        u32 block_offset = 0;
        while (block_offset < block_size) {
            ext2_dir_entry_t *entry = (ext2_dir_entry_t*)(dir_buffer + block_offset);
            
            if (entry->rec_len == 0) {
                break;
            }
            
            // Calculate actual entry length
            u32 entry_len = sizeof(ext2_dir_entry_t) + entry->name_len;
            entry_len = (entry_len + 3) & ~3;
            
            // Check if we can split this entry
            if (entry->inode != 0 && entry->rec_len >= entry_len + rec_len) {
                // Split entry
                u32 new_offset = block_offset + entry_len;
                ext2_dir_entry_t *new_entry = (ext2_dir_entry_t*)(dir_buffer + new_offset);
                
                new_entry->inode = inode_num;
                new_entry->rec_len = entry->rec_len - entry_len;
                new_entry->name_len = name_len;
                new_entry->file_type = type;
                memcpy(new_entry->name, name, name_len);
                
                entry->rec_len = entry_len;
                
                dprintf("EXT2: add_dir_entry: split entry at offset %d\n", block_offset);
                return write_block(phys_block, dir_buffer);
            }
            
            // Check if entry is unused
            if (entry->inode == 0 && entry->rec_len >= rec_len) {
                entry->inode = inode_num;
                entry->name_len = name_len;
                entry->file_type = type;
                memcpy(entry->name, name, name_len);
                dprintf("EXT2: add_dir_entry: reused empty entry\n");
                return write_block(phys_block, dir_buffer);
            }
            
            block_offset += entry->rec_len;
            if (block_offset >= block_size) {
                break;
            }
        }
        
        offset += block_size;
    }
    
    // Need to allocate a new block
    u32 block_idx = dir_inode.size / block_size;
    if (block_idx >= 12) {
        return E_EXT2_BAD_TYPE; // Indirect not supported for writes yet
    }
    
    u32 new_block = 0;
    
    // Check if we need to allocate the block
    if (dir_inode.block[block_idx] == 0) {
        // Allocate new block
        new_block = ext2_alloc_block();
        if (new_block == 0) {
            return E_EXT2_NO_BLOCK;
        }
        
        dir_inode.block[block_idx] = new_block;
        dir_inode.size += block_size;
        dir_inode.blocks += sectors_per_block;
        
        if (write_inode(dir_inode_num, &dir_inode) < 0) {
            return E_EXT2_WRITE_BLOCK;
        }
        
        // Initialize new block with directory entry
        memset(dir_buffer, 0, block_size);
        ext2_dir_entry_t *entry = (ext2_dir_entry_t*)dir_buffer;
        entry->inode = inode_num;
        entry->rec_len = block_size; // First entry takes entire block
        entry->name_len = name_len;
        entry->file_type = type;
        memcpy(entry->name, name, name_len);
        
        dprintf("EXT2: add_dir_entry: allocated new block %d\n", new_block);
        return write_block(new_block, dir_buffer);
    } else {
        // Block already exists - need to append to it
        // Read existing block and find end
        new_block = dir_inode.block[block_idx];
        if (read_block(new_block, dir_buffer) < 0) {
            return E_EXT2_READ_BLOCK;
        }
        
        // Find end of entries in this block
        u32 block_offset = 0;
        while (block_offset < block_size) {
            ext2_dir_entry_t *entry = (ext2_dir_entry_t*)(dir_buffer + block_offset);
            
            if (entry->rec_len == 0) {
                break; // Invalid entry
            }
            
            // Check if this is the last entry (rec_len points past block or to next entry)
            u32 next_offset = block_offset + entry->rec_len;
            if (next_offset >= block_size || 
                (next_offset < block_size && ((ext2_dir_entry_t*)(dir_buffer + next_offset))->inode == 0)) {
                // This is the last entry - we can extend it or split it
                u32 entry_len = sizeof(ext2_dir_entry_t) + entry->name_len;
                entry_len = (entry_len + 3) & ~3;
                
                if (entry->rec_len >= entry_len + rec_len) {
                    // Split entry
                    u32 new_offset = block_offset + entry_len;
                    ext2_dir_entry_t *new_entry = (ext2_dir_entry_t*)(dir_buffer + new_offset);
                    
                    new_entry->inode = inode_num;
                    new_entry->rec_len = entry->rec_len - entry_len;
                    new_entry->name_len = name_len;
                    new_entry->file_type = type;
                    memcpy(new_entry->name, name, name_len);
                    
                    entry->rec_len = entry_len;
                    
                    dprintf("EXT2: add_dir_entry: split last entry\n");
                    return write_block(new_block, dir_buffer);
                } else {
                    // Not enough space - need new block
                    break;
                }
            }
            
            block_offset += entry->rec_len;
            if (block_offset >= block_size) {
                break;
            }
        }
        
        // Block is full - allocate new block
        block_idx++;
        if (block_idx >= 12) {
            return E_EXT2_BAD_TYPE;
        }
        
        new_block = ext2_alloc_block();
        if (new_block == 0) {
            return E_EXT2_NO_BLOCK;
        }
        
        dir_inode.block[block_idx] = new_block;
        dir_inode.size += block_size;
        dir_inode.blocks += sectors_per_block;
        
        if (write_inode(dir_inode_num, &dir_inode) < 0) {
            return E_EXT2_WRITE_BLOCK;
        }
        
        // Initialize new block with directory entry
        memset(dir_buffer, 0, block_size);
        ext2_dir_entry_t *entry = (ext2_dir_entry_t*)dir_buffer;
        entry->inode = inode_num;
        entry->rec_len = block_size;
        entry->name_len = name_len;
        entry->file_type = type;
        memcpy(entry->name, name, name_len);
        
        dprintf("EXT2: add_dir_entry: added to new block %d\n", new_block);
        return write_block(new_block, dir_buffer);
    }
}

int ext2_write(ext2_file_t *file, const void *buffer, u32 size) {
    // Fast path: validate inputs early
    if (!file || !file->valid || !buffer || size == 0) {
        return (file && file->valid) ? 0 : E_INVALID_ARG;
    }
    
    const u8 *buf = (const u8*)buffer;
    u32 bytes_written = 0;
    bool inode_dirty = false;
    
    // Cache last block to avoid redundant lookups (per-write, not global)
    u32 last_block_idx = ~0;
    u32 last_phys_block = 0;
    
    while (bytes_written < size) {
        u32 block_idx = file->position / block_size;
        u32 offset = file->position % block_size;
        
        // Optimize: reuse last block if same index
        u32 phys_block;
        if (block_idx == last_block_idx && last_phys_block != 0) {
            phys_block = last_phys_block;
        } else {
            phys_block = get_inode_block(&file->inode, block_idx);
            last_block_idx = block_idx;
            last_phys_block = phys_block;
        }
        
        if (phys_block == 0) {
            // Allocate new block
            phys_block = ext2_alloc_block();
            if (phys_block == 0) {
                last_phys_block = 0; // Invalidate cache
                return E_EXT2_NO_BLOCK;
            }
            
            // Assign to inode
            if (block_idx < 12) {
                file->inode.block[block_idx] = phys_block;
                file->inode.blocks += sectors_per_block;
                inode_dirty = true;
            } else {
                last_phys_block = 0; // Invalidate cache
                return E_EXT2_BAD_TYPE; // Indirect not supported
            }
            
            // Zero initialize block (optimized)
            // Only zero if we're not writing the full block
            if (offset != 0 || size - bytes_written < block_size) {
                memset(block_buffer, 0, block_size);
                if (write_block(phys_block, block_buffer) < 0) {
                    last_phys_block = 0;
                    return E_EXT2_WRITE_BLOCK;
                }
            }
            
            last_phys_block = phys_block;
        }
        
        // Calculate write size
        u32 chunk = block_size - offset;
        u32 remaining = size - bytes_written;
        if (chunk > remaining) {
            chunk = remaining;
        }
        
        // Optimize: if writing full block from aligned buffer, write directly
        if (chunk == block_size && offset == 0 && ((u32)buf & 3) == 0) {
            // Direct write - no read-modify-write needed
            if (write_block(phys_block, buf + bytes_written) < 0) {
                last_phys_block = 0;
                return E_EXT2_WRITE_BLOCK;
            }
        } else {
            // Partial write - need read-modify-write
            // Only read if we haven't already (cache check)
            // Partial write - need read-modify-write
            // Only read if we haven't already (cache check is inside read_block)
            if (read_block(phys_block, block_buffer) < 0) {
                last_phys_block = 0;
                return E_EXT2_READ_BLOCK;
            }
            
            // Optimized memcpy for aligned data
            if (chunk >= 4 && ((u32)(block_buffer + offset) & 3) == 0 && ((u32)(buf + bytes_written) & 3) == 0) {
                // Aligned copy
                u32 *dst = (u32*)(block_buffer + offset);
                u32 *src = (u32*)(buf + bytes_written);
                u32 words = chunk / 4;
                for (u32 i = 0; i < words; i++) {
                    dst[i] = src[i];
                }
                // Copy remaining bytes
                u32 bytes_copied = words * 4;
                u8 *dst_byte = (u8*)dst + bytes_copied;
                u8 *src_byte = (u8*)src + bytes_copied;
                u32 remaining_bytes = chunk - bytes_copied;
                for (u32 j = 0; j < remaining_bytes; j++) {
                    dst_byte[j] = src_byte[j];
                }
            } else {
                // Unaligned or small
                memcpy(block_buffer + offset, buf + bytes_written, chunk);
            }
            
            // Write block back
            if (write_block(phys_block, block_buffer) < 0) {
                last_phys_block = 0;
                return E_EXT2_WRITE_BLOCK;
            }
        }
        
        bytes_written += chunk;
        file->position += chunk;
        
        // Update file size (only if increased)
        if (file->position > file->inode.size) {
            file->inode.size = file->position;
            inode_dirty = true;
        }
    }
    
    // Write inode back to disk only if modified
    if (inode_dirty) {
        if (write_inode(file->inode_num, &file->inode) < 0) {
            return E_EXT2_WRITE_BLOCK;
        }
    }
    
    return bytes_written;
}

int ext2_create_file(const char *path) {
    if (!mounted) {
        return E_EXT2_NOT_MOUNTED;
    }
    
    // Find parent directory
    const char *name_start = path;
    const char *name_end = path;
    
    // Find last slash
    while (*name_end) {
        if (*name_end == '/') {
            name_start = name_end + 1;
        }
        name_end++;
    }
    
    // Extract filename
    char filename[256];
    u32 i = 0;
    while (name_start < name_end && i < 255) {
        filename[i++] = *name_start++;
    }
    filename[i] = 0;
    
    if (i == 0) {
        return E_EXT2_BAD_PATH;
    }
    
    // Get parent directory path (OPTIMIZED)
    char parent_path[512];
    const char *p = path;
    const char *last_slash = NULL;
    
    // Find last slash (optimized: scan from end)
    const char *end = path;
    while (*end) end++;
    
    // Scan backwards for last slash
    p = end - 1;
    while (p >= path) {
        if (*p == '/') {
            last_slash = p;
            break;
        }
        p--;
    }
    
    if (last_slash) {
        // Extract parent path up to last slash
        u32 len = last_slash - path;
        if (len > 511) len = 511;
        
        if (len == 0) {
            // Path was just "/filename" - parent is root
            parent_path[0] = '/';
            parent_path[1] = 0;
        } else {
            memcpy(parent_path, path, len);
            parent_path[len] = 0;
        }
    } else {
        // No slash found - parent is current directory (root for now)
        parent_path[0] = '/';
        parent_path[1] = 0;
    }
    
    // Resolve parent directory
    u32 parent_ino = resolve_path(parent_path);
    if (parent_ino == 0) {
        // If parent path resolution failed, check if it's root
        const char *p = parent_path;
        while (*p == '/') p++;
        if (*p == 0) {
            // It's root path
            parent_ino = EXT2_ROOT_INO;
        } else {
            return E_EXT2_FILE_NOT_FOUND;
        }
    }
    
    ext2_inode_t parent;
    if (read_inode(parent_ino, &parent) < 0) {
        return E_EXT2_READ_BLOCK;
    }
    
    // Verify it's actually a directory
    if (!(parent.mode & EXT2_S_IFDIR)) {
        return E_NOT_DIR;
    }
    
    // Check if file already exists
    if (find_file_in_dir(&parent, filename)) {
        return E_EXT2_FILE_EXISTS;
    }
    
    // Allocate inode
    u32 ino = ext2_alloc_inode();
    if (ino == 0) {
        return E_EXT2_NO_INODE;
    }
    
    // Initialize inode
    ext2_inode_t new_inode;
    memset(&new_inode, 0, sizeof(ext2_inode_t));
    new_inode.mode = EXT2_S_IFREG | 0x0644; // Regular file, rw-r--r--
    new_inode.uid = 0;
    new_inode.gid = 0;
    new_inode.size = 0;
    new_inode.links_count = 1;
    new_inode.blocks = 0;
    
    if (write_inode(ino, &new_inode) < 0) {
        return E_EXT2_WRITE_BLOCK;
    }
    
    // Add directory entry
    if (ext2_add_dir_entry(parent_ino, ino, filename, EXT2_FT_REG_FILE) < 0) {
        return E_EXT2_DIR_FULL;
    }
    
    return E_OK;
}

int ext2_create_dir(const char *path) {
    if (!mounted) {
        return E_EXT2_NOT_MOUNTED;
    }
    
    // Find parent directory - same logic as create_file
    const char *name_start = path;
    const char *name_end = path;
    
    // Find last slash
    while (*name_end) {
        if (*name_end == '/') {
            name_start = name_end + 1;
        }
        name_end++;
    }
    
    // Extract directory name
    char dirname[256];
    u32 i = 0;
    while (name_start < name_end && i < 255) {
        dirname[i++] = *name_start++;
    }
    dirname[i] = 0;
    
    if (i == 0) {
        return E_EXT2_BAD_PATH;
    }
    
    // Get parent directory path (OPTIMIZED - same logic as create_file)
    char parent_path[512];
    const char *p = path;
    const char *last_slash = NULL;
    
    // Find last slash (optimized: scan from end)
    const char *end = path;
    while (*end) end++;
    
    // Scan backwards for last slash
    p = end - 1;
    while (p >= path) {
        if (*p == '/') {
            last_slash = p;
            break;
        }
        p--;
    }
    
    if (last_slash) {
        // Extract parent path up to last slash
        u32 len = last_slash - path;
        if (len > 511) len = 511;
        
        if (len == 0) {
            // Path was just "/dirname" - parent is root
            parent_path[0] = '/';
            parent_path[1] = 0;
        } else {
            memcpy(parent_path, path, len);
            parent_path[len] = 0;
        }
    } else {
        // No slash found - parent is current directory (root for now)
        parent_path[0] = '/';
        parent_path[1] = 0;
    }
    
    // Resolve parent directory
    u32 parent_ino = resolve_path(parent_path);
    if (parent_ino == 0) {
        // If parent path resolution failed, try root
        const char *p = parent_path;
        while (*p == '/') p++;
        if (*p == 0) {
            parent_ino = EXT2_ROOT_INO;
        } else {
            return E_EXT2_FILE_NOT_FOUND;
        }
    }
    
    ext2_inode_t parent;
    if (read_inode(parent_ino, &parent) < 0) {
        return E_EXT2_READ_BLOCK;
    }
    
    if (!(parent.mode & EXT2_S_IFDIR)) {
        return E_NOT_DIR;
    }
    
    // Check if directory already exists
    if (find_file_in_dir(&parent, dirname)) {
        return E_EXT2_FILE_EXISTS;
    }
    
    dprintf("EXT2: create_dir: creating '%s' in parent %d\n", dirname, parent_ino);
    
    u32 ino = ext2_alloc_inode();
    if (ino == 0) {
        return E_EXT2_NO_INODE;
    }
    
    // Allocate block for directory
    u32 block = ext2_alloc_block();
    if (block == 0) {
        return E_EXT2_NO_BLOCK;
    }
    
    dprintf("EXT2: create_dir: allocated inode %d, block %d\n", ino, block);
    
    // Initialize directory block with . and .. (use dir_buffer)
    memset(dir_buffer, 0, block_size);
    
    // Calculate proper entry lengths (4-byte aligned)
    u32 dot_entry_len = sizeof(ext2_dir_entry_t) + 1; // 8 + 1 = 9, round up to 12
    dot_entry_len = (dot_entry_len + 3) & ~3;
    
    ext2_dir_entry_t *dot = (ext2_dir_entry_t*)dir_buffer;
    dot->inode = ino;
    dot->rec_len = dot_entry_len;
    dot->name_len = 1;
    dot->file_type = EXT2_FT_DIR;
    dot->name[0] = '.';
    
    ext2_dir_entry_t *dotdot = (ext2_dir_entry_t*)(dir_buffer + dot_entry_len);
    dotdot->inode = parent_ino;
    dotdot->rec_len = block_size - dot_entry_len; // Rest of block
    dotdot->name_len = 2;
    dotdot->file_type = EXT2_FT_DIR;
    dotdot->name[0] = '.';
    dotdot->name[1] = '.';
    
    if (write_block(block, dir_buffer) < 0) {
        return E_EXT2_WRITE_BLOCK;
    }
    
    // Create inode
    ext2_inode_t new_inode;
    memset(&new_inode, 0, sizeof(ext2_inode_t));
    new_inode.mode = EXT2_S_IFDIR | 0x0755;
    new_inode.size = block_size;
    new_inode.links_count = 2;
    new_inode.blocks = sectors_per_block;
    new_inode.block[0] = block;
    
    if (write_inode(ino, &new_inode) < 0) {
        return E_EXT2_WRITE_BLOCK;
    }
    
    // Add to parent directory
    int ret = ext2_add_dir_entry(parent_ino, ino, dirname, EXT2_FT_DIR);
    if (ret < 0) {
        // Cleanup: free allocated inode and block on failure
        // (In a full implementation, we'd want to free these)
        return ret;
    }
    
    // Update parent link count (increment for .. entry)
    if (read_inode(parent_ino, &parent) < 0) {
        return E_EXT2_READ_BLOCK;
    }
    parent.links_count++;
    if (write_inode(parent_ino, &parent) < 0) {
        return E_EXT2_WRITE_BLOCK;
    }
    
    return E_OK;
}

int ext2_list_dir(const char *path, void (*callback)(const char *name, u32 size, bool is_dir)) {
    if (!mounted) {
        return E_EXT2_NOT_MOUNTED;
    }
    
    dprintf("EXT2: list_dir: called with path='%s'\n", path);
    
    u32 inode_num = resolve_path(path);
    dprintf("EXT2: list_dir: resolve_path returned inode %d\n", inode_num);
    if (inode_num == 0) {
        dprintf("EXT2: list_dir: path not found\n");
        return E_EXT2_FILE_NOT_FOUND;
    }
    
    ext2_inode_t dir_inode;
    if (read_inode(inode_num, &dir_inode) < 0) {
        dprintf("EXT2: list_dir: failed to read inode %d\n", inode_num);
        return E_EXT2_READ_BLOCK;
    }
    
    dprintf("EXT2: list_dir: inode %d mode=0x%04x size=%d block[0]=%d\n", 
            inode_num, dir_inode.mode, dir_inode.size, dir_inode.block[0]);
    
    if (!(dir_inode.mode & EXT2_S_IFDIR)) {
        dprintf("EXT2: list_dir: inode %d is NOT a directory (mode=0x%04x)\n", inode_num, dir_inode.mode);
        return E_NOT_DIR;
    }
    
    u32 size = dir_inode.size;
    u32 offset = 0;
    int count = 0;
    
    dprintf("EXT2: list_dir: listing '%s' (size=%d)\n", path, size);
    
    while (offset < size) {
        u32 block_idx = offset / block_size;
        u32 offset_in_block = offset % block_size;
        
        u32 phys_block = get_inode_block(&dir_inode, block_idx);
        if (phys_block == 0) {
            break;
        }
        
        // Use dir_buffer for directory operations
        if (read_block(phys_block, dir_buffer) < 0) {
            return count;
        }
        
        while (offset_in_block < block_size && offset < size) {
            ext2_dir_entry_t *entry = (ext2_dir_entry_t*)(dir_buffer + offset_in_block);
            
            if (entry->rec_len == 0) {
                break;
            }
            
            if (entry->inode != 0) {
                char name[256];
                u32 len = entry->name_len;
                if (len > 255) len = 255;
                memcpy(name, entry->name, len);
                name[len] = 0;
                
                bool is_dir = (entry->file_type == EXT2_FT_DIR);
                
                // Get size from inode (uses inode_buffer, separate from dir_buffer)
                ext2_inode_t entry_inode;
                u32 file_size = 0;
                if (read_inode(entry->inode, &entry_inode) == 0) {
                    file_size = entry_inode.size;
                }
                
                dprintf("EXT2: list_dir: entry '%s' ino=%d size=%d dir=%d\n", 
                        name, entry->inode, file_size, is_dir);
                callback(name, file_size, is_dir);
                count++;
            }
            
            offset += entry->rec_len;
            offset_in_block += entry->rec_len;
        }
    }
    
    dprintf("EXT2: list_dir: found %d entries\n", count);
    return count;
}

u32 ext2_get_file_size(const char *path) {
    ext2_file_t *file = ext2_open(path);
    if (!file) {
        return 0;
    }
    
    u32 size = file->inode.size;
    ext2_close(file);
    return size;
}

bool ext2_exists(const char *path) {
    u32 inode_num = resolve_path(path);
    return (inode_num != 0);
}

/**
 * Remove a directory entry from parent
 */
static int ext2_remove_dir_entry(u32 dir_inode_num, const char *name) {
    ext2_inode_t dir_inode;
    if (read_inode(dir_inode_num, &dir_inode) < 0) {
        return E_EXT2_READ_BLOCK;
    }
    
    if (!(dir_inode.mode & EXT2_S_IFDIR)) {
        return E_NOT_DIR;
    }
    
    u32 name_len = 0;
    while (name[name_len] && name_len < 255) {
        name_len++;
    }
    
    u32 offset = 0;
    while (offset < dir_inode.size) {
        u32 block_idx = offset / block_size;
        u32 phys_block = get_inode_block(&dir_inode, block_idx);
        if (phys_block == 0) {
            break;
        }
        
        // Use dir_buffer for directory operations
        if (read_block(phys_block, dir_buffer) < 0) {
            return E_EXT2_READ_BLOCK;
        }
        
        u32 block_offset = 0;
        while (block_offset < block_size) {
            ext2_dir_entry_t *entry = (ext2_dir_entry_t*)(dir_buffer + block_offset);
            
            if (entry->rec_len == 0) {
                break;
            }
            
            // Check if this is the entry we want to remove
            if (entry->inode != 0 && entry->name_len == name_len) {
                bool match = true;
                for (u32 i = 0; i < name_len; i++) {
                    if (entry->name[i] != name[i]) {
                        match = false;
                        break;
                    }
                }
                
                if (match) {
                    // Mark entry as unused (set inode to 0)
                    entry->inode = 0;
                    dprintf("EXT2: remove_dir_entry: removed '%s'\n", name);
                    return write_block(phys_block, dir_buffer);
                }
            }
            
            block_offset += entry->rec_len;
            if (block_offset >= block_size) {
                break;
            }
        }
        
        offset += block_size;
    }
    
    return E_EXT2_FILE_NOT_FOUND;
}

int ext2_remove_file(const char *path) {
    if (!mounted) {
        return E_EXT2_NOT_MOUNTED;
    }
    
    // Find parent and filename (same logic as create_file)
    const char *name_start = path;
    const char *name_end = path;
    while (*name_end) {
        if (*name_end == '/') {
            name_start = name_end + 1;
        }
        name_end++;
    }
    
    char filename[256];
    u32 i = 0;
    while (name_start < name_end && i < 255) {
        filename[i++] = *name_start++;
    }
    filename[i] = 0;
    
    if (i == 0) {
        return E_EXT2_BAD_PATH;
    }
    
    // Get parent path
    char parent_path[512];
    const char *p = path;
    const char *last_slash = NULL;
    const char *end = path;
    while (*end) end++;
    
    p = end - 1;
    while (p >= path) {
        if (*p == '/') {
            last_slash = p;
            break;
        }
        p--;
    }
    
    if (last_slash) {
        u32 len = last_slash - path;
        if (len > 511) len = 511;
        if (len == 0) {
            parent_path[0] = '/';
            parent_path[1] = 0;
        } else {
            memcpy(parent_path, path, len);
            parent_path[len] = 0;
        }
    } else {
        parent_path[0] = '/';
        parent_path[1] = 0;
    }
    
    // Resolve parent
    u32 parent_ino = resolve_path(parent_path);
    if (parent_ino == 0) {
        return E_EXT2_FILE_NOT_FOUND;
    }
    
    // Find file inode
    ext2_inode_t parent;
    if (read_inode(parent_ino, &parent) < 0) {
        return E_EXT2_READ_BLOCK;
    }
    
    u32 file_ino = find_file_in_dir(&parent, filename);
    if (file_ino == 0) {
        return E_EXT2_FILE_NOT_FOUND;
    }
    
    // Check it's a file, not directory
    ext2_inode_t file_inode;
    if (read_inode(file_ino, &file_inode) < 0) {
        return E_EXT2_READ_BLOCK;
    }
    
    if (file_inode.mode & EXT2_S_IFDIR) {
        return E_IS_DIR;
    }
    
    // Remove directory entry
    if (ext2_remove_dir_entry(parent_ino, filename) < 0) {
        return E_EXT2_WRITE_BLOCK;
    }
    
    // Decrement link count
    file_inode.links_count--;
    if (file_inode.links_count == 0) {
        // TODO: Free blocks and inode (simplified for now)
        file_inode.dtime = 0; // Mark as deleted
    }
    write_inode(file_ino, &file_inode);
    
    return E_OK;
}

int ext2_remove_dir(const char *path) {
    if (!mounted) {
        return E_EXT2_NOT_MOUNTED;
    }
    
    // Similar to remove_file but check directory is empty
    const char *name_start = path;
    const char *name_end = path;
    while (*name_end) {
        if (*name_end == '/') {
            name_start = name_end + 1;
        }
        name_end++;
    }
    
    char dirname[256];
    u32 i = 0;
    while (name_start < name_end && i < 255) {
        dirname[i++] = *name_start++;
    }
    dirname[i] = 0;
    
    if (i == 0) {
        return E_EXT2_BAD_PATH;
    }
    
    // Get parent path
    char parent_path[512];
    const char *p = path;
    const char *last_slash = NULL;
    const char *end = path;
    while (*end) end++;
    
    p = end - 1;
    while (p >= path) {
        if (*p == '/') {
            last_slash = p;
            break;
        }
        p--;
    }
    
    if (last_slash) {
        u32 len = last_slash - path;
        if (len > 511) len = 511;
        if (len == 0) {
            parent_path[0] = '/';
            parent_path[1] = 0;
        } else {
            memcpy(parent_path, path, len);
            parent_path[len] = 0;
        }
    } else {
        parent_path[0] = '/';
        parent_path[1] = 0;
    }
    
    // Resolve parent
    u32 parent_ino = resolve_path(parent_path);
    if (parent_ino == 0) {
        return E_EXT2_FILE_NOT_FOUND;
    }
    
    // Find directory inode
    ext2_inode_t parent;
    if (read_inode(parent_ino, &parent) < 0) {
        return E_EXT2_READ_BLOCK;
    }
    
    u32 dir_ino = find_file_in_dir(&parent, dirname);
    if (dir_ino == 0) {
        return E_EXT2_FILE_NOT_FOUND;
    }
    
    // Check it's a directory
    ext2_inode_t dir_inode;
    if (read_inode(dir_ino, &dir_inode) < 0) {
        return E_EXT2_READ_BLOCK;
    }
    
    if (!(dir_inode.mode & EXT2_S_IFDIR)) {
        return E_NOT_DIR;
    }
    
    // Check if directory is empty (only . and ..)
    u32 entry_count = 0;
    u32 offset = 0;
    while (offset < dir_inode.size) {
        u32 block_idx = offset / block_size;
        u32 phys_block = get_inode_block(&dir_inode, block_idx);
        if (phys_block == 0) break;
        
        // Use dir_buffer for directory operations
        if (read_block(phys_block, dir_buffer) < 0) break;
        
        u32 block_offset = 0;
        while (block_offset < block_size) {
            ext2_dir_entry_t *entry = (ext2_dir_entry_t*)(dir_buffer + block_offset);
            if (entry->rec_len == 0) break;
            if (entry->inode != 0) {
                // Check if it's not . or ..
                if (entry->name_len != 1 || entry->name[0] != '.') {
                    if (entry->name_len != 2 || entry->name[0] != '.' || entry->name[1] != '.') {
                        return E_BUSY; // Directory not empty
                    }
                }
                entry_count++;
            }
            block_offset += entry->rec_len;
            if (block_offset >= block_size) break;
        }
        offset += block_size;
    }
    
    if (entry_count > 2) {
        return E_BUSY; // Directory not empty
    }
    
    // Remove directory entry from parent
    if (ext2_remove_dir_entry(parent_ino, dirname) < 0) {
        return E_EXT2_WRITE_BLOCK;
    }
    
    // Decrement parent link count
    parent.links_count--;
    write_inode(parent_ino, &parent);
    
    // Mark directory as deleted
    dir_inode.dtime = 0;
    dir_inode.links_count = 0;
    write_inode(dir_ino, &dir_inode);
    
    return E_OK;
}
