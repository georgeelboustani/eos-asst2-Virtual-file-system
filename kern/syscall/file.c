#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <kern/seek.h>
#include <kern/unistd.h>
#include <lib.h>
#include <uio.h>
#include <thread.h>
#include <current.h>
#include <synch.h>
#include <vfs.h>
#include <vnode.h>
#include <file.h>
#include <syscall.h>
#include <copyinout.h>

#define FAILED -1
#define FREE_FD -1

int get_next_free_fd(void);

/*
 * Add your file-related functions here ...
 */
struct retval mywrite(int fd, const void *buf, size_t nbytes) {
	struct retval retval;
	retval.errno = NO_ERROR;
	retval.val = (int*) 0;
	(void) fd;
	(void) buf;
	(void) nbytes;
	return retval;
}

// On error, result is -1 and sets the error
struct retval myopen(const char *filename, int flags) {
	struct retval retval;
	retval.errno = NO_ERROR;
	retval.val = (int*) -1;

	if (filename == NULL) {
		// TODO - set appropriate error flag
		return retval;
	}
	
	lock_acquire(curthread->fd_table_lock);
	int current_fd = get_next_free_fd();

	if (current_fd > -1) {
		int result;
	
		struct file_descriptor* fd = kmalloc(sizeof(struct file_descriptor));
		// TODO - make sure the length of filename is < NAME_MAX
		fd->name = (char*)filename;
		fd->flags = flags;
		fd->ref_count = 1;
		fd->offset = 0; // TODO - logic appending not appending - filesize
		fd->lock = lock_create(filename);
		if (fd->lock == NULL) {
			// TODO - return an error
		}

		curthread->file_descriptors[current_fd] = fd;
		curthread->previous_fd = current_fd;
		lock_release(curthread->fd_table_lock);

		result = vfs_open((char *)filename, flags, 0664, &fd->vnode);

		if (result == 0) {
			retval.val = (int*) current_fd;
		} else {
			// TODO - Throw error if needed; and remove the file descriptor from the array
			// retval.errno = 
		}
	} else {
		lock_release(curthread->fd_table_lock);
		// TODO - error no more files
	}
	kprintf("FUCKING OPEN\n");
	return retval;
}

struct retval myread(int fd, void *buf, size_t buflen) {
	struct retval retval;
	retval.errno = NO_ERROR;
	retval.val = (int*) 0;
	(void) fd;
	(void) buf;
	(void) buflen;
	return retval;
}

struct retval mylseek(int fd, off_t pos, int whence) {
	struct retval retval;
	retval.errno = NO_ERROR;
	retval.val = (off_t*) 0;
	(void) fd;
	(void) pos;
	(void) whence;
	return retval;
}

struct retval myclose(int fd_id) {
	struct retval retval;
	retval.errno = NO_ERROR;
	retval.val = (int*) -1;

	if (fd_id < 0 || fd_id >= OPEN_MAX) {
		retval.errno = EBADF;
		return retval;
	}

	// TODO - change this lock to a per fd basis. we stille need this table lock for previous_fd stuff
	lock_acquire(curthread->fd_table_lock);
	struct file_descriptor* fd = curthread->file_descriptors[fd_id];

	if (fd != NULL) {
		vfs_close(fd->vnode);
		fd->ref_count--;
		
		if (fd->ref_count == 0) {
			lock_destroy(fd->lock);
			kfree(fd);
			curthread->file_descriptors[fd_id] = NULL;

			// TODO - update the previous_fd
			if (fd_id < curthread->previous_fd) {
				curthread->previous_fd = fd_id;
			}
		}

		lock_release(curthread->fd_table_lock);
	} else {
		lock_release(curthread->fd_table_lock);
		retval.errno = EBADF;
		return retval;
	}

	return retval;
}

struct retval mydup2(int oldfd, int newfd) {
	struct retval retval;
	retval.errno = NO_ERROR;
	retval.val = (int*) 0;
	(void) oldfd;
	(void) newfd;
	return retval;
}

int get_next_free_fd(void) {
	int next_free = curthread->previous_fd;

	while(next_free < OPEN_MAX) {
		if (curthread->file_descriptors[next_free] == NULL) {
			return next_free;
		}
		next_free++;
	}

	return -1;
}
