#ifndef __MP_H__
#define __MP_H__

#include <inc/memlayout.h>

#include <kern/smp-params.h>
#include <kern/cpu.h>

// Table entry types
#define MPPROC    0x00  // One per processor
#define MPBUS     0x01  // One per bus
#define MPIOAPIC  0x02  // One per I/O APIC
#define MPIOINTR  0x03  // One per bus interrupt source
#define MPLINTR   0x04  // One per system interrupt source

#define MPBOOT 0x02     // This proc is the bootstrap processor.

struct mp;
struct mpconf;

struct CPU cpus[NCPU];
struct CPU *bcpu;
int ismp;
int ncpu;
uchar ioapicid;

//#define thiscpu (&cpus[cpunum()])

uchar kstacks[NCPU][KSTKSIZE]
__attribute__ ((aligned(PGSIZE)));

void			mpinit(void);
static struct mpconf*	mpconfig(struct mp **pmp);
static struct mp*	mpsearch(void);
int			mpbcpu(void);

#endif
