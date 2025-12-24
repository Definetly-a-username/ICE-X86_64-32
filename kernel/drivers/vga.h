/*
 * ICE Operating System - VGA Text Mode Header
 */

#ifndef ICE_VGA_H
#define ICE_VGA_H

#include "../types.h"

/* VGA text mode dimensions */
#define VGA_WIDTH  80
#define VGA_HEIGHT 25

/* VGA text mode buffer */
#define VGA_BUFFER 0xB8000

/* VGA colors */
typedef enum {
    VGA_COLOR_BLACK         = 0,
    VGA_COLOR_BLUE          = 1,
    VGA_COLOR_GREEN         = 2,
    VGA_COLOR_CYAN          = 3,
    VGA_COLOR_RED           = 4,
    VGA_COLOR_MAGENTA       = 5,
    VGA_COLOR_BROWN         = 6,
    VGA_COLOR_LIGHT_GREY    = 7,
    VGA_COLOR_DARK_GREY     = 8,
    VGA_COLOR_LIGHT_BLUE    = 9,
    VGA_COLOR_LIGHT_GREEN   = 10,
    VGA_COLOR_LIGHT_CYAN    = 11,
    VGA_COLOR_LIGHT_RED     = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN   = 14,
    VGA_COLOR_WHITE         = 15,
} vga_color_t;

/* Initialize VGA */
void vga_init(void);

/* Set text color */
void vga_set_color(vga_color_t fg, vga_color_t bg);

/* Clear screen */
void vga_clear(void);

/* Put character at current position */
void vga_putc(char c);

/* Print string */
void vga_puts(const char *s);

/* Print formatted */
void vga_printf(const char *format, ...);

/* Set cursor position */
void vga_set_cursor(int x, int y);

/* Get cursor position */
void vga_get_cursor(int *x, int *y);

/* Enable/disable cursor */
void vga_cursor_enable(bool enable);

/* Scroll screen */
void vga_scroll(void);

#endif /* ICE_VGA_H */
