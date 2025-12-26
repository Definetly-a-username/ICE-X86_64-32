#include "serial.h"

#define PORT 0x3f8   /* COM1 */

static inline u8 inb(u16 port) {
    u8 ret;
    __asm__ volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

static inline void outb(u16 port, u8 val) {
    __asm__ volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

void serial_init(void) {
    outb(PORT + 1, 0x00);    // Disable all interrupts
    outb(PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(PORT + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
    outb(PORT + 1, 0x00);    //                  (hi byte)
    outb(PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}

int is_transmit_empty(void) {
    return inb(PORT + 5) & 0x20;
}

void serial_putc(char c) {
    while (is_transmit_empty() == 0);
    outb(PORT, c);
}

void serial_puts(const char *s) {
    while (*s) {
        serial_putc(*s++);
    }
}

// Minimal printf implementation for serial
void serial_printf(const char *format, ...) {
    char **arg = (char **)&format;
    int c;
    char buf[20];
    
    arg++;
    
    while ((c = *format++) != 0) {
        if (c != '%') {
            serial_putc(c);
        } else {
            char *p, *p2;
            int pad0 = 0, pad = 0;
            
            c = *format++;
            if (c == '0') {
                pad0 = 1;
                c = *format++;
            }
            if (c >= '0' && c <= '9') {
                pad = c - '0';
                c = *format++;
            }
            
            switch (c) {
                case 'd':
                case 'u':
                case 'x': {
                    int num = *((int *)arg++);
                    int base = (c == 'x') ? 16 : 10;
                    if (c == 'd' && num < 0) {
                        serial_putc('-');
                        num = -num;
                    }
                    
                    p = buf;
                    do {
                        int rem = num % base;
                        *p++ = (rem < 10) ? rem + '0' : rem - 10 + 'A';
                        num /= base;
                    } while (num > 0);
                    
                    int len = p - buf;
                    while (len++ < pad) serial_putc(pad0 ? '0' : ' ');
                    
                    p2 = p - 1;
                    while (p2 >= buf) serial_putc(*p2--);
                    break;
                }
                case 's': {
                    char *s = *((char **)arg++);
                    if (!s) s = "(null)";
                    serial_puts(s);
                    break;
                }
                case '%':
                    serial_putc('%');
                    break;
            }
        }
    }
}
