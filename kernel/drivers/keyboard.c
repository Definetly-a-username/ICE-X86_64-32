/*
 * ICE Operating System - PS/2 Keyboard Implementation
 */

#include "keyboard.h"
#include "pic.h"
#include "vga.h"
#include "../cpu/idt.h"

/* I/O helpers */
static inline void outb(u16 port, u8 value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline u8 inb(u16 port) {
    u8 ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* Keyboard state */
static volatile bool shift_pressed = false;
static volatile bool ctrl_pressed = false;
static volatile bool caps_lock = false;

/* Input buffer */
#define KB_BUFFER_SIZE 256
static char kb_buffer[KB_BUFFER_SIZE];
static volatile int kb_read_idx = 0;
static volatile int kb_write_idx = 0;

/* US keyboard layout - lowercase */
static const char scancode_to_char[] = {
    0,    0x1B, '1',  '2',  '3',  '4',  '5',  '6',   /* 0x00-0x07 */
    '7',  '8',  '9',  '0',  '-',  '=',  '\b', '\t',  /* 0x08-0x0F */
    'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',   /* 0x10-0x17 */
    'o',  'p',  '[',  ']',  '\n', 0,    'a',  's',   /* 0x18-0x1F */
    'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',   /* 0x20-0x27 */
    '\'', '`',  0,    '\\', 'z',  'x',  'c',  'v',   /* 0x28-0x2F */
    'b',  'n',  'm',  ',',  '.',  '/',  0,    '*',   /* 0x30-0x37 */
    0,    ' ',  0,    0,    0,    0,    0,    0,     /* 0x38-0x3F */
};

/* US keyboard layout - shifted */
static const char scancode_to_char_shift[] = {
    0,    0x1B, '!',  '@',  '#',  '$',  '%',  '^',
    '&',  '*',  '(',  ')',  '_',  '+',  '\b', '\t',
    'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',
    'O',  'P',  '{',  '}',  '\n', 0,    'A',  'S',
    'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',
    '"',  '~',  0,    '|',  'Z',  'X',  'C',  'V',
    'B',  'N',  'M',  '<',  '>',  '?',  0,    '*',
    0,    ' ',  0,    0,    0,    0,    0,    0,
};

/* Buffer operations */
static void kb_buffer_put(char c) {
    int next = (kb_write_idx + 1) % KB_BUFFER_SIZE;
    if (next != kb_read_idx) {
        kb_buffer[kb_write_idx] = c;
        kb_write_idx = next;
    }
}

static char kb_buffer_get(void) {
    if (kb_read_idx == kb_write_idx) {
        return 0;
    }
    char c = kb_buffer[kb_read_idx];
    kb_read_idx = (kb_read_idx + 1) % KB_BUFFER_SIZE;
    return c;
}

/* Keyboard interrupt handler */
static void keyboard_handler(interrupt_frame_t *frame) {
    (void)frame;
    
    u8 scancode = inb(KB_DATA_PORT);
    bool released = scancode & 0x80;
    scancode &= 0x7F;
    
    /* Handle modifiers */
    if (scancode == KEY_LSHIFT || scancode == KEY_RSHIFT) {
        shift_pressed = !released;
        return;
    }
    
    if (scancode == KEY_LCTRL) {
        ctrl_pressed = !released;
        return;
    }
    
    if (scancode == KEY_CAPS && !released) {
        caps_lock = !caps_lock;
        return;
    }
    
    /* Only process key press, not release */
    if (released) return;
    
    /* Convert scancode to character */
    char c = 0;
    if (scancode < sizeof(scancode_to_char)) {
        bool use_shift = shift_pressed ^ caps_lock;
        c = use_shift ? scancode_to_char_shift[scancode] : scancode_to_char[scancode];
    }
    
    if (c) {
        kb_buffer_put(c);
    }
}

void keyboard_init(void) {
    /* Register handler */
    idt_register_handler(33, keyboard_handler);
    
    /* Unmask IRQ1 */
    pic_unmask_irq(IRQ_KEYBOARD);
}

char keyboard_getc(void) {
    char c;
    while ((c = kb_buffer_get()) == 0) {
        __asm__ volatile ("hlt");
    }
    return c;
}

bool keyboard_available(void) {
    return kb_read_idx != kb_write_idx;
}

char keyboard_read(void) {
    return kb_buffer_get();
}

int keyboard_getline(char *buffer, int max_len) {
    int i = 0;
    
    while (i < max_len - 1) {
        char c = keyboard_getc();
        
        if (c == '\n') {
            buffer[i] = '\0';
            vga_putc('\n');
            return i;
        } else if (c == '\b') {
            if (i > 0) {
                i--;
                vga_putc('\b');
            }
        } else {
            buffer[i++] = c;
            vga_putc(c);
        }
    }
    
    buffer[i] = '\0';
    return i;
}
