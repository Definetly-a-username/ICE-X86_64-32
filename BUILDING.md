# ICE Operating System - Build Requirements

## Required Tools

Install the following tools:

### Arch Linux
```bash
sudo pacman -S nasm qemu-full
```

### Ubuntu/Debian
```bash
sudo apt install nasm qemu-system-x86 grub-pc-bin xorriso
```

### Fedora
```bash
sudo dnf install nasm qemu-system-x86 grub2-tools xorriso
```

## Build Commands

```bash
# Build kernel
make

# Run in QEMU (direct kernel boot)
make run

# Create bootable ISO
make iso

# Run ISO in QEMU
make run-iso
```

## Cross-Compiler (Optional)

For a proper OS toolchain, build i686-elf-gcc:
https://wiki.osdev.org/GCC_Cross-Compiler
