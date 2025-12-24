/*
 * ICE Operating System - TTY Implementation
 */

#include "tty.h"
#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include <stdarg.h>

/* TTY state */
static int current_scheme = TTY_SCHEME_DEFAULT;
static const char *current_prompt = "ice> ";

/* Color scheme definitions: {fg, bg} */
static const int schemes[5][2] = {
    {VGA_COLOR_WHITE, VGA_COLOR_BLACK},         /* Unused */
    {VGA_COLOR_WHITE, VGA_COLOR_BLACK},         /* Default */
    {VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK},   /* Dark/Matrix */
    {VGA_COLOR_BLACK, VGA_COLOR_WHITE},         /* Light */
    {VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK},    /* Mono */
};

void tty_init(void) {
    current_scheme = TTY_SCHEME_DEFAULT;
    tty_set_color_scheme(current_scheme);
}

void tty_set_color_scheme(int scheme) {
    if (scheme < 1 || scheme > 4) return;
    
    current_scheme = scheme;
    vga_set_color(schemes[scheme][0], schemes[scheme][1]);
}

int tty_get_color_scheme(void) {
    return current_scheme;
}

void tty_puts(const char *s) {
    vga_puts(s);
}

void tty_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    while (*format) {
        if (*format == '%') {
            format++;
            switch (*format) {
                case 'd':
                case 'i': {
                    int n = va_arg(args, int);
                    vga_printf("%d", n);
                    break;
                }
                case 'u': {
                    unsigned int n = va_arg(args, unsigned int);
                    vga_printf("%u", n);
                    break;
                }
                case 'x':
                case 'X': {
                    unsigned int n = va_arg(args, unsigned int);
                    vga_printf("%x", n);
                    break;
                }
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

int tty_getline(char *buffer, int max_len) {
    return keyboard_getline(buffer, max_len);
}

void tty_clear(void) {
    vga_clear();
}

void tty_set_prompt(const char *prompt) {
    current_prompt = prompt;
}

void tty_print_prompt(void) {
    vga_set_color(VGA_COLOR_LIGHT_CYAN, schemes[current_scheme][1]);
    vga_puts(current_prompt);
    vga_set_color(schemes[current_scheme][0], schemes[current_scheme][1]);
}
