/*
 * ICE Operating System - EXC Executable Format Implementation
 */

#include "exc.h"
#include "../drivers/vga.h"

/* Executable registry */
static exc_entry_t registry[MAX_EXECUTABLES];
static int registry_count = 0;
static exec_id_t next_exec_id = 1;

/* String copy helper */
static void str_copy(char *dest, const char *src, int max) {
    int i;
    for (i = 0; i < max - 1 && src[i]; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
}

void exc_init(void) {
    /* Clear registry */
    for (int i = 0; i < MAX_EXECUTABLES; i++) {
        registry[i].valid = false;
        registry[i].id = 0;
    }
    
    registry_count = 0;
    next_exec_id = 1;
    
    /* Register built-in executables */
    exc_register("/bin/pm.exc", "pm", EXC_FLAG_NONE);
    exc_register("/bin/gpm.exc", "gpm", EXC_FLAG_NONE);
}

exec_id_t exc_register(const char *path, const char *name, u8 flags) {
    if (registry_count >= MAX_EXECUTABLES) {
        return 0;
    }
    
    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < MAX_EXECUTABLES; i++) {
        if (!registry[i].valid) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        return 0;
    }
    
    /* Create entry */
    exc_entry_t *entry = &registry[slot];
    entry->id = next_exec_id++;
    str_copy(entry->path, path, sizeof(entry->path));
    str_copy(entry->name, name, sizeof(entry->name));
    entry->type = EXC_TYPE_NATIVE;
    entry->flags = flags;
    entry->valid = true;
    entry->entry_point = 0;
    entry->load_addr = 0;
    
    registry_count++;
    
    return entry->id;
}

exc_entry_t* exc_find(exec_id_t id) {
    for (int i = 0; i < MAX_EXECUTABLES; i++) {
        if (registry[i].valid && registry[i].id == id) {
            return &registry[i];
        }
    }
    return 0;
}

int exc_get_count(void) {
    return registry_count;
}

void exc_list(void (*callback)(exc_entry_t *entry)) {
    for (int i = 0; i < MAX_EXECUTABLES; i++) {
        if (registry[i].valid) {
            /* Skip hidden entries */
            if (!(registry[i].flags & EXC_FLAG_HIDDEN)) {
                callback(&registry[i]);
            }
        }
    }
}

u32 exc_load(exec_id_t id) {
    exc_entry_t *entry = exc_find(id);
    if (!entry) {
        return 0;
    }
    
    /* TODO: Load from filesystem when FAT32 is implemented */
    /* For now, return a dummy entry point */
    
    /* Check if it's a built-in */
    /* Built-ins run in kernel context for now */
    
    return entry->entry_point;
}
