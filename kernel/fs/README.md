# Filesystem Implementation

This directory contains a modular filesystem implementation for the ICE kernel, supporting EXT2 and EXT4 filesystems.

## Architecture

The filesystem implementation is organized into several layers:

### 1. Block Device Abstraction (`blockdev.h/c`)
- Provides a unified interface for block-level I/O operations
- Abstracts away hardware-specific details (ATA, IDE, etc.)
- Allows filesystem code to work with any block device
- Currently supports ATA devices

### 2. Filesystem Drivers

#### EXT2 (`ext2.h/c`)
- Full read/write support for EXT2 filesystems
- Features:
  - Superblock and block group descriptor management
  - Inode reading and writing
  - Block allocation and deallocation
  - Directory traversal and manipulation
  - File creation and writing
  - Proper error handling

#### EXT4 (`ext4.h/c`)
- EXT4 support with backward compatibility for EXT2/EXT3
- Currently uses EXT2 block mapping approach
- Can be extended to support extents and other EXT4 features

### 3. Virtual Filesystem Layer (`vfs.h/c`)
- Unified interface for filesystem operations
- Abstracts differences between EXT2, EXT4, etc.
- Allows kernel code to work with files without knowing the specific filesystem

### 4. Example Usage (`fs_examples.c`)
- Demonstrates common filesystem operations
- Examples for reading, writing, and listing directories

## Usage

### Initialization

```c
#include "fs/vfs.h"
#include "fs/blockdev.h"

// Initialize block device layer
blockdev_init();

// Initialize VFS
vfs_init();

// Mount filesystem (try EXT4 first, fall back to EXT2)
int ret = vfs_mount(BLOCKDEV_PRIMARY, VFS_FS_EXT4);
if (ret < 0) {
    ret = vfs_mount(BLOCKDEV_PRIMARY, VFS_FS_EXT2);
}
```

### Reading a File

```c
vfs_file_t *file = vfs_open("/path/to/file");
if (file) {
    char buffer[1024];
    int bytes_read = vfs_read(file, buffer, sizeof(buffer));
    vfs_close(file);
}
```

### Writing a File

```c
// Create file if it doesn't exist
if (!vfs_exists("/path/to/file")) {
    vfs_create_file("/path/to/file");
}

vfs_file_t *file = vfs_open("/path/to/file");
if (file) {
    const char *data = "Hello, World!";
    vfs_write(file, data, strlen(data));
    vfs_close(file);
}
```

### Listing a Directory

```c
void callback(const char *name, u32 size, bool is_dir) {
    if (is_dir) {
        printf("[DIR]  %s\n", name);
    } else {
        printf("[FILE] %s (%d bytes)\n", name, size);
    }
}

vfs_list_dir("/", callback);
```

## Features

### Supported Operations
- ✅ Read files
- ✅ Write files
- ✅ Create files
- ✅ Create directories
- ✅ List directories
- ✅ Check file existence
- ✅ Get file size

### Block Allocation
- Automatic block allocation when writing to files
- Proper bitmap management
- Superblock and block group descriptor updates

### Error Handling
- All operations return proper error codes
- No kernel panics on filesystem errors
- Graceful degradation on failures

## Limitations

### Current Limitations
- Indirect blocks (doubly/triply) not fully implemented for writes
- EXT4 extents not yet implemented (uses block mapping)
- No file deletion support yet
- No symbolic link support
- Limited to 32 block groups cached (can be increased)

### Block Size Support
- Currently optimized for 1024-byte blocks
- 4096-byte blocks should work but may need blockdev adjustments

## Testing

The filesystem can be tested using QEMU with a disk image:

```bash
# Create a disk image with EXT2 filesystem
dd if=/dev/zero of=disk.img bs=1M count=32
mkfs.ext2 -F disk.img

# Run kernel in QEMU
qemu-system-i386 -kernel build/ice.bin -drive file=disk.img,format=raw -m 128M
```

## 32/64-bit Compatibility

The implementation is designed to work on both 32-bit and 64-bit architectures:
- Uses fixed-size types (u32, u64) from `types.h`
- Proper structure packing with `__attribute__((packed))`
- No assumptions about pointer sizes in filesystem structures

## Error Codes

See `errno.h` for complete error code definitions. Filesystem-specific errors:
- `E_EXT2_NOT_MOUNTED`: Filesystem not mounted
- `E_EXT2_BAD_MAGIC`: Invalid filesystem magic number
- `E_EXT2_READ_BLOCK`: Block read failed
- `E_EXT2_WRITE_BLOCK`: Block write failed
- `E_EXT2_NO_INODE`: No free inodes available
- `E_EXT2_NO_BLOCK`: No free blocks available
- `E_EXT2_FILE_EXISTS`: File already exists
- `E_EXT2_FILE_NOT_FOUND`: File not found

## Future Enhancements

- Full EXT4 extent support
- File deletion and truncation
- Symbolic and hard links
- Extended attributes
- Journaling support (EXT3)
- Performance optimizations (caching, read-ahead)

