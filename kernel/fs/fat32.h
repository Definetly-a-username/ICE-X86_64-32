/*
 * ICE Operating System - FAT32 Filesystem Header
 */

#ifndef ICE_FAT32_H
#define ICE_FAT32_H

#include "../types.h"

/* FAT32 Boot Sector (BPB) */
typedef struct __attribute__((packed)) {
    u8  jmp[3];
    u8  oem[8];
    u16 bytes_per_sector;
    u8  sectors_per_cluster;
    u16 reserved_sectors;
    u8  num_fats;
    u16 root_entry_count;
    u16 total_sectors_16;
    u8  media_type;
    u16 fat_size_16;
    u16 sectors_per_track;
    u16 num_heads;
    u32 hidden_sectors;
    u32 total_sectors_32;
    
    /* FAT32 extended */
    u32 fat_size_32;
    u16 ext_flags;
    u16 fs_version;
    u32 root_cluster;
    u16 fs_info;
    u16 backup_boot;
    u8  reserved[12];
    u8  drive_num;
    u8  reserved1;
    u8  boot_sig;
    u32 volume_id;
    u8  volume_label[11];
    u8  fs_type[8];
} fat32_bpb_t;

/* FAT32 Directory Entry */
typedef struct __attribute__((packed)) {
    u8  name[8];
    u8  ext[3];
    u8  attr;
    u8  nt_reserved;
    u8  create_time_tenth;
    u16 create_time;
    u16 create_date;
    u16 access_date;
    u16 cluster_high;
    u16 modify_time;
    u16 modify_date;
    u16 cluster_low;
    u32 file_size;
} fat32_dir_entry_t;

/* File attributes */
#define FAT_ATTR_READ_ONLY  0x01
#define FAT_ATTR_HIDDEN     0x02
#define FAT_ATTR_SYSTEM     0x04
#define FAT_ATTR_VOLUME_ID  0x08
#define FAT_ATTR_DIRECTORY  0x10
#define FAT_ATTR_ARCHIVE    0x20
#define FAT_ATTR_LFN        0x0F

/* File handle */
typedef struct {
    u32 cluster;
    u32 size;
    u32 position;
    bool valid;
} fat32_file_t;

/* Initialize FAT32 filesystem */
int fat32_init(void);

/* Open file */
fat32_file_t* fat32_open(const char *path);

/* Read from file */
int fat32_read(fat32_file_t *file, void *buffer, u32 size);

/* Write to file */
int fat32_write(fat32_file_t *file, const void *buffer, u32 size);

/* Create new file */
int fat32_create_file(const char *path);

/* Create new directory */
int fat32_create_dir(const char *path);

/* Close file */
void fat32_close(fat32_file_t *file);

/* Check if filesystem is mounted */
bool fat32_is_mounted(void);

/* List directory */
int fat32_list_dir(const char *path, void (*callback)(fat32_dir_entry_t *entry));

#endif /* ICE_FAT32_H */
