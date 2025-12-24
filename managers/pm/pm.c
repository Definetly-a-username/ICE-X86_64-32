/*
 * ICE Operating System - Process Manager (pm)
 * User-facing process management interface
 * 
 * pm does NOT touch hardware directly.
 * All operations go through EXC API to MPM.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../mpm/core/mpm.h"
#include "../../tty/tty.h"

/* ============================================================================
 * HELPER FUNCTIONS
 * ============================================================================ */

static void print_usage(void) {
    printf("ICE Process Manager\n");
    printf("\n");
    printf("Usage:\n");
    printf("  pm          List running processes (same as pm crp)\n");
    printf("  pm crp      List running processes\n");
    printf("  pm kp <id>  Kill process by executable ID\n");
    printf("  pm rp <id>  Restart process by executable ID\n");
    printf("  pm exit     Exit current shell\n");
    printf("\n");
}

static exec_id_t parse_exec_id(const char *str) {
    exec_id_t id = 0;
    if (str[0] == '#') {
        /* Format: #00000001 */
        id = (exec_id_t)strtoul(str + 1, NULL, 16);
    } else {
        /* Plain number */
        id = (exec_id_t)strtoul(str, NULL, 10);
    }
    return id;
}

static const char* state_to_string(proc_state_t state) {
    switch (state) {
        case PROC_STATE_OFF:    return "OFF";
        case PROC_STATE_ON:     return "ON";
        case PROC_STATE_PAUSED: return "PAUSED";
        case PROC_STATE_ZOMBIE: return "ZOMBIE";
        default:                return "UNKNOWN";
    }
}

/* ============================================================================
 * COMMANDS
 * ============================================================================ */

static int cmd_list_processes(void) {
    mpm_request_t req = {
        .type = API_PROCESS_LIST,
        .caller = CALLER_PM
    };
    
    mpm_response_t resp = mpm_process_request(&req);
    
    if (resp.error != MPM_OK) {
        fprintf(stderr, "Error: %s\n", mpm_error_string(resp.error));
        return 1;
    }
    
    int count = resp.data.process_list.count;
    
    if (count == 0) {
        printf("No running processes.\n");
        return 0;
    }
    
    printf("%-12s %-12s %-10s %-10s %-6s\n", 
           "EXEC_ID", "PID", "STATE", "MEMORY", "TTY");
    printf("%-12s %-12s %-10s %-10s %-6s\n",
           "-------", "---", "-----", "------", "---");
    
    for (int i = 0; i < count; i++) {
        process_info_t *p = &resp.data.process_list.processes[i];
        char id_str[16];
        snprintf(id_str, sizeof(id_str), EXEC_ID_FORMAT, p->exec_id);
        
        printf("%-12s %-12u %-10s %-10zu %-6d\n",
               id_str, p->pid, state_to_string(p->state),
               p->memory_used, p->tty_id);
    }
    
    printf("\nTotal: %d process(es)\n", count);
    return 0;
}

static int cmd_kill_process(const char *id_str) {
    exec_id_t id = parse_exec_id(id_str);
    if (id == EXEC_ID_INVALID) {
        fprintf(stderr, "Invalid executable ID: %s\n", id_str);
        return 1;
    }
    
    mpm_request_t req = {
        .type = API_PROCESS_KILL,
        .caller = CALLER_PM,
        .params.process.exec_id = id
    };
    
    mpm_response_t resp = mpm_process_request(&req);
    
    if (resp.error != MPM_OK) {
        fprintf(stderr, "Error: %s\n", mpm_error_string(resp.error));
        if (resp.error_msg[0]) {
            fprintf(stderr, "       %s\n", resp.error_msg);
        }
        return 1;
    }
    
    printf("Process " EXEC_ID_FORMAT " terminated.\n", id);
    return 0;
}

static int cmd_restart_process(const char *id_str) {
    exec_id_t id = parse_exec_id(id_str);
    if (id == EXEC_ID_INVALID) {
        fprintf(stderr, "Invalid executable ID: %s\n", id_str);
        return 1;
    }
    
    mpm_request_t req = {
        .type = API_PROCESS_RESTART,
        .caller = CALLER_PM,
        .params.process.exec_id = id
    };
    
    mpm_response_t resp = mpm_process_request(&req);
    
    if (resp.error != MPM_OK) {
        fprintf(stderr, "Error: %s\n", mpm_error_string(resp.error));
        return 1;
    }
    
    printf("Process " EXEC_ID_FORMAT " restarted.\n", id);
    return 0;
}

static int cmd_exit(void) {
    printf("Exiting shell.\n");
    exit(0);
    return 0;
}

/* ============================================================================
 * MAIN
 * ============================================================================ */

int pm_main(int argc, char *argv[]) {
    /* Initialize MPM if needed */
    mpm_init();
    
    if (argc < 2) {
        /* Default: list processes */
        return cmd_list_processes();
    }
    
    const char *cmd = argv[1];
    
    if (strcmp(cmd, "crp") == 0) {
        return cmd_list_processes();
    } else if (strcmp(cmd, "kp") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: pm kp <exec_id>\n");
            return 1;
        }
        return cmd_kill_process(argv[2]);
    } else if (strcmp(cmd, "rp") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: pm rp <exec_id>\n");
            return 1;
        }
        return cmd_restart_process(argv[2]);
    } else if (strcmp(cmd, "exit") == 0) {
        return cmd_exit();
    } else if (strcmp(cmd, "val") == 0) {
        /* Boot validation - implemented in boot/val.c */
        printf("Running boot validation...\n");
        return mpm_validate_system();
    } else if (strcmp(cmd, "-h") == 0 || strcmp(cmd, "--help") == 0) {
        print_usage();
        return 0;
    } else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        print_usage();
        return 1;
    }
}

#ifndef ICE_UNIFIED_BUILD
int main(int argc, char *argv[]) {
    return pm_main(argc, argv);
}
#endif
