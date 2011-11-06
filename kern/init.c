/* See COPYRIGHT for copyright information. */

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/monitor.h>
#include <kern/console.h>
#include <kern/pmap.h>
#include <kern/kclock.h>
#include <kern/env.h>
#include <kern/trap.h>
#include <kern/sched.h>
#include <kern/picirq.h>

#include <kern/mp.h>
#include <kern/lapic.h>
#include <kern/ioapic.h>
#include <kern/cpu.h>

#include <inc/types.h>
#include <inc/x86.h>

static void ap_init(void);  // ap = application CPU (non BSP)
static void bootothers(void);

void
i386_init(void)
{
	extern char edata[], end[];
	
	// Before doing anything else, complete the ELF loading process.
	// Clear the uninitialized global data (BSS) section of our program.
	// This ensures that all static/global variables start out zero.
	memset(edata, 0, end - edata);
 	
	// Initialize the console.
	// Can't call cprintf until after we do this!
	// TODO (?)
	// cprintf("ohrly\n");
	cons_init();
	
	// Lab 2 memory management initialization functions
	i386_detect_memory();
	i386_vm_init();
	
	// Lab 3 user environment initialization functions
	env_init();
//	cprintf("(i386_init) cpus = %x, cpunum() = %d\n", cpus, cpunum());
	thisCPU = &cpus[cpunum()];
		
	idt_init();

	// ------------------------------------------------------------
	// xv6 smp code

	// right now this code must be after i386_detect_memory() because
	// we use KADDR which depends on npage var
	mpinit();  // collect info about this machine
	
//	cprintf("(i386_init) bcpu = %x\n", bcpu);
	
	lapicinit(mpbcpu()); // Local APIC
	ioapicinit();        // IO APIC

//	cprintf("(i386_init) lapic = %x\n", lapic);
	// ------------------------------------------------------------


	// Lab 4 multitasking initialization functions
	// TODO is this really needed with the ioapic enabled?
	pic_init();
	
	// the kclock init isn't needed anymore beacuse we init the
	//clock tick in lapicinit
//	kclock_init();
	if (ncpu > 1)
		bootothers();

	/* int k = 0; */
	/* while (1) { */
	/* 	if (k++ % 10000000 == 0) */
	/* 		cprintf("cpu%d reporting in\n", thisCPU->id); */
	/* } */
	
	// Should always have an idle process as first one.
	extern uint8_t _binary_obj_user_idle_start[];
	cprintf("(i386_init) idle addr = %x\n", _binary_obj_user_idle_start);

	thisCPU->booted = 1;
	ENV_CREATE(user_idle);
//	ENV_CREATE(user_primes);

	// Start fs.
	// TODO enable this fucker after playing with mpinit
//	ENV_CREATE(fs_fs);

	// this is a sanity check for the scheduler
	/* ENV_CREATE(user_yield); */
	/* ENV_CREATE(user_yield); */
	/* ENV_CREATE(user_yield); */


#if defined(TEST)
	// Don't touch -- used by grading script!
	ENV_CREATE2(TEST, TESTSIZE);
#else
	// Touch all you want.
	// ENV_CREATE(user_writemotd);
	// ENV_CREATE(user_testfile);
	// ENV_CREATE(user_icode);
#endif // TEST*

	// Schedule and run the first user environment!
 	sched_yield();

	// Drop into the kernel monitor.
	while (1)
		monitor(NULL);
}

// Start the non-boot processors.
static void
bootothers(void)
{
        extern uchar bootother_start[], bootother_end[];
        uchar *code;
        struct CPU *c;
//        char *stack;
        // Write bootstrap code to unused memory at 0x7000.
        // The linker has placed the image of bootother.S in
        // _binary_bootother_start.
        code = (uchar*)KADDR(0x7000);
        memmove(code, bootother_start, bootother_end - bootother_start);

	cprintf("bootoher_start = %x\n", bootother_start);
	cprintf("bootoher_end = %x\n", bootother_end);
	cprintf("code = %x\n", code);
        for(c = cpus; c < cpus+ncpu; c++){
                if(c == cpus+cpunum())  // We've started already.
                        continue;

                // Tell bootother.S what stack to use and the address of mpinit;
                // it expects to find these two addresses stored just before
                // its first instruction.
//              stack = kalloc();
//		stack = kstacks[cpunum()];
                *(void**)(code-4) = kstacks[cpunum()] + KSTACKSIZE;
                *(void**)(code-8) = (void*)ap_init;

                lapicstartap(c->id, (uint)PADDR(code));

                // Wait for cpu to finish mpmain()
                while(c->booted == 0)
                        ;
        }
}

// Common CPU setup code.
// Bootstrap CPU comes here from mainc().
// Other CPUs jump here from bootother.S.
static void
ap_init(void)
{
	lcr3(PADDR(boot_pgdir));
	thisCPU = &cpus[cpunum()];
	cprintf("(ap_init) cpu%d: starting\n", thisCPU->id);
	
	lapicinit(cpunum());
	
	seginit();
	// For good measure, clear the local descriptor table (LDT),
	// since we don't use it.
	lldt(0);
	
	tss_init_percpu();
	// ----------------------------------------

//        idtinit();       // load idt register
        xchg(&thisCPU->booted, 1); // tell bootothers() we're up
//        scheduler();     // start running processes
	sched_yield();

	int k = 0;
	while(1) { if (k++ == 1) cprintf("(ap_init) ap entered while(1)\n"); }
	/* int k = 0; */
	/* while(1) { */
	/* 	if (k++ % 10000000 == 0) */
	/* 		cprintf("--- cpu%d reporting in ---\n", cpunum()); */
	/* } */
}

/*
 * Variable panicstr contains argument to first call to panic; used as flag
 * to indicate that the kernel has already called panic.
 */
static const char *panicstr;

/*
 * Panic is called on unresolvable fatal errors.
 * It prints "panic: mesg", and then enters the kernel monitor.
 */
void
_panic(const char *file, int line, const char *fmt,...)
{
	va_list ap;

	if (panicstr)
		goto dead;
	panicstr = fmt;

	// Be extra sure that the machine is in as reasonable state
	__asm __volatile("cli; cld");

	va_start(ap, fmt);
	cprintf("kernel panic at %s:%d: ", file, line);
	vcprintf(fmt, ap);
	cprintf("\n");
	va_end(ap);

dead:
	/* break into the kernel monitor */
	while (1)
		monitor(NULL);
}

/* like panic, but don't */
void
_warn(const char *file, int line, const char *fmt,...)
{
	va_list ap;

	va_start(ap, fmt);
	cprintf("kernel warning at %s:%d: ", file, line);
	vcprintf(fmt, ap);
	cprintf("\n");
	va_end(ap);
}
