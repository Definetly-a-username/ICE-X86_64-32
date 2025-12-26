/**
 * EXT4 Filesystem Driver
 * 
 * Provides read/write support for EXT4 filesystems with backward
 * compatibility for EXT2/EXT3 filesystems.
 * 
 * EXT4 features supported:
 * - Larger file sizes (48-bit block numbers)
 * - Extents (simplified support, falls back to block maps)
 * - Extended attributes (basic support)
 * - Backward compatible with EXT2/EXT3
 */

#ifndef ICE_EXT4_H
#define ICE_EXT4_H

#include "../types.h"
#include "ext2.h"

// EXT4 uses same magic number as EXT2
#define EXT4_SUPER_MAGIC EXT2_SUPER_MAGIC

// EXT4 Feature Flags
#define EXT4_FEATURE_INCOMPAT_64BIT      0x0080
#define EXT4_FEATURE_INCOMPAT_EXTENTS    0x0040
#define EXT4_FEATURE_INCOMPAT_FLEX_BG    0x0200
#define EXT4_FEATURE_INCOMPAT_MMP        0x0100
#define EXT4_FEATURE_INCOMPAT_META_BG    0x0010

// EXT4 Inode Flags
#define EXT4_EXTENTS_FL                 0x00080000

/**
 * EXT4 Extent Structure
 */
typedef struct {
    u32 ee_block;       ///< First logical block
    u16 ee_len;         ///< Number of blocks
    u16 ee_start_hi;     ///< High 16 bits of physical block
    u32 ee_start_lo;    ///< Low 32 bits of physical block
} __attribute__((packed)) ext4_extent_t;

/**
 * EXT4 Extent Header
 */
typedef struct {
    u16 eh_magic;       ///< Magic number (0xF30A)
    u16 eh_entries;     ///< Number of entries
    u16 eh_max;         ///< Maximum entries
    u16 eh_depth;       ///< Depth of tree
    u32 eh_generation;  ///< Generation
} __attribute__((packed)) ext4_extent_header_t;

/**
 * EXT4 Extent Index (for tree structure)
 */
typedef struct {
    u32 ei_block;       ///< Logical block
    u32 ei_leaf_lo;     ///< Low 32 bits of physical block
    u16 ei_leaf_hi;     ///< High 16 bits of physical block
    u16 ei_unused;
} __attribute__((packed)) ext4_extent_idx_t;

/**
 * Initialize EXT4 filesystem (with EXT2/EXT3 compatibility)
 * @param dev_id Block device ID to mount
 * @return 0 on success, negative error code on failure
 */
int ext4_init(u32 dev_id);

/**
 * Check if EXT4 filesystem is mounted
 * @return true if mounted, false otherwise
 */
bool ext4_is_mounted(void);

/**
 * Check if filesystem uses extents
 * @return true if extents are used, false for block maps
 */
bool ext4_uses_extents(void);

/**
 * Open a file or directory (EXT4 compatible)
 * @param path Path to file
 * @return Pointer to file handle, or NULL on error
 */
ext2_file_t* ext4_open(const char *path);

/**
 * Close a file handle
 * @param file File handle to close
 */
void ext4_close(ext2_file_t *file);

/**
 * Read data from a file (EXT4 compatible)
 * @param file File handle
 * @param buffer Destination buffer
 * @param size Number of bytes to read
 * @return Number of bytes read, or negative error code
 */
int ext4_read(ext2_file_t *file, void *buffer, u32 size);

/**
 * Write data to a file (EXT4 compatible)
 * @param file File handle
 * @param buffer Source buffer
 * @param size Number of bytes to write
 * @return Number of bytes written, or negative error code
 */
int ext4_write(ext2_file_t *file, const void *buffer, u32 size);

/**
 * Create a new file
 * @param path Path where to create the file
 * @return 0 on success, negative error code on failure
 */
int ext4_create_file(const char *path);

/**
 * Create a new directory
 * @param path Path where to create the directory
 * @return 0 on success, negative error code on failure
 */
int ext4_create_dir(const char *path);

/**
 * List directory contents
 * @param path Path to directory
 * @param callback Function called for each entry
 * @return Number of entries found, or negative error code
 */
int ext4_list_dir(const char *path, void (*callback)(const char *name, u32 size, bool is_dir));

/**
 * Get file size
 * @param path Path to file
 * @return File size in bytes, or 0 on error
 */
u32 ext4_get_file_size(const char *path);

/**
 * Check if path exists
 * @param path Path to check
 * @return true if exists, false otherwise
 */
bool ext4_exists(const char *path);

#endif // ICE_EXT4_H

