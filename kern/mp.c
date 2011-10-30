#include <kern/mp.h>
#include <kern/smp-params.h>
#include <kern/cpu.h>
#include <kern/pmap.h> // kaddr
#include <kern/lapic.h>

#include <inc/x86.h>
#include <inc/types.h>
#include <inc/string.h> // for memcmp
#include <inc/stdio.h>

// xv6 doesn't use segmentation, JOS does.
// before i386_init runs segmentation is set so that addresses
// 0x0 through 0x0f...f (physical) are mapped to 0xf0...0 through 0xf...f

struct mp {			// floating pointer
        uchar signature[4];     // "_MP_"
        void *physaddr;         // phys addr of MP config table
        uchar length;           // 1
        uchar specrev;          // [14]
        uchar checksum;         // all bytes must add up to 0
        uchar type;             // MP system config type
        uchar imcrp;
        uchar reserved[3];
} __attribute__((__packed__));

struct mpconf {         // configuration table header
        uchar signature[4];           // "PCMP"
        ushort length;                // total table length
        uchar version;                // [14]
        uchar checksum;               // all bytes must add up to 0
        uchar product[20];            // product id
        uint *oemtable;               // OEM table pointer
        ushort oemlength;             // OEM table length
        ushort entry;                 // entry count
        uint *lapicaddr;              // address of local APIC
        ushort xlength;               // extended table length
        uchar xchecksum;              // extended table checksum
        uchar reserved;
} __attribute__((__packed__));

struct mpproc {         // processor table entry
        uchar type;                   // entry type (0)
        uchar apicid;                 // local APIC id
        uchar version;                // local APIC verison
        uchar flags;                  // CPU flags
        uchar signature[4];           // CPU signature
        uint feature;                 // feature flags from CPUID instruction
        uchar reserved[8];
} __attribute__((__packed__));

struct mpioapic {       // I/O APIC table entry
        uchar type;                   // entry type (2)
        uchar apicno;                 // I/O APIC id
        uchar version;                // I/O APIC version
        uchar flags;                  // I/O APIC flags
        uint *addr;                  // I/O APIC address
} __attribute__((__packed__));

static uchar
sum(void *addr, int len)
{
        int i, sum;
  
        sum = 0;
        for(i=0; i<len; i++)
		sum += ((uint8_t *)addr)[i];
        return sum;
}

// Look for an MP structure in the len bytes at addr.
static struct mp*
mpsearch1(uchar *addr, int len)
{
        /* uchar *e, *p; */

        /* e = KADDR(addr + len); */
        /* for(p = addr; p < e; p += sizeof(struct mp) { */
	/* 	cprintf(" -- debug -- (mpsearch1) current p = %x\n", p); */
        /*         if(memcmp(p, "_MP_", 4) == 0 && sum(p, sizeof(struct mp)) == 0) */
        /*                 return (struct mp*)p; */
	/* } */
        /* return 0; */
	struct mp *mp = KADDR((physaddr_t)addr), *end = KADDR((physaddr_t)addr + len);

	for (; mp < end; mp++)
		if (memcmp(mp->signature, "_MP_", 4) == 0 &&
		    sum(mp, sizeof(*mp)) == 0)
			return mp;
	return NULL;
	    
}

// Search for the MP Floating Pointer Structure, which according to the
// spec is in one of the following three locations:
// 1) in the first KB of the EBDA;
// 2) in the last KB of system base memory;
// 3) in the BIOS ROM between 0xE0000 and 0xFFFFF.
static struct mp*
mpsearch(void)
{
        uchar *bda;
        uint p;
        struct mp *mp;

	// bda = BIOS data area (?)
        bda = (uchar*)KADDR(0x400);
        if((p = ((bda[0x0F]<<8)|bda[0x0E]) << 4)){
                if((mp = mpsearch1((uchar*)p, 1024)))
                        return mp;
        } else {
                p = ((bda[0x14]<<8)|bda[0x13])*1024;
                if((mp = mpsearch1((uchar*)p-1024, 1024)))
                        return mp;
        }
        return mpsearch1((uchar*)0xF0000, 0x10000);
}

// Search for an MP configuration table.  For now,
// don't accept the default configurations (physaddr == 0).
// Check for correct signature, calculate the checksum and,
// if correct, check the version.
// To do: check extended table checksum.
static struct mpconf*
mpconfig(struct mp **pmp)
{
        struct mpconf *conf;
        struct mp *mp;

	// debug
	cprintf(" -- debug -- (mpconfig) mpsearch() result = %x\n", mpsearch());
	cprintf(" -- debug -- (mpconfig) mp->physaddr = %x\n", mpsearch()->physaddr);
	
        if((mp = mpsearch()) == 0 || mp->physaddr == 0)
                return 0;
        conf = (struct mpconf*)(KADDR((uintptr_t)mp->physaddr));
        if(memcmp(conf, "PCMP", 4) != 0)
                return 0;
        if(conf->version != 1 && conf->version != 4)
                return 0;
        if(sum((uchar*)conf, conf->length) != 0)
                return 0;
        *pmp = mp;
        return conf;
}

int
mpbcpu(void)
{
        return bcpu-cpus;
}

void
mpinit(void) {
	uchar *p, *e;
        struct mp *mp;
        struct mpconf *conf;
        struct mpproc *proc;
        struct mpioapic *ioapic;

        bcpu = &cpus[0];
        if((conf = mpconfig(&mp)) == 0) {
		// debug
		cprintf(" -- debug -- (mpinit) mpconfig has returned 0\n");
                return;
	}
	
        ismp = 1;
        lapic = (uint*)conf->lapicaddr;
	   for(p=(uchar*)(conf+1), e=(uchar*)conf+conf->length; p<e; ){
                switch(*p){
                case MPPROC:
                        proc = (struct mpproc*)p;
                        if(ncpu != proc->apicid){
                                cprintf("mpinit: ncpu=%d apicid=%d\n", ncpu, proc->apicid);
                                ismp = 0;
                        }
                        if(proc->flags & MPBOOT)
                                bcpu = &cpus[ncpu];
			if (ncpu < NCPU) {
				cpus[ncpu].id = ncpu;
				ncpu++;
			}
			else
				cprintf("(mpinit) too many CPUs detected, CPU %d disabled\n",
					proc->apicid);
			
                        p += sizeof(struct mpproc);
                        continue;
                case MPIOAPIC:
                        ioapic = (struct mpioapic*)p;
                        ioapicid = ioapic->apicno;
                        p += sizeof(struct mpioapic);
                        continue;
                case MPBUS:
                case MPIOINTR:
                case MPLINTR:
                        p += 8;
                        continue;
                default:
                        cprintf("mpinit: unknown config type %x\n", *p);
                        ismp = 0;
                }
        }
	   
        if(!ismp){
		cprintf(" -- debug -- (mpinit) SMP conf not found\n");
		
                // Didn't like what we found; fall back to no MP.
                ncpu = 1;
                lapic = 0;
                ioapicid = 0;
                return;
        }

        if(mp->imcrp){
                // Bochs doesn't support IMCR, so this doesn't run on Bochs.
                // But it would on real hardware.
                outb(0x22, 0x70);   // Select IMCR
                outb(0x23, inb(0x23) | 1);  // Mask external interrupts.
        }
}

