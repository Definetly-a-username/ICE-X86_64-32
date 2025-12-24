/*
 * ICE Operating System - Executable Registry
 * Manages the persistent registry of executables
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "../format/exc.h"
#include "../../mpm/core/mpm.h"

#define REGISTRY_FILE "/home/delta/basement/ice/EXC/registry/registry.db"
#define REGISTRY_MAGIC 0x52454749  /* "REGI" */

/* ============================================================================
 * REGISTRY FILE FORMAT
 * ============================================================================ */

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t entry_count;
    uint32_t next_id;
} registry_header_t;

/* ============================================================================
 * REGISTRY OPERATIONS
 * ============================================================================ */

static int ensure_registry_dir(void) {
    struct stat st = {0};
    if (stat("/home/delta/basement/ice/EXC/registry", &st) == -1) {
        return mkdir("/home/delta/basement/ice/EXC/registry", 0755);
    }
    return 0;
}

int registry_load(registry_entry_t *entries, int max_entries, 
                  uint32_t *next_id, int *count) {
    if (ensure_registry_dir() != 0) return -1;
    
    FILE *fp = fopen(REGISTRY_FILE, "rb");
    if (!fp) {
        /* No registry yet, start fresh */
        *next_id = 1;
        *count = 0;
        return 0;
    }
    
    registry_header_t header;
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        return -1;
    }
    
    if (header.magic != REGISTRY_MAGIC) {
        fclose(fp);
        return -1;
    }
    
    int to_read = header.entry_count < max_entries ? 
                  header.entry_count : max_entries;
    
    for (int i = 0; i < to_read; i++) {
        if (fread(&entries[i], sizeof(registry_entry_t), 1, fp) != 1) {
            fclose(fp);
            return -1;
        }
    }
    
    *next_id = header.next_id;
    *count = to_read;
    
    fclose(fp);
    return 0;
}

int registry_save(const registry_entry_t *entries, int count, uint32_t next_id) {
    if (ensure_registry_dir() != 0) return -1;
    
    FILE *fp = fopen(REGISTRY_FILE, "wb");
    if (!fp) return -1;
    
    registry_header_t header = {
        .magic = REGISTRY_MAGIC,
        .version = 1,
        .entry_count = count,
        .next_id = next_id
    };
    
    if (fwrite(&header, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        return -1;
    }
    
    for (int i = 0; i < count; i++) {
        if (fwrite(&entries[i], sizeof(registry_entry_t), 1, fp) != 1) {
            fclose(fp);
            return -1;
        }
    }
    
    fclose(fp);
    return 0;
}

int registry_add(registry_entry_t *entries, int *count, uint32_t *next_id,
                 const char *path, uint8_t flags, exc_type_t type) {
    if (*count >= MAX_EXECUTABLES) return -1;
    
    registry_entry_t *entry = &entries[*count];
    entry->id = (*next_id)++;
    strncpy(entry->path, path, MAX_PATH_LEN - 1);
    entry->flags = flags;
    entry->type = type;
    entry->valid = true;
    
    /* Extract name */
    const char *name = strrchr(path, '/');
    name = name ? name + 1 : path;
    strncpy(entry->name, name, sizeof(entry->name) - 1);
    
    (*count)++;
    return entry->id;
}

registry_entry_t* registry_find(registry_entry_t *entries, int count, 
                                 exec_id_t id) {
    for (int i = 0; i < count; i++) {
        if (entries[i].id == id && entries[i].valid) {
            return &entries[i];
        }
    }
    return NULL;
}

int registry_remove(registry_entry_t *entries, int count, exec_id_t id) {
    for (int i = 0; i < count; i++) {
        if (entries[i].id == id) {
            entries[i].valid = false;
            return 0;
        }
    }
    return -1;
}
