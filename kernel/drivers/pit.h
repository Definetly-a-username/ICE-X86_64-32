/*
 * ICE Operating System - PIT Timer Header
 * Programmable Interval Timer (8254)
 */

#ifndef ICE_PIT_H
#define ICE_PIT_H

#include "../types.h"

/* PIT ports */
#define PIT_CHANNEL0    0x40
#define PIT_CHANNEL1    0x41
#define PIT_CHANNEL2    0x42
#define PIT_COMMAND     0x43

/* PIT frequency */
#define PIT_FREQUENCY   1193182

/* Initialize PIT with given frequency (Hz) */
void pit_init(u32 frequency);

/* Get tick count since boot */
u64 pit_get_ticks(void);

/* Sleep for milliseconds */
void pit_sleep_ms(u32 ms);

#endif /* ICE_PIT_H */
