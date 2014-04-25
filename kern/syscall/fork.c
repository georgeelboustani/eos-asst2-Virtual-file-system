#include <fork.h>
#include <mips/trapframe.h>

pid_t myfork(struct trapframe *tf) {

	(void*)tf;

	return (pid_t) 0;
}
