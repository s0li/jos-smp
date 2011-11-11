#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>

#include <kern/cpu.h>
#include <kern/mp.h>

// Choose a user environment to run and run it.
void
sched_yield(void)
{
	// Implement simple round-robin scheduling.
	// Search through 'envs' for a runnable environment,
	// in circular fashion starting after the previously running env,
	// and switch to the first such environment found.
	// It's OK to choose the previously running env if no other env
	// is runnable.
	// But never choose envs[0], the idle environment,
	// unless NOTHING else is runnable.

	// LAB 4: Your code here.
	int curenv_indx, i;
	curenv_indx = (curenv == NULL) ? 0 : ENVX(curenv->env_id);

	for (i = (curenv_indx + 1) % NENV; i != curenv_indx; i = (i + 1) % NENV) {
		if (i == 0)
			continue;
		if (envs[i].env_status == ENV_RUNNABLE)
			env_run(&envs[i]);
	}

	if (envs[curenv_indx].env_status == ENV_RUNNABLE)
		env_run(&envs[curenv_indx]);

	// Run the special idle environment when nothing else is runnable.
	if (envs[0].env_status == ENV_RUNNABLE)
		env_run(&envs[0]);
	else {
		cprintf("Destroyed all environments - nothing more to do!\n");
		while (1)
			monitor(NULL);
	}
}

void
sched_yield_smp(void)
{
	// Implement simple round-robin scheduling.
	// Search through 'envs' for a runnable environment,
	// in circular fashion starting after the previously running env,
	// and switch to the first such environment found.
	// It's OK to choose the previously running env if no other env
	// is runnable.
	// But never choose envs[0], the idle environment,
	// unless NOTHING else is runnable.

	// LAB 4: Your code here.
	int curenv_indx, i;
	int low_id, high_id;

	curenv_indx = (curenv == NULL) ? (thisCPU->id * NENV_PER_CPU) : ENVX(curenv->env_id);

	low_id = thisCPU->id * NENV_PER_CPU;
	high_id = (thisCPU->id + 1) * NENV_PER_CPU - 1;
//	cprintf("(sched_yield_smp) cpu%d, lowid = %d, highid = %d\n", thisCPU->id, low_id, high_id);
	
#define increment_id(x)		(((x + 1) % (high_id + 1)) == 0 ? low_id : x + 1)
	
	for (i = increment_id(curenv_indx); i != curenv_indx; i = increment_id(i)) {
		if (i == low_id)
			continue;
		if (envs[i].env_status == ENV_RUNNABLE)
			env_run(&envs[i]);
//		cprintf("wat");
	}

	// found nothing to run except for the env that was runnign previously,
	// try to run it again.
	if (envs[curenv_indx].env_status == ENV_RUNNABLE ||
	    envs[curenv_indx].env_status == ENV_RUNNING)
		env_run(&envs[curenv_indx]);

	// Run the special idle environment when nothing else is runnable.
	// Added the second condition because the idle env may be the current
	// env running.
	if (envs[thisCPU->id * NENV_PER_CPU].env_status == ENV_RUNNABLE ||
	    envs[thisCPU->id * NENV_PER_CPU].env_status == ENV_RUNNING)
		env_run(&envs[thisCPU->id * NENV_PER_CPU]);
	else {
		cprintf("Destroyed all environments - nothing more to do!\n");
		while (1)
			monitor(NULL);
	}
}
