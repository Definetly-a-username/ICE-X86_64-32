/*
 * ICE Operating System - TTY Header
 * TTY subsystem with ANSI support
 */

#ifndef ICE_TTY_H
#define ICE_TTY_H

#include "../types.h"

/* Color schemes */
#define TTY_SCHEME_DEFAULT  1
#define TTY_SCHEME_DARK     2
#define TTY_SCHEME_LIGHT    3
#define TTY_SCHEME_MONO     4

/* Initialize TTY subsystem */
void tty_init(void);

/* Set color scheme */
void tty_set_color_scheme(int scheme);

/* Get current color scheme */
int tty_get_color_scheme(void);

/* Print string */
void tty_puts(const char *s);

/* Print formatted */
void tty_printf(const char *format, ...);

/* Read line with editing */
int tty_getline(char *buffer, int max_len);

/* Clear screen */
void tty_clear(void);

/* Set prompt */
void tty_set_prompt(const char *prompt);

/* Print prompt */
void tty_print_prompt(void);

#endif /* ICE_TTY_H */
