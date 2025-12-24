/*
 * ICE Operating System - EXT2 Filesystem Driver
 */

#ifndef ICE_EXT2_H
#define ICE_EXT2_H

#include "../types.h"

#define EXT2_SUPER_MAGIC 0xEF53

/* Superblock */
typedef struct {
    u32 inodes_count;
    u32 blocks_count;
    u32 r_blocks_count;
    u32 free_blocks_count;
    u32 free_inodes_count;
    u32 first_data_block;
    u32 log_block_size;
    u32 log_frag_size;
    u32 blocks_per_group;
    u32 frags_per_group;
    u32 inodes_per_group;
    u32 mtime;
    u32 wtime;
    u16 mount_count;
    u16 max_mount_count;
    u16 magic;
    u16 state;
    u16 errors;
    u16 minor_rev_level;
    u32 lastcheck;
    u32 checkinterval;
    u32 creator_os;
    u32 rev_level;
    u16 def_resuid;
    u16 def_resgid;
    
    /* Extended (dynamic rev) */
    u32 first_ino;
    u16 inode_size;
    u16 block_group_nr;
    u32 feature_compat;
    u32 feature_incompat;
    u32 feature_ro_compat;
    u8  uuid[16];
    char volume_name[16];
    char last_mounted[64];
    u32 algo_bitmap;
    
    /* Performance hints */
    u8  prealloc_blocks;
    u8  prealloc_dir_blocks;
    u16 padding1;
    
    /* Journaling */
    u8  journal_uuid[16];
    u32 journal_inum;
    u32 journal_dev;
    u32 last_orphan;
    
    u32 hash_seed[4];
    u8  def_hash_version;
    u8  padding2[3];
    
    u32 default_mount_opts;
    u32 first_meta_bg;
    u8  reserved[760];
} ext2_superblock_t;

/* Block Group Descriptor */
typedef struct {
    u32 block_bitmap;
    u32 inode_bitmap;
    u32 inode_table;
    u16 free_blocks_count;
    u16 free_inodes_count;
    u16 used_dirs_count;
    u16 pad;
    u32 reserved[3];
} ext2_bg_desc_t;

/* Inode */
typedef struct {
    u16 mode;
    u16 uid;
    u32 size;
    u32 atime;
    u32 ctime;
    u32 mtime;
    u32 dtime;
    u16 gid;
    u16 links_count;
    u32 blocks;
    u32 flags;
    u32 osd1;
    u32 block[15];
    u32 generation;
    u32 file_acl;
    u32 dir_acl;
    u32 faddr;
    u8  osd2[12];
} ext2_inode_t;

/* Directory Entry */
typedef struct {
    u32 inode;
    u16 rec_len;
    u8  name_len;
    u8  file_type;
    char name[];
} ext2_dir_entry_t;

/* File types */
#define EXT2_FT_UNKNOWN  0
#define EXT2_FT_REG_FILE 1
#define EXT2_FT_DIR      2
#define EXT2_FT_CHRDEV   3
#define EXT2_FT_BLKDEV   4
#define EXT2_FT_FIFO     5
#define EXT2_FT_SOCK     6
#define EXT2_FT_SYMLINK  7

/* Inodes */
#define EXT2_ROOT_INO 2

/* File handle (abstract) */
typedef struct {
    u32 inode_num;
    ext2_inode_t inode;
    u32 position;
    bool valid;
} ext2_file_t;

/* API */
int ext2_init(void);
ext2_file_t* ext2_open(const char *path);
int ext2_read(ext2_file_t *file, void *buffer, u32 size);
int ext2_write(ext2_file_t *file, const void *buffer, u32 size); // User requested Write
void ext2_close(ext2_file_t *file);
int ext2_create_file(const char *path);
int ext2_create_dir(const char *path);
int ext2_list_dir(const char *path, void (*callback)(const char *name, u32 size, bool is_dir));

#endif
