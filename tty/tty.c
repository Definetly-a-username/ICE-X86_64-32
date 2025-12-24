/*
 * ICE Operating System - TTY Implementation
 * Arch-style TTY with ANSI color support
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "tty.h"

/* ============================================================================
 * STATE
 * ============================================================================ */

static struct {
    int initialized;
    int color_scheme;
    int current_fg;
    int current_bg;
} tty_state = {0};

/* Color scheme definitions: {fg, bg} */
static const int color_schemes[5][2] = {
    {TTY_WHITE, TTY_BLACK},     /* 0: unused */
    {TTY_WHITE, TTY_BLACK},     /* 1: Default - white on black */
    {TTY_GREEN, TTY_BLACK},     /* 2: Dark - green on black */
    {TTY_BLACK, TTY_WHITE},     /* 3: Light - black on white */
    {TTY_WHITE, TTY_BLACK},     /* 4: Mono - white on black */
};

/* ============================================================================
 * INIT/SHUTDOWN
 * ============================================================================ */

int tty_init(void) {
    if (tty_state.initialized) return 0;
    
    tty_state.color_scheme = TTY_COLOR_DEFAULT;
    tty_state.current_fg = color_schemes[1][0];
    tty_state.current_bg = color_schemes[1][1];
    tty_state.initialized = 1;
    
    /* Apply default colors */
    tty_reset_color();
    
    return 0;
}

void tty_shutdown(void) {
    if (!tty_state.initialized) return;
    
    tty_reset_color();
    tty_state.initialized = 0;
}

/* ============================================================================
 * COLOR MANAGEMENT
 * ============================================================================ */

int tty_set_color_scheme(int scheme) {
    if (scheme < 1 || scheme > 4) return -1;
    
    tty_state.color_scheme = scheme;
    tty_state.current_fg = color_schemes[scheme][0];
    tty_state.current_bg = color_schemes[scheme][1];
    
    /* Apply colors */
    printf("\033[%d;%dm", 30 + tty_state.current_fg, 40 + tty_state.current_bg);
    fflush(stdout);
    
    return 0;
}

int tty_get_color_scheme(void) {
    return tty_state.color_scheme;
}

void tty_reset_color(void) {
    printf("\033[0m");
    fflush(stdout);
}

/* ============================================================================
 * SCREEN CONTROL
 * ============================================================================ */

void tty_clear(void) {
    printf("\033[2J\033[H");
    fflush(stdout);
}

void tty_set_cursor(int row, int col) {
    printf("\033[%d;%dH", row, col);
    fflush(stdout);
}

void tty_cursor_visible(int visible) {
    if (visible) {
        printf("\033[?25h");
    } else {
        printf("\033[?25l");
    }
    fflush(stdout);
}

/* ============================================================================
 * OUTPUT
 * ============================================================================ */

int tty_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vprintf(format, args);
    va_end(args);
    fflush(stdout);
    return result;
}

int tty_printf_color(int fg, int bg, const char *format, ...) {
    /* Set color */
    printf("\033[%d;%dm", 30 + fg, 40 + bg);
    
    /* Print */
    va_list args;
    va_start(args, format);
    int result = vprintf(format, args);
    va_end(args);
    
    /* Reset to scheme colors */
    printf("\033[%d;%dm", 30 + tty_state.current_fg, 40 + tty_state.current_bg);
    fflush(stdout);
    
    return result;
}
