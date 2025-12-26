/**
 * EXT2 Filesystem Driver
 * 
 * Provides read/write support for EXT2 filesystems with proper
 * block allocation, inode management, and superblock handling.
 * 
 * Supports both 32-bit and 64-bit architectures with proper
 * calling conventions and memory handling.
 */

#ifndef ICE_EXT2_H
#define ICE_EXT2_H

#include "../types.h"

// EXT2 Superblock Magic Number
#define EXT2_SUPER_MAGIC 0xEF53

// EXT2 Root Inode Number
#define EXT2_ROOT_INO 2

// File Type Constants
#define EXT2_FT_UNKNOWN  0
#define EXT2_FT_REG_FILE 1
#define EXT2_FT_DIR      2
#define EXT2_FT_CHRDEV   3
#define EXT2_FT_BLKDEV   4
#define EXT2_FT_FIFO     5
#define EXT2_FT_SOCK     6
#define EXT2_FT_SYMLINK  7

// Inode Mode Flags
#define EXT2_S_IFREG  0x8000  // Regular file
#define EXT2_S_IFDIR  0x4000  // Directory
#define EXT2_S_IFCHR  0x2000  // Character device
#define EXT2_S_IFBLK  0x6000  // Block device
#define EXT2_S_IFIFO  0x1000  // FIFO
#define EXT2_S_IFLNK  0xA000  // Symbolic link
#define EXT2_S_IFSOCK 0xC000  // Socket

/**
 * EXT2 Superblock Structure
 * Located at offset 1024 from the start of the partition
 */
typedef struct {
    u32 inodes_count;          ///< Total number of inodes
    u32 blocks_count;          ///< Total number of blocks
    u32 r_blocks_count;        ///< Reserved blocks count
    u32 free_blocks_count;     ///< Free blocks count
    u32 free_inodes_count;     ///< Free inodes count
    u32 first_data_block;      ///< First data block
    u32 log_block_size;        ///< Block size = 1024 << log_block_size
    u32 log_frag_size;         ///< Fragment size (usually same as block)
    u32 blocks_per_group;      ///< Blocks per group
    u32 frags_per_group;       ///< Fragments per group
    u32 inodes_per_group;      ///< Inodes per group
    u32 mtime;                 ///< Mount time
    u32 wtime;                 ///< Write time
    u16 mount_count;           ///< Mount count
    u16 max_mount_count;       ///< Max mount count
    u16 magic;                 ///< Magic signature (0xEF53)
    u16 state;                 ///< Filesystem state
    u16 errors;                ///< Behavior when errors detected
    u16 minor_rev_level;       ///< Minor revision level
    u32 lastcheck;             ///< Time of last check
    u32 checkinterval;         ///< Max time between checks
    u32 creator_os;            ///< OS creator
    u32 rev_level;              ///< Revision level
    u16 def_resuid;            ///< Default uid for reserved blocks
    u16 def_resgid;            ///< Default gid for reserved blocks
    
    // Extended fields (rev_level >= 1)
    u32 first_ino;             ///< First non-reserved inode
    u16 inode_size;            ///< Size of inode structure
    u16 block_group_nr;        ///< Block group this superblock belongs to
    u32 feature_compat;        ///< Compatible feature set
    u32 feature_incompat;       ///< Incompatible feature set
    u32 feature_ro_compat;      ///< Read-only compatible feature set
    u8  uuid[16];              ///< 128-bit UUID
    char volume_name[16];      ///< Volume name
    char last_mounted[64];     ///< Path where last mounted
    u32 algo_bitmap;           ///< Algorithm usage bitmap
    
    // Performance hints
    u8  prealloc_blocks;       ///< # of blocks to preallocate
    u8  prealloc_dir_blocks;   ///< # of blocks to preallocate for dirs
    u16 padding1;
    
    // Journaling support
    u8  journal_uuid[16];      ///< UUID of journal superblock
    u32 journal_inum;          ///< Inode number of journal file
    u32 journal_dev;           ///< Device number of journal file
    u32 last_orphan;           ///< Start of list of orphaned inodes
    
    // Hash indexing
    u32 hash_seed[4];          ///< HTREE hash seed
    u8  def_hash_version;      ///< Default hash version
    u8  padding2[3];
    
    // Other options
    u32 default_mount_opts;    ///< Default mount options
    u32 first_meta_bg;         ///< First metablock group
    u8  reserved[760];         ///< Padding to 1024 bytes
} __attribute__((packed)) ext2_superblock_t;

/**
 * Block Group Descriptor
 */
typedef struct {
    u32 block_bitmap;          ///< Block number of block bitmap
    u32 inode_bitmap;          ///< Block number of inode bitmap
    u32 inode_table;           ///< Block number of inode table
    u16 free_blocks_count;     ///< Number of free blocks
    u16 free_inodes_count;     ///< Number of free inodes
    u16 used_dirs_count;       ///< Number of directories
    u16 pad;                   ///< Padding
    u32 reserved[3];           ///< Reserved
} __attribute__((packed)) ext2_bg_desc_t;

/**
 * EXT2 Inode Structure
 */
typedef struct {
    u16 mode;                  ///< File mode & type
    u16 uid;                   ///< Low 16 bits of owner UID
    u32 size;                  ///< Size in bytes
    u32 atime;                 ///< Access time
    u32 ctime;                 ///< Creation time
    u32 mtime;                 ///< Modification time
    u32 dtime;                 ///< Deletion time
    u16 gid;                   ///< Low 16 bits of group GID
    u16 links_count;           ///< Links count
    u32 blocks;                ///< Blocks count (in 512-byte units)
    u32 flags;                 ///< File flags
    u32 osd1;                  ///< OS dependent 1
    u32 block[15];             ///< Pointers to blocks (12 direct, 1 indirect, 1 double, 1 triple)
    u32 generation;            ///< File version (for NFS)
    u32 file_acl;              ///< File ACL
    u32 dir_acl;               ///< Directory ACL (obsolete)
    u32 faddr;                 ///< Fragment address
    u8  osd2[12];              ///< OS dependent 2
} __attribute__((packed)) ext2_inode_t;

/**
 * Directory Entry Structure
 */
typedef struct {
    u32 inode;                 ///< Inode number
    u16 rec_len;               ///< Record length
    u8  name_len;              ///< Name length
    u8  file_type;             ///< File type
    char name[];               ///< File name (variable length)
} __attribute__((packed)) ext2_dir_entry_t;

/**
 * Open File Handle
 */
typedef struct {
    u32 inode_num;             ///< Inode number
    ext2_inode_t inode;        ///< Cached inode data
    u32 position;              ///< Current file position
    bool valid;                ///< Whether this handle is valid
} ext2_file_t;

/**
 * Initialize EXT2 filesystem
 * @param dev_id Block device ID to mount
 * @return 0 on success, negative error code on failure
 */
int ext2_init(u32 dev_id);

/**
 * Check if EXT2 filesystem is mounted
 * @return true if mounted, false otherwise
 */
bool ext2_is_mounted(void);

/**
 * Open a file or directory
 * @param path Path to file (absolute or relative to root)
 * @return Pointer to file handle, or NULL on error
 */
ext2_file_t* ext2_open(const char *path);

/**
 * Close a file handle
 * @param file File handle to close
 */
void ext2_close(ext2_file_t *file);

/**
 * Read data from a file
 * @param file File handle
 * @param buffer Destination buffer
 * @param size Number of bytes to read
 * @return Number of bytes read, or negative error code
 */
int ext2_read(ext2_file_t *file, void *buffer, u32 size);

/**
 * Write data to a file
 * @param file File handle
 * @param buffer Source buffer
 * @param size Number of bytes to write
 * @return Number of bytes written, or negative error code
 */
int ext2_write(ext2_file_t *file, const void *buffer, u32 size);

/**
 * Create a new file
 * @param path Path where to create the file
 * @return 0 on success, negative error code on failure
 */
int ext2_create_file(const char *path);

/**
 * Create a new directory
 * @param path Path where to create the directory
 * @return 0 on success, negative error code on failure
 */
int ext2_create_dir(const char *path);

/**
 * List directory contents
 * @param path Path to directory
 * @param callback Function called for each entry
 * @return Number of entries found, or negative error code
 */
int ext2_list_dir(const char *path, void (*callback)(const char *name, u32 size, bool is_dir));

/**
 * Get file size
 * @param path Path to file
 * @return File size in bytes, or 0 on error
 */
u32 ext2_get_file_size(const char *path);

/**
 * Check if path exists
 * @param path Path to check
 * @return true if exists, false otherwise
 */
bool ext2_exists(const char *path);

/**
 * Remove a file
 * @param path File path
 * @return 0 on success, negative error code on failure
 */
int ext2_remove_file(const char *path);

/**
 * Remove a directory (must be empty)
 * @param path Directory path
 * @return 0 on success, negative error code on failure
 */
int ext2_remove_dir(const char *path);

#endif // ICE_EXT2_H
