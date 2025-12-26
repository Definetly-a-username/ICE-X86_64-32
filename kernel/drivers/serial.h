#ifndef ICE_SERIAL_H
#define ICE_SERIAL_H

#include "../types.h"

void serial_init(void);
void serial_putc(char c);
void serial_puts(const char *s);
void serial_printf(const char *format, ...);

#endif
