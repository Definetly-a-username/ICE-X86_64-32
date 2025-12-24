/*
 * ICE Operating System - General Process Manager (gpm)
 * System inspection, management, and executable tools
 * 
 * gpm does NOT touch hardware directly.
 * All operations go through EXC API to MPM.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>

#include "../../mpm/core/mpm.h"
#include "../../EXC/format/exc.h"
#include "../../EXC/registry/registry.h"
#include "../../tty/tty.h"

#define ICE_VERSION "1.0.0"

/* Forward declarations */
int mef_main(int argc, char *argv[]);

/* ============================================================================
 * HELPER FUNCTIONS
 * ============================================================================ */

static void print_usage(void) {
    printf("ICE General Process Manager\n");
    printf("\n");
    printf("Usage:\n");
    printf("  gpm version         Show ICE version\n");
    printf("  gpm uptime          Show system uptime\n");
    printf("  gpm mem             Show memory information\n");
    printf("  gpm clear           Clear the screen\n");
    printf("  gpm info            Show system information\n");
    printf("  gpm col <1-4>       Set TTY color scheme\n");
    printf("  gpm run <id>        Execute by ID\n");
    printf("  gpm mef <path>      Create executable from source\n");
    printf("  gpm list            List registered executables\n");
    printf("\n");
}

static exec_id_t parse_exec_id(const char *str) {
    exec_id_t id = 0;
    if (str[0] == '#') {
        id = (exec_id_t)strtoul(str + 1, NULL, 16);
    } else {
        id = (exec_id_t)strtoul(str, NULL, 10);
    }
    return id;
}

/* ============================================================================
 * COMMANDS
 * ============================================================================ */

static int cmd_version(void) {
    printf("ICE Operating System v%s\n", ICE_VERSION);
    printf("MPM (Main Process Manager) Kernel\n");
    return 0;
}

static int cmd_uptime(void) {
    time_t uptime = mpm_get_uptime();
    
    int days = uptime / 86400;
    int hours = (uptime % 86400) / 3600;
    int mins = (uptime % 3600) / 60;
    int secs = uptime % 60;
    
    printf("System uptime: ");
    if (days > 0) printf("%d day(s), ", days);
    printf("%02d:%02d:%02d\n", hours, mins, secs);
    
    return 0;
}

static int cmd_mem(void) {
    mpm_request_t req = {
        .type = API_MEMORY_INFO,
        .caller = CALLER_GPM
    };
    
    mpm_response_t resp = mpm_process_request(&req);
    
    if (resp.error != MPM_OK) {
        fprintf(stderr, "Error: %s\n", mpm_error_string(resp.error));
        return 1;
    }
    
    size_t total = resp.data.memory_info.total;
    size_t used = resp.data.memory_info.used;
    size_t free = resp.data.memory_info.free;
    
    printf("Memory Information:\n");
    printf("  Total:  %zu bytes (%.2f MB)\n", total, (double)total / (1024*1024));
    printf("  Used:   %zu bytes (%.2f MB)\n", used, (double)used / (1024*1024));
    printf("  Free:   %zu bytes (%.2f MB)\n", free, (double)free / (1024*1024));
    printf("  Usage:  %.1f%%\n", 100.0 * used / total);
    
    return 0;
}

static int cmd_clear(void) {
    /* ANSI clear screen */
    printf("\033[2J\033[H");
    fflush(stdout);
    return 0;
}

static int cmd_info(void) {
    printf("ICE Operating System\n");
    printf("====================\n");
    printf("\n");
    printf("Kernel:     MPM (Main Process Manager)\n");
    printf("Version:    %s\n", ICE_VERSION);
    printf("Type:       Monolithic, Single-user\n");
    printf("Exec:       .exc format only\n");
    printf("\n");
    
    cmd_uptime();
    printf("\n");
    
    printf("Registered executables: %zu\n", mpm_get_registry_count());
    
    return 0;
}

static int cmd_color(int scheme) {
    if (scheme < 1 || scheme > 4) {
        fprintf(stderr, "Color scheme must be 1-4\n");
        return 1;
    }
    
    mpm_request_t req = {
        .type = API_TTY_COLOR,
        .caller = CALLER_GPM,
        .params.tty.color_scheme = scheme
    };
    
    mpm_response_t resp = mpm_process_request(&req);
    
    if (resp.error != MPM_OK) {
        fprintf(stderr, "Error: %s\n", mpm_error_string(resp.error));
        return 1;
    }
    
    /* Apply color scheme */
    tty_set_color_scheme(scheme);
    printf("Color scheme set to %d\n", scheme);
    
    return 0;
}

static int cmd_run(const char *id_str) {
    exec_id_t id = parse_exec_id(id_str);
    if (id == EXEC_ID_INVALID) {
        fprintf(stderr, "Invalid executable ID: %s\n", id_str);
        return 1;
    }
    
    mpm_request_t req = {
        .type = API_EXEC_RUN,
        .caller = CALLER_GPM,
        .params.exec.exec_id = id
    };
    
    mpm_response_t resp = mpm_process_request(&req);
    
    if (resp.error != MPM_OK) {
        fprintf(stderr, "Error: %s\n", mpm_error_string(resp.error));
        if (resp.error_msg[0]) {
            fprintf(stderr, "       %s\n", resp.error_msg);
        }
        return 1;
    }
    
    printf("Started " EXEC_ID_FORMAT " (PID: %u)\n", 
           resp.data.exec.exec_id, resp.data.exec.pid);
    
    return 0;
}

static int cmd_list(void) {
    size_t count = mpm_get_registry_count();
    const registry_entry_t *reg = mpm_get_registry();
    
    if (count == 0) {
        printf("No registered executables.\n");
        return 0;
    }
    
    printf("%-12s %-24s %-10s %-8s\n", "ID", "NAME", "TYPE", "FLAGS");
    printf("%-12s %-24s %-10s %-8s\n", "--", "----", "----", "-----");
    
    for (size_t i = 0; i < count; i++) {
        if (!reg[i].valid) continue;
        if (reg[i].flags & EXC_FLAG_HIDDEN) continue; /* Hide kernel executables */
        
        char id_str[16];
        snprintf(id_str, sizeof(id_str), EXEC_ID_FORMAT, reg[i].id);
        
        const char *flags = (reg[i].flags & EXC_FLAG_KRNL) ? "KRNL" : "-";
        
        printf("%-12s %-24s %-10s %-8s\n",
               id_str, reg[i].name, exc_type_string(reg[i].type), flags);
    }
    
    return 0;
}

/* ============================================================================
 * MAIN
 * ============================================================================ */

int gpm_main(int argc, char *argv[]) {
    /* Initialize MPM if needed */
    mpm_init();
    
    if (argc < 2) {
        print_usage();
        return 0;
    }
    
    const char *cmd = argv[1];
    
    if (strcmp(cmd, "version") == 0) {
        return cmd_version();
    } else if (strcmp(cmd, "uptime") == 0) {
        return cmd_uptime();
    } else if (strcmp(cmd, "mem") == 0) {
        return cmd_mem();
    } else if (strcmp(cmd, "clear") == 0) {
        return cmd_clear();
    } else if (strcmp(cmd, "info") == 0) {
        return cmd_info();
    } else if (strcmp(cmd, "col") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: gpm col <1-4>\n");
            return 1;
        }
        return cmd_color(atoi(argv[2]));
    } else if (strcmp(cmd, "run") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: gpm run <exec_id>\n");
            return 1;
        }
        return cmd_run(argv[2]);
    } else if (strcmp(cmd, "mef") == 0) {
        return mef_main(argc - 1, argv + 1);
    } else if (strcmp(cmd, "list") == 0) {
        return cmd_list();
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
    return gpm_main(argc, argv);
}
#endif
