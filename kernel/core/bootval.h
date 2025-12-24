/*
 * ICE Operating System - Boot Validation Header
 * pm val functionality
 */

#ifndef ICE_BOOTVAL_H
#define ICE_BOOTVAL_H

#include "../types.h"

/* Validation result */
typedef struct {
    int files_checked;
    int files_restored;
    int errors;
} val_result_t;

/* Initialize boot validation system */
void bootval_init(void);

/* Perform boot validation */
val_result_t bootval_validate(void);

/* Restore file from fallback */
int bootval_restore(const char *path);

/* Check file integrity */
bool bootval_check_file(const char *path);

#endif /* ICE_BOOTVAL_H */
