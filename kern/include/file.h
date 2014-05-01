/*
 * Declarations for file handle and file table management.
 */

#ifndef _FILE_H_
#define _FILE_H_

/*
 * Contains some file-related maximum length constants
 */
#include <limits.h>
#include <types.h>

/*
 * Put your function declarations and data types here ...
 */
#define NO_ERROR 0

struct file_descriptor {
	char* name;
	int flags;
	off_t offset;
	int ref_count;
	struct lock* lock;
	struct vnode* vnode;
};

struct retval {
	int errno;
	void* val_h;
	void* val_l;
};

struct retval mywrite(int fd, void *buf, size_t nbytes);
struct retval myopen(const_userptr_t filename, int flags);
struct retval myread(int fd, void *buf, size_t buflen);
struct retval mylseek(int fd, off_t pos, int whence);
struct retval myclose(int fd);
struct retval mydup2(int oldfd, int newfd);

struct trapframe;
struct retval myfork(struct trapframe *tf);
struct retval mygetpid(void);

#endif /* _FILE_H_ */
