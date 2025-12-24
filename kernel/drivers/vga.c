/*
 * ICE Operating System - VGA Text Mode Implementation
 */

#include "vga.h"
#include <stdarg.h>

/* VGA state */
static u16 *vga_buffer;
static int cursor_x = 0;
static int cursor_y = 0;
static u8 current_color;

/* I/O helpers */
static inline void outb(u16 port, u8 value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline u8 inb(u16 port) {
    u8 ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* Create VGA entry */
static u16 vga_entry(char c, u8 color) {
    return (u16)c | ((u16)color << 8);
}

/* Make color byte */
static u8 vga_make_color(vga_color_t fg, vga_color_t bg) {
    return fg | (bg << 4);
}

/* Update hardware cursor */
static void update_cursor(void) {
    u16 pos = cursor_y * VGA_WIDTH + cursor_x;
    
    outb(0x3D4, 0x0F);
    outb(0x3D5, (u8)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (u8)((pos >> 8) & 0xFF));
}

void vga_init(void) {
    vga_buffer = (u16*)VGA_BUFFER;
    current_color = vga_make_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    cursor_x = 0;
    cursor_y = 0;
    vga_clear();
    vga_cursor_enable(true);
}

void vga_set_color(vga_color_t fg, vga_color_t bg) {
    current_color = vga_make_color(fg, bg);
}

void vga_clear(void) {
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = vga_entry(' ', current_color);
        }
    }
    cursor_x = 0;
    cursor_y = 0;
    update_cursor();
}

void vga_scroll(void) {
    /* Move all lines up by one */
    for (int y = 0; y < VGA_HEIGHT - 1; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = vga_buffer[(y + 1) * VGA_WIDTH + x];
        }
    }
    
    /* Clear last line */
    for (int x = 0; x < VGA_WIDTH; x++) {
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', current_color);
    }
    
    cursor_y = VGA_HEIGHT - 1;
}

void vga_putc(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\t') {
        cursor_x = (cursor_x + 8) & ~7;
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = vga_entry(' ', current_color);
        }
    } else {
        vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = vga_entry(c, current_color);
        cursor_x++;
    }
    
    /* Wrap line */
    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }
    
    /* Scroll if needed */
    if (cursor_y >= VGA_HEIGHT) {
        vga_scroll();
    }
    
    update_cursor();
}

void vga_puts(const char *s) {
    while (*s) {
        vga_putc(*s++);
    }
}

/* Simple number to string */
static void print_int(int n, int base, bool sign) {
    char buf[32];
    int i = 0;
    bool neg = false;
    
    if (n == 0) {
        vga_putc('0');
        return;
    }
    
    if (sign && n < 0) {
        neg = true;
        n = -n;
    }
    
    unsigned int un = (unsigned int)n;
    
    while (un > 0) {
        int digit = un % base;
        buf[i++] = digit < 10 ? '0' + digit : 'A' + digit - 10;
        un /= base;
    }
    
    if (neg) {
        vga_putc('-');
    }
    
    while (i > 0) {
        vga_putc(buf[--i]);
    }
}

void vga_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    while (*format) {
        if (*format == '%') {
            format++;
            switch (*format) {
                case 'd':
                case 'i':
                    print_int(va_arg(args, int), 10, true);
                    break;
                case 'u':
                    print_int(va_arg(args, unsigned int), 10, false);
                    break;
                case 'x':
                case 'X':
                    print_int(va_arg(args, unsigned int), 16, false);
                    break;
                case 's': {
                    const char *s = va_arg(args, const char*);
                    vga_puts(s ? s : "(null)");
                    break;
                }
                case 'c':
                    vga_putc((char)va_arg(args, int));
                    break;
                case '%':
                    vga_putc('%');
                    break;
                default:
                    vga_putc('%');
                    vga_putc(*format);
                    break;
            }
        } else {
            vga_putc(*format);
        }
        format++;
    }
    
    va_end(args);
}

void vga_set_cursor(int x, int y) {
    cursor_x = x;
    cursor_y = y;
    update_cursor();
}

void vga_get_cursor(int *x, int *y) {
    if (x) *x = cursor_x;
    if (y) *y = cursor_y;
}

void vga_cursor_enable(bool enable) {
    if (enable) {
        outb(0x3D4, 0x0A);
        outb(0x3D5, (inb(0x3D5) & 0xC0) | 14);  /* Start line 14 */
        outb(0x3D4, 0x0B);
        outb(0x3D5, (inb(0x3D5) & 0xE0) | 15);  /* End line 15 */
    } else {
        outb(0x3D4, 0x0A);
        outb(0x3D5, 0x20);  /* Disable cursor */
    }
}
