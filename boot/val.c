/*
 * ICE Operating System - Boot Validation (pm val)
 * Validates kernel-relevant files and restores from fallback
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "../mpm/core/mpm.h"

#define FALLBACK_DIR "/home/delta/basement/ice/mpm/krnl/fallback"
#define EXC_API_DIR  "/home/delta/basement/ice/EXC/API"

/* Files to validate and their fallback sources */
typedef struct {
    const char *path;
    const char *fallback;
    int critical;  /* 1 = system won't boot without this */
} val_entry_t;

static const val_entry_t validation_list[] = {
    {"/home/delta/basement/ice/EXC/API/process.exc", 
     FALLBACK_DIR "/process.exc", 1},
    {"/home/delta/basement/ice/EXC/API/exec.exc", 
     FALLBACK_DIR "/exec.exc", 1},
    {"/home/delta/basement/ice/EXC/API/memory.exc", 
     FALLBACK_DIR "/memory.exc", 1},
    {"/home/delta/basement/ice/EXC/API/tty.exc", 
     FALLBACK_DIR "/tty.exc", 1},
    {"/home/delta/basement/ice/EXC/API/fs.exc", 
     FALLBACK_DIR "/fs.exc", 1},
    {NULL, NULL, 0}  /* Sentinel */
};

/* ============================================================================
 * FILE COMPARISON
 * ============================================================================ */

static int files_match(const char *path1, const char *path2) {
    FILE *f1 = fopen(path1, "rb");
    FILE *f2 = fopen(path2, "rb");
    
    if (!f1 || !f2) {
        if (f1) fclose(f1);
        if (f2) fclose(f2);
        return 0;  /* Files don't match if we can't read one */
    }
    
    /* Compare sizes */
    fseek(f1, 0, SEEK_END);
    fseek(f2, 0, SEEK_END);
    
    long size1 = ftell(f1);
    long size2 = ftell(f2);
    
    if (size1 != size2) {
        fclose(f1);
        fclose(f2);
        return 0;
    }
    
    /* Compare contents */
    fseek(f1, 0, SEEK_SET);
    fseek(f2, 0, SEEK_SET);
    
    char buf1[1024], buf2[1024];
    size_t read1, read2;
    
    while ((read1 = fread(buf1, 1, sizeof(buf1), f1)) > 0) {
        read2 = fread(buf2, 1, sizeof(buf2), f2);
        if (read1 != read2 || memcmp(buf1, buf2, read1) != 0) {
            fclose(f1);
            fclose(f2);
            return 0;
        }
    }
    
    fclose(f1);
    fclose(f2);
    return 1;
}

/* ============================================================================
 * FILE RESTORATION
 * ============================================================================ */

static int copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) {
        fprintf(stderr, "  Error: Cannot read fallback: %s\n", src);
        return -1;
    }
    
    FILE *out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        fprintf(stderr, "  Error: Cannot write to: %s\n", dst);
        return -1;
    }
    
    char buf[4096];
    size_t n;
    
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            return -1;
        }
    }
    
    fclose(in);
    fclose(out);
    return 0;
}

/* ============================================================================
 * FALLBACK CREATION
 * ============================================================================ */

static int ensure_fallback_exists(void) {
    struct stat st;
    
    /* Create fallback directory if needed */
    if (stat(FALLBACK_DIR, &st) != 0) {
        if (mkdir(FALLBACK_DIR, 0755) != 0) {
            fprintf(stderr, "Error: Cannot create fallback directory\n");
            return -1;
        }
    }
    
    /* Copy current API files to fallback if they don't exist */
    for (const val_entry_t *entry = validation_list; entry->path; entry++) {
        if (stat(entry->fallback, &st) != 0) {
            /* Fallback doesn't exist, try to create from current */
            if (stat(entry->path, &st) == 0) {
                printf("  Creating fallback: %s\n", entry->fallback);
                if (copy_file(entry->path, entry->fallback) != 0) {
                    fprintf(stderr, "  Warning: Could not create fallback\n");
                }
            }
        }
    }
    
    return 0;
}

/* ============================================================================
 * VALIDATION
 * ============================================================================ */

int mpm_validate_system(void) {
    printf("ICE Boot Validation\n");
    printf("===================\n\n");
    
    int errors = 0;
    int restored = 0;
    
    /* Ensure fallbacks exist */
    printf("Checking fallback integrity...\n");
    ensure_fallback_exists();
    printf("\n");
    
    /* Validate each file */
    printf("Validating kernel files...\n");
    
    for (const val_entry_t *entry = validation_list; entry->path; entry++) {
        printf("  %s: ", entry->path);
        
        struct stat st;
        
        /* Check if file exists */
        if (stat(entry->path, &st) != 0) {
            printf("MISSING");
            
            /* Try to restore from fallback */
            if (stat(entry->fallback, &st) == 0) {
                printf(" -> RESTORING");
                if (copy_file(entry->fallback, entry->path) == 0) {
                    printf(" -> RESTORED\n");
                    restored++;
                } else {
                    printf(" -> FAILED\n");
                    errors++;
                }
            } else {
                printf(" (no fallback available)\n");
                if (entry->critical) errors++;
            }
            continue;
        }
        
        /* Check if fallback exists for comparison */
        if (stat(entry->fallback, &st) != 0) {
            printf("OK (no fallback to compare)\n");
            continue;
        }
        
        /* Compare with fallback */
        if (files_match(entry->path, entry->fallback)) {
            printf("OK\n");
        } else {
            printf("MODIFIED -> RESTORING");
            if (copy_file(entry->fallback, entry->path) == 0) {
                printf(" -> RESTORED\n");
                restored++;
            } else {
                printf(" -> FAILED\n");
                errors++;
            }
        }
    }
    
    printf("\n");
    printf("Validation complete.\n");
    printf("  Files restored: %d\n", restored);
    printf("  Errors: %d\n", errors);
    
    if (errors > 0) {
        printf("\nWARNING: System may be unstable.\n");
        return 1;
    }
    
    printf("\nSystem ready.\n");
    return 0;
}
