/**
 * VESA VBE Framebuffer Driver Implementation
 * 
 * Note: This requires the bootloader to set up VESA mode and pass
 * framebuffer info to the kernel. For QEMU, we use a fixed mode.
 */

#include "vesa.h"
#include "../drivers/vga.h"

// Simple 8x8 bitmap font
static const u8 font_8x8[128][8] = {
    // 0-31: Control characters (blank)
    [0 ... 31] = {0, 0, 0, 0, 0, 0, 0, 0},
    // Space
    [32] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    // !
    [33] = {0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x00},
    // "
    [34] = {0x6C, 0x6C, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00},
    // # through /
    [35 ... 47] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    // 0
    [48] = {0x3C, 0x66, 0x6E, 0x76, 0x66, 0x66, 0x3C, 0x00},
    // 1
    [49] = {0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00},
    // 2
    [50] = {0x3C, 0x66, 0x06, 0x0C, 0x18, 0x30, 0x7E, 0x00},
    // 3
    [51] = {0x3C, 0x66, 0x06, 0x1C, 0x06, 0x66, 0x3C, 0x00},
    // 4
    [52] = {0x0C, 0x1C, 0x3C, 0x6C, 0x7E, 0x0C, 0x0C, 0x00},
    // 5
    [53] = {0x7E, 0x60, 0x7C, 0x06, 0x06, 0x66, 0x3C, 0x00},
    // 6
    [54] = {0x1C, 0x30, 0x60, 0x7C, 0x66, 0x66, 0x3C, 0x00},
    // 7
    [55] = {0x7E, 0x06, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x00},
    // 8
    [56] = {0x3C, 0x66, 0x66, 0x3C, 0x66, 0x66, 0x3C, 0x00},
    // 9
    [57] = {0x3C, 0x66, 0x66, 0x3E, 0x06, 0x0C, 0x38, 0x00},
    // : through @
    [58 ... 64] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    // A
    [65] = {0x18, 0x3C, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x00},
    // B
    [66] = {0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x7C, 0x00},
    // C
    [67] = {0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C, 0x00},
    // D
    [68] = {0x78, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0x78, 0x00},
    // E
    [69] = {0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x7E, 0x00},
    // F
    [70] = {0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x60, 0x00},
    // G
    [71] = {0x3C, 0x66, 0x60, 0x6E, 0x66, 0x66, 0x3E, 0x00},
    // H
    [72] = {0x66, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00},
    // I
    [73] = {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00},
    // J
    [74] = {0x3E, 0x0C, 0x0C, 0x0C, 0x0C, 0x6C, 0x38, 0x00},
    // K
    [75] = {0x66, 0x6C, 0x78, 0x70, 0x78, 0x6C, 0x66, 0x00},
    // L
    [76] = {0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00},
    // M
    [77] = {0xC6, 0xEE, 0xFE, 0xD6, 0xC6, 0xC6, 0xC6, 0x00},
    // N
    [78] = {0x66, 0x76, 0x7E, 0x7E, 0x6E, 0x66, 0x66, 0x00},
    // O
    [79] = {0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00},
    // P
    [80] = {0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x00},
    // Q
    [81] = {0x3C, 0x66, 0x66, 0x66, 0x6A, 0x6C, 0x36, 0x00},
    // R
    [82] = {0x7C, 0x66, 0x66, 0x7C, 0x6C, 0x66, 0x66, 0x00},
    // S
    [83] = {0x3C, 0x66, 0x60, 0x3C, 0x06, 0x66, 0x3C, 0x00},
    // T
    [84] = {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00},
    // U
    [85] = {0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00},
    // V
    [86] = {0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00},
    // W
    [87] = {0xC6, 0xC6, 0xC6, 0xD6, 0xFE, 0xEE, 0xC6, 0x00},
    // X
    [88] = {0x66, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00},
    // Y
    [89] = {0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x00},
    // Z
    [90] = {0x7E, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x7E, 0x00},
    // [ through `
    [91 ... 96] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    // a
    [97] = {0x00, 0x00, 0x3C, 0x06, 0x3E, 0x66, 0x3E, 0x00},
    // b
    [98] = {0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x7C, 0x00},
    // c
    [99] = {0x00, 0x00, 0x3C, 0x66, 0x60, 0x66, 0x3C, 0x00},
    // d
    [100] = {0x06, 0x06, 0x3E, 0x66, 0x66, 0x66, 0x3E, 0x00},
    // e
    [101] = {0x00, 0x00, 0x3C, 0x66, 0x7E, 0x60, 0x3C, 0x00},
    // f
    [102] = {0x1C, 0x30, 0x7C, 0x30, 0x30, 0x30, 0x30, 0x00},
    // g
    [103] = {0x00, 0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x3C},
    // h
    [104] = {0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00},
    // i
    [105] = {0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3C, 0x00},
    // j
    [106] = {0x0C, 0x00, 0x1C, 0x0C, 0x0C, 0x0C, 0x6C, 0x38},
    // k
    [107] = {0x60, 0x60, 0x66, 0x6C, 0x78, 0x6C, 0x66, 0x00},
    // l
    [108] = {0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00},
    // m
    [109] = {0x00, 0x00, 0xEC, 0xFE, 0xD6, 0xC6, 0xC6, 0x00},
    // n
    [110] = {0x00, 0x00, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00},
    // o
    [111] = {0x00, 0x00, 0x3C, 0x66, 0x66, 0x66, 0x3C, 0x00},
    // p
    [112] = {0x00, 0x00, 0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60},
    // q
    [113] = {0x00, 0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x06},
    // r
    [114] = {0x00, 0x00, 0x7C, 0x66, 0x60, 0x60, 0x60, 0x00},
    // s
    [115] = {0x00, 0x00, 0x3E, 0x60, 0x3C, 0x06, 0x7C, 0x00},
    // t
    [116] = {0x30, 0x30, 0x7C, 0x30, 0x30, 0x30, 0x1C, 0x00},
    // u
    [117] = {0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x00},
    // v
    [118] = {0x00, 0x00, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00},
    // w
    [119] = {0x00, 0x00, 0xC6, 0xC6, 0xD6, 0xFE, 0x6C, 0x00},
    // x
    [120] = {0x00, 0x00, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x00},
    // y
    [121] = {0x00, 0x00, 0x66, 0x66, 0x66, 0x3E, 0x06, 0x3C},
    // z
    [122] = {0x00, 0x00, 0x7E, 0x0C, 0x18, 0x30, 0x7E, 0x00},
    // Remaining characters (blank for now)
    [123 ... 127] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
};

// Framebuffer state
static bool vesa_active = false;
static u32 *framebuffer = NULL;
static u32 screen_width = 0;
static u32 screen_height = 0;
static u32 screen_bpp = 0;
static u32 screen_pitch = 0;

// Double buffering
static u32 *back_buffer = NULL;
static bool double_buffering = false;

// Simple memory operations
static void *memset_gui(void *s, int c, u32 n) {
    u8 *p = (u8*)s;
    while (n--) *p++ = (u8)c;
    return s;
}

static void *memcpy_gui(void *dest, const void *src, u32 n) {
    u8 *d = (u8*)dest;
    const u8 *s = (const u8*)src;
    while (n--) *d++ = *s++;
    return dest;
}

int vesa_init(u32 width, u32 height, u32 bpp) {
    // In a real implementation, we would:
    // 1. Query VBE modes using BIOS interrupts (before protected mode)
    // 2. Set the desired mode
    // 3. Get the framebuffer address
    
    // For now, we use a fixed framebuffer address that QEMU provides
    // This assumes the bootloader has set up VESA mode 0x118 (1024x768x24)
    // or similar and passed the info to us
    
    // Default fallback framebuffer address (typical VESA LFB)
    framebuffer = (u32*)0xFD000000;  // Common VESA LFB address
    
    screen_width = width ? width : 800;
    screen_height = height ? height : 600;
    screen_bpp = bpp ? bpp : 32;
    screen_pitch = screen_width * (screen_bpp / 8);
    
    // For simplicity, we'll use text mode as fallback
    // Real VESA requires BIOS calls before entering protected mode
    vesa_active = false;
    
    return 0;
}

bool vesa_is_active(void) {
    return vesa_active;
}

u32 vesa_get_width(void) {
    return screen_width;
}

u32 vesa_get_height(void) {
    return screen_height;
}

u32 vesa_get_bpp(void) {
    return screen_bpp;
}

void vesa_put_pixel(u32 x, u32 y, u32 color) {
    if (!vesa_active || x >= screen_width || y >= screen_height) return;
    
    u32 *target = double_buffering ? back_buffer : framebuffer;
    target[y * screen_width + x] = color;
}

u32 vesa_get_pixel(u32 x, u32 y) {
    if (!vesa_active || x >= screen_width || y >= screen_height) return 0;
    return framebuffer[y * screen_width + x];
}

void vesa_fill_rect(u32 x, u32 y, u32 w, u32 h, u32 color) {
    if (!vesa_active) return;
    
    u32 *target = double_buffering ? back_buffer : framebuffer;
    
    for (u32 py = y; py < y + h && py < screen_height; py++) {
        for (u32 px = x; px < x + w && px < screen_width; px++) {
            target[py * screen_width + px] = color;
        }
    }
}

void vesa_draw_rect(u32 x, u32 y, u32 w, u32 h, u32 color) {
    if (!vesa_active) return;
    
    // Top and bottom
    for (u32 px = x; px < x + w && px < screen_width; px++) {
        vesa_put_pixel(px, y, color);
        vesa_put_pixel(px, y + h - 1, color);
    }
    
    // Left and right
    for (u32 py = y; py < y + h && py < screen_height; py++) {
        vesa_put_pixel(x, py, color);
        vesa_put_pixel(x + w - 1, py, color);
    }
}

void vesa_draw_line(u32 x1, u32 y1, u32 x2, u32 y2, u32 color) {
    if (!vesa_active) return;
    
    int dx = (x2 > x1) ? (x2 - x1) : (x1 - x2);
    int dy = (y2 > y1) ? (y2 - y1) : (y1 - y2);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;
    
    while (1) {
        vesa_put_pixel(x1, y1, color);
        
        if (x1 == x2 && y1 == y2) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

void vesa_clear(u32 color) {
    if (!vesa_active) return;
    
    u32 *target = double_buffering ? back_buffer : framebuffer;
    u32 total = screen_width * screen_height;
    
    for (u32 i = 0; i < total; i++) {
        target[i] = color;
    }
}

void vesa_draw_char(u32 x, u32 y, char c, u32 fg, u32 bg) {
    if (!vesa_active) return;
    if ((u8)c > 127) c = '?';
    
    const u8 *glyph = font_8x8[(u8)c];
    
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            u32 color = (glyph[row] & (0x80 >> col)) ? fg : bg;
            vesa_put_pixel(x + col, y + row, color);
        }
    }
}

void vesa_draw_string(u32 x, u32 y, const char *str, u32 fg, u32 bg) {
    u32 start_x = x;
    
    while (*str) {
        if (*str == '\n') {
            x = start_x;
            y += 10;
        } else {
            vesa_draw_char(x, y, *str, fg, bg);
            x += 8;
        }
        str++;
    }
}

void vesa_draw_string_transparent(u32 x, u32 y, const char *str, u32 fg) {
    if (!vesa_active) return;
    
    u32 start_x = x;
    
    while (*str) {
        if (*str == '\n') {
            x = start_x;
            y += 10;
        } else if ((u8)*str <= 127) {
            const u8 *glyph = font_8x8[(u8)*str];
            
            for (int row = 0; row < 8; row++) {
                for (int col = 0; col < 8; col++) {
                    if (glyph[row] & (0x80 >> col)) {
                        vesa_put_pixel(x + col, y + row, fg);
                    }
                }
            }
            x += 8;
        }
        str++;
    }
}

void vesa_swap_buffers(void) {
    if (!vesa_active || !double_buffering || !back_buffer) return;
    
    memcpy_gui(framebuffer, back_buffer, screen_width * screen_height * 4);
}

void vesa_copy_region(u32 src_x, u32 src_y, u32 dst_x, u32 dst_y, u32 w, u32 h) {
    if (!vesa_active) return;
    
    for (u32 row = 0; row < h; row++) {
        for (u32 col = 0; col < w; col++) {
            u32 pixel = vesa_get_pixel(src_x + col, src_y + row);
            vesa_put_pixel(dst_x + col, dst_y + row, pixel);
        }
    }
}

void vesa_fallback_text_mode(void) {
    // Reset to VGA text mode
    vesa_active = false;
    vga_clear();
}

