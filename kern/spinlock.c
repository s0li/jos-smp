#include <inc/x86.h>
#include <inc/assert.h>

#include <kern/spinlock.h>

#include <kern/cpu.h>
#include <kern/mp.h>

// The big kernel lock
struct Spinlock kernel_lock = {
	.name = "kernel_lock"
};

void
lock_kernel(void)
{
	spinlock_acquire(&kernel_lock);
}

void
unlock_kernel(void)
{
	spinlock_release(&kernel_lock);

	// Normally we wouldn't need to do this, but QEMU only runs
	// one CPU at a time and has a long time-slice.  Without the
	// pause, this CPU is likely to reacquire the lock before
	// another CPU has even been given a chance to acquire it.
	asm volatile("pause");
}

void
spinlock_init(struct Spinlock *lk, char *name)
{
        lk->name = name;
        lk->locked = 0;
        lk->cpu = 0;
}

void
spinlock_acquire(struct Spinlock *lk)
{
	if (lk == NULL)
		panic("(spinlock_acquire) lock is NULL");
	if (lk->locked && lk->cpu == thisCPU)
		panic("(spinlock_acquire) CPU%d is trying to acquire a lock that" \
		      "it already has aqcuired", thisCPU->id);

	// The xchg is atomic.
	// It also serializes, so that reads after acquire are not
	// reordered before it. 
	while (xchg(&lk->locked, 1) != 0)
		asm volatile ("pause");
	
	lk->cpu = thisCPU;
	// TODO
//	get_caller_pcs(lk->pcs);
}

void
spinlock_release(struct Spinlock *lk)
{
	if (!lk->locked || lk->cpu != thisCPU)
		panic("(spinlock_release) lock isn't locked or CPU doesn't have it");

	lk->cpu = 0;
	lk->pcs[0] = 0;
	
	// The xchg serializes, so that reads before release are 
	// not reordered after it.  The 1996 PentiumPro manual (Volume 3,
	// 7.2) says reads can be carried out speculatively and in
	// any order, which implies we need to serialize here.
	// But the 2007 Intel 64 Architecture Memory Ordering White
	// Paper says that Intel 64 and IA-32 will not move a load
	// after a store. So lock->locked = 0 would work here.
	// The xchg being asm volatile ensures gcc emits it after
	// the above assignments (and after the critical section).
	xchg(&lk->locked, 0);
}
