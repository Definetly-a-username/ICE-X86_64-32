/**
 * PS/2 Mouse Driver Implementation
 * Supports standard PS/2 mouse protocol
 */

#include "mouse.h"
#include "pic.h"
#include "../drivers/vga.h"

// PS/2 Controller ports
#define PS2_DATA    0x60
#define PS2_STATUS  0x64
#define PS2_COMMAND 0x64

// PS/2 Commands
#define PS2_CMD_READ_CONFIG   0x20
#define PS2_CMD_WRITE_CONFIG  0x60
#define PS2_CMD_DISABLE_PORT2 0xA7
#define PS2_CMD_ENABLE_PORT2  0xA8
#define PS2_CMD_TEST_PORT2    0xA9
#define PS2_CMD_SEND_PORT2    0xD4

// Mouse commands
#define MOUSE_CMD_RESET       0xFF
#define MOUSE_CMD_RESEND      0xFE
#define MOUSE_CMD_SET_DEFAULT 0xF6
#define MOUSE_CMD_DISABLE     0xF5
#define MOUSE_CMD_ENABLE      0xF4
#define MOUSE_CMD_SET_RATE    0xF3
#define MOUSE_CMD_GET_ID      0xF2
#define MOUSE_CMD_SET_STREAM  0xEA
#define MOUSE_CMD_STATUS_REQ  0xE9

// Mouse state
static bool mouse_ready = false;
static mouse_state_t state = {0};
static int mouse_cycle = 0;
static u8 mouse_bytes[4] = {0};

// Bounds
static int bound_min_x = 0;
static int bound_min_y = 0;
static int bound_max_x = 79;  // VGA text mode width
static int bound_max_y = 24;  // VGA text mode height

// Port I/O
static inline void outb(u16 port, u8 value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline u8 inb(u16 port) {
    u8 ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Wait for PS/2 controller to be ready for input
static void mouse_wait_input(void) {
    int timeout = 100000;
    while (timeout--) {
        if ((inb(PS2_STATUS) & 0x02) == 0) return;
    }
}

// Wait for PS/2 controller to have output data
static void mouse_wait_output(void) {
    int timeout = 100000;
    while (timeout--) {
        if ((inb(PS2_STATUS) & 0x01) != 0) return;
    }
}

// Send command to PS/2 controller
static void mouse_write_cmd(u8 cmd) {
    mouse_wait_input();
    outb(PS2_COMMAND, cmd);
}

// Send data to PS/2 controller
static void mouse_write_data(u8 data) {
    mouse_wait_input();
    outb(PS2_DATA, data);
}

// Read data from PS/2 controller
static u8 mouse_read_data(void) {
    mouse_wait_output();
    return inb(PS2_DATA);
}

// Send command to mouse (through PS/2 port 2)
static u8 mouse_send_cmd(u8 cmd) {
    mouse_write_cmd(PS2_CMD_SEND_PORT2);
    mouse_write_data(cmd);
    return mouse_read_data();
}

int mouse_init(void) {
    // Enable auxiliary device (mouse port)
    mouse_write_cmd(PS2_CMD_ENABLE_PORT2);
    
    // Get current config
    mouse_write_cmd(PS2_CMD_READ_CONFIG);
    u8 config = mouse_read_data();
    
    // Enable interrupt for port 2 and enable clock for port 2
    config |= 0x02;  // Enable IRQ12
    config &= ~0x20; // Enable mouse clock
    
    // Write new config
    mouse_write_cmd(PS2_CMD_WRITE_CONFIG);
    mouse_write_data(config);
    
    // Reset mouse
    u8 ack = mouse_send_cmd(MOUSE_CMD_RESET);
    if (ack != 0xFA) {
        // Try again
        mouse_send_cmd(MOUSE_CMD_RESET);
    }
    
    // Wait for self-test result
    mouse_read_data(); // Should be 0xAA
    mouse_read_data(); // Mouse ID
    
    // Set defaults
    mouse_send_cmd(MOUSE_CMD_SET_DEFAULT);
    
    // Enable data reporting
    ack = mouse_send_cmd(MOUSE_CMD_ENABLE);
    
    if (ack == 0xFA) {
        mouse_ready = true;
        state.x = bound_max_x / 2;
        state.y = bound_max_y / 2;
        vga_puts("[MOUSE] PS/2 mouse initialized\n");
        return 0;
    }
    
    vga_puts("[MOUSE] Mouse initialization failed\n");
    return -1;
}

bool mouse_available(void) {
    return mouse_ready;
}

mouse_state_t* mouse_get_state(void) {
    return &state;
}

void mouse_get_pos(int *x, int *y) {
    if (x) *x = state.x;
    if (y) *y = state.y;
}

void mouse_set_pos(int x, int y) {
    state.x = x;
    state.y = y;
    
    // Clamp to bounds
    if (state.x < bound_min_x) state.x = bound_min_x;
    if (state.x > bound_max_x) state.x = bound_max_x;
    if (state.y < bound_min_y) state.y = bound_min_y;
    if (state.y > bound_max_y) state.y = bound_max_y;
}

void mouse_set_bounds(int min_x, int min_y, int max_x, int max_y) {
    if (min_x > max_x) { int t = min_x; min_x = max_x; max_x = t; }
    if (min_y > max_y) { int t = min_y; min_y = max_y; max_y = t; }
    
    bound_min_x = min_x;
    bound_min_y = min_y;
    bound_max_x = max_x;
    bound_max_y = max_y;
    
    // Clamp current position
    mouse_set_pos(state.x, state.y);
}

bool mouse_button_down(u8 button) {
    return (state.buttons & button) != 0;
}

bool mouse_clicked(u8 button) {
    if (button == MOUSE_LEFT) return state.left_click;
    if (button == MOUSE_RIGHT) return state.right_click;
    if (button == MOUSE_MIDDLE) return state.middle_click;
    return false;
}

void mouse_clear_click(void) {
    state.left_click = false;
    state.right_click = false;
    state.middle_click = false;
    state.moved = false;
}

// Process mouse packet
static void process_mouse_packet(void) {
    // First byte: buttons and sign bits
    u8 flags = mouse_bytes[0];
    
    // Check for valid packet
    if (!(flags & 0x08)) {
        mouse_cycle = 0;
        return;
    }
    
    // Get movement
    int dx = mouse_bytes[1];
    int dy = mouse_bytes[2];
    
    // Apply sign extension
    if (flags & 0x10) dx |= 0xFFFFFF00;
    if (flags & 0x20) dy |= 0xFFFFFF00;
    
    // Check for overflow
    if (flags & 0x40) dx = 0;
    if (flags & 0x80) dy = 0;
    
    // Update position (scale for text mode)
    // Use saturating arithmetic or temporary variables to prevent overflow wrap-around
    int new_x = state.x + dx / 8;
    int new_y = state.y - dy / 8; // Y is inverted
    
    // Clamp to bounds
    if (new_x < bound_min_x) new_x = bound_min_x;
    if (new_x > bound_max_x) new_x = bound_max_x;
    if (new_y < bound_min_y) new_y = bound_min_y;
    if (new_y > bound_max_y) new_y = bound_max_y;
    
    state.x = new_x;
    state.y = new_y;
    
    if (dx != 0 || dy != 0) {
        state.moved = true;
    }
    
    // Update buttons
    u8 prev_buttons = state.buttons;
    state.buttons = flags & 0x07;
    
    // Detect clicks (button just pressed)
    if ((state.buttons & MOUSE_LEFT) && !(prev_buttons & MOUSE_LEFT)) {
        state.left_click = true;
    }
    if ((state.buttons & MOUSE_RIGHT) && !(prev_buttons & MOUSE_RIGHT)) {
        state.right_click = true;
    }
    if ((state.buttons & MOUSE_MIDDLE) && !(prev_buttons & MOUSE_MIDDLE)) {
        state.middle_click = true;
    }
}

// Mouse interrupt handler (called from IRQ12)
void mouse_handler(void) {
    u8 status = inb(PS2_STATUS);
    
    // Check if data is from mouse
    if (!(status & 0x20)) {
        return;
    }
    
    if (!(status & 0x01)) {
        return;
    }
    
    u8 data = inb(PS2_DATA);
    
    mouse_bytes[mouse_cycle] = data;
    mouse_cycle++;
    
    if (mouse_cycle >= 3) {
        process_mouse_packet();
        mouse_cycle = 0;
    }
    
    // Send EOI
    outb(0xA0, 0x20);
    outb(0x20, 0x20);
}

// Poll mouse for data (use when interrupts not working)
void mouse_poll(void) {
    if (!mouse_ready) return;
    
    u8 status = inb(PS2_STATUS);
    
    // Check if data available and from mouse
    if ((status & 0x21) == 0x21) {
        u8 data = inb(PS2_DATA);
        
        mouse_bytes[mouse_cycle] = data;
        mouse_cycle++;
        
        if (mouse_cycle >= 3) {
            process_mouse_packet();
            mouse_cycle = 0;
        }
    }
}

