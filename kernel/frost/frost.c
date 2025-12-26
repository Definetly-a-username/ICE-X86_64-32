/*
 * FROST - Graphical User Interface Layer for ICE OS
 * Optimized rendering with dirty rectangle tracking
 */

#include "frost.h"
#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "../drivers/pit.h"
#include "../mm/pmm.h"
#include "../fs/vfs.h"

// String functions
static int frost_strlen(const char *s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

static void frost_strcpy(char *dst, const char *src) {
    while ((*dst++ = *src++));
}

static int frost_strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a - *b;
}

static void frost_memset(void *ptr, int val, u32 size) {
    u8 *p = ptr;
    while (size--) *p++ = val;
}

static void frost_memcpy(void *dst, const void *src, u32 size) {
    u8 *d = dst;
    const u8 *s = src;
    while (size--) *d++ = *s++;
}

// Integer to string
static void frost_itoa(int val, char *buf) {
    if (val == 0) { buf[0] = '0'; buf[1] = 0; return; }
    char tmp[16];
    int i = 0, neg = 0;
    if (val < 0) { neg = 1; val = -val; }
    while (val) { tmp[i++] = '0' + (val % 10); val /= 10; }
    int j = 0;
    if (neg) buf[j++] = '-';
    while (i > 0) buf[j++] = tmp[--i];
    buf[j] = 0;
}

// ============ Global State ============

static frost_buffer_t screen_buffer;
static frost_buffer_t back_buffer;
static bool frost_initialized = false;
static bool frost_running = false;

// App registry
#define MAX_FROST_APPS 32
static frost_app_t *registered_apps[MAX_FROST_APPS];
static int app_count = 0;

// Current state
static frost_app_t *current_app = NULL;
static int desktop_selection = 0;
static bool show_launcher = false;
static int launcher_scroll = 0;

// Cursor state - smooth animation
static u32 cursor_tick = 0;
static int cursor_phase = 0;  // 0-3 for smooth animation
#define CURSOR_PHASE_TICKS 6  // Ticks per phase (faster = smoother)
#define CURSOR_PHASES 4       // Number of animation phases

// Cursor animation frames (smooth pulse effect)
static const char cursor_frames[4] = {'\x10', '\x1A', '\xFE', '\x1A'};  // ► → ■ →

// Cursor characters
#define CURSOR_ARROW     '\x10'  // ►
#define CURSOR_POINTER   '\x1A'  // →
#define CURSOR_BLOCK     '\xDB'  // █

// Debug mode - enabled by default for testing
static bool frost_debug = true;
static unsigned char last_key = 0;
static int key_count = 0;

// Theme configuration (loadable from .frostui)
typedef struct {
    u8 bg_dark;
    u8 bg_panel;
    u8 bg_widget;
    u8 fg_primary;
    u8 fg_secondary;
    u8 fg_accent;
    u8 fg_highlight;
    u8 fg_success;
    u8 fg_error;
    u8 border;
    char wallpaper_char;
    char cursor_char;
    char title[32];
} frost_theme_t;

static frost_theme_t theme = {
    .bg_dark = 0x0A,
    .bg_panel = 0x01,
    .bg_widget = 0x09,
    .fg_primary = 0x0F,
    .fg_secondary = 0x07,
    .fg_accent = 0x0B,
    .fg_highlight = 0x0E,
    .fg_success = 0x0A,
    .fg_error = 0x0C,
    .border = 0x03,
    .wallpaper_char = ' ',
    .cursor_char = '\x10',
    .title = "FROST"
};

// Direct VGA access for speed
static u16 *vga_mem = (u16*)0xB8000;

// ============ Configuration Loading ============

static int frost_atoi(const char *s) {
    int n = 0;
    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (*s - '0');
        s++;
    }
    return n;
}

static bool frost_strncmp(const char *a, const char *b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i]) return false;
        if (a[i] == 0) break;
    }
    return true;
}

// Parse .frostui configuration file
static void frost_load_config(void) {
    char buffer[512];
    vfs_file_t *file = vfs_open("/.frostui");
    if (!file) return;
    int len = vfs_read(file, buffer, sizeof(buffer) - 1);
    vfs_close(file);
    if (len <= 0) return;
    buffer[len] = 0;
    
    char *p = buffer;
    while (*p) {
        // Skip whitespace
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (*p == 0 || *p == '#') {
            // Skip comment lines
            while (*p && *p != '\n') p++;
            continue;
        }
        
        // Parse key=value
        if (frost_strncmp(p, "bg_dark=", 8)) {
            theme.bg_dark = frost_atoi(p + 8);
        } else if (frost_strncmp(p, "bg_panel=", 9)) {
            theme.bg_panel = frost_atoi(p + 9);
        } else if (frost_strncmp(p, "bg_widget=", 10)) {
            theme.bg_widget = frost_atoi(p + 10);
        } else if (frost_strncmp(p, "fg_primary=", 11)) {
            theme.fg_primary = frost_atoi(p + 11);
        } else if (frost_strncmp(p, "fg_secondary=", 13)) {
            theme.fg_secondary = frost_atoi(p + 13);
        } else if (frost_strncmp(p, "fg_accent=", 10)) {
            theme.fg_accent = frost_atoi(p + 10);
        } else if (frost_strncmp(p, "fg_highlight=", 13)) {
            theme.fg_highlight = frost_atoi(p + 13);
        } else if (frost_strncmp(p, "wallpaper=", 10)) {
            theme.wallpaper_char = p[10];
        } else if (frost_strncmp(p, "cursor=", 7)) {
            theme.cursor_char = p[7];
        } else if (frost_strncmp(p, "title=", 6)) {
            int i = 0;
            p += 6;
            while (*p && *p != '\n' && i < 31) {
                theme.title[i++] = *p++;
            }
            theme.title[i] = 0;
            continue;
        } else if (frost_strncmp(p, "debug=", 6)) {
            frost_debug = (p[6] == '1' || p[6] == 't' || p[6] == 'y');
        }
        
        // Skip to end of line
        while (*p && *p != '\n') p++;
    }
}

// ============ Low-level Rendering ============

static inline void vga_put_fast(int x, int y, char c, u8 color) {
    if (x >= 0 && x < FROST_SCREEN_W && y >= 0 && y < FROST_SCREEN_H) {
        vga_mem[y * FROST_SCREEN_W + x] = (u16)c | ((u16)color << 8);
    }
}

static inline u8 make_color(u8 fg, u8 bg) {
    return (bg << 4) | (fg & 0x0F);
}

// ============ Buffer Operations ============

void frost_clear(u8 bg) {
    u8 color = make_color(FROST_FG_PRIMARY, bg);
    for (int y = 0; y < FROST_SCREEN_H; y++) {
        for (int x = 0; x < FROST_SCREEN_W; x++) {
            // Preserve keyboard debug area (row 0, cols 66-79)
            if (y == 0 && x >= 66) continue;
            back_buffer.chars[y][x] = ' ';
            back_buffer.colors[y][x] = color;
            back_buffer.dirty[y][x] = true;
        }
    }
}

void frost_text(int x, int y, const char *text, u8 fg, u8 bg) {
    u8 color = make_color(fg, bg);
    while (*text && x < FROST_SCREEN_W) {
        if (x >= 0 && y >= 0 && y < FROST_SCREEN_H) {
            if (back_buffer.chars[y][x] != *text || back_buffer.colors[y][x] != color) {
                back_buffer.chars[y][x] = *text;
                back_buffer.colors[y][x] = color;
                back_buffer.dirty[y][x] = true;
            }
        }
        x++;
        text++;
    }
}

void frost_box(int x, int y, int w, int h, u8 fg, u8 bg, bool border) {
    u8 color = make_color(fg, bg);
    
    for (int py = y; py < y + h && py < FROST_SCREEN_H; py++) {
        for (int px = x; px < x + w && px < FROST_SCREEN_W; px++) {
            if (px < 0 || py < 0) continue;
            
            char ch = ' ';
            if (border) {
                bool top = (py == y);
                bool bot = (py == y + h - 1);
                bool left = (px == x);
                bool right = (px == x + w - 1);
                
                if (top && left) ch = 0xC9;       // ╔
                else if (top && right) ch = 0xBB;  // ╗
                else if (bot && left) ch = 0xC8;   // ╚
                else if (bot && right) ch = 0xBC;  // ╝
                else if (top || bot) ch = 0xCD;    // ═
                else if (left || right) ch = 0xBA; // ║
            }
            
            if (back_buffer.chars[py][px] != ch || back_buffer.colors[py][px] != color) {
                back_buffer.chars[py][px] = ch;
                back_buffer.colors[py][px] = color;
                back_buffer.dirty[py][px] = true;
            }
        }
    }
}

void frost_hline(int x, int y, int len, char ch, u8 fg, u8 bg) {
    u8 color = make_color(fg, bg);
    for (int i = 0; i < len && x + i < FROST_SCREEN_W; i++) {
        if (x + i >= 0 && y >= 0 && y < FROST_SCREEN_H) {
            if (back_buffer.chars[y][x+i] != ch || back_buffer.colors[y][x+i] != color) {
                back_buffer.chars[y][x+i] = ch;
                back_buffer.colors[y][x+i] = color;
                back_buffer.dirty[y][x+i] = true;
            }
        }
    }
}

void frost_vline(int x, int y, int len, char ch, u8 fg, u8 bg) {
    u8 color = make_color(fg, bg);
    for (int i = 0; i < len && y + i < FROST_SCREEN_H; i++) {
        if (x >= 0 && x < FROST_SCREEN_W && y + i >= 0) {
            if (back_buffer.chars[y+i][x] != ch || back_buffer.colors[y+i][x] != color) {
                back_buffer.chars[y+i][x] = ch;
                back_buffer.colors[y+i][x] = color;
                back_buffer.dirty[y+i][x] = true;
            }
        }
    }
}

// Optimized flush - only updates changed cells (preserves keyboard debug at row 0, cols 66-79)
void frost_flush(void) {
    for (int y = 0; y < FROST_SCREEN_H; y++) {
        for (int x = 0; x < FROST_SCREEN_W; x++) {
            // Skip keyboard debug area
            if (y == 0 && x >= 66) continue;
            if (back_buffer.dirty[y][x]) {
                vga_put_fast(x, y, back_buffer.chars[y][x], back_buffer.colors[y][x]);
                screen_buffer.chars[y][x] = back_buffer.chars[y][x];
                screen_buffer.colors[y][x] = back_buffer.colors[y][x];
                back_buffer.dirty[y][x] = false;
            }
        }
    }
}

void frost_redraw(void) {
    for (int y = 0; y < FROST_SCREEN_H; y++) {
        for (int x = 0; x < FROST_SCREEN_W; x++) {
            back_buffer.dirty[y][x] = true;
        }
    }
    frost_flush();
}

// ============ Widget System ============

static frost_widget_t widget_pool[64];
static int widget_pool_idx = 0;

static frost_widget_t *alloc_widget(void) {
    if (widget_pool_idx >= 64) return NULL;
    frost_widget_t *w = &widget_pool[widget_pool_idx++];
    frost_memset(w, 0, sizeof(frost_widget_t));
    w->flags = WIDGET_VISIBLE | WIDGET_ENABLED;
    w->fg_color = FROST_FG_PRIMARY;
    w->bg_color = FROST_BG_WIDGET;
    return w;
}

frost_widget_t *frost_label(int x, int y, const char *text) {
    frost_widget_t *w = alloc_widget();
    if (!w) return NULL;
    w->type = FROST_WIDGET_LABEL;
    w->x = x; w->y = y;
    w->w = frost_strlen(text);
    w->h = 1;
    frost_strcpy(w->text, text);
    w->bg_color = FROST_BG_DARK;
    return w;
}

frost_widget_t *frost_button(int x, int y, const char *text, frost_callback_t on_click) {
    frost_widget_t *w = alloc_widget();
    if (!w) return NULL;
    w->type = FROST_WIDGET_BUTTON;
    w->x = x; w->y = y;
    w->w = frost_strlen(text) + 4;
    w->h = 1;
    frost_strcpy(w->text, text);
    w->on_click = on_click;
    return w;
}

frost_widget_t *frost_input(int x, int y, int width) {
    frost_widget_t *w = alloc_widget();
    if (!w) return NULL;
    w->type = FROST_WIDGET_INPUT;
    w->x = x; w->y = y;
    w->w = width;
    w->h = 1;
    w->value[0] = 0;
    return w;
}

frost_widget_t *frost_checkbox(int x, int y, const char *text, bool checked) {
    frost_widget_t *w = alloc_widget();
    if (!w) return NULL;
    w->type = FROST_WIDGET_CHECKBOX;
    w->x = x; w->y = y;
    w->w = frost_strlen(text) + 4;
    w->h = 1;
    frost_strcpy(w->text, text);
    w->int_value = checked ? 1 : 0;
    return w;
}

frost_widget_t *frost_progress(int x, int y, int width, int value, int max) {
    frost_widget_t *w = alloc_widget();
    if (!w) return NULL;
    w->type = FROST_WIDGET_PROGRESS;
    w->x = x; w->y = y;
    w->w = width;
    w->h = 1;
    w->int_value = value;
    // Store max in user_data as int
    w->user_data = (void*)(long)max;
    return w;
}

void frost_widget_set_text(frost_widget_t *w, const char *text) {
    if (w) frost_strcpy(w->text, text);
}

void frost_widget_set_value(frost_widget_t *w, int value) {
    if (w) w->int_value = value;
}

void frost_widget_render(frost_widget_t *w) {
    if (!w || !(w->flags & WIDGET_VISIBLE)) return;
    
    bool focused = (w->flags & WIDGET_FOCUSED) != 0;
    u8 fg = focused ? FROST_FG_HIGHLIGHT : w->fg_color;
    u8 bg = w->bg_color;
    
    switch (w->type) {
        case FROST_WIDGET_LABEL:
            frost_text(w->x, w->y, w->text, fg, bg);
            break;
            
        case FROST_WIDGET_BUTTON: {
            char buf[68];
            buf[0] = '['; buf[1] = ' ';
            frost_strcpy(buf + 2, w->text);
            int len = frost_strlen(buf);
            buf[len] = ' '; buf[len+1] = ']'; buf[len+2] = 0;
            frost_text(w->x, w->y, buf, focused ? 0x0E : 0x0B, focused ? 0x01 : 0x09);
            break;
        }
        
        case FROST_WIDGET_INPUT: {
            frost_box(w->x, w->y, w->w, 1, 0x0F, 0x00, false);
            frost_text(w->x, w->y, w->value, 0x0F, 0x00);
            if (focused) {
                int cursor = frost_strlen(w->value);
                if (cursor < w->w - 1) {
                    frost_text(w->x + cursor, w->y, "_", 0x0F, 0x00);
                }
            }
            break;
        }
        
        case FROST_WIDGET_CHECKBOX: {
            char buf[68];
            buf[0] = '[';
            buf[1] = w->int_value ? 'X' : ' ';
            buf[2] = ']'; buf[3] = ' ';
            frost_strcpy(buf + 4, w->text);
            frost_text(w->x, w->y, buf, fg, bg);
            break;
        }
        
        case FROST_WIDGET_PROGRESS: {
            int max = (int)(long)w->user_data;
            if (max <= 0) max = 100;
            int filled = (w->int_value * (w->w - 2)) / max;
            frost_text(w->x, w->y, "[", FROST_FG_SECONDARY, FROST_BG_DARK);
            for (int i = 0; i < w->w - 2; i++) {
                frost_text(w->x + 1 + i, w->y, i < filled ? "#" : "-", 
                          i < filled ? FROST_FG_SUCCESS : FROST_FG_SECONDARY, FROST_BG_DARK);
            }
            frost_text(w->x + w->w - 1, w->y, "]", FROST_FG_SECONDARY, FROST_BG_DARK);
            break;
        }
        
        default:
            break;
    }
}

// ============ App Registration ============

void frost_register_app(frost_app_t *app) {
    if (app_count < MAX_FROST_APPS) {
        registered_apps[app_count++] = app;
    }
}

frost_app_t *frost_get_app(const char *name) {
    for (int i = 0; i < app_count; i++) {
        if (frost_strcmp(registered_apps[i]->name, name) == 0) {
            return registered_apps[i];
        }
    }
    return NULL;
}

frost_app_t **frost_get_apps(int *count) {
    *count = app_count;
    return registered_apps;
}

void frost_launch_app(const char *name) {
    frost_app_t *app = frost_get_app(name);
    if (app && app->run) {
        current_app = app;
        app->running = true;
        app->run();
        app->running = false;
        current_app = NULL;
        frost_redraw();
    }
}

// ============ Dialogs ============

void frost_msgbox(const char *title, const char *message) {
    int msg_len = frost_strlen(message);
    int title_len = frost_strlen(title);
    int w = (msg_len > title_len ? msg_len : title_len) + 6;
    if (w > 60) w = 60;
    int h = 7;
    int x = (FROST_SCREEN_W - w) / 2;
    int y = (FROST_SCREEN_H - h) / 2;
    
    frost_box(x, y, w, h, FROST_FG_ACCENT, FROST_BG_PANEL, true);
    frost_text(x + 2, y, title, FROST_FG_HIGHLIGHT, FROST_BG_PANEL);
    frost_text(x + 2, y + 2, message, FROST_FG_PRIMARY, FROST_BG_PANEL);
    frost_text(x + (w - 8) / 2, y + 4, "[ OK ]", FROST_FG_ACCENT, FROST_BG_WIDGET);
    frost_flush();
    
    // Wait for key
    while (1) {
        unsigned char key = keyboard_getc();
        if (key == '\n' || key == '\r' || key == ' ' || key == 27) break;
    }
}

bool frost_confirm(const char *title, const char *message) {
    int msg_len = frost_strlen(message);
    int title_len = frost_strlen(title);
    int w = (msg_len > title_len ? msg_len : title_len) + 6;
    if (w < 30) w = 30;
    if (w > 60) w = 60;
    int h = 7;
    int x = (FROST_SCREEN_W - w) / 2;
    int y = (FROST_SCREEN_H - h) / 2;
    
    int selected = 0;
    
    while (1) {
        frost_box(x, y, w, h, FROST_FG_ACCENT, FROST_BG_PANEL, true);
        frost_text(x + 2, y, title, FROST_FG_HIGHLIGHT, FROST_BG_PANEL);
        frost_text(x + 2, y + 2, message, FROST_FG_PRIMARY, FROST_BG_PANEL);
        
        // Buttons
        int btn_y = y + 4;
        int btn_x1 = x + w/2 - 12;
        int btn_x2 = x + w/2 + 2;
        
        frost_text(btn_x1, btn_y, selected == 0 ? "[ YES ]" : "  YES  ", 
                  selected == 0 ? FROST_FG_HIGHLIGHT : FROST_FG_SECONDARY,
                  selected == 0 ? FROST_BG_WIDGET : FROST_BG_PANEL);
        frost_text(btn_x2, btn_y, selected == 1 ? "[  NO  ]" : "   NO   ", 
                  selected == 1 ? FROST_FG_HIGHLIGHT : FROST_FG_SECONDARY,
                  selected == 1 ? FROST_BG_WIDGET : FROST_BG_PANEL);
        frost_flush();
        
        unsigned char key = keyboard_getc();
        if (key == KEY_LEFT || key == 'h' || key == 'a') selected = 0;
        else if (key == KEY_RIGHT || key == 'l' || key == 'd') selected = 1;
        else if (key == '\n' || key == '\r') return selected == 0;
        else if (key == 27) return false;
        else if (key == 'y' || key == 'Y') return true;
        else if (key == 'n' || key == 'N') return false;
    }
}

// ============ Desktop Drawing ============

static void draw_desktop(void) {
    // Background with theme color
    frost_clear(theme.bg_dark);
    
    // Draw wallpaper pattern if set
    if (theme.wallpaper_char != ' ') {
        for (int y = 2; y < FROST_SCREEN_H - 2; y += 2) {
            for (int x = 0; x < FROST_SCREEN_W; x += 4) {
                char wp[2] = {theme.wallpaper_char, 0};
                frost_text(x, y, wp, theme.bg_panel, theme.bg_dark);
            }
        }
    }
    
    // Top bar with theme (leave right side for keyboard debug: cols 66-79)
    frost_hline(0, 0, 65, ' ', theme.fg_primary, theme.bg_panel);
    char title_buf[40];
    title_buf[0] = '\x04';
    title_buf[1] = ' ';
    frost_strcpy(title_buf + 2, theme.title);
    frost_text(2, 0, title_buf, theme.fg_accent, theme.bg_panel);
    frost_text(50, 0, "ICE OS", theme.fg_secondary, theme.bg_panel);
    
    // Status bar at bottom
    frost_hline(0, FROST_SCREEN_H - 1, FROST_SCREEN_W, ' ', theme.fg_primary, theme.bg_panel);
    frost_text(2, FROST_SCREEN_H - 1, "Arrows/WASD: Move | ENTER: Open | TAB: Next | 1-8: Quick | ESC: Exit", 
              theme.fg_secondary, theme.bg_panel);
    
    // Desktop icons (improved layout)
    const char *icons[][2] = {
        {"\x0F", "Terminal"},
        {"\x07", "Files"},
        {"\x0E", "Notepad"},
        {"\x04", "Calculator"},
        {"\xEC", "Browser"},
        {"\x02", "Settings"},
        {"\x01", "System"},
        {"\x05", "Games"}
    };
    int num_icons = 8;
    
    int start_x = 5;
    int start_y = 3;
    int icon_spacing = 12;
    
    for (int i = 0; i < num_icons; i++) {
        int row = i / 4;
        int col = i % 4;
        int x = start_x + col * (icon_spacing + 6);
        int y = start_y + row * 4;
        
        bool selected = (!show_launcher && desktop_selection == i);
        
        // Icon box with theme colors
        u8 bg = selected ? theme.bg_widget : theme.bg_dark;
        u8 fg = selected ? theme.fg_highlight : theme.fg_accent;
        
        frost_box(x, y, 10, 3, fg, bg, selected);
        frost_text(x + 4, y + 1, icons[i][0], fg, bg);
        
        // Draw smooth animated cursor next to selected icon
        if (selected) {
            // Use animated cursor frames for smooth effect
            char cursor_str[2] = {cursor_frames[cursor_phase], 0};
            // Alternate color intensity for extra smoothness
            u8 cursor_color = (cursor_phase % 2 == 0) ? theme.fg_highlight : theme.fg_accent;
            frost_text(x - 2, y + 1, cursor_str, cursor_color, theme.bg_dark);
        }
        
        // Label below with underline effect for selected
        int label_len = frost_strlen(icons[i][1]);
        int label_x = x + (10 - label_len) / 2;
        if (selected) {
            // Draw brackets around selected label
            frost_text(label_x - 1, y + 3, "[", theme.fg_accent, theme.bg_dark);
            frost_text(label_x, y + 3, icons[i][1], theme.fg_highlight, theme.bg_dark);
            frost_text(label_x + label_len, y + 3, "]", theme.fg_accent, theme.bg_dark);
        } else {
            frost_text(label_x, y + 3, icons[i][1], theme.fg_secondary, theme.bg_dark);
        }
    }
}

static void draw_launcher(void) {
    int w = 40;
    int h = 16;
    int x = (FROST_SCREEN_W - w) / 2;
    int y = (FROST_SCREEN_H - h) / 2;
    
    // Panel with border
    frost_box(x, y, w, h, FROST_FG_ACCENT, FROST_BG_PANEL, true);
    frost_text(x + 2, y, " Applications ", FROST_FG_HIGHLIGHT, FROST_BG_PANEL);
    
    // App list
    int visible = h - 4;
    for (int i = 0; i < visible && i + launcher_scroll < app_count; i++) {
        int idx = i + launcher_scroll;
        frost_app_t *app = registered_apps[idx];
        bool sel = (idx == desktop_selection);
        
        u8 bg = sel ? FROST_BG_WIDGET : FROST_BG_PANEL;
        u8 fg = sel ? FROST_FG_HIGHLIGHT : FROST_FG_PRIMARY;
        
        // Clear line
        frost_hline(x + 1, y + 2 + i, w - 2, ' ', fg, bg);
        
        // Draw smooth animated cursor for selected item
        if (sel) {
            char cursor_str[2] = {cursor_frames[cursor_phase], 0};
            u8 cursor_color = (cursor_phase % 2 == 0) ? FROST_FG_HIGHLIGHT : FROST_FG_ACCENT;
            frost_text(x + 2, y + 2 + i, cursor_str, cursor_color, bg);
            frost_text(x + 4, y + 2 + i, app->icon, FROST_FG_ACCENT, bg);
            frost_text(x + 7, y + 2 + i, app->name, fg, bg);
        } else {
            // Icon and name (no cursor)
            frost_text(x + 2, y + 2 + i, app->icon, FROST_FG_ACCENT, bg);
            frost_text(x + 5, y + 2 + i, app->name, fg, bg);
        }
    }
    
    // Scroll indicator
    if (app_count > visible) {
        if (launcher_scroll > 0) frost_text(x + w - 3, y + 2, "^", FROST_FG_SECONDARY, FROST_BG_PANEL);
        if (launcher_scroll + visible < app_count) frost_text(x + w - 3, y + h - 3, "v", FROST_FG_SECONDARY, FROST_BG_PANEL);
    }
}

// ============ Built-in Apps ============

// Forward declarations for app implementations
static void run_calc(void);
static void run_browser(void);
static void run_notepad(void);
static void run_files(void);
static void run_terminal(void);
static void run_settings(void);
static void run_sysinfo(void);
static void run_games(void);

// App definitions
static frost_app_t builtin_apps[] = {
    {"Terminal",   "\x0F", "Command line interface",     run_terminal, NULL, NULL, NULL, false},
    {"Files",      "\x07", "File manager",               run_files,    NULL, NULL, NULL, false},
    {"Notepad",    "\x0E", "Text editor",                run_notepad,  NULL, NULL, NULL, false},
    {"Calculator", "\x04", "Basic calculator",           run_calc,     NULL, NULL, NULL, false},
    {"Browser",    "\xEC", "ICE Web Browser",            run_browser,  NULL, NULL, NULL, false},
    {"Settings",   "\x02", "System settings",            run_settings, NULL, NULL, NULL, false},
    {"System",     "\x01", "System information",         run_sysinfo,  NULL, NULL, NULL, false},
    {"Games",      "\x05", "Simple games",               run_games,    NULL, NULL, NULL, false}
};

// ============ Calculator App ============

static void run_calc(void) {
    char display[32] = "0";
    char input[32] = "";
    int input_pos = 0;
    long result = 0;
    long operand = 0;
    char op = 0;
    bool new_num = true;
    
    while (1) {
        frost_clear(FROST_BG_DARK);
        
        // Calculator frame
        int x = 20, y = 3;
        frost_box(x, y, 40, 18, FROST_FG_ACCENT, FROST_BG_PANEL, true);
        frost_text(x + 2, y, " Calculator ", FROST_FG_HIGHLIGHT, FROST_BG_PANEL);
        
        // Display
        frost_box(x + 2, y + 2, 36, 3, FROST_FG_PRIMARY, 0x00, true);
        int disp_len = frost_strlen(display);
        frost_text(x + 36 - disp_len, y + 3, display, FROST_FG_HIGHLIGHT, 0x00);
        
        // Buttons layout
        const char *buttons[] = {
            "7", "8", "9", "/", "C",
            "4", "5", "6", "*", "<",
            "1", "2", "3", "-", "(",
            "0", ".", "=", "+", ")"
        };
        
        for (int row = 0; row < 4; row++) {
            for (int col = 0; col < 5; col++) {
                int bx = x + 3 + col * 7;
                int by = y + 6 + row * 2;
                const char *btn = buttons[row * 5 + col];
                
                u8 bg = FROST_BG_WIDGET;
                if (*btn >= '0' && *btn <= '9') bg = 0x01;
                else if (*btn == '=') bg = 0x02;
                else if (*btn == 'C') bg = 0x04;
                
                frost_text(bx, by, "[", FROST_FG_SECONDARY, FROST_BG_PANEL);
                frost_text(bx + 1, by, " ", FROST_FG_PRIMARY, bg);
                frost_text(bx + 2, by, btn, FROST_FG_PRIMARY, bg);
                frost_text(bx + 3, by, " ", FROST_FG_PRIMARY, bg);
                frost_text(bx + 4, by, "]", FROST_FG_SECONDARY, FROST_BG_PANEL);
            }
        }
        
        frost_text(x + 2, y + 15, "Keys: 0-9, +-*/, Enter=, C=Clear, ESC=Exit", FROST_FG_SECONDARY, FROST_BG_PANEL);
        frost_flush();
        
        unsigned char key = keyboard_getc();
        
        if (key == 27) break; // ESC
        
        if (key >= '0' && key <= '9') {
            if (new_num) {
                input_pos = 0;
                new_num = false;
            }
            if (input_pos < 15) {
                input[input_pos++] = key;
                input[input_pos] = 0;
                frost_strcpy(display, input);
            }
        } else if (key == '+' || key == '-' || key == '*' || key == '/') {
            // Parse current input
            long num = 0;
            for (int i = 0; input[i]; i++) {
                num = num * 10 + (input[i] - '0');
            }
            if (op && !new_num) {
                switch (op) {
                    case '+': result = operand + num; break;
                    case '-': result = operand - num; break;
                    case '*': result = operand * num; break;
                    case '/': result = num ? operand / num : 0; break;
                }
            } else {
                result = num;
            }
            operand = result;
            op = key;
            frost_itoa((int)result, display);
            new_num = true;
        } else if (key == '=' || key == '\n' || key == '\r') {
            long num = 0;
            for (int i = 0; input[i]; i++) {
                num = num * 10 + (input[i] - '0');
            }
            if (op) {
                switch (op) {
                    case '+': result = operand + num; break;
                    case '-': result = operand - num; break;
                    case '*': result = operand * num; break;
                    case '/': result = num ? operand / num : 0; break;
                }
            } else {
                result = num;
            }
            frost_itoa((int)result, display);
            op = 0;
            operand = 0;
            new_num = true;
        } else if (key == 'c' || key == 'C') {
            result = 0;
            operand = 0;
            op = 0;
            input_pos = 0;
            input[0] = 0;
            frost_strcpy(display, "0");
            new_num = true;
        } else if (key == '\b' && input_pos > 0) {
            input[--input_pos] = 0;
            if (input_pos == 0) frost_strcpy(display, "0");
            else frost_strcpy(display, input);
        }
    }
}

// ============ Browser App ============

static void run_browser(void) {
    char url[128] = "ice://home";
    
    while (1) {
        frost_clear(FROST_BG_DARK);
        
        // Browser frame
        frost_box(0, 0, FROST_SCREEN_W, FROST_SCREEN_H, FROST_FG_ACCENT, FROST_BG_PANEL, true);
        frost_text(2, 0, " ICE Browser ", FROST_FG_HIGHLIGHT, FROST_BG_PANEL);
        
        // URL bar
        frost_text(2, 2, "URL:", FROST_FG_SECONDARY, FROST_BG_PANEL);
        frost_box(7, 2, 60, 1, FROST_FG_PRIMARY, 0x00, false);
        frost_text(7, 2, url, FROST_FG_ACCENT, 0x00);
        
        // Navigation buttons
        frost_text(68, 2, "[<]", FROST_FG_ACCENT, FROST_BG_WIDGET);
        frost_text(72, 2, "[>]", FROST_FG_ACCENT, FROST_BG_WIDGET);
        frost_text(76, 2, "[R]", FROST_FG_ACCENT, FROST_BG_WIDGET);
        
        // Content area
        frost_hline(1, 4, FROST_SCREEN_W - 2, 0xC4, FROST_FG_SECONDARY, FROST_BG_PANEL);
        
        // Display page content
        if (frost_strcmp(url, "ice://home") == 0) {
            frost_text(3, 6, "Welcome to ICE Browser", FROST_FG_HIGHLIGHT, FROST_BG_PANEL);
            frost_text(3, 8, "This is a simple text-based browser for ICE OS.", FROST_FG_PRIMARY, FROST_BG_PANEL);
            frost_text(3, 10, "Quick Links:", FROST_FG_ACCENT, FROST_BG_PANEL);
            frost_text(5, 12, "[1] ice://about  - About ICE OS", FROST_FG_SECONDARY, FROST_BG_PANEL);
            frost_text(5, 13, "[2] ice://help   - Help & Documentation", FROST_FG_SECONDARY, FROST_BG_PANEL);
            frost_text(5, 14, "[3] ice://apps   - Application Guide", FROST_FG_SECONDARY, FROST_BG_PANEL);
            frost_text(5, 15, "[4] ice://frost  - About Frost GUI", FROST_FG_SECONDARY, FROST_BG_PANEL);
        } else if (frost_strcmp(url, "ice://about") == 0) {
            frost_text(3, 6, "About ICE OS", FROST_FG_HIGHLIGHT, FROST_BG_PANEL);
            frost_text(3, 8, "ICE OS - A minimalist x86 operating system", FROST_FG_PRIMARY, FROST_BG_PANEL);
            frost_text(3, 9, "Version 1.0", FROST_FG_PRIMARY, FROST_BG_PANEL);
            frost_text(3, 11, "Features:", FROST_FG_ACCENT, FROST_BG_PANEL);
            frost_text(5, 12, "- EXT2 Filesystem", FROST_FG_SECONDARY, FROST_BG_PANEL);
            frost_text(5, 13, "- Frost GUI Layer", FROST_FG_SECONDARY, FROST_BG_PANEL);
            frost_text(5, 14, "- Multi-language App Support", FROST_FG_SECONDARY, FROST_BG_PANEL);
            frost_text(5, 15, "- Basic Networking", FROST_FG_SECONDARY, FROST_BG_PANEL);
        } else if (frost_strcmp(url, "ice://frost") == 0) {
            frost_text(3, 6, "Frost GUI Layer", FROST_FG_HIGHLIGHT, FROST_BG_PANEL);
            frost_text(3, 8, "Frost is the graphical layer for ICE OS.", FROST_FG_PRIMARY, FROST_BG_PANEL);
            frost_text(3, 10, "Technologies:", FROST_FG_ACCENT, FROST_BG_PANEL);
            frost_text(5, 11, "- Double-buffered rendering", FROST_FG_SECONDARY, FROST_BG_PANEL);
            frost_text(5, 12, "- Dirty rectangle optimization", FROST_FG_SECONDARY, FROST_BG_PANEL);
            frost_text(5, 13, "- Widget-based UI system", FROST_FG_SECONDARY, FROST_BG_PANEL);
            frost_text(5, 14, "- Keyboard and mouse support", FROST_FG_SECONDARY, FROST_BG_PANEL);
        } else {
            frost_text(3, 10, "Page not found", FROST_FG_ERROR, FROST_BG_PANEL);
            frost_text(3, 12, "Press H to go home", FROST_FG_SECONDARY, FROST_BG_PANEL);
        }
        
        frost_text(2, FROST_SCREEN_H - 2, "ESC=Exit | H=Home | 1-4=Links | G=Go to URL", FROST_FG_SECONDARY, FROST_BG_PANEL);
        frost_flush();
        
        unsigned char key = keyboard_getc();
        
        if (key == 27) break;
        else if (key == 'h' || key == 'H') frost_strcpy(url, "ice://home");
        else if (key == '1') frost_strcpy(url, "ice://about");
        else if (key == '2') frost_strcpy(url, "ice://help");
        else if (key == '3') frost_strcpy(url, "ice://apps");
        else if (key == '4') frost_strcpy(url, "ice://frost");
    }
}

// ============ Notepad App ============

static void run_notepad(void) {
    char text[20][80];
    int cursor_x = 0, cursor_y = 0;
    int num_lines = 1;
    
    for (int i = 0; i < 20; i++) text[i][0] = 0;
    
    while (1) {
        frost_clear(FROST_BG_DARK);
        
        // Editor frame
        frost_box(0, 0, FROST_SCREEN_W, FROST_SCREEN_H, FROST_FG_ACCENT, FROST_BG_PANEL, true);
        frost_text(2, 0, " Notepad ", FROST_FG_HIGHLIGHT, FROST_BG_PANEL);
        
        // Text area
        for (int y = 0; y < 20 && y < num_lines + 1; y++) {
            // Line number
            char lnum[4];
            frost_itoa(y + 1, lnum);
            frost_text(2, y + 2, lnum, FROST_FG_SECONDARY, FROST_BG_PANEL);
            frost_text(5, y + 2, "|", FROST_FG_SECONDARY, FROST_BG_PANEL);
            
            // Line content
            frost_text(7, y + 2, text[y], FROST_FG_PRIMARY, FROST_BG_PANEL);
        }
        
        // Cursor
        frost_text(7 + cursor_x, 2 + cursor_y, "_", FROST_FG_HIGHLIGHT, FROST_BG_PANEL);
        
        // Status bar
        char status[40];
        frost_text(2, FROST_SCREEN_H - 2, "Ln:", FROST_FG_SECONDARY, FROST_BG_PANEL);
        frost_itoa(cursor_y + 1, status);
        frost_text(6, FROST_SCREEN_H - 2, status, FROST_FG_PRIMARY, FROST_BG_PANEL);
        frost_text(10, FROST_SCREEN_H - 2, "Col:", FROST_FG_SECONDARY, FROST_BG_PANEL);
        frost_itoa(cursor_x + 1, status);
        frost_text(15, FROST_SCREEN_H - 2, status, FROST_FG_PRIMARY, FROST_BG_PANEL);
        frost_text(25, FROST_SCREEN_H - 2, "ESC=Exit | Ctrl+S=Save", FROST_FG_SECONDARY, FROST_BG_PANEL);
        
        frost_flush();
        
        unsigned char key = keyboard_getc();
        
        if (key == 27) break;
        else if (key == KEY_UP && cursor_y > 0) cursor_y--;
        else if (key == KEY_DOWN && cursor_y < num_lines - 1) cursor_y++;
        else if (key == KEY_LEFT && cursor_x > 0) cursor_x--;
        else if (key == KEY_RIGHT && cursor_x < frost_strlen(text[cursor_y])) cursor_x++;
        else if (key == '\n' || key == '\r') {
            if (num_lines < 19) {
                num_lines++;
                cursor_y++;
                cursor_x = 0;
            }
        } else if (key == '\b') {
            if (cursor_x > 0) {
                int len = frost_strlen(text[cursor_y]);
                for (int i = cursor_x - 1; i < len; i++) {
                    text[cursor_y][i] = text[cursor_y][i + 1];
                }
                cursor_x--;
            }
        } else if (key >= 32 && key < 127) {
            int len = frost_strlen(text[cursor_y]);
            if (len < 70) {
                // Shift characters right
                for (int i = len + 1; i > cursor_x; i--) {
                    text[cursor_y][i] = text[cursor_y][i - 1];
                }
                text[cursor_y][cursor_x++] = key;
            }
        }
    }
}

// ============ File Manager App ============

static void run_files(void) {
    char path[128] = "/";
    char files[32][64];
    int file_count = 0;
    int selected = 0;
    int scroll = 0;
    
    // List files callback
    auto void list_callback(const char *name, u32 size, bool is_dir);
    void list_callback(const char *name, u32 size, bool is_dir) {
        (void)size;
        if (file_count < 32) {
            if (is_dir) files[file_count][0] = '/';
            else files[file_count][0] = ' ';
            frost_strcpy(files[file_count] + 1, name);
            file_count++;
        }
    }
    
    while (1) {
        // Refresh file list
        file_count = 0;
        vfs_list_dir(path, list_callback);
        
        frost_clear(FROST_BG_DARK);
        
        // Frame
        frost_box(0, 0, FROST_SCREEN_W, FROST_SCREEN_H, FROST_FG_ACCENT, FROST_BG_PANEL, true);
        frost_text(2, 0, " File Manager ", FROST_FG_HIGHLIGHT, FROST_BG_PANEL);
        
        // Path bar
        frost_text(2, 2, "Path:", FROST_FG_SECONDARY, FROST_BG_PANEL);
        frost_text(8, 2, path, FROST_FG_ACCENT, FROST_BG_PANEL);
        frost_hline(1, 3, FROST_SCREEN_W - 2, 0xC4, FROST_FG_SECONDARY, FROST_BG_PANEL);
        
        // File list
        int visible = FROST_SCREEN_H - 7;
        for (int i = 0; i < visible && i + scroll < file_count; i++) {
            int idx = i + scroll;
            bool sel = (idx == selected);
            bool is_dir = (files[idx][0] == '/');
            
            u8 bg = sel ? FROST_BG_WIDGET : FROST_BG_PANEL;
            u8 fg = is_dir ? FROST_FG_ACCENT : FROST_FG_PRIMARY;
            if (sel) fg = FROST_FG_HIGHLIGHT;
            
            frost_hline(1, 4 + i, FROST_SCREEN_W - 2, ' ', fg, bg);
            frost_text(3, 4 + i, is_dir ? "[DIR]" : "[FILE]", FROST_FG_SECONDARY, bg);
            frost_text(10, 4 + i, files[idx] + 1, fg, bg);
        }
        
        if (file_count == 0) {
            frost_text(3, 6, "(empty directory)", FROST_FG_SECONDARY, FROST_BG_PANEL);
        }
        
        frost_text(2, FROST_SCREEN_H - 2, "ESC=Exit | ENTER=Open | BACKSPACE=Up", FROST_FG_SECONDARY, FROST_BG_PANEL);
        frost_flush();
        
        unsigned char key = keyboard_getc();
        
        if (key == 27) break;
        else if (key == KEY_UP && selected > 0) {
            selected--;
            if (selected < scroll) scroll = selected;
        }
        else if (key == KEY_DOWN && selected < file_count - 1) {
            selected++;
            if (selected >= scroll + visible) scroll = selected - visible + 1;
        }
        else if (key == '\b' && frost_strlen(path) > 1) {
            // Go up
            int len = frost_strlen(path);
            while (len > 1 && path[len - 1] == '/') path[--len] = 0;
            while (len > 1 && path[len - 1] != '/') path[--len] = 0;
            if (len == 0) { path[0] = '/'; path[1] = 0; }
            selected = 0;
            scroll = 0;
        }
        else if ((key == '\n' || key == '\r') && file_count > 0) {
            if (files[selected][0] == '/') {
                // Enter directory
                int plen = frost_strlen(path);
                if (path[plen - 1] != '/') path[plen++] = '/';
                frost_strcpy(path + plen, files[selected] + 1);
                selected = 0;
                scroll = 0;
            }
        }
    }
}

// ============ Terminal App ============

static void run_terminal(void) {
    // Return to TTY mode
    frost_running = false;
}

// ============ Settings App ============

static void run_settings(void) {
    int selected = 0;
    const char *options[] = {
        "Display Settings",
        "Keyboard Settings", 
        "Network Settings",
        "System Information",
        "About Frost"
    };
    int num_options = 5;
    
    while (1) {
        frost_clear(FROST_BG_DARK);
        
        frost_box(10, 3, 60, 18, FROST_FG_ACCENT, FROST_BG_PANEL, true);
        frost_text(12, 3, " Settings ", FROST_FG_HIGHLIGHT, FROST_BG_PANEL);
        
        for (int i = 0; i < num_options; i++) {
            bool sel = (i == selected);
            u8 bg = sel ? FROST_BG_WIDGET : FROST_BG_PANEL;
            u8 fg = sel ? FROST_FG_HIGHLIGHT : FROST_FG_PRIMARY;
            
            frost_hline(11, 5 + i * 2, 58, ' ', fg, bg);
            frost_text(14, 5 + i * 2, options[i], fg, bg);
        }
        
        frost_text(12, 18, "UP/DOWN=Navigate | ENTER=Select | ESC=Exit", FROST_FG_SECONDARY, FROST_BG_PANEL);
        frost_flush();
        
        unsigned char key = keyboard_getc();
        
        if (key == 27) break;
        else if (key == KEY_UP && selected > 0) selected--;
        else if (key == KEY_DOWN && selected < num_options - 1) selected++;
        else if (key == '\n' || key == '\r') {
            frost_msgbox("Settings", "Settings panel coming soon!");
        }
    }
}

// ============ System Info App ============

static void run_sysinfo(void) {
    while (1) {
        frost_clear(FROST_BG_DARK);
        
        frost_box(5, 2, 70, 20, FROST_FG_ACCENT, FROST_BG_PANEL, true);
        frost_text(7, 2, " System Information ", FROST_FG_HIGHLIGHT, FROST_BG_PANEL);
        
        frost_text(8, 4, "Operating System:", FROST_FG_SECONDARY, FROST_BG_PANEL);
        frost_text(30, 4, "ICE OS v1.0", FROST_FG_PRIMARY, FROST_BG_PANEL);
        
        frost_text(8, 5, "GUI Layer:", FROST_FG_SECONDARY, FROST_BG_PANEL);
        frost_text(30, 5, "Frost 1.0", FROST_FG_PRIMARY, FROST_BG_PANEL);
        
        frost_text(8, 6, "Architecture:", FROST_FG_SECONDARY, FROST_BG_PANEL);
        frost_text(30, 6, "x86 (32-bit)", FROST_FG_PRIMARY, FROST_BG_PANEL);
        
        frost_text(8, 7, "Filesystem:", FROST_FG_SECONDARY, FROST_BG_PANEL);
        frost_text(30, 7, "EXT2", FROST_FG_PRIMARY, FROST_BG_PANEL);
        
        frost_hline(6, 9, 68, 0xC4, FROST_FG_SECONDARY, FROST_BG_PANEL);
        frost_text(8, 9, " Memory ", FROST_FG_ACCENT, FROST_BG_PANEL);
        
        u32 total = pmm_get_total_memory();
        u32 free_mem = pmm_get_free_memory();
        char buf[32];
        
        frost_text(8, 11, "Total Memory:", FROST_FG_SECONDARY, FROST_BG_PANEL);
        frost_itoa(total / 1024, buf);
        frost_text(30, 11, buf, FROST_FG_PRIMARY, FROST_BG_PANEL);
        frost_text(30 + frost_strlen(buf), 11, " KB", FROST_FG_SECONDARY, FROST_BG_PANEL);
        
        frost_text(8, 12, "Free Memory:", FROST_FG_SECONDARY, FROST_BG_PANEL);
        frost_itoa(free_mem / 1024, buf);
        frost_text(30, 12, buf, FROST_FG_SUCCESS, FROST_BG_PANEL);
        frost_text(30 + frost_strlen(buf), 12, " KB", FROST_FG_SECONDARY, FROST_BG_PANEL);
        
        frost_text(8, 13, "Used Memory:", FROST_FG_SECONDARY, FROST_BG_PANEL);
        frost_itoa((total - free_mem) / 1024, buf);
        frost_text(30, 13, buf, FROST_FG_ERROR, FROST_BG_PANEL);
        frost_text(30 + frost_strlen(buf), 13, " KB", FROST_FG_SECONDARY, FROST_BG_PANEL);
        
        frost_hline(6, 15, 68, 0xC4, FROST_FG_SECONDARY, FROST_BG_PANEL);
        frost_text(8, 15, " Supported Languages ", FROST_FG_ACCENT, FROST_BG_PANEL);
        
        frost_text(8, 17, "C, C++, Assembly, Python, Rust, Go, Shell,", FROST_FG_SECONDARY, FROST_BG_PANEL);
        frost_text(8, 18, "Lua, Ruby, Perl, JavaScript, BASIC, Tcl, AWK", FROST_FG_SECONDARY, FROST_BG_PANEL);
        
        frost_text(8, 20, "Press ESC to return", FROST_FG_SECONDARY, FROST_BG_PANEL);
        frost_flush();
        
        unsigned char key = keyboard_getc();
        if (key == 27) break;
    }
}

// ============ Games App ============

static void run_games(void) {
    int selected = 0;
    const char *games[] = {
        "Snake",
        "Guess the Number",
        "Tic-Tac-Toe",
        "Memory Match"
    };
    int num_games = 4;
    
    while (1) {
        frost_clear(FROST_BG_DARK);
        
        frost_box(15, 4, 50, 16, FROST_FG_ACCENT, FROST_BG_PANEL, true);
        frost_text(17, 4, " Games ", FROST_FG_HIGHLIGHT, FROST_BG_PANEL);
        
        for (int i = 0; i < num_games; i++) {
            bool sel = (i == selected);
            u8 bg = sel ? FROST_BG_WIDGET : FROST_BG_PANEL;
            u8 fg = sel ? FROST_FG_HIGHLIGHT : FROST_FG_PRIMARY;
            
            frost_hline(16, 6 + i * 2, 48, ' ', fg, bg);
            frost_text(20, 6 + i * 2, games[i], fg, bg);
        }
        
        frost_text(17, 17, "UP/DOWN=Navigate | ENTER=Play | ESC=Exit", FROST_FG_SECONDARY, FROST_BG_PANEL);
        frost_flush();
        
        unsigned char key = keyboard_getc();
        
        if (key == 27) break;
        else if (key == KEY_UP && selected > 0) selected--;
        else if (key == KEY_DOWN && selected < num_games - 1) selected++;
        else if (key == '\n' || key == '\r') {
            frost_msgbox("Games", "Game coming soon!");
        }
    }
}

// ============ Main Frost Loop ============

void frost_init(void) {
    if (frost_initialized) return;
    
    // Clear buffers
    frost_memset(&screen_buffer, 0, sizeof(screen_buffer));
    frost_memset(&back_buffer, 0, sizeof(back_buffer));
    
    // Register built-in apps
    for (int i = 0; i < 8; i++) {
        frost_register_app(&builtin_apps[i]);
    }
    
    frost_initialized = true;
}

void frost_run(void) {
    frost_init();
    frost_load_config();  // Load .frostui if exists
    frost_running = true;
    cursor_tick = 0;
    cursor_phase = 0;
    key_count = 0;
    last_key = 0;
    desktop_selection = 0;
    show_launcher = false;
    
    // Initial draw
    draw_desktop();
    frost_flush();
    
    while (frost_running) {
        // Update smooth cursor animation
        cursor_tick++;
        if (cursor_tick >= CURSOR_PHASE_TICKS) {
            cursor_tick = 0;
            cursor_phase = (cursor_phase + 1) % CURSOR_PHASES;
        }
        
        // Render
        if (show_launcher) {
            draw_desktop();
            draw_launcher();
        } else {
            draw_desktop();
        }
        
        // Debug info overlay - shows selection and last key received
        if (frost_debug) {
            char dbg[20];
            // Show selection
            frost_text(35, 0, "Sel:", theme.fg_secondary, theme.bg_panel);
            frost_itoa(desktop_selection, dbg);
            frost_text(40, 0, dbg, theme.fg_highlight, theme.bg_panel);
            // Show last key code
            frost_text(43, 0, "Key:", theme.fg_secondary, theme.bg_panel);
            frost_itoa(last_key, dbg);
            frost_text(48, 0, "   ", theme.bg_panel, theme.bg_panel);  // Clear
            frost_text(48, 0, dbg, theme.fg_accent, theme.bg_panel);
        }
        
        frost_flush();
        
        // Handle input - non-blocking check
        unsigned char key = keyboard_read();
        
        if (key == 0) {
            // No key - small delay for cursor animation
            pit_sleep_ms(30);
            continue;
        }
        
        // Debug tracking
        last_key = key;
        key_count++;
        
        // Handle navigation keys
        bool handled = false;
        
        // ESC key
        if (key == 27) {
            handled = true;
            if (show_launcher) {
                show_launcher = false;
            } else {
                if (frost_confirm("Exit Frost", "Return to terminal?")) {
                    frost_running = false;
                }
            }
        }
        
        // Space - toggle launcher
        if (key == ' ') {
            handled = true;
            show_launcher = !show_launcher;
            if (show_launcher) {
                desktop_selection = 0;
                launcher_scroll = 0;
            }
        }
        
        // Arrow keys and WASD navigation
        if (key == KEY_UP || key == 'w' || key == 'W') {
            handled = true;
            if (show_launcher) {
                if (desktop_selection > 0) {
                    desktop_selection--;
                    if (desktop_selection < launcher_scroll) launcher_scroll = desktop_selection;
                }
            } else {
                // Desktop: 2 rows x 4 cols - go up one row
                if (desktop_selection >= 4) {
                    desktop_selection -= 4;
                }
            }
        }
        
        if (key == KEY_DOWN || key == 's' || key == 'S') {
            handled = true;
            if (show_launcher) {
                if (desktop_selection < app_count - 1) {
                    desktop_selection++;
                    if (desktop_selection >= launcher_scroll + 12) launcher_scroll = desktop_selection - 11;
                }
            } else {
                // Desktop: go down one row
                if (desktop_selection < 4) {
                    desktop_selection += 4;
                }
            }
        }
        
        if (key == KEY_LEFT || key == 'a' || key == 'A') {
            handled = true;
            if (!show_launcher) {
                // Stay in same row when moving left
                int col = desktop_selection % 4;
                if (col > 0) desktop_selection--;
            }
        }
        
        if (key == KEY_RIGHT || key == 'd' || key == 'D') {
            handled = true;
            if (!show_launcher) {
                // Stay in same row when moving right  
                int col = desktop_selection % 4;
                if (col < 3) desktop_selection++;
            }
        }
        
        // Enter - activate selection
        if (key == '\n' || key == '\r') {
            handled = true;
            if (show_launcher && desktop_selection < app_count) {
                show_launcher = false;
                registered_apps[desktop_selection]->run();
                if (!frost_running) break;
                keyboard_flush();  // Reset keyboard after app exit
                frost_redraw();
            } else if (!show_launcher && desktop_selection < 8) {
                builtin_apps[desktop_selection].run();
                if (!frost_running) break;
                keyboard_flush();  // Reset keyboard after app exit
                frost_redraw();
            }
        }
        
        // Number keys 1-8 for quick launch
        if (key >= '1' && key <= '8') {
            handled = true;
            int idx = key - '1';
            builtin_apps[idx].run();
            if (!frost_running) break;
            keyboard_flush();  // Reset keyboard after app exit
            frost_redraw();
        }
        
        // Tab - cycle through options
        if (key == '\t') {
            handled = true;
            if (show_launcher) {
                desktop_selection = (desktop_selection + 1) % app_count;
                if (desktop_selection >= launcher_scroll + 12) launcher_scroll = desktop_selection - 11;
                if (desktop_selection < launcher_scroll) launcher_scroll = desktop_selection;
            } else {
                desktop_selection = (desktop_selection + 1) % 8;
            }
        }
        
        // Ctrl+D toggle debug
        if (key == 4) {  // Ctrl+D = 4
            frost_debug = !frost_debug;
        }
        
        (void)handled; // Suppress unused warning
        
        // Small delay after key processing
        pit_sleep_ms(10);
    }
    
    // Return to TTY
    vga_clear();
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

void frost_shutdown(void) {
    frost_running = false;
    frost_initialized = false;
    app_count = 0;
    widget_pool_idx = 0;
}

