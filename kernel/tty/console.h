/*
 * ICE Operating System - Virtual Console Header
 */

#ifndef ICE_CONSOLE_H
#define ICE_CONSOLE_H

#include "../types.h"

/* Number of virtual consoles */
#define NUM_CONSOLES 4

/* Console structure */
typedef struct {
    u16 buffer[80 * 25];    /* VGA text buffer */
    int cursor_x;
    int cursor_y;
    u8 color;
    bool active;
} console_t;

/* Initialize console subsystem */
void console_init(void);

/* Switch to console */
void console_switch(int num);

/* Get current console */
int console_get_current(void);

/* Write to console */
void console_write(int num, const char *s);

/* Clear console */
void console_clear(int num);

/* Handle console hotkey (Ctrl+Alt+F1-F4) */
void console_handle_hotkey(int fkey);

#endif /* ICE_CONSOLE_H */
