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
#include <spl.h>
#include <kern/wait.h>

#define NO_OPTS 0

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

	struct lock* pid_table_lock = get_pid_table_lock();
	lock_acquire(pid_table_lock);
	struct proc *new_proc = proc_create_runprogram((const char*)"child");
	lock_release(pid_table_lock);
	if (new_proc == NULL) {
		rv.errno = ENOMEM;
		return rv;
	}
	new_proc->parent_pid = curthread->t_proc->pid;

	struct addrspace *new_as;
	int result = as_copy(curthread->t_proc->p_addrspace, &new_as);
	if (result) {
		rv.errno = result;
		return rv;
	}

	result = thread_fork(strcat(curthread->t_name, "_child"), new_proc, (void*)&enter_forked_process, (void *)new_tf, (unsigned long) new_as);
	if (result) {
		kfree(new_tf);
		as_destroy(new_as);
		rv.errno = result;
		return rv;
	}

	// Parent returns 1
	rv.val_h = (pid_t*) new_proc->pid;
	return rv;
}

struct retval mygetpid(void) {
	struct retval rv;
	rv.errno = NO_ERROR;
	rv.val_h = (pid_t*)curthread->t_proc->pid;
	rv.val_l = (pid_t*)0;
	return rv;
}

struct retval mywaitpid(pid_t proc_id, int *status, int options) {
	struct retval rv;
	rv.errno = NO_ERROR;
	rv.val_h = (pid_t*)-1;
	rv.val_l = (pid_t*)0;

	if (options != NO_OPTS) {
		rv.errno = EINVAL;
		return rv;
	}

	lock_acquire(get_pid_table_lock());

	struct proc *child_proc = get_process_by_id(proc_id);

	if (child_proc == NULL) {
		lock_release(get_pid_table_lock());
		rv.errno = ESRCH;
		return rv;
	}

	if (curthread->t_proc->pid == child_proc->pid ||
		curthread->t_proc->pid != child_proc->parent_pid) {
		lock_release(get_pid_table_lock());
		rv.errno = ECHILD;
		return rv;
	}

	lock_release(get_pid_table_lock());

	*status = child_proc->exitcode;
	int result = copyout(&child_proc->exitcode, (userptr_t)status, sizeof(int));
	if (result != NO_ERROR) {
		rv.errno = result;
		return rv;
	}
	rv.val_h = (pid_t*) child_proc->pid;

	P(child_proc->exit_semaphore);

	// Clean up child process.
	// Free up pid_table slot.
//	struct proc* proc = curthread->t_proc;
//	struct thread* child_thread = child_proc->p_thread;
//	proc_remthread(child_thread);
//	proc_destroy(child_proc);
//	thread_destroy(child_thread);

	return rv;
}

struct retval myexit(int exitcode) {
	struct retval rv;
	rv.errno = NO_ERROR;
	rv.val_h = (int*)-1;
	rv.val_l = (int*)0;

	lock_acquire(get_pid_table_lock());
	if (curthread->t_proc->parent_pid != UNASSIGNED) {
		// Indicate that we've exited.
		curthread->t_proc->exitcode = _MKWAIT_EXIT(exitcode);
		V(curthread->t_proc->exit_semaphore);
	}
	lock_release(get_pid_table_lock());

	// Child should be able to kill itself and exit.
	// Thread exit is hanging on main thread exiting (no other threads to switch to)
//	thread_exit();

	return rv;
}
