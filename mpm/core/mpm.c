/*
 * ICE Operating System - MPM Kernel Core
 * Main Process Manager - The sole authority for all system operations
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#include "mpm.h"
#include "../../EXC/format/exc.h"

/* ============================================================================
 * KERNEL STATE
 * ============================================================================ */

typedef struct {
    bool initialized;
    time_t boot_time;
    
    /* Process table */
    process_info_t processes[MAX_PROCESSES];
    int process_count;
    ice_pid_t next_pid;
    
    /* Executable registry */
    registry_entry_t registry[MAX_EXECUTABLES];
    int registry_count;
    exec_id_t next_exec_id;
    
    /* Memory tracking (simulated) */
    size_t total_memory;
    size_t used_memory;
    
    /* TTY state */
    int current_tty;
    int color_scheme;
} kernel_state_t;

static kernel_state_t kernel = {0};

/* ============================================================================
 * AUTHORIZATION MATRIX
 * ============================================================================ */

typedef struct {
    api_request_type_t type;
    bool kernel_allowed;
    bool pm_allowed;
    bool gpm_allowed;
} auth_entry_t;

static const auth_entry_t auth_matrix[] = {
    /* Process API */
    {API_PROCESS_LIST,      true,  true,  true},
    {API_PROCESS_KILL,      true,  true,  false},
    {API_PROCESS_RESTART,   true,  true,  false},
    {API_PROCESS_INFO,      true,  true,  true},
    
    /* Exec API */
    {API_EXEC_RUN,          true,  false, true},
    {API_EXEC_REGISTER,     true,  false, true},
    
    /* Memory API */
    {API_MEMORY_ALLOC,      true,  false, false},
    {API_MEMORY_FREE,       true,  false, false},
    {API_MEMORY_INFO,       true,  true,  true},
    
    /* TTY API */
    {API_TTY_BIND,          true,  false, false},
    {API_TTY_UNBIND,        true,  false, false},
    {API_TTY_WRITE,         true,  true,  true},
    {API_TTY_READ,          true,  true,  true},
    {API_TTY_COLOR,         true,  false, true},
    
    /* FS API */
    {API_FS_READ,           true,  true,  true},
    {API_FS_WRITE,          true,  true,  true},
    {API_FS_LIST,           true,  true,  true},
    {API_FS_EXISTS,         true,  true,  true},
};

#define AUTH_MATRIX_SIZE (sizeof(auth_matrix) / sizeof(auth_entry_t))

/* ============================================================================
 * ERROR MESSAGES
 * ============================================================================ */

static const char* error_messages[] = {
    [MPM_OK]                = "Success",
    [MPM_ERR_INVALID_REQUEST] = "Invalid request",
    [MPM_ERR_UNAUTHORIZED]  = "Unauthorized: caller not permitted for this operation",
    [MPM_ERR_NOT_FOUND]     = "Not found",
    [MPM_ERR_ALREADY_EXISTS]= "Already exists",
    [MPM_ERR_NO_MEMORY]     = "Out of memory",
    [MPM_ERR_INVALID_STATE] = "Invalid state for operation",
    [MPM_ERR_IO_ERROR]      = "I/O error",
    [MPM_ERR_INVALID_FORMAT]= "Invalid executable format",
    [MPM_ERR_REGISTRY_FULL] = "Executable registry is full",
};

const char* mpm_error_string(mpm_error_t error) {
    if (error >= 0 && error <= MPM_ERR_REGISTRY_FULL) {
        return error_messages[error];
    }
    return "Unknown error";
}

/* ============================================================================
 * AUTHORIZATION
 * ============================================================================ */

bool mpm_authorize(api_request_type_t type, caller_type_t caller) {
    for (size_t i = 0; i < AUTH_MATRIX_SIZE; i++) {
        if (auth_matrix[i].type == type) {
            switch (caller) {
                case CALLER_KERNEL: return auth_matrix[i].kernel_allowed;
                case CALLER_PM:     return auth_matrix[i].pm_allowed;
                case CALLER_GPM:    return auth_matrix[i].gpm_allowed;
            }
        }
    }
    return false;
}

/* ============================================================================
 * INTERNAL HELPERS
 * ============================================================================ */

static process_info_t* find_process_by_exec_id(exec_id_t exec_id) {
    for (int i = 0; i < kernel.process_count; i++) {
        if (kernel.processes[i].exec_id == exec_id && 
            kernel.processes[i].state != PROC_STATE_OFF) {
            return &kernel.processes[i];
        }
    }
    return NULL;
}

static registry_entry_t* find_registry_entry(exec_id_t exec_id) {
    for (int i = 0; i < kernel.registry_count; i++) {
        if (kernel.registry[i].id == exec_id && kernel.registry[i].valid) {
            return &kernel.registry[i];
        }
    }
    return NULL;
}

static int allocate_process_slot(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (kernel.processes[i].state == PROC_STATE_OFF) {
            return i;
        }
    }
    return -1;
}

/* ============================================================================
 * REQUEST HANDLERS
 * ============================================================================ */

static mpm_response_t handle_process_list(const mpm_request_t *req) {
    mpm_response_t resp = {.error = MPM_OK};
    
    int count = 0;
    for (int i = 0; i < kernel.process_count && count < MAX_PROCESSES; i++) {
        if (kernel.processes[i].state != PROC_STATE_OFF) {
            resp.data.process_list.processes[count++] = kernel.processes[i];
        }
    }
    resp.data.process_list.count = count;
    
    return resp;
}

static mpm_response_t handle_process_kill(const mpm_request_t *req) {
    mpm_response_t resp = {.error = MPM_OK};
    
    process_info_t *proc = find_process_by_exec_id(req->params.process.exec_id);
    if (!proc) {
        resp.error = MPM_ERR_NOT_FOUND;
        snprintf(resp.error_msg, MAX_ERROR_MSG, 
                 "Process with exec_id " EXEC_ID_FORMAT " not found",
                 req->params.process.exec_id);
        return resp;
    }
    
    /* Transition to ZOMBIE, then OFF */
    proc->state = PROC_STATE_ZOMBIE;
    kernel.used_memory -= proc->memory_used;
    proc->memory_used = 0;
    proc->state = PROC_STATE_OFF;
    
    resp.data.success = true;
    return resp;
}

static mpm_response_t handle_process_restart(const mpm_request_t *req) {
    mpm_response_t resp = {.error = MPM_OK};
    
    process_info_t *proc = find_process_by_exec_id(req->params.process.exec_id);
    if (!proc) {
        resp.error = MPM_ERR_NOT_FOUND;
        return resp;
    }
    
    /* Kill and restart */
    proc->state = PROC_STATE_ZOMBIE;
    proc->state = PROC_STATE_ON;
    
    resp.data.success = true;
    return resp;
}

static mpm_response_t handle_process_info(const mpm_request_t *req) {
    mpm_response_t resp = {.error = MPM_OK};
    
    process_info_t *proc = find_process_by_exec_id(req->params.process.exec_id);
    if (!proc) {
        resp.error = MPM_ERR_NOT_FOUND;
        return resp;
    }
    
    resp.data.process_info = *proc;
    return resp;
}

static mpm_response_t handle_exec_run(const mpm_request_t *req) {
    mpm_response_t resp = {.error = MPM_OK};
    
    registry_entry_t *entry = find_registry_entry(req->params.exec.exec_id);
    if (!entry) {
        resp.error = MPM_ERR_NOT_FOUND;
        snprintf(resp.error_msg, MAX_ERROR_MSG,
                 "Executable " EXEC_ID_FORMAT " not found in registry",
                 req->params.exec.exec_id);
        return resp;
    }
    
    int slot = allocate_process_slot();
    if (slot < 0) {
        resp.error = MPM_ERR_NO_MEMORY;
        strcpy(resp.error_msg, "Process table full");
        return resp;
    }
    
    /* Allocate memory (simulated) */
    size_t mem_needed = 4096; /* Base allocation */
    if (kernel.used_memory + mem_needed > kernel.total_memory) {
        resp.error = MPM_ERR_NO_MEMORY;
        return resp;
    }
    
    /* Create process */
    kernel.processes[slot].pid = kernel.next_pid++;
    kernel.processes[slot].exec_id = entry->id;
    kernel.processes[slot].state = PROC_STATE_ON;
    kernel.processes[slot].memory_used = mem_needed;
    kernel.processes[slot].tty_id = kernel.current_tty;
    
    kernel.used_memory += mem_needed;
    if (slot >= kernel.process_count) {
        kernel.process_count = slot + 1;
    }
    
    resp.data.exec.exec_id = entry->id;
    resp.data.exec.pid = kernel.processes[slot].pid;
    
    return resp;
}

static mpm_response_t handle_exec_register(const mpm_request_t *req) {
    mpm_response_t resp = {.error = MPM_OK};
    
    if (kernel.registry_count >= MAX_EXECUTABLES) {
        resp.error = MPM_ERR_REGISTRY_FULL;
        return resp;
    }
    
    /* Validate the executable exists */
    if (access(req->params.exec.path, R_OK) != 0) {
        resp.error = MPM_ERR_NOT_FOUND;
        snprintf(resp.error_msg, MAX_ERROR_MSG, "File not found: %s",
                 req->params.exec.path);
        return resp;
    }
    
    /* Register the executable */
    registry_entry_t *entry = &kernel.registry[kernel.registry_count];
    entry->id = kernel.next_exec_id++;
    strncpy(entry->path, req->params.exec.path, MAX_PATH_LEN - 1);
    entry->flags = req->params.exec.flags;
    entry->valid = true;
    
    /* Extract name from path */
    const char *name = strrchr(req->params.exec.path, '/');
    name = name ? name + 1 : req->params.exec.path;
    strncpy(entry->name, name, sizeof(entry->name) - 1);
    
    kernel.registry_count++;
    
    resp.data.exec.exec_id = entry->id;
    return resp;
}

static mpm_response_t handle_memory_info(const mpm_request_t *req) {
    mpm_response_t resp = {.error = MPM_OK};
    
    resp.data.memory_info.total = kernel.total_memory;
    resp.data.memory_info.used = kernel.used_memory;
    resp.data.memory_info.free = kernel.total_memory - kernel.used_memory;
    
    return resp;
}

static mpm_response_t handle_tty_color(const mpm_request_t *req) {
    mpm_response_t resp = {.error = MPM_OK};
    
    int scheme = req->params.tty.color_scheme;
    if (scheme < 1 || scheme > 4) {
        resp.error = MPM_ERR_INVALID_REQUEST;
        strcpy(resp.error_msg, "Color scheme must be 1-4");
        return resp;
    }
    
    kernel.color_scheme = scheme;
    resp.data.success = true;
    return resp;
}

/* ============================================================================
 * MAIN REQUEST DISPATCHER
 * ============================================================================ */

mpm_response_t mpm_process_request(const mpm_request_t *request) {
    mpm_response_t resp = {.error = MPM_ERR_INVALID_REQUEST};
    
    if (!kernel.initialized) {
        resp.error = MPM_ERR_INVALID_STATE;
        strcpy(resp.error_msg, "Kernel not initialized");
        return resp;
    }
    
    /* Check authorization */
    if (!mpm_authorize(request->type, request->caller)) {
        resp.error = MPM_ERR_UNAUTHORIZED;
        snprintf(resp.error_msg, MAX_ERROR_MSG,
                 "Caller %d not authorized for request type %d",
                 request->caller, request->type);
        return resp;
    }
    
    /* Dispatch to handler */
    switch (request->type) {
        case API_PROCESS_LIST:
            return handle_process_list(request);
        case API_PROCESS_KILL:
            return handle_process_kill(request);
        case API_PROCESS_RESTART:
            return handle_process_restart(request);
        case API_PROCESS_INFO:
            return handle_process_info(request);
        case API_EXEC_RUN:
            return handle_exec_run(request);
        case API_EXEC_REGISTER:
            return handle_exec_register(request);
        case API_MEMORY_INFO:
            return handle_memory_info(request);
        case API_TTY_COLOR:
            return handle_tty_color(request);
        default:
            resp.error = MPM_ERR_INVALID_REQUEST;
            snprintf(resp.error_msg, MAX_ERROR_MSG,
                     "Unknown request type: %d", request->type);
            return resp;
    }
}

/* ============================================================================
 * INITIALIZATION & SHUTDOWN
 * ============================================================================ */

int mpm_init(void) {
    if (kernel.initialized) {
        return 0;
    }
    
    memset(&kernel, 0, sizeof(kernel));
    
    kernel.boot_time = time(NULL);
    kernel.next_pid = 1;
    kernel.next_exec_id = 1;
    kernel.total_memory = 1024 * 1024 * 256; /* 256 MB simulated */
    kernel.used_memory = 0;
    kernel.current_tty = 0;
    kernel.color_scheme = 1;
    kernel.initialized = true;
    
    return 0;
}

void mpm_shutdown(void) {
    /* Kill all processes */
    for (int i = 0; i < kernel.process_count; i++) {
        if (kernel.processes[i].state != PROC_STATE_OFF) {
            kernel.processes[i].state = PROC_STATE_OFF;
        }
    }
    
    kernel.initialized = false;
}

/* ============================================================================
 * SYSTEM INFORMATION
 * ============================================================================ */

time_t mpm_get_uptime(void) {
    if (!kernel.initialized) return 0;
    return time(NULL) - kernel.boot_time;
}

int mpm_get_color_scheme(void) {
    return kernel.color_scheme;
}

size_t mpm_get_registry_count(void) {
    return kernel.registry_count;
}

const registry_entry_t* mpm_get_registry(void) {
    return kernel.registry;
}
