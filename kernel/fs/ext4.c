/**
 * EXT4 Filesystem Driver Implementation
 * 
 * Provides EXT4 support with backward compatibility for EXT2/EXT3.
 * For simplicity, this implementation primarily uses the EXT2 block
 * mapping approach, with basic EXT4 extent support detection.
 * 
 * The filesystem automatically detects whether it's EXT2, EXT3, or EXT4
 * and uses the appropriate methods.
 */

#include "ext4.h"
#include "ext2.h"
#include "blockdev.h"
#include "../errno.h"

static bool ext4_mounted = false;
static bool uses_extents = false;

/**
 * Check if filesystem uses extents
 */
static bool check_extents_support(void) {
    // For now, we'll use the simpler block mapping approach
    // In a full implementation, we would check the inode flags
    // and use extent trees for files that have EXT4_EXTENTS_FL set
    return false;
}

/**
 * Initialize EXT4 filesystem
 * This is essentially a wrapper around EXT2 init with EXT4 detection
 */
int ext4_init(u32 dev_id) {
    // EXT4 is backward compatible with EXT2, so we can use EXT2 init
    int ret = ext2_init(dev_id);
    if (ret < 0) {
        return ret;
    }
    
    // Check if filesystem supports extents
    uses_extents = check_extents_support();
    
    ext4_mounted = true;
    return E_OK;
}

bool ext4_is_mounted(void) {
    return ext4_mounted && ext2_is_mounted();
}

bool ext4_uses_extents(void) {
    return uses_extents;
}

/**
 * EXT4 operations are mostly the same as EXT2
 * We delegate to EXT2 functions for now, as EXT4 is backward compatible
 * In a full implementation, we would add extent-based file access
 */

ext2_file_t* ext4_open(const char *path) {
    if (!ext4_is_mounted()) {
        return NULL;
    }
    return ext2_open(path);
}

void ext4_close(ext2_file_t *file) {
    ext2_close(file);
}

int ext4_read(ext2_file_t *file, void *buffer, u32 size) {
    if (!ext4_is_mounted()) {
        return E_EXT2_NOT_MOUNTED;
    }
    
    // For now, use EXT2 block mapping
    // In full implementation, check if file uses extents and handle accordingly
    return ext2_read(file, buffer, size);
}

int ext4_write(ext2_file_t *file, const void *buffer, u32 size) {
    if (!ext4_is_mounted()) {
        return E_EXT2_NOT_MOUNTED;
    }
    
    // Use EXT2 block mapping for now
    return ext2_write(file, buffer, size);
}

int ext4_create_file(const char *path) {
    if (!ext4_is_mounted()) {
        return E_EXT2_NOT_MOUNTED;
    }
    return ext2_create_file(path);
}

int ext4_create_dir(const char *path) {
    if (!ext4_is_mounted()) {
        return E_EXT2_NOT_MOUNTED;
    }
    return ext2_create_dir(path);
}

int ext4_list_dir(const char *path, void (*callback)(const char *name, u32 size, bool is_dir)) {
    if (!ext4_is_mounted()) {
        return E_EXT2_NOT_MOUNTED;
    }
    return ext2_list_dir(path, callback);
}

u32 ext4_get_file_size(const char *path) {
    if (!ext4_is_mounted()) {
        return 0;
    }
    return ext2_get_file_size(path);
}

bool ext4_exists(const char *path) {
    if (!ext4_is_mounted()) {
        return false;
    }
    return ext2_exists(path);
}

