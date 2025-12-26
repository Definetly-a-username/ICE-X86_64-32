 

#ifndef ICE_SCHEDULER_H
#define ICE_SCHEDULER_H

#include "../types.h"

 
#define MAX_PROCESSES 64

 
typedef enum {
    PROC_STATE_FREE = 0,
    PROC_STATE_READY,
    PROC_STATE_RUNNING,
    PROC_STATE_BLOCKED,
    PROC_STATE_ZOMBIE
} sched_state_t;

 
typedef struct {
    u32 gs, fs, es, ds;      // Segment registers
    u32 edi, esi, ebp, esp;  // Pushed by pusha
    u32 ebx, edx, ecx, eax;  // Pushed by pusha
    u32 eip;                 // Instruction pointer
    u32 eflags;              // Flags
    u32 cr3;                 // CR3 (PDBR) - kept for future paging support
} cpu_context_t;

 
typedef struct {
    ice_pid_t pid;
    exec_id_t exec_id;
    sched_state_t state;
    char name[32];
    
     
    cpu_context_t context;
    
    u32 saved_esp;
    
    u32 kernel_stack;
    u32 user_stack;
    
     
    u32 memory_used;
    
     
    int tty_id;
    
     
    u32 timeslice;
    u32 ticks_remaining;
} pcb_t;

 
void scheduler_init(void);

 
ice_pid_t scheduler_create_process(const char *name, u32 entry_point);

 
void scheduler_kill_process(ice_pid_t pid);

 
void scheduler_tick(void);

 
void scheduler_yield(void);

 
pcb_t* scheduler_get_current(void);

 
pcb_t* scheduler_get_process(ice_pid_t pid);

 
int scheduler_get_process_count(void);

 
void scheduler_list_processes(void (*callback)(pcb_t *proc));

#endif  
