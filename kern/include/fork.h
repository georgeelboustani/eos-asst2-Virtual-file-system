/*
 * Declarations for fork.
 */

#ifndef _FORK_H_
#define _FORK_H_

/*
 * Contains some file-related maximum length constants
 */
#include <limits.h>
#include <types.h>

/*
 * Put your function declarations and data types here ...
 */

pid_t myfork(struct trapframe *tf);

#endif /* _FORK_H_ */
