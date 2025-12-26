global process_switch_context

section .text

; void process_switch_context(cpu_context_t **old_context, cpu_context_t *new_context)
; Stack layout:
; [esp + 8] = new_context (pointer to struct)
; [esp + 4] = old_context (pointer to storage for old stack pointer)
; [esp + 0] = return address

process_switch_context:
    ; Save EFLAGS
    pushf

    ; Save Segment Registers
    push ds
    push es
    push fs
    push gs

    ; Save General Purpose Registers
    pusha

    ; Get arguments
    mov eax, [esp + 4 + 4 + 16 + 32] ; old_context (skip ret, flags, segs, pusha)
    mov edx, [esp + 8 + 4 + 16 + 32] ; new_context

    ; Save current stack pointer to *old_context
    mov [eax], esp

    ; Switch stack to new_context (which points to the top of the saved stack)
    ; In our struct, esp isn't the first member, but new_context points to the
    ; base of the struct where we expect the stack to be if we popped everything.
    ; Wait, the C struct `cpu_context_t` defines the layout of data ON THE STACK.
    ; So `new_context` IS the value we want to put in ESP.
    mov esp, edx

    ; Restore General Purpose Registers
    popa

    ; Restore Segment Registers
    pop gs
    pop fs
    pop es
    pop ds

    ; Restore EFLAGS
    popf

    ret
