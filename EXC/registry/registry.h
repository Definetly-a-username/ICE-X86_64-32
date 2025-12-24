/*
 * ICE Operating System - Registry Header
 */

#ifndef ICE_REGISTRY_H
#define ICE_REGISTRY_H

#include "../format/exc.h"
#include "../../mpm/core/mpm.h"

/* Load registry from disk */
int registry_load(registry_entry_t *entries, int max_entries, 
                  uint32_t *next_id, int *count);

/* Save registry to disk */
int registry_save(const registry_entry_t *entries, int count, uint32_t next_id);

/* Add entry to registry */
int registry_add(registry_entry_t *entries, int *count, uint32_t *next_id,
                 const char *path, uint8_t flags, exc_type_t type);

/* Find entry by ID */
registry_entry_t* registry_find(registry_entry_t *entries, int count, 
                                 exec_id_t id);

/* Remove entry from registry */
int registry_remove(registry_entry_t *entries, int count, exec_id_t id);

#endif
