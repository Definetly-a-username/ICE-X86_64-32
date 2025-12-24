/*
 * ICE Operating System - Virtual Console Implementation
 */

#include "console.h"
#include "../drivers/vga.h"

/* Console state */
static console_t consoles[NUM_CONSOLES];
static int current_console = 0;

/* VGA buffer pointer */
#define VGA_BUFFER ((u16*)0xB8000)

void console_init(void) {
    /* Initialize all consoles */
    for (int i = 0; i < NUM_CONSOLES; i++) {
        consoles[i].cursor_x = 0;
        consoles[i].cursor_y = 0;
        consoles[i].color = 0x07;  /* Light grey on black */
        consoles[i].active = (i == 0);
        
        /* Clear buffer */
        for (int j = 0; j < 80 * 25; j++) {
            consoles[i].buffer[j] = 0x0720;  /* Space with default color */
        }
    }
    
    current_console = 0;
}

void console_switch(int num) {
    if (num < 0 || num >= NUM_CONSOLES) return;
    if (num == current_console) return;
    
    /* Save current console */
    console_t *old = &consoles[current_console];
    for (int i = 0; i < 80 * 25; i++) {
        old->buffer[i] = VGA_BUFFER[i];
    }
    vga_get_cursor(&old->cursor_x, &old->cursor_y);
    old->active = false;
    
    /* Switch to new console */
    current_console = num;
    console_t *new_con = &consoles[num];
    
    /* Restore buffer */
    for (int i = 0; i < 80 * 25; i++) {
        VGA_BUFFER[i] = new_con->buffer[i];
    }
    vga_set_cursor(new_con->cursor_x, new_con->cursor_y);
    new_con->active = true;
}

int console_get_current(void) {
    return current_console;
}

void console_write(int num, const char *s) {
    if (num < 0 || num >= NUM_CONSOLES) return;
    
    /* If writing to current console, use VGA directly */
    if (num == current_console) {
        vga_puts(s);
    } else {
        /* Write to buffer only */
        console_t *con = &consoles[num];
        while (*s) {
            if (*s == '\n') {
                con->cursor_x = 0;
                con->cursor_y++;
            } else {
                int pos = con->cursor_y * 80 + con->cursor_x;
                if (pos < 80 * 25) {
                    con->buffer[pos] = (u16)*s | ((u16)con->color << 8);
                }
                con->cursor_x++;
            }
            
            if (con->cursor_x >= 80) {
                con->cursor_x = 0;
                con->cursor_y++;
            }
            
            if (con->cursor_y >= 25) {
                /* Scroll */
                for (int i = 0; i < 80 * 24; i++) {
                    con->buffer[i] = con->buffer[i + 80];
                }
                for (int i = 0; i < 80; i++) {
                    con->buffer[24 * 80 + i] = 0x0720;
                }
                con->cursor_y = 24;
            }
            
            s++;
        }
    }
}

void console_clear(int num) {
    if (num < 0 || num >= NUM_CONSOLES) return;
    
    console_t *con = &consoles[num];
    for (int i = 0; i < 80 * 25; i++) {
        con->buffer[i] = 0x0720;
    }
    con->cursor_x = 0;
    con->cursor_y = 0;
    
    if (num == current_console) {
        vga_clear();
    }
}

void console_handle_hotkey(int fkey) {
    /* F1-F4 switch to consoles 0-3 */
    if (fkey >= 1 && fkey <= 4) {
        console_switch(fkey - 1);
    }
}
