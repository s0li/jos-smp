#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

#include <inc/types.h>

#include <kern/cpu.h>

struct Spinlock {
	uint locked;

	// For debugging:
        char *name;        // Name of lock.
        struct CPU *cpu;   // The cpu holding the lock.

	// pcs is currently no implemented TODO
        uint pcs[10];      // The call stack (an array of program counters) 
        // that locked the lock.
};


void spinlock_init(struct Spinlock *lk, char *name);
void spinlock_acquire(struct Spinlock *lk);
void spinlock_release(struct Spinlock *lk);

extern struct Spinlock kernel_lock;

void lock_kernel(void);
void unlock_kernel(void);

#endif /* __SPINLOCK_H__ */
