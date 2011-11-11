#include <inc/x86.h>
#include <inc/lib.h>

void
umain(void)
{
	int k = 0;
	binaryname = "smp_test";
	while (1) {
		if (k++ % 10000000 == 0)
			cprintf("<< USER SPACE >> sys_getcpuid() = %d\n", sys_getcpuid());
	}
}

