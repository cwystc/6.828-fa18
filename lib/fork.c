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
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	if ( ((uvpt[(uint32_t)addr/PGSIZE] & PTE_COW) == PTE_COW) && ((err & FEC_WR) == FEC_WR) ){

	}
	else
		panic("pgfault: not a write to a copy-on-write page!!!");

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	envid_t id = sys_getenvid();
	r = sys_page_alloc(id, PFTEMP, PTE_U | PTE_W | PTE_P);
	if (r<0)
		panic("pgfault: %e", r);
	memcpy(PFTEMP, ROUNDDOWN(addr, PGSIZE), PGSIZE);
	r = sys_page_map(id, PFTEMP, id, ROUNDDOWN(addr, PGSIZE), PTE_U | PTE_W | PTE_P);
	if (r<0)
		panic("pgfault: %e", r);
	r = sys_page_unmap(id, PFTEMP);
	if (r<0)
		panic("pgfault: %e", r);
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
	void * va = (void *)(pn * PGSIZE);
	envid_t id = sys_getenvid();
	if (uvpt[pn]&PTE_SHARE){
		r = sys_page_map(id, va, envid, va, PTE_SYSCALL & uvpt[pn]);
		if (r<0)
			return r;
	}
	else if ((uvpt[pn] & PTE_W) == PTE_W || (uvpt[pn] & PTE_COW) == PTE_COW){
		r = sys_page_map(id, va, envid, va, PTE_COW | PTE_U | PTE_P);
		if (r<0)
			return r;
		r = sys_page_map(id, va, id, va, PTE_COW | PTE_U | PTE_P);
		if (r<0)
			return r;
	}
	else{
		r = sys_page_map(id, va, envid, va, PTE_U | PTE_P);
		if (r<0)
			return r;
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
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	
	int r;
	envid_t id = sys_getenvid();
	set_pgfault_handler(pgfault);
	envid_t t = sys_exofork();
	if (t<0)
		panic("fork: exofork error!!!");
	if (t == 0){
		thisenv = (&envs[ENVX(sys_getenvid())]);
	}
	else{
		for (uint32_t addr = 0;addr<USTACKTOP; addr+=PGSIZE)
			if ((uvpd[PDX(addr)] & PTE_P) == PTE_P && (uvpt[PGNUM(addr)] & PTE_P) == PTE_P)
				duppage(t, addr/PGSIZE);
		r = sys_page_alloc(t, (void *)(UTOP-PGSIZE), PTE_U | PTE_P | PTE_W);
		if (r<0)
			panic("fork: %e", r);
		void _pgfault_upcall();
		r = sys_env_set_pgfault_upcall(t, _pgfault_upcall);
		if (r<0)
			panic("set_pgfault_handler failed!!!");
		r = sys_env_set_status(t, ENV_RUNNABLE);
		if (r<0)
			panic("fork: %e", r);
	}
	return t;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
