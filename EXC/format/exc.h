/*
 * ICE Operating System - EXC Format Specification
 * .exc is the only executable format in ICE
 */

#ifndef ICE_EXC_FORMAT_H
#define ICE_EXC_FORMAT_H

#include <stdint.h>

/* ============================================================================
 * EXC FILE FORMAT
 * ============================================================================
 * 
 * .exc files have the following structure:
 * 
 * +------------------+
 * | EXC Header       |  (64 bytes)
 * +------------------+
 * | Metadata Section |  (variable)
 * +------------------+
 * | Code/Data        |  (variable)
 * +------------------+
 * 
 * The same format is used for:
 * - User programs
 * - Managers
 * - Kernel helpers
 * 
 * Authority is determined by flags, not format.
 * ============================================================================ */

/* Magic number: "ICE\x00" */
#define EXC_MAGIC 0x00454349

/* Current format version */
#define EXC_VERSION_MAJOR 1
#define EXC_VERSION_MINOR 0

/* Executable types */
typedef enum {
    EXC_TYPE_NATIVE = 0,    /* Compiled C binary */
    EXC_TYPE_PYTHON,        /* Python script with runtime wrapper */
} exc_type_t;

/* EXC Header Structure (64 bytes) */
typedef struct __attribute__((packed)) {
    uint32_t magic;             /* Must be EXC_MAGIC */
    uint8_t  version_major;     /* Format version major */
    uint8_t  version_minor;     /* Format version minor */
    uint8_t  type;              /* exc_type_t: NATIVE or PYTHON */
    uint8_t  flags;             /* EXC_FLAG_* */
    
    uint32_t exec_id;           /* Assigned executable ID */
    uint32_t entry_offset;      /* Offset to entry point or script */
    uint32_t code_size;         /* Size of code/script section */
    uint32_t metadata_offset;   /* Offset to metadata section */
    uint32_t metadata_size;     /* Size of metadata */
    
    uint64_t created_time;      /* Creation timestamp */
    uint64_t modified_time;     /* Last modification timestamp */
    
    char     name[20];          /* Executable name (null-terminated) */
} exc_header_t;

_Static_assert(sizeof(exc_header_t) == 64, "exc_header_t must be 64 bytes");

/* Metadata entry types */
typedef enum {
    META_SOURCE_PATH = 1,       /* Original source file path */
    META_COMPILER,              /* Compiler used (for native) */
    META_RUNTIME,               /* Runtime required (for interpreted) */
    META_DEPENDENCIES,          /* List of dependencies */
    META_AUTHOR,                /* Author information */
    META_DESCRIPTION,           /* Description text */
} metadata_type_t;

/* Metadata entry structure */
typedef struct __attribute__((packed)) {
    uint8_t  type;              /* metadata_type_t */
    uint16_t length;            /* Length of data */
    /* Followed by 'length' bytes of data */
} metadata_entry_t;

/* ============================================================================
 * EXC FORMAT FUNCTIONS
 * ============================================================================ */

/* Validate an EXC header */
int exc_validate_header(const exc_header_t *header);

/* Create a new EXC file from source */
int exc_create_from_source(const char *source_path, const char *output_path,
                           uint8_t flags, uint32_t exec_id);

/* Read EXC header from file */
int exc_read_header(const char *path, exc_header_t *header);

/* Check if a file is a valid EXC executable */
int exc_is_valid(const char *path);

/* Get executable name from EXC file */
const char* exc_get_name(const exc_header_t *header);

/* Get executable type as string */
const char* exc_type_string(uint8_t type);

#endif /* ICE_EXC_FORMAT_H */
