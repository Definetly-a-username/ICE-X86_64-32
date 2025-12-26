 

#ifndef ICE_TTY_H
#define ICE_TTY_H

#include "../types.h"

 
#define TTY_SCHEME_DEFAULT  1
#define TTY_SCHEME_DARK     2
#define TTY_SCHEME_LIGHT    3
#define TTY_SCHEME_MONO     4

 
void tty_init(void);

 
void tty_set_color_scheme(int scheme);

 
int tty_get_color_scheme(void);

 
void tty_puts(const char *s);

 
void tty_printf(const char *format, ...);

 
int tty_getline(char *buffer, int max_len);

 
void tty_clear(void);

 
void tty_set_prompt(const char *prompt);

// Print colored prompt
void tty_print_prompt(void);

// Get hostname from config
const char *tty_get_hostname(void);

// Get theme colors
int tty_get_error_color(void);
int tty_get_success_color(void);

// Reload configuration from /.icetty
void tty_reload_config(void);

#endif  
