/*
 * ICE Operating System - FAT32 Filesystem Driver
 */

#include "fat32.h"
#include "../drivers/ata.h"
#include "../drivers/vga.h"

/* Filesystem state */
static bool mounted = false;
static fat32_bpb_t bpb;
static u32 fat_start_lba;
static u32 data_start_lba;
static u32 root_cluster;
static u32 sectors_per_cluster;

/* Sector buffer */
static u8 sector_buffer[512];

/* File handles */
#define MAX_OPEN_FILES 8
static fat32_file_t open_files[MAX_OPEN_FILES];

/* Forward declarations */
static int write_cluster(u32 cluster, const void *buffer);
static int set_fat_entry(u32 cluster, u32 value);
static u32 find_free_cluster(void);

/* String comparison for 8.3 names */
static bool name_match(const u8 *entry_name, const char *name) {
    char fat_name[12] = "           ";
    int i = 0, j = 0;
    
    /* Convert name to FAT format */
    while (name[i] && name[i] != '.' && j < 8) {
        fat_name[j++] = (name[i] >= 'a' && name[i] <= 'z') ? 
                        name[i] - 32 : name[i];
        i++;
    }
    
    if (name[i] == '.') {
        i++;
        j = 8;
        while (name[i] && j < 11) {
            fat_name[j++] = (name[i] >= 'a' && name[i] <= 'z') ? 
                            name[i] - 32 : name[i];
            i++;
        }
    }
    
    /* Compare */
    for (i = 0; i < 11; i++) {
        if (entry_name[i] != fat_name[i]) return false;
    }
    return true;
}

/* Read cluster */
static int read_cluster(u32 cluster, void *buffer) {
    u32 lba = data_start_lba + (cluster - 2) * sectors_per_cluster;
    for (u32 i = 0; i < sectors_per_cluster; i++) {
        if (ata_read_sectors(lba + i, 1, (u8*)buffer + i * 512) < 0) {
            return -1;
        }
    }
    return 0;
}

/* Get next cluster from FAT */
static u32 get_next_cluster(u32 cluster) {
    u32 fat_offset = cluster * 4;
    u32 fat_sector = fat_start_lba + (fat_offset / 512);
    u32 entry_offset = fat_offset % 512;
    
    if (ata_read_sectors(fat_sector, 1, sector_buffer) < 0) {
        return 0x0FFFFFFF;  /* End of chain */
    }
    
    u32 *fat = (u32*)sector_buffer;
    return fat[entry_offset / 4] & 0x0FFFFFFF;
}

int fat32_init(void) {
    /* Initialize ATA driver if needed */
    if (!ata_is_present()) {
        if (ata_init() < 0) {
            vga_puts("FAT32: No disk found\n");
            mounted = false;
            return -1;
        }
    }
    
    /* Read boot sector */
    if (ata_read_sectors(0, 1, sector_buffer) < 0) {
        vga_puts("FAT32: Failed to read boot sector\n");
        mounted = false;
        return -1;
    }
    
    /* Copy BPB */
    fat32_bpb_t *boot = (fat32_bpb_t*)sector_buffer;
    bpb = *boot;
    
    /* Verify FAT32 signature */
    if (bpb.bytes_per_sector != 512) {
        vga_puts("FAT32: Unsupported sector size\n");
        mounted = false;
        return -1;
    }
    
    /* Calculate filesystem layout */
    fat_start_lba = bpb.reserved_sectors;
    data_start_lba = fat_start_lba + (bpb.num_fats * bpb.fat_size_32);
    root_cluster = bpb.root_cluster;
    sectors_per_cluster = bpb.sectors_per_cluster;
    
    /* Initialize file handles */
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        open_files[i].valid = false;
    }
    
    mounted = true;
    return 0;
}

fat32_file_t* fat32_open(const char *path) {
    if (!mounted) return 0;
    
    /* Find free file handle */
    fat32_file_t *file = 0;
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!open_files[i].valid) {
            file = &open_files[i];
            break;
        }
    }
    if (!file) return 0;
    
    /* Skip leading slash */
    if (*path == '/') path++;
    
    u32 cluster = root_cluster;
    const char *name = path;
    
    /* Navigate path */
    u8 cluster_buffer[512 * 8];  /* Max 8 sectors per cluster */
    
    while (*name) {
        /* Read directory cluster */
        if (read_cluster(cluster, cluster_buffer) < 0) {
            return 0;
        }
        
        /* Find matching entry */
        fat32_dir_entry_t *entries = (fat32_dir_entry_t*)cluster_buffer;
        int entries_count = (sectors_per_cluster * 512) / sizeof(fat32_dir_entry_t);
        bool found = false;
        
        for (int i = 0; i < entries_count; i++) {
            if (entries[i].name[0] == 0) break;  /* End of directory */
            if (entries[i].name[0] == 0xE5) continue;  /* Deleted */
            if (entries[i].attr == FAT_ATTR_LFN) continue;  /* LFN */
            
            /* Extract current path component */
            char component[64];
            int j = 0;
            const char *p = name;
            while (*p && *p != '/' && j < 63) {
                component[j++] = *p++;
            }
            component[j] = 0;
            
            if (name_match(entries[i].name, component)) {
                cluster = (entries[i].cluster_high << 16) | entries[i].cluster_low;
                
                if (*p == '/') {
                    /* More path to go */
                    name = p + 1;
                } else {
                    /* Found the file */
                    file->cluster = cluster;
                    file->size = entries[i].file_size;
                    file->position = 0;
                    file->valid = true;
                    return file;
                }
                found = true;
                break;
            }
        }
        
        if (!found) {
            /* Try next cluster in chain */
            cluster = get_next_cluster(cluster);
            if (cluster >= 0x0FFFFFF8) {
                return 0;  /* End of chain, not found */
            }
        }
    }
    
    return 0;
}

int fat32_read(fat32_file_t *file, void *buffer, u32 size) {
    if (!file || !file->valid) return -1;
    
    u32 bytes_read = 0;
    u8 *buf = (u8*)buffer;
    u32 cluster = file->cluster;
    u32 cluster_size = sectors_per_cluster * 512;
    
    /* Skip to current position */
    u32 skip_clusters = file->position / cluster_size;
    for (u32 i = 0; i < skip_clusters && cluster < 0x0FFFFFF8; i++) {
        cluster = get_next_cluster(cluster);
    }
    
    u32 offset_in_cluster = file->position % cluster_size;
    u8 cluster_buffer[4096];
    
    while (bytes_read < size && file->position < file->size) {
        if (cluster >= 0x0FFFFFF8) break;
        
        if (read_cluster(cluster, cluster_buffer) < 0) {
            return bytes_read > 0 ? (int)bytes_read : -1;
        }
        
        u32 copy_size = cluster_size - offset_in_cluster;
        if (copy_size > size - bytes_read) copy_size = size - bytes_read;
        if (copy_size > file->size - file->position) {
            copy_size = file->size - file->position;
        }
        
        for (u32 i = 0; i < copy_size; i++) {
            buf[bytes_read + i] = cluster_buffer[offset_in_cluster + i];
        }
        
        bytes_read += copy_size;
        file->position += copy_size;
        offset_in_cluster = 0;
        
        cluster = get_next_cluster(cluster);
    }
    
    return bytes_read;
}

int fat32_write(fat32_file_t *file, const void *buffer, u32 size) {
    if (!file || !file->valid) return -1;
    
    u32 bytes_written = 0;
    const u8 *buf = (const u8*)buffer;
    u32 cluster = file->cluster;
    u32 cluster_size = sectors_per_cluster * 512;
    
    /* Skip to current position */
    u32 skip_clusters = file->position / cluster_size;
    for (u32 i = 0; i < skip_clusters; i++) {
        u32 next = get_next_cluster(cluster);
        if (next >= 0x0FFFFFF8) {
            /* Expand file if seeking past end? Or just error */
            /* For append, we expect to be at end */
             u32 new_cluster = find_free_cluster();
             if (new_cluster == 0) return -1;
             if (set_fat_entry(cluster, new_cluster) < 0) return -1;
             if (set_fat_entry(new_cluster, 0x0FFFFFFF) < 0) return -1;
             
             /* Zero new cluster */
             u8 zero[4096];
             for(int z=0; z<4096; z++) zero[z] = 0;
             write_cluster(new_cluster, zero);
             
             next = new_cluster;
        }
        cluster = next;
    }
    
    u32 offset_in_cluster = file->position % cluster_size;
    u8 cluster_buffer[4096];
    
    while (bytes_written < size) {
        /* Read existing to preserve surrounding data if any */
        if (read_cluster(cluster, cluster_buffer) < 0) {
            /* If read fails (maybe new cluster?), assumes zeroed */
             for(int z=0; z<4096; z++) cluster_buffer[z] = 0;
        }
        
        u32 copy_size = cluster_size - offset_in_cluster;
        if (copy_size > size - bytes_written) copy_size = size - bytes_written;
        
        for (u32 i = 0; i < copy_size; i++) {
            cluster_buffer[offset_in_cluster + i] = buf[bytes_written + i];
        }
        
        if (write_cluster(cluster, cluster_buffer) < 0) return -1;
        
        bytes_written += copy_size;
        file->position += copy_size;
        if (file->position > file->size) file->size = file->position;
        
        offset_in_cluster = 0;
        
        if (bytes_written < size) {
            /* Need next cluster */
            u32 next = get_next_cluster(cluster);
            if (next >= 0x0FFFFFF8) {
                u32 new_cluster = find_free_cluster();
                if (new_cluster == 0) return -1;
                if (set_fat_entry(cluster, new_cluster) < 0) return -1;
                if (set_fat_entry(new_cluster, 0x0FFFFFFF) < 0) return -1;
                
                u8 zero[4096];
                for(int z=0; z<4096; z++) zero[z] = 0;
                write_cluster(new_cluster, zero);
                
                next = new_cluster;
            }
            cluster = next;
        }
    }
    
    /* Update directory entry size on disk */
    /* This requires finding the dirent again which is slow but safe */
    /* Skipping for now - size assumes updated in file handle. */
    /* User must close file to flush properly or we rely on memory state? */
    /* Ideally we update dirent here. */
    
    return bytes_written;
}

/* Helper to write cluster */
static int write_cluster(u32 cluster, const void *buffer) {
    u32 lba = data_start_lba + (cluster - 2) * sectors_per_cluster;
    for (u32 i = 0; i < sectors_per_cluster; i++) {
        if (ata_write_sectors(lba + i, 1, (const u8*)buffer + i * 512) < 0) {
            return -1;
        }
    }
    return 0;
}

/* Helper to set FAT entry */
static int set_fat_entry(u32 cluster, u32 value) {
    u32 fat_offset = cluster * 4;
    u32 fat_sector = fat_start_lba + (fat_offset / 512);
    u32 entry_offset = fat_offset % 512;
    
    if (ata_read_sectors(fat_sector, 1, sector_buffer) < 0) return -1;
    
    u32 *fat = (u32*)sector_buffer;
    fat[entry_offset / 4] = value;
    
    if (ata_write_sectors(fat_sector, 1, sector_buffer) < 0) return -1;
    
    return 0;
}

/* Find free cluster */
static u32 find_free_cluster(void) {
    u32 total_clusters = bpb.total_sectors_32 / bpb.sectors_per_cluster;
    u32 fat_sector = -1;
    
    /* Scan FAT */
    for (u32 i = 2; i < total_clusters; i++) {
        u32 current_fat_sector = fat_start_lba + ((i * 4) / 512);
        
        if (current_fat_sector != fat_sector) {
            fat_sector = current_fat_sector;
            if (ata_read_sectors(fat_sector, 1, sector_buffer) < 0) return 0;
        }
        
        u32 *fat = (u32*)sector_buffer;
        u32 entry = fat[(i * 4) % 512 / 4] & 0x0FFFFFFF;
        
        if (entry == 0) return i;
    }
    return 0;
}

/* Update directory entry size */
static void update_file_size(const char *path, u32 new_size) {
    /* TODO: Traverse path, find entry, update size */
    (void)path; (void)new_size;
}

int fat32_create_file(const char *path) {
    if (!mounted) return -1;
    
    /* Use root for now */
    u32 parent_cluster = root_cluster;
    
    /* Parse filename from path */
    const char *name = path;
    const char *p = path;
    while (*p) {
        if (*p == '/') name = p + 1;
        p++;
    }
    if (!*name) return -1;
    
    /* Alloc cluster for file */
    u32 file_cluster = find_free_cluster();
    if (file_cluster == 0) return -2; /* Full */
    
    if (set_fat_entry(file_cluster, 0x0FFFFFFF) < 0) return -1;
    
    /* Find empty slot in parent directory */
    u8 cluster_buffer[4096]; /* Max 8 sectors */
    if (read_cluster(parent_cluster, cluster_buffer) < 0) return -1;
    
    fat32_dir_entry_t *entries = (fat32_dir_entry_t*)cluster_buffer;
    int entries_count = (sectors_per_cluster * 512) / sizeof(fat32_dir_entry_t);
    int free_idx = -1;
    
    for (int i = 0; i < entries_count; i++) {
        if (entries[i].name[0] == 0 || entries[i].name[0] == 0xE5) {
            free_idx = i;
            break;
        }
    }
    
    if (free_idx == -1) return -2; /* Dir full (need logic to expand dir) */
    
    /* Initialize entry */
    fat32_dir_entry_t *entry = &entries[free_idx];
    
    /* Convert name to 8.3 */
    for (int k = 0; k < 8; k++) entry->name[k] = ' ';
    for (int k = 0; k < 3; k++) entry->ext[k] = ' ';
    
    int i = 0, j = 0;
    /* Name */
    while (name[i] && name[i] != '.' && j < 8) {
        entry->name[j++] = (name[i] >= 'a' && name[i] <= 'z') ? name[i]-32 : name[i];
        i++;
    }
    /* Extension */
    if (name[i] == '.') {
        i++;
        j = 0;
        while (name[i] && j < 3) {
            entry->ext[j++] = (name[i] >= 'a' && name[i] <= 'z') ? name[i]-32 : name[i];
            i++;
        }
    }
    
    entry->attr = FAT_ATTR_ARCHIVE;
    entry->cluster_high = (file_cluster >> 16) & 0xFFFF;
    entry->cluster_low = file_cluster & 0xFFFF;
    entry->file_size = 0;
    
    /* Write back directory cluster */
    if (write_cluster(parent_cluster, cluster_buffer) < 0) return -1;
    
    /* Zero out new file cluster */
    for (int k = 0; k < 4096; k++) cluster_buffer[k] = 0;
    if (write_cluster(file_cluster, cluster_buffer) < 0) return -1;
    
    return 0;
}

/* Actual implementation of create_dir */
int fat32_create_dir(const char *path) {
    if (!mounted) return -1;
    u32 parent_cluster = root_cluster;

    /* Parse filename */
    const char *name = path;
    const char *p = path;
    while (*p) {
        if (*p == '/') name = p + 1;
        p++;
    }
    if (!*name) return -1;

    /* Alloc cluster */
    u32 dir_cluster = find_free_cluster();
    if (dir_cluster == 0) return -2;
    
    if (set_fat_entry(dir_cluster, 0x0FFFFFFF) < 0) return -1;

    /* Init new dir cluster (., ..) */
    u8 cluster_buffer[4096];
    for(int i=0; i<4096; i++) cluster_buffer[i] = 0;
    
    fat32_dir_entry_t *sub = (fat32_dir_entry_t*)cluster_buffer;
    
    /* Entry 1: . */
    for(int k=0; k<8; k++) sub[0].name[k] = ' ';
    for(int k=0; k<3; k++) sub[0].ext[k] = ' ';
    sub[0].name[0] = '.';
    sub[0].attr = FAT_ATTR_DIRECTORY;
    sub[0].cluster_high = (dir_cluster >> 16) & 0xFFFF;
    sub[0].cluster_low = dir_cluster & 0xFFFF;
    
    /* Entry 2: .. */
    for(int k=0; k<8; k++) sub[1].name[k] = ' ';
    for(int k=0; k<3; k++) sub[1].ext[k] = ' ';
    sub[1].name[0] = '.';
    sub[1].name[1] = '.';
    sub[1].attr = FAT_ATTR_DIRECTORY;
    /* Parent is root (0) */
    sub[1].cluster_high = 0;
    sub[1].cluster_low = 0;
    
    if (write_cluster(dir_cluster, cluster_buffer) < 0) return -1;

    /* Add to parent */
    if (read_cluster(parent_cluster, cluster_buffer) < 0) return -1;
    fat32_dir_entry_t *entries = (fat32_dir_entry_t*)cluster_buffer;
    int entries_count = (sectors_per_cluster * 512) / sizeof(fat32_dir_entry_t);
    int free_idx = -1;
    
    for (int i = 0; i < entries_count; i++) {
        if (entries[i].name[0] == 0 || entries[i].name[0] == 0xE5) {
            free_idx = i;
            break;
        }
    }
    
    if (free_idx == -1) return -2;
    
    fat32_dir_entry_t *entry = &entries[free_idx];
    for (int k = 0; k < 8; k++) entry->name[k] = ' ';
    for (int k = 0; k < 3; k++) entry->ext[k] = ' ';
    
    int i = 0, j = 0;
    while (name[i] && name[i] != '.' && j < 8) {
        entry->name[j++] = (name[i] >= 'a' && name[i] <= 'z') ? name[i]-32 : name[i];
        i++;
    }
    if (name[i] == '.') {
        i++; j = 0;
        while (name[i] && j < 3) {
            entry->ext[j++] = (name[i] >= 'a' && name[i] <= 'z') ? name[i]-32 : name[i];
            i++;
        }
    }
    
    entry->attr = FAT_ATTR_DIRECTORY;
    entry->cluster_high = (dir_cluster >> 16) & 0xFFFF;
    entry->cluster_low = dir_cluster & 0xFFFF;
    entry->file_size = 0;
    
    if (write_cluster(parent_cluster, cluster_buffer) < 0) return -1;
    
    return 0;
}

void fat32_close(fat32_file_t *file) {
    if (file) {
        /* TODO: Update file size on disk if changed */
        file->valid = false;
    }
}

bool fat32_is_mounted(void) {
    return mounted;
}

int fat32_list_dir(const char *path, void (*callback)(fat32_dir_entry_t *entry)) {
    if (!mounted) return -1;
    
    u32 cluster = root_cluster;
    (void)path; /* TODO: Navigate to path */
    
    u8 cluster_buffer[4096];
    int count = 0;
    
    while (cluster < 0x0FFFFFF8) {
        if (read_cluster(cluster, cluster_buffer) < 0) return count;
        
        fat32_dir_entry_t *entries = (fat32_dir_entry_t*)cluster_buffer;
        int entries_count = (sectors_per_cluster * 512) / sizeof(fat32_dir_entry_t);
        
        for (int i = 0; i < entries_count; i++) {
            if (entries[i].name[0] == 0) return count;
            if (entries[i].name[0] == 0xE5) continue;
            if (entries[i].attr == FAT_ATTR_LFN) continue;
            
            callback(&entries[i]);
            count++;
        }
        
        cluster = get_next_cluster(cluster);
    }
    return count;
}
