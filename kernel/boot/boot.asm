; ICE Operating System - Boot Entry
; Multiboot1 compliant boot stub for GRUB/QEMU
; x86 32-bit protected mode entry

BITS 32

; Multiboot1 constants (more widely supported)
MULTIBOOT_MAGIC         equ 0x1BADB002
MULTIBOOT_ALIGN         equ 1<<0            ; Align modules on page boundaries
MULTIBOOT_MEMINFO       equ 1<<1            ; Provide memory map
MULTIBOOT_FLAGS         equ MULTIBOOT_ALIGN | MULTIBOOT_MEMINFO
MULTIBOOT_CHECKSUM      equ -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

; Stack size
KERNEL_STACK_SIZE       equ 16384

section .multiboot
align 4
multiboot_header:
    dd MULTIBOOT_MAGIC
    dd MULTIBOOT_FLAGS
    dd MULTIBOOT_CHECKSUM

section .bss
align 16
stack_bottom:
    resb KERNEL_STACK_SIZE
stack_top:

section .text
global _start
extern kernel_main

_start:
    ; Visual Debug: Print "OK" to top-left corner
    mov dword [0xB8000], 0x2F4B2F4F

    ; Disable interrupts
    cli

    ; Set up stack
    mov esp, stack_top

    ; Reset EFLAGS
    push 0
    popf

    ; Push multiboot info pointer (ebx)
    push ebx
    ; Push multiboot magic number (eax)
    push eax

    ; Call kernel_main(magic, mboot_info)
    call kernel_main

    ; If kernel returns, halt
.halt:
    cli
    hlt
    jmp .halt

global gdt_flush
gdt_flush:
    mov eax, [esp+4]    ; Get pointer to gdt_ptr
    lgdt [eax]          ; Load GDT

    mov ax, 0x10        ; Kernel Data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    jmp 0x08:.flush     ; Far jump to reload CS
.flush:
    ret

global tss_flush
tss_flush:
    mov ax, 0x28        ; TSS segment
    ltr ax              ; Load Task Register
    ret
