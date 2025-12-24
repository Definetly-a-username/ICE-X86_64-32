/*
 * ICE Operating System - TTY Header
 */

#ifndef ICE_TTY_H
#define ICE_TTY_H

/* Color schemes */
#define TTY_COLOR_DEFAULT   1  /* Default terminal colors */
#define TTY_COLOR_DARK      2  /* Dark theme */
#define TTY_COLOR_LIGHT     3  /* Light theme */
#define TTY_COLOR_MONO      4  /* Monochrome */

/* Initialize TTY subsystem */
int tty_init(void);

/* Shutdown TTY subsystem */
void tty_shutdown(void);

/* Set color scheme (1-4) */
int tty_set_color_scheme(int scheme);

/* Get current color scheme */
int tty_get_color_scheme(void);

/* Clear screen */
void tty_clear(void);

/* Write formatted output */
int tty_printf(const char *format, ...) __attribute__((format(printf, 1, 2)));

/* Write with color */
int tty_printf_color(int fg, int bg, const char *format, ...) 
    __attribute__((format(printf, 3, 4)));

/* ANSI color codes */
#define TTY_BLACK   0
#define TTY_RED     1
#define TTY_GREEN   2
#define TTY_YELLOW  3
#define TTY_BLUE    4
#define TTY_MAGENTA 5
#define TTY_CYAN    6
#define TTY_WHITE   7

/* Reset colors to default */
void tty_reset_color(void);

/* Set cursor position */
void tty_set_cursor(int row, int col);

/* Hide/show cursor */
void tty_cursor_visible(int visible);

#endif /* ICE_TTY_H */
