/*
 * FROST - Graphical User Interface Layer for ICE OS
 * A modern, performant GUI framework with widget system
 */

#ifndef FROST_H
#define FROST_H

#include "../types.h"
#include "../drivers/keyboard.h"

// Frost color palette (cyberpunk/ice theme)
#define FROST_BG_DARK       0x0A    // Dark blue-black
#define FROST_BG_PANEL      0x01    // Deep blue
#define FROST_BG_WIDGET     0x09    // Bright blue
#define FROST_FG_PRIMARY    0x0F    // White
#define FROST_FG_SECONDARY  0x07    // Light gray
#define FROST_FG_ACCENT     0x0B    // Cyan
#define FROST_FG_HIGHLIGHT  0x0E    // Yellow
#define FROST_FG_SUCCESS    0x0A    // Green
#define FROST_FG_ERROR      0x0C    // Red
#define FROST_BORDER        0x03    // Cyan dark

// Screen dimensions
#define FROST_SCREEN_W      80
#define FROST_SCREEN_H      25

// Widget types
typedef enum {
    FROST_WIDGET_NONE = 0,
    FROST_WIDGET_LABEL,
    FROST_WIDGET_BUTTON,
    FROST_WIDGET_INPUT,
    FROST_WIDGET_LIST,
    FROST_WIDGET_PANEL,
    FROST_WIDGET_MENU,
    FROST_WIDGET_PROGRESS,
    FROST_WIDGET_CHECKBOX,
    FROST_WIDGET_DIVIDER
} frost_widget_type_t;

// Widget flags
#define WIDGET_VISIBLE      0x01
#define WIDGET_ENABLED      0x02
#define WIDGET_FOCUSED      0x04
#define WIDGET_DIRTY        0x08
#define WIDGET_HOVER        0x10

// Forward declarations
struct frost_widget;
struct frost_app;

// Event types
typedef enum {
    FROST_EVENT_NONE = 0,
    FROST_EVENT_KEY,
    FROST_EVENT_CLICK,
    FROST_EVENT_FOCUS,
    FROST_EVENT_BLUR,
    FROST_EVENT_TICK
} frost_event_type_t;

// Event structure
typedef struct {
    frost_event_type_t type;
    unsigned char key;
    int mouse_x;
    int mouse_y;
    int mouse_btn;
} frost_event_t;

// Widget callback
typedef void (*frost_callback_t)(struct frost_widget *widget, frost_event_t *event);

// Widget structure
typedef struct frost_widget {
    frost_widget_type_t type;
    u8 flags;
    int x, y, w, h;
    char text[64];
    char value[128];
    int int_value;
    u8 fg_color;
    u8 bg_color;
    frost_callback_t on_click;
    frost_callback_t on_key;
    struct frost_widget *next;
    void *user_data;
} frost_widget_t;

// Window structure
typedef struct {
    char title[32];
    int x, y, w, h;
    u8 flags;
    frost_widget_t *widgets;
    frost_widget_t *focused;
    int widget_count;
    bool dirty;
} frost_window_t;

// App structure
typedef struct frost_app {
    char name[32];
    char icon[4];
    char description[64];
    void (*run)(void);
    void (*update)(frost_event_t *event);
    void (*render)(void);
    void (*cleanup)(void);
    bool running;
} frost_app_t;

// Screen buffer for double buffering
typedef struct {
    char chars[FROST_SCREEN_H][FROST_SCREEN_W];
    u8 colors[FROST_SCREEN_H][FROST_SCREEN_W];
    bool dirty[FROST_SCREEN_H][FROST_SCREEN_W];
} frost_buffer_t;

// ============ Core Functions ============

// Initialize Frost layer
void frost_init(void);

// Main loop - run the Frost desktop
void frost_run(void);

// Shutdown Frost
void frost_shutdown(void);

// ============ Rendering Functions ============

// Clear screen buffer
void frost_clear(u8 bg);

// Draw text at position
void frost_text(int x, int y, const char *text, u8 fg, u8 bg);

// Draw box/panel
void frost_box(int x, int y, int w, int h, u8 fg, u8 bg, bool border);

// Draw horizontal line
void frost_hline(int x, int y, int len, char ch, u8 fg, u8 bg);

// Draw vertical line
void frost_vline(int x, int y, int len, char ch, u8 fg, u8 bg);

// Flush buffer to screen (only dirty regions)
void frost_flush(void);

// Force full redraw
void frost_redraw(void);

// ============ Widget Functions ============

// Create widgets
frost_widget_t *frost_label(int x, int y, const char *text);
frost_widget_t *frost_button(int x, int y, const char *text, frost_callback_t on_click);
frost_widget_t *frost_input(int x, int y, int w);
frost_widget_t *frost_checkbox(int x, int y, const char *text, bool checked);
frost_widget_t *frost_progress(int x, int y, int w, int value, int max);

// Widget operations
void frost_widget_set_text(frost_widget_t *w, const char *text);
void frost_widget_set_value(frost_widget_t *w, int value);
void frost_widget_focus(frost_widget_t *w);
void frost_widget_render(frost_widget_t *w);
void frost_widget_free(frost_widget_t *w);

// ============ Window Functions ============

// Create/destroy windows
frost_window_t *frost_window_create(const char *title, int x, int y, int w, int h);
void frost_window_destroy(frost_window_t *win);

// Window operations
void frost_window_add_widget(frost_window_t *win, frost_widget_t *widget);
void frost_window_render(frost_window_t *win);
void frost_window_handle_event(frost_window_t *win, frost_event_t *event);

// ============ App Functions ============

// Register an app
void frost_register_app(frost_app_t *app);

// Get app by name
frost_app_t *frost_get_app(const char *name);

// Launch app
void frost_launch_app(const char *name);

// Get app list
frost_app_t **frost_get_apps(int *count);

// ============ Dialog Functions ============

// Show message box
void frost_msgbox(const char *title, const char *message);

// Show input dialog
bool frost_input_dialog(const char *title, const char *prompt, char *buffer, int max_len);

// Show confirmation dialog
bool frost_confirm(const char *title, const char *message);

// ============ Built-in Apps ============

// App entry points
void frost_app_calculator(void);
void frost_app_browser(void);
void frost_app_notepad(void);
void frost_app_files(void);
void frost_app_terminal(void);
void frost_app_settings(void);
void frost_app_sysinfo(void);
void frost_app_games(void);

#endif // FROST_H

