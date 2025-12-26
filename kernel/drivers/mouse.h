/**
 * PS/2 Mouse Driver
 * Provides mouse input for ICE GUI
 */

#ifndef ICE_MOUSE_H
#define ICE_MOUSE_H

#include "../types.h"

// Mouse buttons
#define MOUSE_LEFT   0x01
#define MOUSE_RIGHT  0x02
#define MOUSE_MIDDLE 0x04

// Mouse state
typedef struct {
    int x;
    int y;
    u8 buttons;
    bool left_click;
    bool right_click;
    bool middle_click;
    bool moved;
} mouse_state_t;

// Initialize mouse driver
int mouse_init(void);

// Check if mouse is available
bool mouse_available(void);

// Get current mouse state
mouse_state_t* mouse_get_state(void);

// Get mouse position
void mouse_get_pos(int *x, int *y);

// Set mouse position
void mouse_set_pos(int x, int y);

// Set mouse bounds
void mouse_set_bounds(int min_x, int min_y, int max_x, int max_y);

// Check if button is pressed
bool mouse_button_down(u8 button);

// Check if button was just clicked
bool mouse_clicked(u8 button);

// Clear click state (call after handling click)
void mouse_clear_click(void);

// Mouse interrupt handler
void mouse_handler(void);

// Poll mouse (for systems without interrupts)
void mouse_poll(void);

#endif // ICE_MOUSE_H

