/*
 * ICE Operating System - Executable Format Implementation
 * .exc file format handling
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#include "exc.h"

/* ============================================================================
 * VALIDATION
 * ============================================================================ */

int exc_validate_header(const exc_header_t *header) {
    if (!header) return -1;
    
    /* Check magic number */
    if (header->magic != EXC_MAGIC) {
        return -1;
    }
    
    /* Check version */
    if (header->version_major > EXC_VERSION_MAJOR) {
        return -1;
    }
    
    /* Check type */
    if (header->type != EXC_TYPE_NATIVE && header->type != EXC_TYPE_PYTHON) {
        return -1;
    }
    
    return 0;
}

/* ============================================================================
 * FILE OPERATIONS
 * ============================================================================ */

int exc_read_header(const char *path, exc_header_t *header) {
    if (!path || !header) return -1;
    
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    
    size_t read = fread(header, sizeof(exc_header_t), 1, fp);
    fclose(fp);
    
    if (read != 1) return -1;
    
    return exc_validate_header(header);
}

int exc_is_valid(const char *path) {
    exc_header_t header;
    return exc_read_header(path, &header) == 0;
}

/* ============================================================================
 * CREATION
 * ============================================================================ */

static exc_type_t detect_source_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return EXC_TYPE_NATIVE;
    
    if (strcmp(ext, ".py") == 0) return EXC_TYPE_PYTHON;
    if (strcmp(ext, ".c") == 0) return EXC_TYPE_NATIVE;
    
    return EXC_TYPE_NATIVE;
}

int exc_create_from_source(const char *source_path, const char *output_path,
                           uint8_t flags, uint32_t exec_id) {
    if (!source_path || !output_path) return -1;
    
    /* Read source file */
    FILE *src = fopen(source_path, "rb");
    if (!src) return -1;
    
    fseek(src, 0, SEEK_END);
    long src_size = ftell(src);
    fseek(src, 0, SEEK_SET);
    
    char *source = malloc(src_size + 1);
    if (!source) {
        fclose(src);
        return -1;
    }
    
    fread(source, 1, src_size, src);
    source[src_size] = '\0';
    fclose(src);
    
    /* Detect type */
    exc_type_t type = detect_source_type(source_path);
    
    /* Create header */
    exc_header_t header = {0};
    header.magic = EXC_MAGIC;
    header.version_major = EXC_VERSION_MAJOR;
    header.version_minor = EXC_VERSION_MINOR;
    header.type = type;
    header.flags = flags;
    header.exec_id = exec_id;
    header.entry_offset = sizeof(exc_header_t);
    header.code_size = src_size;
    header.metadata_offset = sizeof(exc_header_t) + src_size;
    header.metadata_size = 0;
    header.created_time = time(NULL);
    header.modified_time = header.created_time;
    
    /* Extract name from path */
    const char *name = strrchr(source_path, '/');
    name = name ? name + 1 : source_path;
    strncpy(header.name, name, sizeof(header.name) - 1);
    
    /* Write output file */
    FILE *out = fopen(output_path, "wb");
    if (!out) {
        free(source);
        return -1;
    }
    
    fwrite(&header, sizeof(header), 1, out);
    fwrite(source, 1, src_size, out);
    fclose(out);
    
    free(source);
    return 0;
}

/* ============================================================================
 * UTILITIES
 * ============================================================================ */

const char* exc_get_name(const exc_header_t *header) {
    if (!header) return "unknown";
    return header->name;
}

const char* exc_type_string(uint8_t type) {
    switch (type) {
        case EXC_TYPE_NATIVE: return "native";
        case EXC_TYPE_PYTHON: return "python";
        default: return "unknown";
    }
}
