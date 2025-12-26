#include "spinlock.h"

// Inline assembly helper to read EFLAGS
static inline u32 read_eflags(void) {
    u32 eflags;
    asm volatile("pushf; pop %0" : "=r"(eflags));
    return eflags;
}

// Inline assembly helper to disable interrupts (CLI)
static inline void cli(void) {
    asm volatile("cli");
}

// Inline assembly helper to restore interrupt state
static inline void irq_restore(u32 flags) {
    if (flags & 0x200) { // Check IF bit (0x200)
        asm volatile("sti");
    } else {
        asm volatile("cli");
    }
}

// Atomic exchange function
static inline u32 atomic_xchg(volatile u32 *ptr, u32 new_val) {
    u32 ret;
    asm volatile("lock xchg %0, %1"
                 : "+m"(*ptr), "=r"(ret)
                 : "1"(new_val)
                 : "memory");
    return ret;
}

void spinlock_init(spinlock_t *lock) {
    lock->locked = 0;
    lock->eflags = 0;
}

void spinlock_acquire(spinlock_t *lock) {
    // 1. Save interrupt state and disable interrupts
    u32 flags = read_eflags();
    cli();

    // 2. Try to acquire the lock
    // Simple test-and-set spinloop
    while (atomic_xchg(&lock->locked, 1) == 1) {
        // If we failed, paused interrupts are still disabled, which is good.
        // But we should momentarily re-enable them? 
        // NO, if we are single core, we CANNOT wait for a lock held by THIS cpu if IRQs are off.
        // But if we are multi-core, other CPU releases it.
        // Requirement says "single-CPU safety". On single CPU, a spinlock is just disable IRQ.
        // If locked==1 on single CPU, it means WE hold it (deadlock) or ISR holds it (deadlock if we wait in ISR).
        // For simplicity and "single-CPU safety", we just rely on CLI.
        // However, to support future SMP, we do the atomic check.
        
        // Spin hint
        asm volatile("pause");
        
        // If we are spinning, we shouldn't have interrupts disabled usually?
        // But if we use spinlock as "irq save", we must keep them disabled to prevent
        // the other thread (ISR) from running on THIS CPU.
        // So we just spin.
    }
    
    // We strictly store the flags from BEFORE we acquired.
    // NOTE: This basic implementation assumes non-recursive locks.
    lock->eflags = flags;
}

void spinlock_release(spinlock_t *lock) {
    // 1. Get saved flags
    u32 flags = lock->eflags;
    
    // 2. Release lock
    // Use simple store with barrier, but xchg is safer/easier to reason about order.
    // atomic_xchg(&lock->locked, 0); 
    // Just a release barrier and store is enough on x86. x86 stores are ordered.
    // Compiler barrier:
    asm volatile("" ::: "memory");
    lock->locked = 0;
    
    // 3. Restore interrupts
    irq_restore(flags);
}
