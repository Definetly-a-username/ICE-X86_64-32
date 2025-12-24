/*
 * ICE Operating System - APM (Application Process Manager) Header
 * Package manager supporting .apm packages and multi-language sources
 */

#ifndef ICE_APM_H
#define ICE_APM_H

#include "../types.h"

/* APM Package Magic: "IAPM" */
#define APM_MAGIC 0x4D504149

/* APM Package Version */
#define APM_VERSION 1

/* Supported languages */
typedef enum {
    LANG_UNKNOWN = 0,
    LANG_C,
    LANG_CPP,
    LANG_PYTHON,
    LANG_ASM_X86,
    LANG_ASM_X64,
    LANG_RUST,
    LANG_HTML,
    LANG_CSS,
    LANG_JS,
    LANG_GOLANG,
    LANG_MIXED,         /* Multiple languages in project */
    LANG_EXC,           /* Already compiled .exc */
} apm_lang_t;

/* APM Package Header (128 bytes) */
typedef struct __attribute__((packed)) {
    u32 magic;              /* APM_MAGIC */
    u8  version;            /* Package version */
    u8  lang;               /* Primary language */
    u8  flags;              /* Package flags */
    u8  reserved;
    
    u32 exec_id;            /* Assigned exec ID */
    u32 entry_offset;       /* Offset to entry/main */
    u32 code_size;          /* Total code size */
    u32 data_size;          /* Data section size */
    
    char name[32];          /* Package name */
    char author[32];        /* Author name */
    char desc[40];          /* Short description */
    
    u32 checksum;           /* Package checksum */
} apm_header_t;

/* APM flags */
#define APM_FLAG_COMPRESSED  0x01
#define APM_FLAG_SIGNED      0x02
#define APM_FLAG_NATIVE      0x04
#define APM_FLAG_SCRIPT      0x08

/* Installed package entry */
typedef struct {
    u32 id;
    char name[32];
    char path[64];
    apm_lang_t lang;
    bool installed;
    u32 size;
} apm_entry_t;

/* Maximum packages */
#define MAX_PACKAGES 128

/* Initialize APM */
void apm_init(void);

/* Install package from .apm file */
int apm_install(const char *path);

/* Install from source directory */
int apm_setup(const char *source_path);

/* Run installed package */
int apm_run(const char *name, int argc, char **argv);

/* List installed packages */
void apm_list(void);

/* Remove package */
int apm_remove(const char *name);

/* Get package info */
apm_entry_t* apm_get(const char *name);

/* Detect language from file extension */
apm_lang_t apm_detect_lang(const char *filename);

/* Get language name string */
const char* apm_lang_name(apm_lang_t lang);

/* Create .exc from source */
int apm_compile(const char *source, const char *output);

/* APM CLI command handler */
int app_apm(int argc, char **argv);

#endif /* ICE_APM_H */
