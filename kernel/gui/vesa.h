/**
 * VESA VBE Framebuffer Driver
 * Provides basic graphics capabilities for ICE GUI
 */

#ifndef ICE_VESA_H
#define ICE_VESA_H

#include "../types.h"

// VBE Mode Info structure (from BIOS)
typedef struct __attribute__((packed)) {
    u16 mode_attributes;
    u8  win_a_attributes;
    u8  win_b_attributes;
    u16 win_granularity;
    u16 win_size;
    u16 win_a_segment;
    u16 win_b_segment;
    u32 win_func_ptr;
    u16 bytes_per_scan_line;
    u16 x_resolution;
    u16 y_resolution;
    u8  x_char_size;
    u8  y_char_size;
    u8  number_of_planes;
    u8  bits_per_pixel;
    u8  number_of_banks;
    u8  memory_model;
    u8  bank_size;
    u8  number_of_image_pages;
    u8  reserved0;
    u8  red_mask_size;
    u8  red_field_position;
    u8  green_mask_size;
    u8  green_field_position;
    u8  blue_mask_size;
    u8  blue_field_position;
    u8  reserved_mask_size;
    u8  reserved_field_position;
    u8  direct_color_mode_info;
    u32 phys_base_ptr;
    u32 reserved1;
    u16 reserved2;
} vbe_mode_info_t;

// Colors (32-bit ARGB)
#define COLOR_BLACK       0xFF000000
#define COLOR_WHITE       0xFFFFFFFF
#define COLOR_RED         0xFFFF0000
#define COLOR_GREEN       0xFF00FF00
#define COLOR_BLUE        0xFF0000FF
#define COLOR_CYAN        0xFF00FFFF
#define COLOR_MAGENTA     0xFFFF00FF
#define COLOR_YELLOW      0xFFFFFF00
#define COLOR_ORANGE      0xFFFF8000
#define COLOR_GRAY        0xFF808080
#define COLOR_DARK_GRAY   0xFF404040
#define COLOR_LIGHT_GRAY  0xFFC0C0C0

// Desktop theme colors
#define COLOR_DESKTOP_BG  0xFF1E3A5F   // Dark blue
#define COLOR_TASKBAR_BG  0xFF2D2D2D   // Dark gray
#define COLOR_WINDOW_BG   0xFFF0F0F0   // Light gray
#define COLOR_WINDOW_TITLE 0xFF3C78C8  // Blue
#define COLOR_BUTTON_BG   0xFFE0E0E0   // Button gray
#define COLOR_BUTTON_HOVER 0xFFD0D0FF  // Button hover
#define COLOR_TEXT        0xFF000000   // Black text
#define COLOR_TEXT_LIGHT  0xFFFFFFFF   // White text

// Initialize VESA graphics mode
int vesa_init(u32 width, u32 height, u32 bpp);

// Check if VESA mode is active
bool vesa_is_active(void);

// Get screen dimensions
u32 vesa_get_width(void);
u32 vesa_get_height(void);
u32 vesa_get_bpp(void);

// Basic drawing functions
void vesa_put_pixel(u32 x, u32 y, u32 color);
u32 vesa_get_pixel(u32 x, u32 y);
void vesa_fill_rect(u32 x, u32 y, u32 w, u32 h, u32 color);
void vesa_draw_rect(u32 x, u32 y, u32 w, u32 h, u32 color);
void vesa_draw_line(u32 x1, u32 y1, u32 x2, u32 y2, u32 color);
void vesa_clear(u32 color);

// Text rendering (simple bitmap font)
void vesa_draw_char(u32 x, u32 y, char c, u32 fg, u32 bg);
void vesa_draw_string(u32 x, u32 y, const char *str, u32 fg, u32 bg);
void vesa_draw_string_transparent(u32 x, u32 y, const char *str, u32 fg);

// Buffer operations
void vesa_swap_buffers(void);
void vesa_copy_region(u32 src_x, u32 src_y, u32 dst_x, u32 dst_y, u32 w, u32 h);

// Fallback for text mode (when VESA unavailable)
void vesa_fallback_text_mode(void);

#endif // ICE_VESA_H

