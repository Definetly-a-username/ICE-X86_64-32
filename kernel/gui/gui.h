/**
 * ICE GUI Framework
 * Simple graphical user interface for ICE OS
 */

#ifndef ICE_GUI_H
#define ICE_GUI_H

#include "../types.h"

// GUI Mode
typedef enum {
    GUI_MODE_TEXT = 0,  // Text/TTY mode
    GUI_MODE_GRAPHIC    // Graphical mode
} gui_mode_t;

// Widget types
typedef enum {
    WIDGET_WINDOW,
    WIDGET_BUTTON,
    WIDGET_LABEL,
    WIDGET_TEXTBOX,
    WIDGET_LISTBOX,
    WIDGET_MENUBAR,
    WIDGET_ICON
} widget_type_t;

// Window flags
#define WIN_FLAG_VISIBLE    0x01
#define WIN_FLAG_MOVABLE    0x02
#define WIN_FLAG_RESIZABLE  0x04
#define WIN_FLAG_CLOSEABLE  0x08
#define WIN_FLAG_FOCUSED    0x10
#define WIN_FLAG_MAXIMIZED  0x20

// Maximum windows and icons
#define MAX_WINDOWS 16
#define MAX_ICONS   32
#define MAX_MENU_ITEMS 10

// Window structure
typedef struct {
    u32 id;
    char title[64];
    u32 x, y, w, h;
    u32 flags;
    u32 bg_color;
    void (*on_click)(u32 x, u32 y);
    void (*on_draw)(u32 id);
    void (*on_key)(char key);
    bool active;
} gui_window_t;

// Desktop icon
typedef struct {
    char name[32];
    u32 x, y;
    u32 icon_id;  // Icon identifier
    void (*on_click)(void);
    bool visible;
} gui_icon_t;

// Menu item
typedef struct {
    char label[32];
    void (*on_click)(void);
    bool enabled;
} gui_menu_item_t;

// Menu
typedef struct {
    char title[32];
    gui_menu_item_t items[MAX_MENU_ITEMS];
    int item_count;
    bool open;
} gui_menu_t;

// Initialize GUI system
int gui_init(void);

// Set GUI mode (text or graphic)
void gui_set_mode(gui_mode_t mode);
gui_mode_t gui_get_mode(void);

// Boot menu to choose mode
gui_mode_t gui_boot_menu(void);

// Main GUI loop
void gui_run(void);

// Window management
u32 gui_create_window(const char *title, u32 x, u32 y, u32 w, u32 h, u32 flags);
void gui_destroy_window(u32 id);
void gui_show_window(u32 id);
void gui_hide_window(u32 id);
void gui_focus_window(u32 id);
gui_window_t* gui_get_window(u32 id);

// Drawing
void gui_draw_desktop(void);
void gui_draw_taskbar(void);
void gui_draw_window(u32 id);
void gui_draw_button(u32 x, u32 y, u32 w, u32 h, const char *text, bool pressed);
void gui_draw_icon(u32 x, u32 y, u32 icon_id, const char *label);

// Event handling
void gui_handle_mouse(u32 x, u32 y, bool left, bool right);
void gui_handle_key(char key);

// Desktop icons
void gui_add_icon(const char *name, u32 x, u32 y, u32 icon_id, void (*on_click)(void));

// Menu system
void gui_create_menu(const char *title);
void gui_add_menu_item(const char *label, void (*on_click)(void));

// Message box
void gui_message_box(const char *title, const char *message);

// Terminal emulator window
u32 gui_create_terminal(void);
void gui_terminal_putchar(u32 term_id, char c);
void gui_terminal_puts(u32 term_id, const char *str);

// File browser
u32 gui_create_file_browser(const char *path);

// Simple text-based GUI (for text mode)
void gui_text_menu(const char *title, const char **options, int count, int *selected);
void gui_text_dialog(const char *title, const char *message);
int gui_text_input(const char *prompt, char *buffer, int max_len);

#endif // ICE_GUI_H

