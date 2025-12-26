/**
 * Virtual Filesystem (VFS) Layer
 * 
 * Provides a unified interface for filesystem operations,
 * abstracting away the differences between EXT2, EXT4, etc.
 * 
 * This allows kernel code to work with files without knowing
 * which specific filesystem is being used.
 */

#ifndef ICE_VFS_H
#define ICE_VFS_H

#include "../types.h"

/**
 * File handle (opaque type)
 */
typedef struct vfs_file vfs_file_t;

/**
 * Filesystem types
 */
typedef enum {
    VFS_FS_NONE = 0,
    VFS_FS_EXT2,
    VFS_FS_EXT4
} vfs_fs_type_t;

/**
 * Initialize VFS layer
 * @return 0 on success, negative error code on failure
 */
int vfs_init(void);

/**
 * Mount a filesystem
 * @param dev_id Block device ID
 * @param fs_type Filesystem type (EXT2 or EXT4)
 * @return 0 on success, negative error code on failure
 */
int vfs_mount(u32 dev_id, vfs_fs_type_t fs_type);

/**
 * Check if filesystem is mounted
 * @return true if mounted, false otherwise
 */
bool vfs_is_mounted(void);

/**
 * Open a file
 * @param path File path
 * @return File handle, or NULL on error
 */
vfs_file_t* vfs_open(const char *path);

/**
 * Close a file
 * @param file File handle
 */
void vfs_close(vfs_file_t *file);

/**
 * Read from a file
 * @param file File handle
 * @param buffer Destination buffer
 * @param size Number of bytes to read
 * @return Number of bytes read, or negative error code
 */
int vfs_read(vfs_file_t *file, void *buffer, u32 size);

/**
 * Write to a file
 * @param file File handle
 * @param buffer Source buffer
 * @param size Number of bytes to write
 * @return Number of bytes written, or negative error code
 */
int vfs_write(vfs_file_t *file, const void *buffer, u32 size);

/**
 * Create a file
 * @param path File path
 * @return 0 on success, negative error code on failure
 */
int vfs_create_file(const char *path);

/**
 * Create a directory
 * @param path Directory path
 * @return 0 on success, negative error code on failure
 */
int vfs_create_dir(const char *path);

/**
 * List directory contents
 * @param path Directory path
 * @param callback Function called for each entry
 * @return Number of entries, or negative error code
 */
int vfs_list_dir(const char *path, void (*callback)(const char *name, u32 size, bool is_dir));

/**
 * Get file size
 * @param path File path
 * @return File size in bytes, or 0 on error
 */
u32 vfs_get_file_size(const char *path);

/**
 * Check if path exists
 * @param path Path to check
 * @return true if exists, false otherwise
 */
bool vfs_exists(const char *path);

/**
 * Remove a file
 * @param path File path
 * @return 0 on success, negative error code on failure
 */
int vfs_remove_file(const char *path);

/**
 * Remove a directory (must be empty)
 * @param path Directory path
 * @return 0 on success, negative error code on failure
 */
int vfs_remove_dir(const char *path);

#endif // ICE_VFS_H

