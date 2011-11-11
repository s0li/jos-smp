#ifndef __RUNQUEUE_H__
#define __RUNQUEUE_H__

#include <kern/env.h>

/* Each CPU has an array with NENV environments */

// l - signifies local (i.e. per cpu)
struct Runqueue {
	struct Env*	l_envs;			// All local environments
	struct Env*	l_curenv;		// Current local environment
	struct Env_list l_env_free_list;	// Free list
};

#endif /* __RUNQUEUE_H__ */
