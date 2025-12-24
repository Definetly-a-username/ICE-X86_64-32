/*
 * ICE Operating System - EXC Executable Format Header
 * .exc is the only executable format in ICE
 */

#ifndef ICE_EXC_H
#define ICE_EXC_H

#include "../types.h"

/* EXC Magic: "IEXC" */
#define EXC_MAGIC 0x43584549

/* EXC format version */
#define EXC_VERSION 1

/* EXC flags */
#define EXC_FLAG_NONE   0x00
#define EXC_FLAG_KRNL   0x01    /* Kernel mode executable */
#define EXC_FLAG_HIDDEN 0x02    /* Hidden from user listing */

/* EXC types */
typedef enum {
    EXC_TYPE_NATIVE = 0,        /* Native x86 binary */
    EXC_TYPE_SCRIPT,            /* Interpreted script */
} exc_type_t;

/* EXC Header (64 bytes) */
typedef struct __attribute__((packed)) {
    u32 magic;                  /* Must be EXC_MAGIC */
    u8  version;                /* Format version */
    u8  type;                   /* exc_type_t */
    u8  flags;                  /* EXC_FLAG_* */
    u8  reserved;
    
    u32 exec_id;                /* Executable ID (assigned at registration) */
    u32 entry_point;            /* Entry point offset */
    u32 code_size;              /* Size of code section */
    u32 data_size;              /* Size of data section */
    u32 bss_size;               /* Size of BSS section */
    u32 stack_size;             /* Required stack size */
    
    char name[32];              /* Executable name */
} exc_header_t;

/* Executable registry entry */
typedef struct {
    exec_id_t id;
    char path[64];
    char name[32];
    exc_type_t type;
    u8 flags;
    bool valid;
    u32 entry_point;
    u32 load_addr;
} exc_entry_t;

/* Maximum registered executables */
#define MAX_EXECUTABLES 256

/* Initialize EXC system */
void exc_init(void);

/* Register an executable */
exec_id_t exc_register(const char *path, const char *name, u8 flags);

/* Find executable by ID */
exc_entry_t* exc_find(exec_id_t id);

/* Get entry count */
int exc_get_count(void);

/* List executables */
void exc_list(void (*callback)(exc_entry_t *entry));

/* Load EXC file (returns entry point, 0 on failure) */
u32 exc_load(exec_id_t id);

#endif /* ICE_EXC_H */
