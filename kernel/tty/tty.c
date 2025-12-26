/*
 * ICE OS TTY - Terminal with .icetty configuration support
 */

#include "tty.h"
#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "../fs/vfs.h"
#include <stdarg.h>

// TTY Configuration (loadable from .icetty)
typedef struct {
    int fg_color;
    int bg_color;
    int prompt_color;
    int error_color;
    int success_color;
    char prompt[32];
    char hostname[16];
    bool show_path;
    bool color_prompt;
} tty_config_t;

static tty_config_t tty_cfg = {
    .fg_color = VGA_COLOR_WHITE,
    .bg_color = VGA_COLOR_BLACK,
    .prompt_color = VGA_COLOR_LIGHT_CYAN,
    .error_color = VGA_COLOR_LIGHT_RED,
    .success_color = VGA_COLOR_LIGHT_GREEN,
    .prompt = "ice> ",
    .hostname = "ice",
    .show_path = true,
    .color_prompt = true
};

static int current_scheme = TTY_SCHEME_DEFAULT;

// Customizable color schemes
static int schemes[5][2] = {
    {VGA_COLOR_WHITE, VGA_COLOR_BLACK},          
    {VGA_COLOR_WHITE, VGA_COLOR_BLACK},          
    {VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK},    
    {VGA_COLOR_BLACK, VGA_COLOR_WHITE},          
    {VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK},     
};

// Helper functions
static int tty_atoi(const char *s) {
    int n = 0;
    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (*s - '0');
        s++;
    }
    return n;
}

static bool tty_strncmp(const char *a, const char *b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i]) return false;
        if (a[i] == 0) break;
    }
    return true;
}

static void tty_strcpy(char *dst, const char *src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

// Load configuration from /.icetty
static void tty_load_config(void) {
    char buffer[512];
    vfs_file_t *file = vfs_open("/.icetty");
    if (!file) return;
    int len = vfs_read(file, buffer, sizeof(buffer) - 1);
    vfs_close(file);
    if (len <= 0) return;
    buffer[len] = 0;
    
    char *p = buffer;
    while (*p) {
        // Skip whitespace and comments
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (*p == 0 || *p == '#') {
            while (*p && *p != '\n') p++;
            continue;
        }
        
        // Parse key=value
        if (tty_strncmp(p, "fg=", 3)) {
            tty_cfg.fg_color = tty_atoi(p + 3);
            schemes[1][0] = tty_cfg.fg_color;
        } else if (tty_strncmp(p, "bg=", 3)) {
            tty_cfg.bg_color = tty_atoi(p + 3);
            schemes[1][1] = tty_cfg.bg_color;
        } else if (tty_strncmp(p, "prompt_color=", 13)) {
            tty_cfg.prompt_color = tty_atoi(p + 13);
        } else if (tty_strncmp(p, "error_color=", 12)) {
            tty_cfg.error_color = tty_atoi(p + 12);
        } else if (tty_strncmp(p, "success_color=", 14)) {
            tty_cfg.success_color = tty_atoi(p + 14);
        } else if (tty_strncmp(p, "prompt=", 7)) {
            int i = 0;
            p += 7;
            while (*p && *p != '\n' && i < 31) {
                tty_cfg.prompt[i++] = *p++;
            }
            tty_cfg.prompt[i] = 0;
            continue;
        } else if (tty_strncmp(p, "hostname=", 9)) {
            int i = 0;
            p += 9;
            while (*p && *p != '\n' && i < 15) {
                tty_cfg.hostname[i++] = *p++;
            }
            tty_cfg.hostname[i] = 0;
            continue;
        } else if (tty_strncmp(p, "show_path=", 10)) {
            tty_cfg.show_path = (p[10] == '1' || p[10] == 't' || p[10] == 'y');
        } else if (tty_strncmp(p, "color_prompt=", 13)) {
            tty_cfg.color_prompt = (p[13] == '1' || p[13] == 't' || p[13] == 'y');
        } else if (tty_strncmp(p, "scheme=", 7)) {
            current_scheme = tty_atoi(p + 7);
            if (current_scheme < 1) current_scheme = 1;
            if (current_scheme > 4) current_scheme = 4;
        }
        
        // Skip to end of line
        while (*p && *p != '\n') p++;
    }
}

void tty_init(void) {
    current_scheme = TTY_SCHEME_DEFAULT;
    tty_load_config();  // Load .icetty if exists
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
    tty_strcpy(tty_cfg.prompt, prompt, 32);
}

void tty_print_prompt(void) {
    if (tty_cfg.color_prompt) {
        vga_set_color(tty_cfg.prompt_color, schemes[current_scheme][1]);
    }
    vga_puts(tty_cfg.prompt);
    vga_set_color(schemes[current_scheme][0], schemes[current_scheme][1]);
}

// Get hostname for prompts
const char *tty_get_hostname(void) {
    return tty_cfg.hostname;
}

// Get colors for commands
int tty_get_error_color(void) {
    return tty_cfg.error_color;
}

int tty_get_success_color(void) {
    return tty_cfg.success_color;
}

// Reload configuration
void tty_reload_config(void) {
    tty_load_config();
    tty_set_color_scheme(current_scheme);
}
