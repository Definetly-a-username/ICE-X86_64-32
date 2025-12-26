/*
 * ICE OS Keyboard Driver - PS/2 Keyboard Header
 * 
 * Complete PS/2 keyboard driver for baremetal x86 systems.
 * Supports Intel 8042 compatible keyboard controllers.
 * 
 * Features:
 *   - Full scancode Set 1 translation
 *   - Extended key support (0xE0 prefix)
 *   - Modifier key tracking (Shift, Ctrl, Alt, GUI)
 *   - Lock key handling (Caps, Num, Scroll)
 *   - LED control
 *   - Typematic rate configuration
 *   - Circular input buffer
 *   - Both interrupt-driven and polled operation
 */

#ifndef ICE_KEYBOARD_H
#define ICE_KEYBOARD_H

#include "../types.h"
#include <stdbool.h>

/*============================================================================
 * PS/2 Controller I/O Ports
 *============================================================================*/

#define KB_DATA_PORT        0x60    /* Data port (read/write) */
#define KB_STATUS_PORT      0x64    /* Status register (read) */
#define KB_COMMAND_PORT     0x64    /* Command register (write) */

/*============================================================================
 * Status Register Bits (Port 0x64 read)
 *============================================================================*/

#define KB_STATUS_OUTPUT_FULL   0x01    /* Output buffer full (data ready) */
#define KB_STATUS_INPUT_FULL    0x02    /* Input buffer full (wait before write) */
#define KB_STATUS_SYSTEM_FLAG   0x04    /* System flag (POST passed) */
#define KB_STATUS_CMD_DATA      0x08    /* 0 = data, 1 = command */
#define KB_STATUS_KEYBOARD_LOCK 0x10    /* Keyboard lock status */
#define KB_STATUS_AUX_FULL      0x20    /* Auxiliary output buffer full (mouse) */
#define KB_STATUS_TIMEOUT       0x40    /* Timeout error */
#define KB_STATUS_PARITY        0x80    /* Parity error */

/*============================================================================
 * 8042 Controller Commands (sent to port 0x64)
 *============================================================================*/

#define KB_CTRL_READ_CMD        0x20    /* Read command byte */
#define KB_CTRL_WRITE_CMD       0x60    /* Write command byte */
#define KB_CTRL_DISABLE_AUX     0xA7    /* Disable auxiliary device (mouse) */
#define KB_CTRL_ENABLE_AUX      0xA8    /* Enable auxiliary device */
#define KB_CTRL_TEST_AUX        0xA9    /* Test auxiliary device interface */
#define KB_CTRL_SELF_TEST       0xAA    /* Controller self-test */
#define KB_CTRL_KB_TEST         0xAB    /* Keyboard interface test */
#define KB_CTRL_DISABLE_KB      0xAD    /* Disable keyboard */
#define KB_CTRL_ENABLE_KB       0xAE    /* Enable keyboard */
#define KB_CTRL_READ_INPUT      0xC0    /* Read input port */
#define KB_CTRL_READ_OUTPUT     0xD0    /* Read output port */
#define KB_CTRL_WRITE_OUTPUT    0xD1    /* Write output port */
#define KB_CTRL_WRITE_KB_OUT    0xD2    /* Write keyboard output buffer */
#define KB_CTRL_WRITE_AUX_OUT   0xD3    /* Write auxiliary output buffer */
#define KB_CTRL_WRITE_AUX       0xD4    /* Write to auxiliary device */

/* Controller self-test result */
#define KB_CTRL_TEST_OK         0x55    /* Self-test passed */
#define KB_CTRL_TEST_FAIL       0xFC    /* Self-test failed */

/* Interface test results */
#define KB_INTF_TEST_OK         0x00    /* Interface OK */
#define KB_INTF_CLOCK_LOW       0x01    /* Clock line stuck low */
#define KB_INTF_CLOCK_HIGH      0x02    /* Clock line stuck high */
#define KB_INTF_DATA_LOW        0x03    /* Data line stuck low */
#define KB_INTF_DATA_HIGH       0x04    /* Data line stuck high */

/*============================================================================
 * Command Byte Bits (read/write with 0x20/0x60)
 *============================================================================*/

#define KB_CMD_KB_INT           0x01    /* Enable keyboard interrupt (IRQ1) */
#define KB_CMD_AUX_INT          0x02    /* Enable auxiliary interrupt (IRQ12) */
#define KB_CMD_SYSTEM_FLAG      0x04    /* System flag */
#define KB_CMD_KB_DISABLE       0x10    /* Disable keyboard clock */
#define KB_CMD_AUX_DISABLE      0x20    /* Disable auxiliary clock */
#define KB_CMD_TRANSLATE        0x40    /* Enable scancode translation */

/*============================================================================
 * Keyboard Commands (sent to port 0x60)
 *============================================================================*/

#define KB_CMD_SET_LEDS         0xED    /* Set/reset LEDs */
#define KB_CMD_ECHO             0xEE    /* Echo (diagnostic) */
#define KB_CMD_GET_SET_SCANCODE 0xF0    /* Get/set current scancode set */
#define KB_CMD_IDENTIFY         0xF2    /* Identify keyboard */
#define KB_CMD_SET_TYPEMATIC    0xF3    /* Set typematic rate/delay */
#define KB_CMD_ENABLE_SCAN      0xF4    /* Enable scanning */
#define KB_CMD_DISABLE_SCAN     0xF5    /* Disable scanning, reset to defaults */
#define KB_CMD_SET_DEFAULTS     0xF6    /* Set default parameters */
#define KB_CMD_RESEND           0xFE    /* Resend last byte */
#define KB_CMD_RESET            0xFF    /* Reset and self-test */

/* Keyboard responses */
#define KB_RESP_ACK             0xFA    /* Command acknowledged */
#define KB_RESP_RESEND          0xFE    /* Resend request */
#define KB_RESP_ECHO            0xEE    /* Echo response */
#define KB_RESP_SELF_TEST_OK    0xAA    /* Self-test passed */
#define KB_RESP_SELF_TEST_FAIL1 0xFC    /* Self-test failed */
#define KB_RESP_SELF_TEST_FAIL2 0xFD    /* Self-test failed */

/*============================================================================
 * LED Bits (for KB_CMD_SET_LEDS)
 *============================================================================*/

#define KB_LED_SCROLL_LOCK      0x01    /* Scroll Lock LED */
#define KB_LED_NUM_LOCK         0x02    /* Num Lock LED */
#define KB_LED_CAPS_LOCK        0x04    /* Caps Lock LED */

/*============================================================================
 * Typematic Rate/Delay Values (for KB_CMD_SET_TYPEMATIC)
 *============================================================================*/

/* Delay before repeat (bits 5-6) */
#define KB_TYPEMATIC_DELAY_250MS    0x00
#define KB_TYPEMATIC_DELAY_500MS    0x20
#define KB_TYPEMATIC_DELAY_750MS    0x40
#define KB_TYPEMATIC_DELAY_1000MS   0x60

/* Repeat rate (bits 0-4), selected common values */
#define KB_TYPEMATIC_RATE_30CPS     0x00    /* 30 chars/sec */
#define KB_TYPEMATIC_RATE_24CPS     0x02    /* 24 chars/sec */
#define KB_TYPEMATIC_RATE_20CPS     0x04    /* 20 chars/sec */
#define KB_TYPEMATIC_RATE_15CPS     0x08    /* 15 chars/sec */
#define KB_TYPEMATIC_RATE_10CPS     0x0A    /* 10 chars/sec */
#define KB_TYPEMATIC_RATE_5CPS      0x14    /* 5 chars/sec */
#define KB_TYPEMATIC_RATE_2CPS      0x1F    /* 2 chars/sec (slowest) */

/*============================================================================
 * Scancode Set 1 - Make Codes (key press)
 *============================================================================*/

/* Row 1: Escape and Function Keys */
#define SC_ESCAPE               0x01
#define SC_F1                   0x3B
#define SC_F2                   0x3C
#define SC_F3                   0x3D
#define SC_F4                   0x3E
#define SC_F5                   0x3F
#define SC_F6                   0x40
#define SC_F7                   0x41
#define SC_F8                   0x42
#define SC_F9                   0x43
#define SC_F10                  0x44
#define SC_F11                  0x57
#define SC_F12                  0x58

/* Row 2: Number Row */
#define SC_BACKTICK             0x29
#define SC_1                    0x02
#define SC_2                    0x03
#define SC_3                    0x04
#define SC_4                    0x05
#define SC_5                    0x06
#define SC_6                    0x07
#define SC_7                    0x08
#define SC_8                    0x09
#define SC_9                    0x0A
#define SC_0                    0x0B
#define SC_MINUS                0x0C
#define SC_EQUALS               0x0D
#define SC_BACKSPACE            0x0E

/* Row 3: QWERTY Row */
#define SC_TAB                  0x0F
#define SC_Q                    0x10
#define SC_W                    0x11
#define SC_E                    0x12
#define SC_R                    0x13
#define SC_T                    0x14
#define SC_Y                    0x15
#define SC_U                    0x16
#define SC_I                    0x17
#define SC_O                    0x18
#define SC_P                    0x19
#define SC_LBRACKET             0x1A
#define SC_RBRACKET             0x1B
#define SC_BACKSLASH            0x2B
#define SC_ENTER                0x1C

/* Row 4: ASDF Row */
#define SC_CAPS_LOCK            0x3A
#define SC_A                    0x1E
#define SC_S                    0x1F
#define SC_D                    0x20
#define SC_F                    0x21
#define SC_G                    0x22
#define SC_H                    0x23
#define SC_J                    0x24
#define SC_K                    0x25
#define SC_L                    0x26
#define SC_SEMICOLON            0x27
#define SC_APOSTROPHE           0x28

/* Row 5: ZXCV Row */
#define SC_LSHIFT               0x2A
#define SC_Z                    0x2C
#define SC_X                    0x2D
#define SC_C                    0x2E
#define SC_V                    0x2F
#define SC_B                    0x30
#define SC_N                    0x31
#define SC_M                    0x32
#define SC_COMMA                0x33
#define SC_PERIOD               0x34
#define SC_SLASH                0x35
#define SC_RSHIFT               0x36

/* Row 6: Bottom Row */
#define SC_LCTRL                0x1D
#define SC_LALT                 0x38
#define SC_SPACE                0x39

/* NumPad */
#define SC_NUM_LOCK             0x45
#define SC_SCROLL_LOCK          0x46
#define SC_KP_7                 0x47
#define SC_KP_8                 0x48
#define SC_KP_9                 0x49
#define SC_KP_MINUS             0x4A
#define SC_KP_4                 0x4B
#define SC_KP_5                 0x4C
#define SC_KP_6                 0x4D
#define SC_KP_PLUS              0x4E
#define SC_KP_1                 0x4F
#define SC_KP_2                 0x50
#define SC_KP_3                 0x51
#define SC_KP_0                 0x52
#define SC_KP_DOT               0x53
#define SC_KP_ENTER             0x1C    /* Same as Enter, but with E0 prefix */
#define SC_KP_SLASH             0x35    /* Same as /, but with E0 prefix */
#define SC_KP_ASTERISK          0x37

/* Extended scancodes (require 0xE0 prefix) */
#define SC_EXT_PREFIX           0xE0
#define SC_EXT_RALT             0x38
#define SC_EXT_RCTRL            0x1D
#define SC_EXT_INSERT           0x52
#define SC_EXT_DELETE           0x53
#define SC_EXT_HOME             0x47
#define SC_EXT_END              0x4F
#define SC_EXT_PAGE_UP          0x49
#define SC_EXT_PAGE_DOWN        0x51
#define SC_EXT_UP               0x48
#define SC_EXT_DOWN             0x50
#define SC_EXT_LEFT             0x4B
#define SC_EXT_RIGHT            0x4D
#define SC_EXT_LGUI             0x5B    /* Left Windows/Super key */
#define SC_EXT_RGUI             0x5C    /* Right Windows/Super key */
#define SC_EXT_APPS             0x5D    /* Application/Menu key */

/* Special sequences */
#define SC_PAUSE_PREFIX         0xE1    /* Pause/Break uses E1 prefix */

/*============================================================================
 * Virtual Key Codes (returned by keyboard functions)
 * 
 * ASCII characters (0x00-0x7F) are returned as-is.
 * Special keys use values 0x80 and above to avoid conflicts.
 *============================================================================*/

/* Navigation keys */
#define KEY_UP                  0x80
#define KEY_DOWN                0x81
#define KEY_LEFT                0x82
#define KEY_RIGHT               0x83
#define KEY_HOME                0x84
#define KEY_END                 0x85
#define KEY_PGUP                0x86
#define KEY_PGDN                0x87
#define KEY_INSERT              0x88
#define KEY_DEL                 0x89

/* Function keys */
#define KEY_F1                  0x8A
#define KEY_F2                  0x8B
#define KEY_F3                  0x8C
#define KEY_F4                  0x8D
#define KEY_F5                  0x8E
#define KEY_F6                  0x8F
#define KEY_F7                  0x90
#define KEY_F8                  0x91
#define KEY_F9                  0x92
#define KEY_F10                 0x93
#define KEY_F11                 0x94
#define KEY_F12                 0x95

/* Special keys */
#define KEY_ESCAPE              0x96
#define KEY_PRINT_SCREEN        0x97
#define KEY_PAUSE               0x98
#define KEY_SCROLL_LOCK         0x99
#define KEY_NUM_LOCK            0x9A
#define KEY_CAPS_LOCK           0x9B
#define KEY_LGUI                0x9C    /* Left Windows/Super */
#define KEY_RGUI                0x9D    /* Right Windows/Super */
#define KEY_APPS                0x9E    /* Application/Menu key */

/* Modifier key events (for raw mode) */
#define KEY_LSHIFT              0xA0
#define KEY_RSHIFT              0xA1
#define KEY_LCTRL               0xA2
#define KEY_RCTRL               0xA3
#define KEY_LALT                0xA4
#define KEY_RALT                0xA5

/* NumPad keys (when Num Lock is off, these are navigation) */
#define KEY_KP_ENTER            0xB0
#define KEY_KP_SLASH            0xB1
#define KEY_KP_ASTERISK         0xB2
#define KEY_KP_MINUS            0xB3
#define KEY_KP_PLUS             0xB4
#define KEY_KP_DOT              0xB5

/* No key / invalid */
#define KEY_NONE                0x00

/*============================================================================
 * Keyboard State Structure
 *============================================================================*/

/* Modifier key flags */
#define KB_MOD_LSHIFT           0x0001
#define KB_MOD_RSHIFT           0x0002
#define KB_MOD_SHIFT            (KB_MOD_LSHIFT | KB_MOD_RSHIFT)
#define KB_MOD_LCTRL            0x0004
#define KB_MOD_RCTRL            0x0008
#define KB_MOD_CTRL             (KB_MOD_LCTRL | KB_MOD_RCTRL)
#define KB_MOD_LALT             0x0010
#define KB_MOD_RALT             0x0020
#define KB_MOD_ALT              (KB_MOD_LALT | KB_MOD_RALT)
#define KB_MOD_LGUI             0x0040
#define KB_MOD_RGUI             0x0080
#define KB_MOD_GUI              (KB_MOD_LGUI | KB_MOD_RGUI)

/* Lock key flags */
#define KB_LOCK_CAPS            0x0100
#define KB_LOCK_NUM             0x0200
#define KB_LOCK_SCROLL          0x0400

/* Keyboard state structure */
typedef struct {
    u16 modifiers;              /* Current modifier/lock key state */
    bool extended_pending;      /* Waiting for extended scancode */
    bool pause_pending;         /* Waiting for Pause sequence */
    u8   pause_count;           /* Bytes remaining in Pause sequence */
    u8   last_scancode;         /* Last scancode received */
    u8   last_error;            /* Last error code */
} keyboard_state_t;

/* Key event structure (for advanced use) */
typedef struct {
    u8   keycode;               /* Virtual key code */
    u8   scancode;              /* Raw scancode */
    u16  modifiers;             /* Modifier state at time of event */
    bool pressed;               /* true = pressed, false = released */
    bool extended;              /* true = extended key (0xE0 prefix) */
    char ascii;                 /* ASCII character (0 if not printable) */
} key_event_t;

/*============================================================================
 * Buffer Configuration
 *============================================================================*/

#define KB_BUFFER_SIZE          256     /* Circular buffer size (power of 2) */
#define KB_EVENT_BUFFER_SIZE    64      /* Raw event buffer size */

/*============================================================================
 * Timeout Values
 *============================================================================*/

#define KB_TIMEOUT_CYCLES       100000  /* Timeout for I/O operations */
#define KB_RESET_TIMEOUT        500000  /* Extended timeout for reset */

/*============================================================================
 * Error Codes
 *============================================================================*/

#define KB_ERR_NONE             0x00    /* No error */
#define KB_ERR_TIMEOUT          0x01    /* Operation timed out */
#define KB_ERR_BUFFER_FULL      0x02    /* Buffer is full */
#define KB_ERR_PARITY           0x03    /* Parity error */
#define KB_ERR_RESEND           0x04    /* Keyboard requested resend */
#define KB_ERR_SELF_TEST        0x05    /* Self-test failed */
#define KB_ERR_INTERFACE        0x06    /* Interface test failed */
#define KB_ERR_NO_ACK           0x07    /* No acknowledgment received */

/*============================================================================
 * Function Prototypes - Initialization
 *============================================================================*/

/**
 * Initialize the keyboard driver.
 * 
 * Performs controller self-test, enables keyboard, sets up interrupt handler,
 * and configures default settings.
 * 
 * @return KB_ERR_NONE on success, error code on failure
 */
u8 keyboard_init(void);

/**
 * Reset the keyboard to default state.
 * 
 * Sends reset command to keyboard, waits for self-test, and reinitializes.
 * 
 * @return KB_ERR_NONE on success, error code on failure
 */
u8 keyboard_reset(void);

/**
 * Shutdown keyboard driver.
 * 
 * Disables keyboard interrupt and scanning.
 */
void keyboard_shutdown(void);

/*============================================================================
 * Function Prototypes - Basic Input
 *============================================================================*/

/**
 * Blocking read - wait for and return next character.
 * 
 * Returns ASCII characters for printable keys, or KEY_* codes for special keys.
 * Blocks until a key is available.
 * 
 * @return Character or key code
 */
u8 keyboard_getc(void);

/**
 * Non-blocking read - return character if available.
 * 
 * @return Character or key code, or KEY_NONE (0) if no key available
 */
u8 keyboard_read(void);

/**
 * Check if a key is available in the buffer.
 * 
 * @return true if a key is available
 */
bool keyboard_available(void);

/**
 * Read a line of input with echo.
 * 
 * Reads characters until Enter is pressed. Supports backspace editing.
 * 
 * @param buffer    Destination buffer
 * @param max_len   Maximum length including null terminator
 * @return          Number of characters read (not including null terminator)
 */
int keyboard_getline(char *buffer, int max_len);

/*============================================================================
 * Function Prototypes - Raw/Advanced Input
 *============================================================================*/

/**
 * Get next raw key event.
 * 
 * Returns both press and release events with full modifier state.
 * 
 * @param event     Pointer to event structure to fill
 * @return          true if event available, false otherwise
 */
bool keyboard_get_event(key_event_t *event);

/**
 * Peek at next key without removing from buffer.
 * 
 * @return Character or key code, or KEY_NONE if buffer empty
 */
u8 keyboard_peek(void);

/*============================================================================
 * Function Prototypes - State Management
 *============================================================================*/

/**
 * Get current keyboard state.
 * 
 * @param state     Pointer to state structure to fill
 */
void keyboard_get_state(keyboard_state_t *state);

/**
 * Get current modifier key state.
 * 
 * @return Modifier flags (KB_MOD_* combined)
 */
u16 keyboard_get_modifiers(void);

/**
 * Check if a specific modifier is currently pressed.
 * 
 * @param modifier  Modifier flag to check (KB_MOD_*)
 * @return          true if modifier is active
 */
bool keyboard_modifier_pressed(u16 modifier);

/**
 * Check if Shift is currently pressed.
 */
bool keyboard_shift_pressed(void);

/**
 * Check if Ctrl is currently pressed.
 */
bool keyboard_ctrl_pressed(void);

/**
 * Check if Alt is currently pressed.
 */
bool keyboard_alt_pressed(void);

/**
 * Check if Caps Lock is active.
 */
bool keyboard_caps_lock_active(void);

/**
 * Check if Num Lock is active.
 */
bool keyboard_num_lock_active(void);

/**
 * Check if Scroll Lock is active.
 */
bool keyboard_scroll_lock_active(void);

/*============================================================================
 * Function Prototypes - LED Control
 *============================================================================*/

/**
 * Set keyboard LEDs.
 * 
 * @param leds      LED flags (KB_LED_* combined)
 * @return          KB_ERR_NONE on success, error code on failure
 */
u8 keyboard_set_leds(u8 leds);

/**
 * Update LEDs to match current lock key state.
 * 
 * @return KB_ERR_NONE on success, error code on failure
 */
u8 keyboard_update_leds(void);

/*============================================================================
 * Function Prototypes - Configuration
 *============================================================================*/

/**
 * Set typematic (auto-repeat) rate and delay.
 * 
 * @param delay     Delay before repeat starts (KB_TYPEMATIC_DELAY_*)
 * @param rate      Repeat rate (KB_TYPEMATIC_RATE_*)
 * @return          KB_ERR_NONE on success, error code on failure
 */
u8 keyboard_set_typematic(u8 delay, u8 rate);

/**
 * Enable keyboard scanning.
 * 
 * @return KB_ERR_NONE on success, error code on failure
 */
u8 keyboard_enable(void);

/**
 * Disable keyboard scanning.
 * 
 * @return KB_ERR_NONE on success, error code on failure
 */
u8 keyboard_disable(void);

/*============================================================================
 * Function Prototypes - Buffer Management
 *============================================================================*/

/**
 * Flush keyboard buffer.
 * 
 * Clears both software buffer and hardware buffer.
 */
void keyboard_flush(void);

/**
 * Get number of characters in buffer.
 * 
 * @return Number of characters available
 */
int keyboard_buffer_count(void);

/*============================================================================
 * Function Prototypes - Polling Mode
 *============================================================================*/

/**
 * Poll keyboard for input (non-interrupt mode).
 * 
 * Call this regularly when interrupts are disabled or unreliable.
 */
void keyboard_poll(void);

/*============================================================================
 * Function Prototypes - Diagnostics
 *============================================================================*/

/**
 * Perform keyboard echo test.
 * 
 * @return true if keyboard responds correctly
 */
bool keyboard_echo_test(void);

/**
 * Get keyboard identification bytes.
 * 
 * @param id1       First ID byte
 * @param id2       Second ID byte
 * @return          KB_ERR_NONE on success, error code on failure
 */
u8 keyboard_identify(u8 *id1, u8 *id2);

/**
 * Get last error code.
 * 
 * @return Last error code (KB_ERR_*)
 */
u8 keyboard_get_error(void);

/**
 * Get driver statistics (for debugging).
 * 
 * @param interrupts    Total interrupts received
 * @param overruns      Buffer overrun count
 * @param errors        Total errors
 */
void keyboard_get_stats(u32 *interrupts, u32 *overruns, u32 *errors);

#endif /* ICE_KEYBOARD_H */
