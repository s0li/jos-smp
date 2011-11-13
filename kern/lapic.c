#include <kern/lapic.h>
#include <kern/picirq.h>
#include <kern/pmap.h>
#include <kern/cpu.h>
#include <kern/mp.h>

#include <inc/stdio.h>
#include <inc/trap.h>
#include <inc/x86.h>
#include <inc/mmu.h>

#ifndef IO_RTC
#define	IO_RTC		0x070		/* RTC port */
#endif

static void
lapicw(int index, int value)
{
        lapic[index] = value;
        lapic[ID];  // wait for write to finish, by reading
}

void
lapicinit(int c)
{
        cprintf("lapicinit: %d 0x%x\n", c, lapic);
        if(!lapic) 
                return;

        // Enable local APIC; set spurious interrupt vector.
        lapicw(SVR, ENABLE | (IRQ_OFFSET + IRQ_SPURIOUS));

        // The timer repeatedly counts down at bus frequency
        // from lapic[TICR] and then issues an interrupt.  
        // If xv6 cared more about precise timekeeping,
        // TICR would be calibrated using an external time source.
        lapicw(TDCR, X1);
        lapicw(TIMER, PERIODIC | (IRQ_OFFSET + IRQ_TIMER));
        lapicw(TICR, 10000000);

        // Disable logical interrupt lines.
	// ----------------------------------------
	// Leave LINT0 of the BSP enabled so that it can get
	// interrupts from the 8259A chip.
	//
	// According to Intel MP Specification, the BIOS should initialize
	// BSP's local APIC in Virtual Wire Mode, in which 8259A's
	// INTR is virtually connected to BSP's LINTIN0. In this mode,
	// we do not need to program the IOAPIC.
	cprintf("(lapicinit) thisCPU = %x, bcpu = %x\n", thisCPU, bcpu);
	if (thisCPU != bcpu)
		lapicw(LINT0, MASKED);
//	lapicw(LINT0, MASKED);
        lapicw(LINT1, MASKED);

        // Disable performance counter overflow interrupts
        // on machines that provide that interrupt entry.
        if(((lapic[VER]>>16) & 0xFF) >= 4)
                lapicw(PCINT, MASKED);

        // Map error interrupt to IRQ_ERROR.
        lapicw(ERROR, IRQ_OFFSET + IRQ_ERROR);

        // Clear error status register (requires back-to-back writes).
        lapicw(ESR, 0);
        lapicw(ESR, 0);

        // Ack any outstanding interrupts.
        lapicw(EOI, 0);

        // Send an Init Level De-Assert to synchronise arbitration ID's.
        lapicw(ICRHI, 0);
        lapicw(ICRLO, BCAST | INIT | LEVEL);
        while(lapic[ICRLO] & DELIVS)
                ;

        // Enable interrupts on the APIC (but not on the processor).
        lapicw(TPR, 0);
}

// Spin for a given number of microseconds.
// On real hardware would want to tune this dynamically.
void
microdelay(int us)
{
}

// Start additional processor running bootstrap code at addr.
// See Appendix B of MultiProcessor Specification.
void
lapicstartap(uchar apicid, uint addr)
{
        int i;
        ushort *wrv;
  
        // "The BSP must initialize CMOS shutdown code to 0AH
        // and the warm reset vector (DWORD based at 40:67) to point at
        // the AP startup code prior to the [universal startup algorithm]."
        outb(IO_RTC, 0xF);  // offset 0xF is shutdown code
        outb(IO_RTC+1, 0x0A);
        wrv = (ushort*)(KADDR(0x40<<4 | 0x67));  // Warm reset vector
        wrv[0] = 0;
        wrv[1] = addr >> 4;

        // "Universal startup algorithm."
        // Send INIT (level-triggered) interrupt to reset other CPU.
        lapicw(ICRHI, apicid<<24);
        lapicw(ICRLO, INIT | LEVEL | ASSERT);
        microdelay(200);
        lapicw(ICRLO, INIT | LEVEL);
        microdelay(100);    // should be 10ms, but too slow in Bochs!
  
        // Send startup IPI (twice!) to enter bootstrap code.
        // Regular hardware is supposed to only accept a STARTUP
        // when it is in the halted state due to an INIT.  So the second
        // should be ignored, but it is part of the official Intel algorithm.
        // Bochs complains about the second one.  Too bad for Bochs.
        for(i = 0; i < 2; i++){
                lapicw(ICRHI, apicid<<24);
                lapicw(ICRLO, STARTUP | (addr>>12));
                microdelay(200);
        }
}

void
lapic_eoi(void)
{
	if (lapic)
		lapicw(EOI, 0);
}

int
cpunum(void) {
	// Cannot call cpu when interrupts are enabled:
        // result not guaranteed to last long enough to be used!
        // Would prefer to panic but even printing is chancy here:
        // almost everything, including cprintf and panic, calls cpu,
        // often indirectly through acquire and release.
        if(read_eflags() & FL_IF){
                static int n;
                if(n++ == 0)
                        cprintf("cpu called from %x with interrupts enabled\n",
                                __builtin_return_address(0));
        }

	/* if (lapic) */
	/* 	assert((lapic[ID]>>24) < 4); */

        if(lapic)
                return lapic[ID]>>24;
        return 0;
}

