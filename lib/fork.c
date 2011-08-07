// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at vpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	if (!(err & FEC_WR))
		panic("(pgfault) fault is not a write: err %08x\n", err);

	if (!(vpt[VPN(addr)] & PTE_COW))
		panic("(pgfault) fault on a non-cow page: va %p\n", addr);
	
	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	//   No need to explicitly delete the old page's mapping.

	// LAB 4: Your code here.
	if ((r = sys_page_alloc(0, PFTEMP, PTE_P|PTE_U|PTE_W)) < 0)
		panic("sys_page_alloc: %e", r);

	memmove(PFTEMP, ROUNDDOWN(addr, PGSIZE), PGSIZE);

	if ((r = sys_page_map(0, PFTEMP, 0, ROUNDDOWN(addr, PGSIZE), 
			      PTE_P|PTE_U|PTE_W)) < 0)
		panic("sys_page_map: %e", r);


	if ((r = sys_page_unmap(0, PFTEMP)) < 0)
		panic("sys_page_unmap: %e\n", r);
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
// 
static int
duppage(envid_t envid, unsigned pn)
{
	int errno;
	uintptr_t addr = pn * PGSIZE;
	int perm = PTE_U | PTE_P;

	if (vpt[pn] & (PTE_W | PTE_COW))
		perm |= PTE_COW;

	// map the page in the target env
	if ((errno = sys_page_map(0, (void*)addr, envid, (void*)addr, perm)) < 0)
	 	panic("(duppage) %e", errno);
	/* re-map the page in ourselves if needed */
	if (perm & PTE_COW) {
		if ((errno = sys_page_map(0, (void*)addr, 0, (void*)addr, perm)) < 0)
			panic("(duppage) %e", errno);
	}

	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use vpd, vpt, and duppage.
//   Remember to fix "env" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	uintptr_t addr;
	envid_t forked_envid;
	
	set_pgfault_handler(pgfault);
	if ((forked_envid = sys_exofork()) < 0)
		return forked_envid; //failure
	
	if (forked_envid == 0) {
		// child
		env = &envs[ENVX(sys_getenvid())];
		return forked_envid;
	}

	addr = 0;
	while (addr < USTACKTOP) {
		if (!(vpd[VPD(addr)] & PTE_P)) {
			addr += PTSIZE;
			continue;
		}
		
		if (!(vpt[VPN(addr)] & PTE_P)) {
			addr += PGSIZE;
			continue;
		}

		duppage(forked_envid, VPN(addr));
		
		addr += PGSIZE;
	}	
	alloc_pgfault_stack(forked_envid);
	sys_env_set_status(forked_envid, ENV_RUNNABLE);

	return forked_envid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
