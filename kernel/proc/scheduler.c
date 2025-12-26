 

#include "scheduler.h"
#include "../drivers/vga.h"
#include "../mm/pmm.h"

 
static pcb_t process_table[MAX_PROCESSES];
static ice_pid_t next_pid = 1;
static int current_process = -1;
static int process_count = 0;

 
#define DEFAULT_TIMESLICE 10

 
static void strncpy_s(char *dest, const char *src, int n) {
    int i;
    for (i = 0; i < n - 1 && src[i]; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
}

void scheduler_init(void) {
     
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_table[i].state = PROC_STATE_FREE;
        process_table[i].pid = 0;
    }
    
    next_pid = 1;
    current_process = -1;
    process_count = 0;
}

ice_pid_t scheduler_create_process(const char *name, u32 entry_point) {
     
    int slot = -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == PROC_STATE_FREE) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        return 0;   
    }
    
    pcb_t *proc = &process_table[slot];
    
     
    proc->pid = next_pid++;
    proc->exec_id = proc->pid;   
    proc->state = PROC_STATE_READY;
    strncpy_s(proc->name, name, sizeof(proc->name));
    
    // Allocate kernel stack
    proc->kernel_stack = pmm_alloc_page();
    if (!proc->kernel_stack) {
        return 0;   
    }
    
    // Initialize context on the stack
    // Stack grows down: EIP, EFLAGS, Segs(4), Regs(8)
    u32 *stack = (u32*)(proc->kernel_stack + PAGE_SIZE);
    
    *(--stack) = entry_point;    // Return address (EIP)
    *(--stack) = 0x202;          // EFLAGS (IF | reserved)
    
    *(--stack) = 0x10;           // GS
    *(--stack) = 0x10;           // FS
    *(--stack) = 0x10;           // ES
    *(--stack) = 0x10;           // DS
    
    *(--stack) = 0;              // EAX
    *(--stack) = 0;              // ECX
    *(--stack) = 0;              // EDX
    *(--stack) = 0;              // EBX
    *(--stack) = 0;              // ESP (ignored)
    *(--stack) = 0;              // EBP
    *(--stack) = 0;              // ESI
    *(--stack) = 0;              // EDI
    
    proc->saved_esp = (u32)stack;
    
    // Legacy support: keep struct updated if anyone uses it
    proc->context.eip = entry_point;
    proc->context.esp = proc->saved_esp;
    
    proc->memory_used = PAGE_SIZE;
    proc->tty_id = 0;
    proc->timeslice = DEFAULT_TIMESLICE;
    proc->ticks_remaining = DEFAULT_TIMESLICE;
    
    process_count++;
    
    return proc->pid;
}

void scheduler_kill_process(ice_pid_t pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].pid == pid && process_table[i].state != PROC_STATE_FREE) {
             
            if (process_table[i].kernel_stack) {
                pmm_free_page(process_table[i].kernel_stack);
            }
            
            process_table[i].state = PROC_STATE_FREE;
            process_table[i].pid = 0;
            process_count--;
            
             
            if (i == current_process) {
                current_process = -1;
            }
            
            return;
        }
    }
}

void scheduler_tick(void) {
    if (current_process < 0) return;
    
    pcb_t *proc = &process_table[current_process];
    if (proc->state == PROC_STATE_RUNNING) {
        proc->ticks_remaining--;
        
        if (proc->ticks_remaining == 0) {
             
            proc->state = PROC_STATE_READY;
            proc->ticks_remaining = proc->timeslice;
            scheduler_yield();
        }
    }
}

extern void process_switch_context(u32 *old_esp_ptr, u32 new_esp);

void scheduler_yield(void) {
    if (process_count == 0) return;
    
    // Round robin scheduling
    int start = (current_process + 1) % MAX_PROCESSES;
    int i = start;
    int next_process = -1;
    
    do {
        if (process_table[i].state == PROC_STATE_READY) {
            next_process = i;
            break;
        }
        i = (i + 1) % MAX_PROCESSES;
    } while (i != start);
    
    if (next_process >= 0) {
        // Switch required
        int prev_process = current_process;
        pcb_t *prev = prev_process >= 0 ? &process_table[prev_process] : 0;
        pcb_t *next = &process_table[next_process];
        
        if (prev && prev->state == PROC_STATE_RUNNING) {
            prev->state = PROC_STATE_READY;
        }
        
        next->state = PROC_STATE_RUNNING;
        current_process = next_process;
        
        // Perform actual context switch
        if (prev) {
            process_switch_context(&prev->saved_esp, next->saved_esp);
        } else {
             // First switch, special case: just dummy old pointer
            u32 dummy;
            process_switch_context(&dummy, next->saved_esp);
        }
    }
}

pcb_t* scheduler_get_current(void) {
    if (current_process < 0) return 0;
    return &process_table[current_process];
}

pcb_t* scheduler_get_process(ice_pid_t pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].pid == pid && process_table[i].state != PROC_STATE_FREE) {
            return &process_table[i];
        }
    }
    return 0;
}

int scheduler_get_process_count(void) {
    return process_count;
}

void scheduler_list_processes(void (*callback)(pcb_t *proc)) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state != PROC_STATE_FREE) {
            callback(&process_table[i]);
        }
    }
}
