#include <kern/ioapic.h>

#include <kern/mp.h>
#include <kern/picirq.h>

#include <inc/types.h>
#include <inc/stdio.h>

// IO APIC MMIO structure: write reg, then read or write data.
struct ioapic {
        uint reg;
        uint pad[3];
        uint data;
};

static uint
ioapicread(int reg)
{
        ioapic->reg = reg;
        return ioapic->data;
}

static void
ioapicwrite(int reg, uint data)
{
        ioapic->reg = reg;
        ioapic->data = data;
}

void
ioapicinit(void)
{
        int i, id, maxintr;

        if(!ismp)
                return;

        ioapic = (volatile struct ioapic*)IOAPIC;
        maxintr = (ioapicread(REG_VER) >> 16) & 0xFF;
        id = ioapicread(REG_ID) >> 24;

	cprintf(" -- debug -- (ioapicinit) id = %d, ioapicid = %d\n", id, ioapicid);
        if(id != ioapicid)
                cprintf("ioapicinit: id isn't equal to ioapicid; not a MP\n");

        // Mark all interrupts edge-triggered, active high, disabled,
        // and not routed to any CPUs.
        for(i = 0; i <= maxintr; i++){
                ioapicwrite(REG_TABLE+2*i, INT_DISABLED | (IRQ_OFFSET + i));
                ioapicwrite(REG_TABLE+2*i+1, 0);
        }
}

void
ioapicenable(int irq, int cpunum)
{
        if(!ismp)
                return;

        // Mark interrupt edge-triggered, active high,
        // enabled, and routed to the given cpunum,
        // which happens to be that cpu's APIC ID.
        ioapicwrite(REG_TABLE+2*irq, IRQ_OFFSET + irq);
        ioapicwrite(REG_TABLE+2*irq+1, cpunum << 24);
}
