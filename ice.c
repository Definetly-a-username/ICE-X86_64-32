/*
 * ICE Operating System - Unified Shell Entry Point
 * Main entry for the ICE operating system simulation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "mpm/core/mpm.h"
#include "tty/tty.h"

/* Forward declarations for managers */
int pm_main(int argc, char *argv[]);
int gpm_main(int argc, char *argv[]);

#define ICE_VERSION "1.0.0"
#define MAX_INPUT 1024
#define MAX_ARGS 32

/* ============================================================================
 * SIGNAL HANDLING
 * ============================================================================ */

static volatile int running = 1;

static void signal_handler(int sig) {
    if (sig == SIGINT) {
        printf("\nUse 'pm exit' to quit.\n");
    }
}

/* ============================================================================
 * COMMAND PARSING
 * ============================================================================ */

static int parse_command(char *input, char **argv) {
    int argc = 0;
    char *token = strtok(input, " \t\n");
    
    while (token && argc < MAX_ARGS - 1) {
        argv[argc++] = token;
        token = strtok(NULL, " \t\n");
    }
    argv[argc] = NULL;
    
    return argc;
}

/* ============================================================================
 * BANNER
 * ============================================================================ */

static void print_banner(void) {
    printf("\n");
    printf("  ██╗ ██████╗███████╗\n");
    printf("  ██║██╔════╝██╔════╝\n");
    printf("  ██║██║     █████╗  \n");
    printf("  ██║██║     ██╔══╝  \n");
    printf("  ██║╚██████╗███████╗\n");
    printf("  ╚═╝ ╚═════╝╚══════╝\n");
    printf("\n");
    printf("  ICE Operating System v%s\n", ICE_VERSION);
    printf("  MPM Kernel initialized.\n");
    printf("\n");
    printf("  Type 'pm' for process management.\n");
    printf("  Type 'gpm' for system management.\n");
    printf("  Type 'help' for more commands.\n");
    printf("\n");
}

/* ============================================================================
 * SHELL COMMANDS
 * ============================================================================ */

static void cmd_help(void) {
    printf("ICE Shell Commands:\n");
    printf("\n");
    printf("  pm [cmd]     Process Manager\n");
    printf("               pm         List processes\n");
    printf("               pm crp     List processes\n");
    printf("               pm kp <id> Kill process\n");
    printf("               pm rp <id> Restart process\n");
    printf("               pm val     Boot validation\n");
    printf("               pm exit    Exit shell\n");
    printf("\n");
    printf("  gpm [cmd]    General Process Manager\n");
    printf("               gpm version   Show version\n");
    printf("               gpm uptime    System uptime\n");
    printf("               gpm mem       Memory info\n");
    printf("               gpm clear     Clear screen\n");
    printf("               gpm info      System info\n");
    printf("               gpm col <1-4> TTY color\n");
    printf("               gpm run <id>  Execute by ID\n");
    printf("               gpm mef <path> Create executable\n");
    printf("               gpm list      List executables\n");
    printf("\n");
    printf("  help         Show this help\n");
    printf("  clear        Clear screen\n");
    printf("  exit         Exit shell\n");
    printf("\n");
}

/* ============================================================================
 * MAIN SHELL LOOP
 * ============================================================================ */

int main(int argc, char *argv[]) {
    /* Initialize kernel */
    if (mpm_init() != 0) {
        fprintf(stderr, "Error: Failed to initialize MPM kernel\n");
        return 1;
    }
    
    /* Initialize TTY */
    tty_init();
    
    /* Run boot validation */
    printf("Running boot validation...\n");
    mpm_validate_system();
    
    /* Setup signal handler */
    signal(SIGINT, signal_handler);
    
    /* Print banner */
    print_banner();
    
    /* Main shell loop */
    char input[MAX_INPUT];
    char *args[MAX_ARGS];
    
    while (running) {
        printf("ice> ");
        fflush(stdout);
        
        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }
        
        /* Remove trailing newline */
        size_t len = strlen(input);
        if (len > 0 && input[len-1] == '\n') {
            input[len-1] = '\0';
        }
        
        /* Skip empty lines */
        if (input[0] == '\0') {
            continue;
        }
        
        /* Parse command */
        int nargs = parse_command(input, args);
        if (nargs == 0) continue;
        
        const char *cmd = args[0];
        
        /* Dispatch to appropriate handler */
        if (strcmp(cmd, "pm") == 0) {
            pm_main(nargs, args);
        } else if (strcmp(cmd, "gpm") == 0) {
            gpm_main(nargs, args);
        } else if (strcmp(cmd, "help") == 0) {
            cmd_help();
        } else if (strcmp(cmd, "clear") == 0) {
            tty_clear();
        } else if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
            running = 0;
        } else {
            printf("Unknown command: %s\n", cmd);
            printf("Type 'help' for available commands.\n");
        }
    }
    
    /* Cleanup */
    printf("Shutting down ICE...\n");
    tty_shutdown();
    mpm_shutdown();
    
    return 0;
}
