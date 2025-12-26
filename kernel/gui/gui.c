/**
 * ICE GUI Framework Implementation
 * 
 * Provides a simple graphical/text-based user interface with mouse support.
 * Uses VGA text mode with extended colors for a pseudo-GUI experience.
 */

#include "gui.h"
#include "vesa.h"
#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "../drivers/mouse.h"
#include "../drivers/pit.h"
#include "../tty/tty.h"
#include "../apps/apps.h"
#include "../apps/script.h"
#include "../fs/vfs.h"

// GUI state
static gui_mode_t current_mode = GUI_MODE_TEXT;
static gui_window_t windows[MAX_WINDOWS];
static gui_icon_t icons[MAX_ICONS];
static int window_count = 0;
static int active_window = -1;

// Mouse cursor position and state
static int cursor_x = 40;
static int cursor_y = 12;
static u16 cursor_saved_char = 0;
static bool cursor_visible = true;

// App launcher state
#define MAX_APPS 20
typedef struct {
    char name[32];
    char description[64];
    char command[64];
    char icon;
    bool is_script;
} gui_app_entry_t;

static gui_app_entry_t app_list[MAX_APPS];
static int app_count = 0;

// Text-mode GUI colors
#define TG_DESKTOP_BG    VGA_COLOR_BLUE
#define TG_WINDOW_BG     VGA_COLOR_LIGHT_GREY
#define TG_WINDOW_TITLE  VGA_COLOR_WHITE
#define TG_TITLE_BG      VGA_COLOR_LIGHT_BLUE
#define TG_BUTTON_BG     VGA_COLOR_DARK_GREY
#define TG_TASKBAR_BG    VGA_COLOR_DARK_GREY
#define TG_TEXT          VGA_COLOR_BLACK
#define TG_HIGHLIGHT     VGA_COLOR_LIGHT_CYAN
#define TG_CURSOR_BG     VGA_COLOR_WHITE

// VGA text mode buffer
#define VGA_TEXT_BUFFER ((u16*)0xB8000)
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

// Simple string operations
static int strlen_gui(const char *s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

static void strcpy_gui(char *dst, const char *src) {
    while ((*dst++ = *src++));
}

static int strcmp_gui(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a - *b;
}

// Draw a box in text mode
static void draw_text_box(int x, int y, int w, int h, u8 fg, u8 bg) {
    u8 attr = (bg << 4) | fg;
    
    // Top border
    VGA_TEXT_BUFFER[y * VGA_WIDTH + x] = 0xC9 | (attr << 8);
    for (int i = 1; i < w - 1; i++) {
        VGA_TEXT_BUFFER[y * VGA_WIDTH + x + i] = 0xCD | (attr << 8);
    }
    VGA_TEXT_BUFFER[y * VGA_WIDTH + x + w - 1] = 0xBB | (attr << 8);
    
    // Middle
    for (int j = 1; j < h - 1; j++) {
        VGA_TEXT_BUFFER[(y + j) * VGA_WIDTH + x] = 0xBA | (attr << 8);
        for (int i = 1; i < w - 1; i++) {
            VGA_TEXT_BUFFER[(y + j) * VGA_WIDTH + x + i] = ' ' | (attr << 8);
        }
        VGA_TEXT_BUFFER[(y + j) * VGA_WIDTH + x + w - 1] = 0xBA | (attr << 8);
    }
    
    // Bottom border
    VGA_TEXT_BUFFER[(y + h - 1) * VGA_WIDTH + x] = 0xC8 | (attr << 8);
    for (int i = 1; i < w - 1; i++) {
        VGA_TEXT_BUFFER[(y + h - 1) * VGA_WIDTH + x + i] = 0xCD | (attr << 8);
    }
    VGA_TEXT_BUFFER[(y + h - 1) * VGA_WIDTH + x + w - 1] = 0xBC | (attr << 8);
}

// Draw text at position
static void draw_text_at(int x, int y, const char *text, u8 fg, u8 bg) {
    u8 attr = (bg << 4) | fg;
    int i = 0;
    while (text[i] && x + i < VGA_WIDTH) {
        VGA_TEXT_BUFFER[y * VGA_WIDTH + x + i] = text[i] | (attr << 8);
        i++;
    }
}

// Fill area with color (preserves keyboard debug area at row 0, cols 66-79)
static void fill_text_area(int x, int y, int w, int h, u8 fg, u8 bg) {
    u8 attr = (bg << 4) | fg;
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            int px = x + i;
            int py = y + j;
            if (py < VGA_HEIGHT && px < VGA_WIDTH) {
                // Skip keyboard debug area
                if (py == 0 && px >= 66) continue;
                VGA_TEXT_BUFFER[py * VGA_WIDTH + px] = ' ' | (attr << 8);
            }
        }
    }
}

// Draw a clickable button
static void draw_text_button(int x, int y, int w, const char *text, bool selected, bool hover) {
    u8 bg, fg;
    
    if (hover) {
        bg = VGA_COLOR_LIGHT_CYAN;
        fg = VGA_COLOR_BLACK;
    } else if (selected) {
        bg = VGA_COLOR_LIGHT_BLUE;
        fg = VGA_COLOR_WHITE;
    } else {
        bg = VGA_COLOR_DARK_GREY;
        fg = VGA_COLOR_WHITE;
    }
    
    u8 attr = (bg << 4) | fg;
    
    // Draw button with brackets
    VGA_TEXT_BUFFER[y * VGA_WIDTH + x] = '[' | (attr << 8);
    
    int text_len = strlen_gui(text);
    int padding = (w - 2 - text_len) / 2;
    
    for (int i = 1; i < w - 1; i++) {
        if (i > padding && i <= padding + text_len) {
            VGA_TEXT_BUFFER[y * VGA_WIDTH + x + i] = text[i - padding - 1] | (attr << 8);
        } else {
            VGA_TEXT_BUFFER[y * VGA_WIDTH + x + i] = ' ' | (attr << 8);
        }
    }
    
    VGA_TEXT_BUFFER[y * VGA_WIDTH + x + w - 1] = ']' | (attr << 8);
}

// Check if point is inside rectangle
static bool point_in_rect(int px, int py, int x, int y, int w, int h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

// Hide mouse cursor
static void hide_cursor(void) {
    if (cursor_visible && cursor_x >= 0 && cursor_x < VGA_WIDTH && 
        cursor_y >= 0 && cursor_y < VGA_HEIGHT) {
        VGA_TEXT_BUFFER[cursor_y * VGA_WIDTH + cursor_x] = cursor_saved_char;
    }
}

// Show mouse cursor
static void show_cursor(void) {
    if (cursor_x >= 0 && cursor_x < VGA_WIDTH && 
        cursor_y >= 0 && cursor_y < VGA_HEIGHT) {
        cursor_saved_char = VGA_TEXT_BUFFER[cursor_y * VGA_WIDTH + cursor_x];
        // Draw cursor as inverse block
        u8 orig_char = cursor_saved_char & 0xFF;
        VGA_TEXT_BUFFER[cursor_y * VGA_WIDTH + cursor_x] = 
            (orig_char ? orig_char : 0xDB) | (((VGA_COLOR_WHITE << 4) | VGA_COLOR_BLACK) << 8);
        cursor_visible = true;
    }
}

// Update cursor position
static void update_cursor(void) {
    hide_cursor();
    
    mouse_state_t *ms = mouse_get_state();
    cursor_x = ms->x;
    cursor_y = ms->y;
    
    show_cursor();
}

// Initialize app list
static void init_app_list(void) {
    app_count = 0;
    
    // Built-in apps
    strcpy_gui(app_list[app_count].name, "Terminal");
    strcpy_gui(app_list[app_count].description, "Command line interface");
    strcpy_gui(app_list[app_count].command, "terminal");
    app_list[app_count].icon = '>';
    app_list[app_count].is_script = false;
    app_count++;
    
    strcpy_gui(app_list[app_count].name, "File Manager");
    strcpy_gui(app_list[app_count].description, "Browse files and directories");
    strcpy_gui(app_list[app_count].command, "files");
    app_list[app_count].icon = 'F';
    app_list[app_count].is_script = false;
    app_count++;
    
    strcpy_gui(app_list[app_count].name, "Text Editor");
    strcpy_gui(app_list[app_count].description, "ICED text editor");
    strcpy_gui(app_list[app_count].command, "iced");
    app_list[app_count].icon = 'E';
    app_list[app_count].is_script = false;
    app_count++;
    
    strcpy_gui(app_list[app_count].name, "ICE Browser");
    strcpy_gui(app_list[app_count].description, "Simple text-based web browser");
    strcpy_gui(app_list[app_count].command, "browser");
    app_list[app_count].icon = 'W';
    app_list[app_count].is_script = false;
    app_count++;
    
    strcpy_gui(app_list[app_count].name, "Calculator");
    strcpy_gui(app_list[app_count].description, "Basic calculator");
    strcpy_gui(app_list[app_count].command, "calc");
    app_list[app_count].icon = '#';
    app_list[app_count].is_script = false;
    app_count++;
    
    strcpy_gui(app_list[app_count].name, "System Info");
    strcpy_gui(app_list[app_count].description, "View system information");
    strcpy_gui(app_list[app_count].command, "sysinfo");
    app_list[app_count].icon = 'i';
    app_list[app_count].is_script = false;
    app_count++;
    
    strcpy_gui(app_list[app_count].name, "Network");
    strcpy_gui(app_list[app_count].description, "Network configuration");
    strcpy_gui(app_list[app_count].command, "network");
    app_list[app_count].icon = 'N';
    app_list[app_count].is_script = false;
    app_count++;
    
    strcpy_gui(app_list[app_count].name, "Settings");
    strcpy_gui(app_list[app_count].description, "System settings");
    strcpy_gui(app_list[app_count].command, "settings");
    app_list[app_count].icon = '*';
    app_list[app_count].is_script = false;
    app_count++;
    
    strcpy_gui(app_list[app_count].name, "Help");
    strcpy_gui(app_list[app_count].description, "View help and documentation");
    strcpy_gui(app_list[app_count].command, "help_gui");
    app_list[app_count].icon = '?';
    app_list[app_count].is_script = false;
    app_count++;
    
    strcpy_gui(app_list[app_count].name, "Python Runner");
    strcpy_gui(app_list[app_count].description, "Run Python scripts");
    strcpy_gui(app_list[app_count].command, "python");
    app_list[app_count].icon = 'P';
    app_list[app_count].is_script = false;
    app_count++;
}

int gui_init(void) {
    // Initialize window array
    for (int i = 0; i < MAX_WINDOWS; i++) {
        windows[i].active = false;
    }
    
    // Initialize icons array
    for (int i = 0; i < MAX_ICONS; i++) {
        icons[i].visible = false;
    }
    
    window_count = 0;
    active_window = -1;
    current_mode = GUI_MODE_TEXT;
    
    // Initialize app list
    init_app_list();
    
    // Initialize mouse
    mouse_init();
    mouse_set_bounds(0, 0, VGA_WIDTH - 1, VGA_HEIGHT - 1);
    mouse_set_pos(VGA_WIDTH / 2, VGA_HEIGHT / 2);
    
    return 0;
}

void gui_set_mode(gui_mode_t mode) {
    current_mode = mode;
}

gui_mode_t gui_get_mode(void) {
    return current_mode;
}

// Boot menu to choose between GUI and TTY
gui_mode_t gui_boot_menu(void) {
    vga_clear();
    
    // Fill background
    fill_text_area(0, 0, 80, 25, VGA_COLOR_WHITE, VGA_COLOR_BLUE);
    
    // Title box
    draw_text_box(20, 5, 40, 3, VGA_COLOR_WHITE, VGA_COLOR_LIGHT_BLUE);
    draw_text_at(28, 6, "ICE Operating System", VGA_COLOR_WHITE, VGA_COLOR_LIGHT_BLUE);
    
    // Menu box
    draw_text_box(20, 9, 40, 10, VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
    draw_text_at(26, 10, "Select Boot Mode:", VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
    
    int selected = 0;
    const int num_options = 2;
    int btn1_y = 12, btn2_y = 14;
    int btn_x = 25, btn_w = 30;
    
    // Initialize mouse for menu
    mouse_set_bounds(0, 0, VGA_WIDTH - 1, VGA_HEIGHT - 1);
    mouse_set_pos(40, 12);
    cursor_x = 40;
    cursor_y = 12;
    
    while (1) {
        // Poll mouse
        mouse_poll();
        mouse_state_t *ms = mouse_get_state();
        
        // Update cursor
        hide_cursor();
        cursor_x = ms->x;
        cursor_y = ms->y;
        
        // Check hover
        bool hover1 = point_in_rect(cursor_x, cursor_y, btn_x, btn1_y, btn_w, 1);
        bool hover2 = point_in_rect(cursor_x, cursor_y, btn_x, btn2_y, btn_w, 1);
        
        // Update selection based on hover
        if (hover1) selected = 0;
        if (hover2) selected = 1;
        
        // Draw options
        draw_text_button(btn_x, btn1_y, btn_w, "1. Graphical Mode (GUI)", selected == 0, hover1);
        draw_text_button(btn_x, btn2_y, btn_w, "2. Text Mode (TTY)", selected == 1, hover2);
        
        // Instructions
        draw_text_at(22, 17, "Click or press 1/2, Enter to boot", VGA_COLOR_DARK_GREY, VGA_COLOR_LIGHT_GREY);
        
        show_cursor();
        
        // Check for mouse click
        if (ms->left_click) {
            mouse_clear_click();
            if (hover1) {
                vga_clear();
                vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
                return GUI_MODE_GRAPHIC;
            }
            if (hover2) {
                vga_clear();
                vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
                return GUI_MODE_TEXT;
            }
        }
        
        // Check keyboard (non-blocking)
        unsigned char key = keyboard_read();
        
        if (key == '\n' || key == '\r') {
            vga_clear();
            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            return selected == 0 ? GUI_MODE_GRAPHIC : GUI_MODE_TEXT;
        } else if (key == '1') {
            vga_clear();
            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            return GUI_MODE_GRAPHIC;
        } else if (key == '2') {
            vga_clear();
            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            return GUI_MODE_TEXT;
        } else if (key == KEY_UP || key == 'k' || key == 'w') {
            selected = (selected - 1 + num_options) % num_options;
        } else if (key == KEY_DOWN || key == 'j' || key == 's') {
            selected = (selected + 1) % num_options;
        }
        
        // Small delay for responsiveness
        pit_sleep_ms(16);
    }
}

// Draw app launcher
static void draw_app_launcher(int selected, int scroll) {
    int list_x = 20, list_y = 4;
    int list_w = 40, list_h = 16;
    
    // Draw window
    draw_text_box(list_x, list_y, list_w, list_h, VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
    
    // Title bar
    fill_text_area(list_x + 1, list_y, list_w - 2, 1, VGA_COLOR_WHITE, VGA_COLOR_LIGHT_BLUE);
    draw_text_at(list_x + 2, list_y, "Applications", VGA_COLOR_WHITE, VGA_COLOR_LIGHT_BLUE);
    
    // Close button
    draw_text_at(list_x + list_w - 4, list_y, "[X]", VGA_COLOR_WHITE, VGA_COLOR_RED);
    
    // Draw apps
    int visible_apps = list_h - 4;
    for (int i = 0; i < visible_apps && (scroll + i) < app_count; i++) {
        int idx = scroll + i;
        int y = list_y + 2 + i;
        
        mouse_state_t *ms = mouse_get_state();
        bool hover = point_in_rect(ms->x, ms->y, list_x + 2, y, list_w - 4, 1);
        
        u8 bg, fg;
        if (idx == selected || hover) {
            bg = VGA_COLOR_LIGHT_CYAN;
            fg = VGA_COLOR_BLACK;
        } else {
            bg = VGA_COLOR_LIGHT_GREY;
            fg = VGA_COLOR_BLACK;
        }
        
        fill_text_area(list_x + 2, y, list_w - 4, 1, fg, bg);
        
        // Icon
        char icon_str[2] = {app_list[idx].icon, 0};
        draw_text_at(list_x + 3, y, icon_str, VGA_COLOR_LIGHT_BLUE, bg);
        
        // Name
        draw_text_at(list_x + 5, y, app_list[idx].name, fg, bg);
    }
    
    // Description at bottom
    if (selected >= 0 && selected < app_count) {
        fill_text_area(list_x + 2, list_y + list_h - 2, list_w - 4, 1, 
                      VGA_COLOR_DARK_GREY, VGA_COLOR_LIGHT_GREY);
        draw_text_at(list_x + 3, list_y + list_h - 2, 
                    app_list[selected].description, VGA_COLOR_DARK_GREY, VGA_COLOR_LIGHT_GREY);
    }
}

// Simple browser simulation
static void run_browser(void) {
    vga_clear();
    fill_text_area(0, 0, 80, 25, VGA_COLOR_WHITE, VGA_COLOR_BLUE);
    
    // Browser window
    draw_text_box(2, 1, 76, 22, VGA_COLOR_BLACK, VGA_COLOR_WHITE);
    
    // Title bar
    fill_text_area(3, 1, 74, 1, VGA_COLOR_WHITE, VGA_COLOR_LIGHT_BLUE);
    draw_text_at(4, 1, "ICE Browser - Home", VGA_COLOR_WHITE, VGA_COLOR_LIGHT_BLUE);
    draw_text_at(74, 1, "[X]", VGA_COLOR_WHITE, VGA_COLOR_RED);
    
    // Address bar
    fill_text_area(3, 2, 74, 1, VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
    draw_text_at(4, 2, "ice://home", VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
    
    // Content
    draw_text_at(5, 4, "Welcome to ICE Browser", VGA_COLOR_LIGHT_BLUE, VGA_COLOR_WHITE);
    draw_text_at(5, 5, "========================", VGA_COLOR_LIGHT_BLUE, VGA_COLOR_WHITE);
    draw_text_at(5, 7, "This is a simple text-based browser for ICE OS.", VGA_COLOR_BLACK, VGA_COLOR_WHITE);
    draw_text_at(5, 8, "It can display local files and basic content.", VGA_COLOR_BLACK, VGA_COLOR_WHITE);
    draw_text_at(5, 10, "Bookmarks:", VGA_COLOR_BLACK, VGA_COLOR_WHITE);
    draw_text_at(7, 11, "[1] ice://help    - Help documentation", VGA_COLOR_DARK_GREY, VGA_COLOR_WHITE);
    draw_text_at(7, 12, "[2] ice://files   - File browser", VGA_COLOR_DARK_GREY, VGA_COLOR_WHITE);
    draw_text_at(7, 13, "[3] ice://apps    - Application list", VGA_COLOR_DARK_GREY, VGA_COLOR_WHITE);
    draw_text_at(5, 15, "Press ESC to close, or click [X]", VGA_COLOR_DARK_GREY, VGA_COLOR_WHITE);
    
    // Taskbar
    fill_text_area(0, 24, 80, 1, VGA_COLOR_WHITE, TG_TASKBAR_BG);
    draw_text_at(1, 24, "[ICE]", VGA_COLOR_WHITE, TG_TASKBAR_BG);
    draw_text_at(8, 24, "ICE Browser", VGA_COLOR_LIGHT_GREY, TG_TASKBAR_BG);
    
    show_cursor();
    
    while (1) {
        mouse_poll();
        update_cursor();
        
        mouse_state_t *ms = mouse_get_state();
        
        // Check close button
        if (ms->left_click && point_in_rect(ms->x, ms->y, 74, 1, 3, 1)) {
            mouse_clear_click();
            return;
        }
        
        char key = keyboard_getc();
        if (key == 27) return; // ESC
        
        pit_sleep_ms(10);
    }
}

// Simple calculator
static void run_calculator(void) {
    vga_clear();
    fill_text_area(0, 0, 80, 25, VGA_COLOR_WHITE, VGA_COLOR_BLUE);
    
    int calc_x = 28, calc_y = 5;
    draw_text_box(calc_x, calc_y, 24, 14, VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
    
    // Title
    fill_text_area(calc_x + 1, calc_y, 22, 1, VGA_COLOR_WHITE, VGA_COLOR_LIGHT_BLUE);
    draw_text_at(calc_x + 2, calc_y, "Calculator", VGA_COLOR_WHITE, VGA_COLOR_LIGHT_BLUE);
    draw_text_at(calc_x + 20, calc_y, "[X]", VGA_COLOR_WHITE, VGA_COLOR_RED);
    
    // Display
    fill_text_area(calc_x + 2, calc_y + 2, 20, 1, VGA_COLOR_BLACK, VGA_COLOR_WHITE);
    draw_text_at(calc_x + 3, calc_y + 2, "0", VGA_COLOR_BLACK, VGA_COLOR_WHITE);
    
    // Buttons
    const char *buttons[] = {
        "7", "8", "9", "/",
        "4", "5", "6", "*",
        "1", "2", "3", "-",
        "C", "0", "=", "+"
    };
    
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            int bx = calc_x + 2 + col * 5;
            int by = calc_y + 4 + row * 2;
            draw_text_button(bx, by, 4, buttons[row * 4 + col], false, false);
        }
    }
    
    // Taskbar
    fill_text_area(0, 24, 80, 1, VGA_COLOR_WHITE, TG_TASKBAR_BG);
    draw_text_at(1, 24, "[ICE]", VGA_COLOR_WHITE, TG_TASKBAR_BG);
    draw_text_at(8, 24, "Calculator", VGA_COLOR_LIGHT_GREY, TG_TASKBAR_BG);
    
    show_cursor();
    
    char display[32] = "0";
    int display_pos = 0;
    int num1 = 0, num2 = 0;
    char op = 0;
    bool new_num = true;
    
    while (1) {
        mouse_poll();
        update_cursor();
        
        mouse_state_t *ms = mouse_get_state();
        
        // Check close button
        if (ms->left_click && point_in_rect(ms->x, ms->y, calc_x + 20, calc_y, 3, 1)) {
            mouse_clear_click();
            return;
        }
        
        // Check calculator buttons
        if (ms->left_click) {
            for (int row = 0; row < 4; row++) {
                for (int col = 0; col < 4; col++) {
                    int bx = calc_x + 2 + col * 5;
                    int by = calc_y + 4 + row * 2;
                    
                    if (point_in_rect(ms->x, ms->y, bx, by, 4, 1)) {
                        mouse_clear_click();
                        char btn = buttons[row * 4 + col][0];
                        
                        if (btn >= '0' && btn <= '9') {
                            if (new_num) {
                                display_pos = 0;
                                new_num = false;
                            }
                            if (display_pos < 10) {
                                display[display_pos++] = btn;
                                display[display_pos] = 0;
                            }
                        } else if (btn == 'C') {
                            display_pos = 0;
                            display[0] = '0';
                            display[1] = 0;
                            num1 = num2 = 0;
                            op = 0;
                            new_num = true;
                        } else if (btn == '=' && op) {
                            // Parse display
                            num2 = 0;
                            for (int i = 0; display[i]; i++) {
                                num2 = num2 * 10 + (display[i] - '0');
                            }
                            
                            int result = 0;
                            switch (op) {
                                case '+': result = num1 + num2; break;
                                case '-': result = num1 - num2; break;
                                case '*': result = num1 * num2; break;
                                case '/': result = num2 ? num1 / num2 : 0; break;
                            }
                            
                            // Convert result to string
                            display_pos = 0;
                            int temp = result < 0 ? -result : result;
                            char temp_buf[16];
                            int temp_pos = 0;
                            do {
                                temp_buf[temp_pos++] = '0' + (temp % 10);
                                temp /= 10;
                            } while (temp);
                            if (result < 0) display[display_pos++] = '-';
                            while (temp_pos > 0) {
                                display[display_pos++] = temp_buf[--temp_pos];
                            }
                            display[display_pos] = 0;
                            
                            op = 0;
                            new_num = true;
                        } else {
                            // Operator
                            num1 = 0;
                            for (int i = 0; display[i]; i++) {
                                num1 = num1 * 10 + (display[i] - '0');
                            }
                            op = btn;
                            new_num = true;
                        }
                        
                        // Update display
                        fill_text_area(calc_x + 2, calc_y + 2, 20, 1, VGA_COLOR_BLACK, VGA_COLOR_WHITE);
                        draw_text_at(calc_x + 21 - display_pos, calc_y + 2, display, VGA_COLOR_BLACK, VGA_COLOR_WHITE);
                    }
                }
            }
        }
        
        char key = keyboard_getc();
        if (key == 27) return; // ESC
        
        pit_sleep_ms(10);
    }
}

// System info display
static void run_sysinfo(void) {
    vga_clear();
    fill_text_area(0, 0, 80, 25, VGA_COLOR_WHITE, VGA_COLOR_BLUE);
    
    draw_text_box(10, 3, 60, 18, VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
    
    // Title
    fill_text_area(11, 3, 58, 1, VGA_COLOR_WHITE, VGA_COLOR_LIGHT_BLUE);
    draw_text_at(12, 3, "System Information", VGA_COLOR_WHITE, VGA_COLOR_LIGHT_BLUE);
    draw_text_at(66, 3, "[X]", VGA_COLOR_WHITE, VGA_COLOR_RED);
    
    // Info
    draw_text_at(13, 5, "ICE Operating System", VGA_COLOR_LIGHT_BLUE, VGA_COLOR_LIGHT_GREY);
    draw_text_at(13, 6, "====================", VGA_COLOR_LIGHT_BLUE, VGA_COLOR_LIGHT_GREY);
    
    draw_text_at(13, 8, "Version:    1.0.0", VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
    draw_text_at(13, 9, "Kernel:     MPM (Main Process Manager)", VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
    draw_text_at(13, 10, "Arch:       i686 (x86-32)", VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
    
    extern u32 pmm_get_total_memory(void);
    extern u32 pmm_get_free_memory(void);
    u32 total = pmm_get_total_memory();
    u32 free_mem = pmm_get_free_memory();
    
    draw_text_at(13, 12, "Memory:", VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
    
    // Simple number formatting
    char mem_str[64];
    int pos = 0;
    mem_str[pos++] = ' '; mem_str[pos++] = ' ';
    u32 total_kb = total / 1024;
    u32 free_kb = free_mem / 1024;
    
    // This is a simple display
    draw_text_at(15, 13, "Total:      See 'free' command", VGA_COLOR_DARK_GREY, VGA_COLOR_LIGHT_GREY);
    draw_text_at(15, 14, "Free:       See 'free' command", VGA_COLOR_DARK_GREY, VGA_COLOR_LIGHT_GREY);
    
    draw_text_at(13, 16, "Press ESC or click [X] to close", VGA_COLOR_DARK_GREY, VGA_COLOR_LIGHT_GREY);
    
    // Taskbar
    fill_text_area(0, 24, 80, 1, VGA_COLOR_WHITE, TG_TASKBAR_BG);
    draw_text_at(1, 24, "[ICE]", VGA_COLOR_WHITE, TG_TASKBAR_BG);
    
    show_cursor();
    
    while (1) {
        mouse_poll();
        update_cursor();
        
        mouse_state_t *ms = mouse_get_state();
        
        if (ms->left_click && point_in_rect(ms->x, ms->y, 66, 3, 3, 1)) {
            mouse_clear_click();
            return;
        }
        
        char key = keyboard_getc();
        if (key == 27) return;
        
        pit_sleep_ms(10);
    }
}

// Run an app by command
static void run_gui_app(const char *cmd) {
    hide_cursor();
    vga_clear();
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    if (strcmp_gui(cmd, "terminal") == 0) {
        // Return to shell
        return;
    }
    else if (strcmp_gui(cmd, "files") == 0) {
        char *args[] = {"ls", "-l", "/"};
        app_ls(3, args);
        tty_puts("\nPress any key to return to GUI...\n");
        keyboard_getc();
    }
    else if (strcmp_gui(cmd, "iced") == 0) {
        char *args[] = {"iced"};
        app_iced(1, args);
    }
    else if (strcmp_gui(cmd, "browser") == 0) {
        run_browser();
    }
    else if (strcmp_gui(cmd, "calc") == 0) {
        run_calculator();
    }
    else if (strcmp_gui(cmd, "sysinfo") == 0) {
        run_sysinfo();
    }
    else if (strcmp_gui(cmd, "network") == 0) {
        char *args[] = {"ifconfig"};
        app_ip(1, args);
        tty_puts("\nPress any key to return to GUI...\n");
        keyboard_getc();
    }
    else if (strcmp_gui(cmd, "settings") == 0) {
        gui_message_box("Settings", "Settings app coming soon!");
    }
    else if (strcmp_gui(cmd, "help_gui") == 0) {
        char *args[] = {"help"};
        app_help(1, args);
        tty_puts("\nPress any key to return to GUI...\n");
        keyboard_getc();
    }
    else if (strcmp_gui(cmd, "python") == 0) {
        tty_puts("Python Script Runner\n");
        tty_puts("====================\n\n");
        tty_puts("Enter path to Python script (e.g., /hello.py):\n> ");
        
        char path[128];
        tty_getline(path, sizeof(path));
        
        if (path[0]) {
            script_run_file(path);
        }
        
        tty_puts("\nPress any key to return to GUI...\n");
        keyboard_getc();
    }
}

// Draw the desktop
void gui_draw_desktop(void) {
    // Desktop background
    fill_text_area(0, 0, 80, 24, VGA_COLOR_WHITE, TG_DESKTOP_BG);
    
    // Desktop title
    draw_text_at(2, 1, "ICE Desktop", VGA_COLOR_LIGHT_CYAN, TG_DESKTOP_BG);
    
    // Desktop icons (2 columns)
    int icon_x1 = 2, icon_x2 = 14;
    int icon_y = 3;
    
    for (int i = 0; i < app_count && i < 10; i++) {
        int x = (i % 2 == 0) ? icon_x1 : icon_x2;
        int y = icon_y + (i / 2) * 2;
        
        char icon_str[16];
        icon_str[0] = '[';
        icon_str[1] = app_list[i].icon;
        icon_str[2] = ']';
        icon_str[3] = ' ';
        int j = 4;
        const char *name = app_list[i].name;
        while (*name && j < 14) {
            icon_str[j++] = *name++;
        }
        icon_str[j] = 0;
        
        mouse_state_t *ms = mouse_get_state();
        bool hover = point_in_rect(ms->x, ms->y, x, y, 12, 1);
        
        if (hover) {
            draw_text_at(x, y, icon_str, VGA_COLOR_BLACK, VGA_COLOR_LIGHT_CYAN);
        } else {
            draw_text_at(x, y, icon_str, VGA_COLOR_WHITE, TG_DESKTOP_BG);
        }
    }
    
    // Draw taskbar at bottom
    fill_text_area(0, 24, 80, 1, VGA_COLOR_WHITE, TG_TASKBAR_BG);
    
    // Start button
    mouse_state_t *ms = mouse_get_state();
    bool start_hover = point_in_rect(ms->x, ms->y, 0, 24, 6, 1);
    if (start_hover) {
        draw_text_at(0, 24, "[ICE]", VGA_COLOR_BLACK, VGA_COLOR_LIGHT_CYAN);
    } else {
        draw_text_at(0, 24, "[ICE]", VGA_COLOR_WHITE, TG_TASKBAR_BG);
    }
    
    // Clock
    extern u32 mpm_get_uptime(void);
    u32 uptime = mpm_get_uptime();
    u32 hours = uptime / 3600;
    u32 mins = (uptime % 3600) / 60;
    
    char time_str[16];
    time_str[0] = '0' + (hours / 10) % 10;
    time_str[1] = '0' + (hours % 10);
    time_str[2] = ':';
    time_str[3] = '0' + (mins / 10);
    time_str[4] = '0' + (mins % 10);
    time_str[5] = 0;
    draw_text_at(74, 24, time_str, VGA_COLOR_WHITE, TG_TASKBAR_BG);
    
    // Status text
    draw_text_at(8, 24, "Click [ICE] for apps", VGA_COLOR_DARK_GREY, TG_TASKBAR_BG);
}

// Main GUI loop
void gui_run(void) {
    bool show_launcher = false;
    int selected = 0;
    int scroll = 0;
    
    while (1) {
        // Poll mouse
        mouse_poll();
        mouse_state_t *ms = mouse_get_state();
        
        // Update cursor position tracking
        hide_cursor();
        cursor_x = ms->x;
        cursor_y = ms->y;
        
        // Draw desktop
        gui_draw_desktop();
        
        // Draw app launcher if open
        if (show_launcher) {
            draw_app_launcher(selected, scroll);
            
            // Check for clicks in launcher
            if (ms->left_click) {
                int list_x = 20, list_y = 4;
                int list_w = 40, list_h = 16;
                
                // Close button
                if (point_in_rect(ms->x, ms->y, list_x + list_w - 4, list_y, 3, 1)) {
                    mouse_clear_click();
                    show_launcher = false;
                    continue;
                }
                
                // App list clicks
                int visible_apps = list_h - 4;
                for (int i = 0; i < visible_apps && (scroll + i) < app_count; i++) {
                    int y = list_y + 2 + i;
                    if (point_in_rect(ms->x, ms->y, list_x + 2, y, list_w - 4, 1)) {
                        mouse_clear_click();
                        int idx = scroll + i;
                        show_launcher = false;
                        
                        // Check if terminal (exit GUI)
                        if (strcmp_gui(app_list[idx].command, "terminal") == 0) {
                            vga_clear();
                            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
                            return;
                        }
                        
                        run_gui_app(app_list[idx].command);
                        break;
                    }
                }
                
                // Click outside launcher closes it
                if (!point_in_rect(ms->x, ms->y, list_x, list_y, list_w, list_h)) {
                    mouse_clear_click();
                    show_launcher = false;
                }
            }
        } else {
            // Desktop clicks
            if (ms->left_click) {
                // Start button
                if (point_in_rect(ms->x, ms->y, 0, 24, 6, 1)) {
                    mouse_clear_click();
                    show_launcher = true;
                    selected = 0;
                    scroll = 0;
                    continue;
                }
                
                // Desktop icons
                int icon_x1 = 2, icon_x2 = 14;
                int icon_y = 3;
                
                for (int i = 0; i < app_count && i < 10; i++) {
                    int x = (i % 2 == 0) ? icon_x1 : icon_x2;
                    int y = icon_y + (i / 2) * 2;
                    
                    if (point_in_rect(ms->x, ms->y, x, y, 12, 1)) {
                        mouse_clear_click();
                        
                        if (strcmp_gui(app_list[i].command, "terminal") == 0) {
                            vga_clear();
                            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
                            return;
                        }
                        
                        run_gui_app(app_list[i].command);
                        break;
                    }
                }
                
                mouse_clear_click();
            }
        }
        
        // Show cursor
        show_cursor();
        
        // Handle keyboard (non-blocking)
        unsigned char key = keyboard_read();
        
        if (key == 27) { // ESC
            if (show_launcher) {
                show_launcher = false;
            } else {
                // Exit GUI
                vga_clear();
                vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
                return;
            }
        } else if (key == '\n' || key == '\r') {
            if (show_launcher && selected >= 0 && selected < app_count) {
                show_launcher = false;
                
                if (strcmp_gui(app_list[selected].command, "terminal") == 0) {
                    vga_clear();
                    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
                    return;
                }
                
                run_gui_app(app_list[selected].command);
            }
        } else if (key == KEY_UP || key == 'k' || key == 'w') {
            if (show_launcher && selected > 0) {
                selected--;
                if (selected < scroll) scroll = selected;
            }
        } else if (key == KEY_DOWN || key == 'j' || key == 's') {
            if (show_launcher && selected < app_count - 1) {
                selected++;
                if (selected >= scroll + 12) scroll = selected - 11;
            }
        } else if (key == ' ' || key == 'a') {
            // Space or 'a' opens launcher
            if (!show_launcher) {
                show_launcher = true;
                selected = 0;
                scroll = 0;
            }
        }
        
        // Small delay (~60fps)
        pit_sleep_ms(16);
    }
}

// Message box
void gui_message_box(const char *title, const char *message) {
    int title_len = strlen_gui(title);
    int msg_len = strlen_gui(message);
    int w = (title_len > msg_len ? title_len : msg_len) + 6;
    if (w < 24) w = 24;
    int h = 7;
    int x = (80 - w) / 2;
    int y = (25 - h) / 2;
    
    draw_text_box(x, y, w, h, VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
    
    // Title bar
    fill_text_area(x + 1, y, w - 2, 1, VGA_COLOR_WHITE, VGA_COLOR_LIGHT_BLUE);
    draw_text_at(x + 2, y, title, VGA_COLOR_WHITE, VGA_COLOR_LIGHT_BLUE);
    
    // Message
    draw_text_at(x + 3, y + 2, message, VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
    
    // OK button
    int btn_x = (80 - 8) / 2;
    int btn_y = y + h - 2;
    
    while (1) {
        mouse_poll();
        update_cursor();
        
        mouse_state_t *ms = mouse_get_state();
        bool hover = point_in_rect(ms->x, ms->y, btn_x, btn_y, 8, 1);
        
        draw_text_button(btn_x, btn_y, 8, "OK", true, hover);
        show_cursor();
        
        if (ms->left_click && hover) {
            mouse_clear_click();
            return;
        }
        
        char key = keyboard_getc();
        if (key == '\n' || key == '\r' || key == 27) {
            return;
        }
        
        pit_sleep_ms(10);
    }
}

// Create window (for compatibility)
u32 gui_create_window(const char *title, u32 x, u32 y, u32 w, u32 h, u32 flags) {
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (!windows[i].active) {
            windows[i].id = i;
            strcpy_gui(windows[i].title, title);
            windows[i].x = x;
            windows[i].y = y;
            windows[i].w = w;
            windows[i].h = h;
            windows[i].flags = flags;
            windows[i].bg_color = TG_WINDOW_BG;
            windows[i].on_click = 0;
            windows[i].on_draw = 0;
            windows[i].on_key = 0;
            windows[i].active = true;
            
            window_count++;
            active_window = i;
            
            return i;
        }
    }
    return (u32)-1;
}

void gui_destroy_window(u32 id) {
    if (id < MAX_WINDOWS && windows[id].active) {
        windows[id].active = false;
        window_count--;
        
        if (active_window == (int)id) {
            active_window = -1;
        }
    }
}

gui_window_t* gui_get_window(u32 id) {
    if (id < MAX_WINDOWS && windows[id].active) {
        return &windows[id];
    }
    return 0;
}
