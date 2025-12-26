#ifndef ICE_SPINLOCK_H
#define ICE_SPINLOCK_H

#include "../types.h"

typedef struct {
    volatile u32 locked;
    u32 eflags; // To save interrupt state
} spinlock_t;

// Initialize a spinlock
void spinlock_init(spinlock_t *lock);

// Acquire the lock (disables interrupts)
void spinlock_acquire(spinlock_t *lock);

// Release the lock (restores interrupts)
void spinlock_release(spinlock_t *lock);

#endif // ICE_SPINLOCK_H
