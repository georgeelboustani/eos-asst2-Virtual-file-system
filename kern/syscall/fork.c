#include <types.h>
#include <kern/errno.h>
#include <kern/syscall.h>
#include <lib.h>
#include <mips/trapframe.h>
#include <thread.h>
#include <current.h>
#include <syscall.h>
#include <file.h>
#include <copyinout.h>
#include <proc.h>
#include <addrspace.h>

struct retval myfork(struct trapframe *tf) {
	struct retval rv;
	rv.errno = NO_ERROR;
	rv.val_h = (void*)-1;
	rv.val_l = (void*)-1;

	struct trapframe *new_tf = (struct trapframe*)kmalloc(sizeof(struct trapframe));
	if (new_tf == NULL) {
		rv.errno = ENOMEM;
		return rv;
	}
	memcpy(new_tf, tf, sizeof(struct trapframe));
	//new_tf->tf_a0 = (uint32_t)curthread->sem;

	struct proc *new_proc = proc_create_runprogram((const char*)"child");
	if (new_proc == NULL) {
		rv.errno = ENOMEM;
		return rv;
	}

	struct addrspace *new_as;
	int result = as_copy(curthread->t_proc->p_addrspace, &new_as);
	if (result) {
		rv.errno = result;
		return rv;
	}

	result = thread_fork(strcat(curthread->t_name, "_child"), new_proc, (void*)&enter_forked_process, (void *)new_tf, (unsigned long) curthread->t_proc->p_addrspace);
	if (result) {
		kfree(new_tf);
		as_destroy(new_as);
		rv.errno = result;
		return rv;
	}

	// Parent returns 1
	rv.val_h = (pid_t*)1;
	return rv;
}
