/*
 * ICE Operating System - MPM Kernel Headers
 * Main Process Manager - The sole authority for all system operations
 */

#ifndef ICE_MPM_H
#define ICE_MPM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#include "../../EXC/format/exc.h"

/* ============================================================================
 * CORE TYPES
 * ============================================================================ */

/* Executable ID: Unique immutable identifier for executables */
typedef uint32_t exec_id_t;
#define EXEC_ID_INVALID 0
#define EXEC_ID_FORMAT "#%08X"

/* Process ID: Runtime identifier for running processes */
typedef uint32_t ice_pid_t;
#define ICE_PID_INVALID 0

/* ============================================================================
 * PROCESS STATES
 * ============================================================================ */

typedef enum {
    PROC_STATE_OFF = 0,     /* Not running */
    PROC_STATE_ON,          /* Running */
    PROC_STATE_PAUSED,      /* Suspended */
    PROC_STATE_ZOMBIE       /* Terminated, awaiting cleanup */
} proc_state_t;

/* ============================================================================
 * EXECUTABLE FLAGS
 * ============================================================================ */

#define EXC_FLAG_NONE   0x00
#define EXC_FLAG_KRNL   0x01    /* Kernel executable (--krnl) */
#define EXC_FLAG_HIDDEN 0x02    /* Hidden from user listing */

/* Note: exc_type_t is defined in EXC/format/exc.h */

/* ============================================================================
 * EXC API REQUEST TYPES
 * ============================================================================ */

typedef enum {
    /* Process API */
    API_PROCESS_LIST = 100,
    API_PROCESS_KILL,
    API_PROCESS_RESTART,
    API_PROCESS_INFO,
    
    /* Exec API */
    API_EXEC_RUN = 200,
    API_EXEC_REGISTER,
    
    /* Memory API */
    API_MEMORY_ALLOC = 300,
    API_MEMORY_FREE,
    API_MEMORY_INFO,
    
    /* TTY API */
    API_TTY_BIND = 400,
    API_TTY_UNBIND,
    API_TTY_WRITE,
    API_TTY_READ,
    API_TTY_COLOR,
    
    /* FS API */
    API_FS_READ = 500,
    API_FS_WRITE,
    API_FS_LIST,
    API_FS_EXISTS,
} api_request_type_t;

/* ============================================================================
 * CALLER AUTHORIZATION
 * ============================================================================ */

typedef enum {
    CALLER_KERNEL = 0,      /* Internal kernel call */
    CALLER_PM,              /* Process Manager */
    CALLER_GPM,             /* General Process Manager */
} caller_type_t;

/* ============================================================================
 * REQUEST/RESPONSE STRUCTURES
 * ============================================================================ */

/* Maximum sizes */
#define MAX_PATH_LEN 256
#define MAX_PROCESSES 64
#define MAX_EXECUTABLES 1024
#define MAX_ERROR_MSG 128

/* API Request Structure */
typedef struct {
    api_request_type_t type;
    caller_type_t caller;
    union {
        /* Process operations */
        struct {
            exec_id_t exec_id;
        } process;
        
        /* Exec operations */
        struct {
            exec_id_t exec_id;
            char path[MAX_PATH_LEN];
            uint8_t flags;
        } exec;
        
        /* Memory operations */
        struct {
            ice_pid_t pid;
            size_t size;
        } memory;
        
        /* TTY operations */
        struct {
            ice_pid_t pid;
            int tty_id;
            int color_scheme;
            char buffer[1024];
        } tty;
        
        /* FS operations */
        struct {
            char path[MAX_PATH_LEN];
        } fs;
    } params;
} mpm_request_t;

/* Error codes */
typedef enum {
    MPM_OK = 0,
    MPM_ERR_INVALID_REQUEST,
    MPM_ERR_UNAUTHORIZED,
    MPM_ERR_NOT_FOUND,
    MPM_ERR_ALREADY_EXISTS,
    MPM_ERR_NO_MEMORY,
    MPM_ERR_INVALID_STATE,
    MPM_ERR_IO_ERROR,
    MPM_ERR_INVALID_FORMAT,
    MPM_ERR_REGISTRY_FULL,
} mpm_error_t;

/* Process information */
typedef struct {
    ice_pid_t pid;
    exec_id_t exec_id;
    proc_state_t state;
    size_t memory_used;
    int tty_id;
} process_info_t;

/* API Response Structure */
typedef struct {
    mpm_error_t error;
    char error_msg[MAX_ERROR_MSG];
    union {
        /* Process list response */
        struct {
            int count;
            process_info_t processes[MAX_PROCESSES];
        } process_list;
        
        /* Single process info */
        process_info_t process_info;
        
        /* Exec response */
        struct {
            exec_id_t exec_id;
            ice_pid_t pid;
        } exec;
        
        /* Memory info */
        struct {
            size_t total;
            size_t used;
            size_t free;
        } memory_info;
        
        /* Generic status */
        bool success;
    } data;
} mpm_response_t;

/* ============================================================================
 * EXECUTABLE REGISTRY ENTRY
 * ============================================================================ */

typedef struct {
    exec_id_t id;
    char path[MAX_PATH_LEN];
    char name[64];
    exc_type_t type;
    uint8_t flags;
    bool valid;
} registry_entry_t;

/* ============================================================================
 * MPM KERNEL API
 * ============================================================================ */

/* Initialize MPM kernel */
int mpm_init(void);

/* Shutdown MPM kernel */
void mpm_shutdown(void);

/* Process an API request */
mpm_response_t mpm_process_request(const mpm_request_t *request);

/* Validate authorization for a request */
bool mpm_authorize(api_request_type_t type, caller_type_t caller);

/* Boot validation */
int mpm_validate_system(void);

/* Get error message for error code */
const char* mpm_error_string(mpm_error_t error);

/* Get system uptime */
time_t mpm_get_uptime(void);

/* Get current color scheme */
int mpm_get_color_scheme(void);

/* Get number of registered executables */
size_t mpm_get_registry_count(void);

/* Get registry pointer (read-only) */
const registry_entry_t* mpm_get_registry(void);

#endif /* ICE_MPM_H */
