# ICE Operating System

Welcome to the **ICE Operating System** project. ICE is a custom kernel and operating system designed for educational purposes and experimental systems development. It features a monolithic kernel architecture with support for modern hardware interfaces, filesystem management, and multitasking.

##  Features

- **Kernel Architecture**: Monolithic kernel design with protected mode support.
- **Filesystem Support**: Implementation of **FAT32** with planned transitions to **EXT2** (compatible with EXT4) for improved performance and reliability.
- **Memory Management**: Paging and heap management.
- **Networking**: Basic networking stack and driver framework.
- **Multitasking**: Preemptive multitasking and process management.
- **Drivers**: Integrated drivers for standard hardware peripherals.

##  Prerequisites

To build and run ICE, you need a Linux environment with the following tools installed:

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

##  Building and Running

The project includes a `Makefile` to simplify the build process.

### Build the Kernel
To compile the kernel and core components:
```bash
make
```

### Run in QEMU (Direct Kernel Boot)
To quickly test the kernel using QEMU:
```bash
make run
```

### Create Bootable ISO
To generate a bootable `.iso` image suitable for virtual machines or real hardware:
```bash
make iso
```

### Run ISO in QEMU
To boot the generated ISO image:
```bash
make run-iso
```

##  Contributing

Contributions are welcome! If you're interested in OS development, feel free to fork the repository and submit pull requests.

1.  Fork the Project
2.  Create your Feature Branch (`git checkout -b feature/AmazingFeature`)
3.  Commit your Changes (`git commit -m 'Add some AmazingFeature'`)
4.  Push to the Branch (`git push origin feature/AmazingFeature`)
5.  Open a Pull Request

##  License

This project is licensed under the **GNU General Public License v3.0**. See the [LICENSE](LICENSE) file for details.
