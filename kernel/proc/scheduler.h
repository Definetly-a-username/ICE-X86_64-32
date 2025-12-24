/*
 * ICE Operating System - Process Scheduler Header
 */

#ifndef ICE_SCHEDULER_H
#define ICE_SCHEDULER_H

#include "../types.h"

/* Maximum processes */
#define MAX_PROCESSES 64

/* Process states */
typedef enum {
    PROC_STATE_FREE = 0,
    PROC_STATE_READY,
    PROC_STATE_RUNNING,
    PROC_STATE_BLOCKED,
    PROC_STATE_ZOMBIE
} sched_state_t;

/* CPU context for context switching */
typedef struct {
    u32 eax, ebx, ecx, edx;
    u32 esi, edi, ebp;
    u32 eip;
    u32 esp;
    u32 eflags;
    u32 cr3;  /* Page directory */
} cpu_context_t;

/* Process Control Block */
typedef struct {
    ice_pid_t pid;
    exec_id_t exec_id;
    sched_state_t state;
    char name[32];
    
    /* CPU context */
    cpu_context_t context;
    
    /* Stack */
    u32 kernel_stack;
    u32 user_stack;
    
    /* Memory */
    u32 memory_used;
    
    /* TTY binding */
    int tty_id;
    
    /* Scheduling */
    u32 timeslice;
    u32 ticks_remaining;
} pcb_t;

/* Initialize scheduler */
void scheduler_init(void);

/* Create a new process */
ice_pid_t scheduler_create_process(const char *name, u32 entry_point);

/* Terminate a process */
void scheduler_kill_process(ice_pid_t pid);

/* Schedule next process (called from timer interrupt) */
void scheduler_tick(void);

/* Yield current process */
void scheduler_yield(void);

/* Get current process */
pcb_t* scheduler_get_current(void);

/* Get process by PID */
pcb_t* scheduler_get_process(ice_pid_t pid);

/* Get process count */
int scheduler_get_process_count(void);

/* List all processes */
void scheduler_list_processes(void (*callback)(pcb_t *proc));

#endif /* ICE_SCHEDULER_H */
