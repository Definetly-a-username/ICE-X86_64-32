/**
 * Virtual Filesystem (VFS) Layer Implementation
 * 
 * Provides unified filesystem interface
 */

#include "vfs.h"
#include "ext2.h"
#include "ext4.h"
#include "blockdev.h"
#include "../errno.h"

static bool vfs_initialized = false;
static bool vfs_mounted = false;
static vfs_fs_type_t current_fs_type = VFS_FS_NONE;

/**
 * VFS file handle wrapper
 */
struct vfs_file {
    void *fs_file;              ///< Filesystem-specific file handle
    vfs_fs_type_t fs_type;      ///< Filesystem type
    bool valid;                  ///< Whether handle is valid
    u32 refcount;               ///< Reference count for the file handle
};

#define MAX_VFS_FILES 32
static vfs_file_t vfs_files[MAX_VFS_FILES];

int vfs_init(void) {
    if (vfs_initialized) {
        return E_OK;
    }
    
    // Initialize block device layer
    blockdev_init();
    
    // Initialize filesystem files
    for (u32 i = 0; i < MAX_VFS_FILES; i++) {
        vfs_files[i].valid = false;
    }
    
    vfs_initialized = true;
    return E_OK;
}

int vfs_mount(u32 dev_id, vfs_fs_type_t fs_type) {
    if (!vfs_initialized) {
        if (vfs_init() < 0) {
            return E_GENERIC;
        }
    }
    
    int ret = E_OK;
    
    switch (fs_type) {
        case VFS_FS_EXT2:
            ret = ext2_init(dev_id);
            break;
        case VFS_FS_EXT4:
            ret = ext4_init(dev_id);
            break;
        default:
            return E_INVALID_ARG;
    }
    
    if (ret == E_OK) {
        vfs_mounted = true;
        current_fs_type = fs_type;
    }
    
    return ret;
}

bool vfs_is_mounted(void) {
    return vfs_mounted;
}

// Sanitize path (resolve .. and .)
static void sanitize_path(const char *input, char *output, int max_len) {
    int out_idx = 0;
    int in_idx = 0;
    
    // Ensure start with /
    if (input[0] != '/') {
        output[out_idx++] = '/';
    }
    
    while (input[in_idx] && out_idx < max_len - 1) {
        // Skip multiple slashes
        if (input[in_idx] == '/') {
            if (out_idx > 0 && output[out_idx-1] == '/') {
                in_idx++;
                continue;
            }
        }
        
        // Check for . and ..
        if (input[in_idx] == '.') {
            if (input[in_idx+1] == '/' || input[in_idx+1] == 0) {
                // ./ -> skip
                in_idx += (input[in_idx+1] == '/') ? 2 : 1;
                continue;
            } else if (input[in_idx+1] == '.' && (input[in_idx+2] == '/' || input[in_idx+2] == 0)) {
                // ../ -> go back
                if (out_idx > 1) {
                    out_idx--; // skip current slash
                    while (out_idx > 0 && output[out_idx-1] != '/') out_idx--;
                }
                in_idx += (input[in_idx+2] == '/') ? 3 : 2;
                continue;
            }
        }
        
        output[out_idx++] = input[in_idx++];
    }
    output[out_idx] = 0;
    
    // Remove trailing slash if not root
    if (out_idx > 1 && output[out_idx-1] == '/') {
        output[out_idx-1] = 0;
    }
}

vfs_file_t* vfs_open(const char *path_in) {
    if (!vfs_mounted) {
        return NULL;
    }
    
    char path[512];
    sanitize_path(path_in, path, sizeof(path));
    
    void *fs_file = NULL;
    
    switch (current_fs_type) {
        case VFS_FS_EXT2:
            fs_file = ext2_open(path);
            break;
        case VFS_FS_EXT4:
            fs_file = ext4_open(path);
            break;
        default:
            return NULL;
    }
    
    if (!fs_file) {
        return NULL;
    }
    
    // Find free slot
    for (u32 i = 0; i < MAX_VFS_FILES; i++) {
        if (!vfs_files[i].valid) {
            vfs_files[i].fs_file = fs_file;
            vfs_files[i].fs_type = current_fs_type;
            vfs_files[i].valid = true;
            vfs_files[i].refcount = 1;
            return &vfs_files[i];
        }
    }
    
    // Close file if we can't track it
    switch (current_fs_type) {
        case VFS_FS_EXT2:
            ext2_close((ext2_file_t*)fs_file);
            break;
        case VFS_FS_EXT4:
            ext4_close((ext2_file_t*)fs_file);
            break;
        default:
            break;
    }
    
    return NULL;
}

void vfs_close(vfs_file_t *file) {
    if (!file || !file->valid) {
        return;
    }
    
    file->refcount--;
    if (file->refcount > 0) {
        return;
    }
    
    switch (file->fs_type) {
        case VFS_FS_EXT2:
            ext2_close((ext2_file_t*)file->fs_file);
            break;
        case VFS_FS_EXT4:
            ext4_close((ext2_file_t*)file->fs_file);
            break;
        default:
            break;
    }
    
    file->fs_file = 0;
    file->valid = false;
    file->refcount = 0;
}

int vfs_read(vfs_file_t *file, void *buffer, u32 size) {
    if (!file || !file->valid) {
        return E_INVALID_ARG;
    }
    
    switch (file->fs_type) {
        case VFS_FS_EXT2:
            return ext2_read((ext2_file_t*)file->fs_file, buffer, size);
        case VFS_FS_EXT4:
            return ext4_read((ext2_file_t*)file->fs_file, buffer, size);
        default:
            return E_INVALID_ARG;
    }
}

int vfs_write(vfs_file_t *file, const void *buffer, u32 size) {
    if (!file || !file->valid) {
        return E_INVALID_ARG;
    }
    
    switch (file->fs_type) {
        case VFS_FS_EXT2:
            return ext2_write((ext2_file_t*)file->fs_file, buffer, size);
        case VFS_FS_EXT4:
            return ext4_write((ext2_file_t*)file->fs_file, buffer, size);
        default:
            return E_INVALID_ARG;
    }
}

int vfs_create_file(const char *path) {
    if (!vfs_mounted) {
        return E_EXT2_NOT_MOUNTED;
    }
    
    switch (current_fs_type) {
        case VFS_FS_EXT2:
            return ext2_create_file(path);
        case VFS_FS_EXT4:
            return ext4_create_file(path);
        default:
            return E_INVALID_ARG;
    }
}

int vfs_create_dir(const char *path) {
    if (!vfs_mounted) {
        return E_EXT2_NOT_MOUNTED;
    }
    
    switch (current_fs_type) {
        case VFS_FS_EXT2:
            return ext2_create_dir(path);
        case VFS_FS_EXT4:
            return ext4_create_dir(path);
        default:
            return E_INVALID_ARG;
    }
}

int vfs_list_dir(const char *path, void (*callback)(const char *name, u32 size, bool is_dir)) {
    if (!vfs_mounted) {
        return E_EXT2_NOT_MOUNTED;
    }
    
    switch (current_fs_type) {
        case VFS_FS_EXT2:
            return ext2_list_dir(path, callback);
        case VFS_FS_EXT4:
            return ext4_list_dir(path, callback);
        default:
            return E_INVALID_ARG;
    }
}

u32 vfs_get_file_size(const char *path) {
    if (!vfs_mounted) {
        return 0;
    }
    
    switch (current_fs_type) {
        case VFS_FS_EXT2:
            return ext2_get_file_size(path);
        case VFS_FS_EXT4:
            return ext4_get_file_size(path);
        default:
            return 0;
    }
}

bool vfs_exists(const char *path) {
    if (!vfs_mounted) {
        return false;
    }
    
    switch (current_fs_type) {
        case VFS_FS_EXT2:
            return ext2_exists(path);
        case VFS_FS_EXT4:
            return ext4_exists(path);
        default:
            return false;
    }
}

int vfs_remove_file(const char *path) {
    if (!vfs_mounted) {
        return E_EXT2_NOT_MOUNTED;
    }
    
    switch (current_fs_type) {
        case VFS_FS_EXT2:
            return ext2_remove_file(path);
        case VFS_FS_EXT4:
            return ext2_remove_file(path); // EXT4 uses same functions
        default:
            return E_INVALID_ARG;
    }
}

int vfs_remove_dir(const char *path) {
    if (!vfs_mounted) {
        return E_EXT2_NOT_MOUNTED;
    }
    
    switch (current_fs_type) {
        case VFS_FS_EXT2:
            return ext2_remove_dir(path);
        case VFS_FS_EXT4:
            return ext2_remove_dir(path); // EXT4 uses same functions
        default:
            return E_INVALID_ARG;
    }
}

