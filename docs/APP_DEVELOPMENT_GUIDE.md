# ICE OS Application Development Guide

This guide explains how to create custom applications for ICE Operating System, including scripts in 15+ languages and Frost GUI applications.

## Table of Contents

1. [Overview](#overview)
2. [Getting Started](#getting-started)
3. [Supported Languages](#supported-languages)
4. [Frost GUI Framework](#frost-gui-framework)
5. [Python Applications](#python-applications)
6. [C/C++ Applications](#cc-applications)
7. [Rust Applications](#rust-applications)
8. [Shell Scripts](#shell-scripts)
9. [Other Languages](#other-languages)
10. [API Reference](#api-reference)
11. [Examples](#examples)

---

## Overview

ICE OS uses a dual-layer architecture:
- **TTY Layer**: Command-line interface with shell
- **Frost Layer**: Graphical user interface with widgets

The Application Process Manager (APM) handles script execution across multiple languages.

### Architecture

```
┌─────────────────────────────────────────┐
│              FROST GUI LAYER            │
│  (Calculator, Browser, Notepad, etc.)   │
├─────────────────────────────────────────┤
│              TTY/SHELL LAYER            │
│  (Commands, Scripts, Applications)      │
├─────────────────────────────────────────┤
│         APPLICATION MANAGER (APM)       │
│  (Python, Rust, Go, C, ASM, Lua, etc.)  │
├─────────────────────────────────────────┤
│              ICE KERNEL                 │
│  (VFS, EXT2, Memory, Drivers)           │
└─────────────────────────────────────────┘
```

---

## Getting Started

### Running Your First Script

1. Create a script file on the ICE filesystem:
   ```
   touch /hello.py
   write /hello.py print("Hello from ICE!")
   ```

2. Run the script:
   ```
   apm run /hello.py
   ```

### Launching Frost GUI

From TTY:
```
frost
```
Or select "Frost GUI" at boot menu.

---

## Supported Languages

| Extension | Language | Support Level | Use Case |
|-----------|----------|---------------|----------|
| `.c` | C | Full | System apps, drivers |
| `.cpp` | C++ | Full | Complex applications |
| `.asm` | Assembly | Full | Low-level, performance |
| `.rs` | Rust | Full | Safe systems programming |
| `.go` | Go | Basic | Concurrent applications |
| `.py` | Python | Basic | Quick scripts, automation |
| `.sh` | Shell | Full | System scripts |
| `.ice` | ICE Script | Full | Native scripting |
| `.js` | JavaScript | Basic | Web-style scripts |
| `.lua` | Lua | Basic | Embedded scripting |
| `.rb` | Ruby | Basic | Text processing |
| `.pl` | Perl | Basic | Text/regex processing |
| `.tcl` | Tcl | Basic | Tool command language |
| `.awk` | AWK | Basic | Data processing |
| `.bas` | BASIC | Basic | Educational/simple |
| `.bat` | Batch | Basic | DOS compatibility |
| `.html` | HTML | Display | Documentation |
| `.css` | CSS | Display | Styling |

---

## Frost GUI Framework

Frost is the graphical layer for ICE OS. It provides:
- Double-buffered, flicker-free rendering
- Widget-based UI (buttons, inputs, labels)
- Optimized dirty-rectangle updates
- Keyboard and mouse navigation

### Frost Built-in Apps

| App | Description | Key |
|-----|-------------|-----|
| Terminal | Return to TTY | 1 |
| Files | File manager | 2 |
| Notepad | Text editor | 3 |
| Calculator | Basic calculator | 4 |
| Browser | ICE web browser | 5 |
| Settings | System settings | 6 |
| System | System info | 7 |
| Games | Simple games | 8 |

### Frost Color Palette

```c
FROST_BG_DARK       // Deep background (0x0A)
FROST_BG_PANEL      // Panel background (0x01)
FROST_BG_WIDGET     // Widget background (0x09)
FROST_FG_PRIMARY    // White text (0x0F)
FROST_FG_SECONDARY  // Gray text (0x07)
FROST_FG_ACCENT     // Cyan accent (0x0B)
FROST_FG_HIGHLIGHT  // Yellow highlight (0x0E)
FROST_FG_SUCCESS    // Green success (0x0A)
FROST_FG_ERROR      // Red error (0x0C)
```

### Creating Frost Widgets

```c
// Create a label
frost_widget_t *label = frost_label(10, 5, "Hello World");

// Create a button
frost_widget_t *btn = frost_button(10, 7, "Click Me", on_click);

// Create an input field
frost_widget_t *input = frost_input(10, 9, 30);

// Create a checkbox
frost_widget_t *check = frost_checkbox(10, 11, "Enable", true);

// Create a progress bar
frost_widget_t *progress = frost_progress(10, 13, 40, 75, 100);
```

### Frost Dialogs

```c
// Message box
frost_msgbox("Title", "This is a message");

// Confirmation dialog
bool yes = frost_confirm("Question", "Are you sure?");
```

---

## Python Applications

ICE supports a subset of Python for quick scripting.

### Supported Features
- `print()` function
- Variable assignment
- String literals
- Comments with `#`

### Example

```python
# weather.py - Weather display app

title = "ICE Weather"
temp = "72F"
condition = "Sunny"

print("===================")
print(title)
print("===================")
print("")
print("Temperature:")
print(temp)
print("")
print("Conditions:")
print(condition)
```

Run: `apm run /weather.py`

---

## C/C++ Applications

C and C++ are the most powerful options for ICE applications.

### C Example

```c
// hello.c - Simple C program

#include <stdio.h>

int main() {
    printf("Hello from C!\n");
    printf("ICE OS C Runtime\n");
    
    int x = 10;
    int y = 20;
    printf("Sum: %d\n", x + y);
    
    return 0;
}
```

### C++ Example

```cpp
// app.cpp - C++ application

#include <iostream>
using namespace std;

class App {
public:
    void run() {
        cout << "C++ App Running!" << endl;
    }
};

int main() {
    App app;
    app.run();
    return 0;
}
```

---

## Rust Applications

Rust provides memory safety guarantees.

### Example

```rust
// calculator.rs - Rust calculator

fn add(a: i32, b: i32) -> i32 {
    a + b
}

fn main() {
    println!("Rust Calculator");
    
    let x = 15;
    let y = 27;
    let sum = add(x, y);
    
    println!("Result: {}", sum);
}
```

---

## Shell Scripts

Shell scripts are fully supported and most versatile.

### Complete Example

```bash
#!/bin/ice-shell
# sysinfo.sh - System information script

echo "================================"
echo "     ICE System Information"
echo "================================"
echo ""

echo ">> User Information"
whoami
echo ""

echo ">> Hostname"
hostname
echo ""

echo ">> System"
uname -a
echo ""

echo ">> Memory Usage"
free
echo ""

echo ">> Disk Usage"
df
echo ""

echo ">> Network"
ifconfig
echo ""

echo ">> Uptime"
uptime
echo ""

echo "================================"
echo "Report generated successfully"
echo "================================"
```

---

## Other Languages

### Go

```go
// server.go
package main

import "fmt"

func main() {
    fmt.Println("Go Application")
    fmt.Println("ICE OS Go Runtime")
}
```

### Lua

```lua
-- game.lua
print("Lua Game Engine")

score = 0
lives = 3

print("Score: " .. score)
print("Lives: " .. lives)
```

### Ruby

```ruby
# script.rb
puts "Ruby on ICE!"

name = "World"
puts "Hello, #{name}!"
```

### Assembly (x86)

```asm
; hello.asm
section .data
    msg db 'Hello from Assembly!', 0

section .text
global _start
_start:
    ; Print message
    mov eax, 4
    mov ebx, 1
    mov ecx, msg
    mov edx, 21
    int 0x80
    
    ; Exit
    mov eax, 1
    xor ebx, ebx
    int 0x80
```

---

## API Reference

### File Operations

| Command | Usage | Description |
|---------|-------|-------------|
| `cat` | `cat <file>` | Display file contents |
| `ls` | `ls [path]` | List directory |
| `touch` | `touch <file>` | Create empty file |
| `mkdir` | `mkdir <dir>` | Create directory |
| `rm` | `rm <file>` | Remove file |
| `rmdir` | `rmdir <dir>` | Remove empty directory |
| `cp` | `cp <src> <dst>` | Copy file |
| `mv` | `mv <src> <dst>` | Move/rename file |
| `write` | `write <file> <text>` | Write text to file |

### Output

| Command | Usage | Description |
|---------|-------|-------------|
| `echo` | `echo <text>` | Print text with newline |
| `print` | `print <text>` | Same as echo |

### Variables

| Command | Usage | Description |
|---------|-------|-------------|
| `set` | `set <name> <value>` | Set variable |
| `let` | `let <name> <value>` | Same as set |

### Control

| Command | Usage | Description |
|---------|-------|-------------|
| `sleep` | `sleep <ms>` | Sleep for milliseconds |
| `exit` | `exit [code]` | Exit script |
| `clear` | `clear` | Clear screen |

### System

| Command | Usage | Description |
|---------|-------|-------------|
| `whoami` | `whoami` | Show current user |
| `hostname` | `hostname` | Show hostname |
| `uname` | `uname [-a]` | System info |
| `uptime` | `uptime` | System uptime |
| `date` | `date` | Current time |
| `free` | `free` | Memory usage |
| `df` | `df` | Disk usage |

---

## Examples

### Example 1: System Monitor

```ice
# monitor.ice
echo "=== ICE System Monitor ==="
echo ""

echo ">> Memory"
free
echo ""

echo ">> Disk"
df
echo ""

echo ">> Network"
ifconfig
echo ""

echo "Monitor complete."
```

### Example 2: Backup Script

```bash
#!/bin/ice-shell
# backup.sh

echo "Starting backup..."
mkdir /backup
cp /config.txt /backup/config.txt
echo "Backup complete!"
ls /backup
```

### Example 3: Multi-language Project

```
/project/
  ├── main.c          # Core logic in C
  ├── utils.rs        # Utilities in Rust
  ├── config.py       # Configuration in Python
  ├── setup.sh        # Setup script
  └── README.md       # Documentation
```

---

## Tips

1. **Use the right language**: C/Rust for performance, Python/Shell for scripts
2. **Test in TTY first**: Debug before running in Frost
3. **Use absolute paths**: `/path/to/file` not `./file`
4. **Comment your code**: Future you will thank you
5. **Keep scripts modular**: Break complex tasks into multiple files

---

## Getting Help

- In TTY: `help` command
- In TTY: `apps` to list applications
- In TTY: `devhelp` for developer guide
- In Frost: System > Help

Happy coding on ICE OS with Frost!
