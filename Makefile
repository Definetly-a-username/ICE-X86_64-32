# ICE Operating System - Kernel Makefile

# Compiler tools
CC := gcc
AS := nasm
LD := ld

# Flags
CFLAGS = -m32 -ffreestanding -fno-stack-protector -fno-pic -nostdlib \
         -Wall -Wextra -Wno-unused-function -I. -Ikernel -O2 \
         -mno-sse -mno-sse2 -mno-mmx -m80387
ASFLAGS = -f elf32
LDFLAGS = -m elf_i386 -T kernel/linker.ld -nostdlib

# Directories
KERNEL_DIR = kernel
BUILD_DIR = build
ISO_DIR = iso

# Source files
ASM_SOURCES = $(KERNEL_DIR)/boot/boot.asm \
              $(KERNEL_DIR)/cpu/isr.asm

C_SOURCES = $(KERNEL_DIR)/kernel_main.c \
            $(KERNEL_DIR)/cpu/gdt.c \
            $(KERNEL_DIR)/cpu/idt.c \
            $(KERNEL_DIR)/drivers/pic.c \
            $(KERNEL_DIR)/drivers/vga.c \
            $(KERNEL_DIR)/drivers/pit.c \
            $(KERNEL_DIR)/drivers/keyboard.c \
            $(KERNEL_DIR)/drivers/ata.c \
            $(KERNEL_DIR)/mm/pmm.c \
            $(KERNEL_DIR)/tty/tty.c \
            $(KERNEL_DIR)/tty/console.c \
            $(KERNEL_DIR)/core/mpm.c \
            $(KERNEL_DIR)/core/exc.c \
            $(KERNEL_DIR)/core/bootval.c \
            $(KERNEL_DIR)/core/user.c \
            $(KERNEL_DIR)/core/sysinfo.c \
            $(KERNEL_DIR)/proc/scheduler.c \
            $(KERNEL_DIR)/fs/fat32.c \
            $(KERNEL_DIR)/fs/ext2.c \
            $(KERNEL_DIR)/apps/apps.c \
            $(KERNEL_DIR)/apps/apm.c \
            $(KERNEL_DIR)/net/net.c

# Object files
ASM_OBJECTS = $(patsubst %.asm,$(BUILD_DIR)/%.o,$(notdir $(ASM_SOURCES)))
C_OBJECTS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(notdir $(C_SOURCES)))
OBJECTS = $(ASM_OBJECTS) $(C_OBJECTS)

# Output
KERNEL = $(BUILD_DIR)/ice.bin
ISO = ice.iso

.PHONY: all clean run iso dirs

all: dirs $(KERNEL)

dirs:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(ISO_DIR)/boot/grub

$(KERNEL): $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^
	@echo "Kernel built: $@"
	@echo "Size: $$(stat -c%s $@) bytes"

# Assembly files
$(BUILD_DIR)/boot.o: $(KERNEL_DIR)/boot/boot.asm
	$(AS) $(ASFLAGS) -o $@ $<

$(BUILD_DIR)/isr.o: $(KERNEL_DIR)/cpu/isr.asm
	$(AS) $(ASFLAGS) -o $@ $<

# Generic C compilation rule
$(BUILD_DIR)/%.o: $(KERNEL_DIR)/*/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/kernel_main.o: $(KERNEL_DIR)/kernel_main.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Create bootable ISO
iso: $(KERNEL)
	@mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL) $(ISO_DIR)/boot/ice.bin
	cp grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) $(ISO_DIR) 2>/dev/null || \
		echo "grub-mkrescue not found."
	@echo "ISO created: $(ISO)"

# Run in QEMU
run: $(KERNEL) disk.img
	qemu-system-i386 -kernel $(KERNEL) -drive file=disk.img,format=raw -m 128M -display sdl

run-gtk: $(KERNEL)
	qemu-system-i386 -kernel $(KERNEL) -drive file=disk.img,format=raw -m 128M -display gtk

run-curses: $(KERNEL)
	qemu-system-i386 -kernel $(KERNEL) -drive file=disk.img,format=raw -m 128M -display curses

# Run with network (RTL8139)
run-net: $(KERNEL)
	qemu-system-i386 -kernel $(KERNEL) -drive file=disk.img,format=raw -m 128M -display sdl \
		-netdev user,id=net0 -device rtl8139,netdev=net0

run-iso: iso
	qemu-system-i386 -cdrom $(ISO) -m 128M -display sdl

run-disk: $(KERNEL)
	qemu-system-i386 -kernel $(KERNEL) -drive file=disk.img,format=raw -m 128M -drive file=disk.img,format=raw -display sdl

clean:
	rm -rf $(BUILD_DIR)
	rm -f $(ISO)
	rm -rf $(ISO_DIR)/boot/ice.bin

# Disk image
disk.img:  
	dd if=/dev/zero of=disk.img bs=1M count=32 && mkfs.ext2 -F disk.img
