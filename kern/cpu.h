#ifndef __CPU_H__
#define __CPU_H__

#include <inc/types.h>
#include <inc/mmu.h>

#include <kern/smp-params.h>
#include <kern/lapic.h>

// Per-CPU state
struct CPU {
        uchar id;                    // Local APIC ID; index into cpus[] below
//        struct context *scheduler;   // swtch() here to enter scheduler
//        struct taskstate ts;         // Used by x86 to find stack for interrupt
//        struct Segdesc gdt[NSEGS];   // x86 global descriptor table
        volatile uint booted;        // Has the CPU started?

	// remains from xv6 TODO
        int ncli;                    // Depth of pushcli nesting.
        int intena;                  // Were interrupts enabled before pushcli?
  
        // Cpu-local storage variables; see below
//        struct CPU *thisCPU;
//      struct proc *proc;           // The currently-running process.
	struct Env* cur_env;
	struct Taskstate ts;           // Used by x86 to find stack
	                               // for interrupt
};

//extern struct cpu cpus[NCPU];
//#define thiscpu (&cpus[cpunum()])

struct CPU *thisCPU;       // &cpus[cpunum()]

#endif // __CPU_H__
