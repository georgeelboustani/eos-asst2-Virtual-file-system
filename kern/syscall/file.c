#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <kern/seek.h>
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

#define FIRST_FD 0
#define FIRST_OPEN 0
#define FAILED -1
#define FREE_FD -1
#define NOT_OPEN -1
#define STDOUT 0
#define STDERR 1
#define STDIN 2

struct file_descriptor {
	int fd;
	long pos;
	int open_id;
};

struct open_file {
	int id;
	const char *filename;
	struct vnode* vnode;
};

int get_next_free(int previous_fd);
int get_next_open(int previous_open);
int file_is_open(const char* filename);
void init_tables(void);

struct open_file* open_files;
struct file_descriptor* file_descriptors;
int previous_fd = FIRST_FD;
int previous_open = FIRST_OPEN;
/*
 * Add your file-related functions here ...
 */
struct retval write(int fd, const void *buf, size_t nbytes) {
	struct retval retval;
	retval.errno = NO_ERROR;
	retval.retval = (int*) 0;
	(void) fd;
	(void) buf;
	(void) nbytes;
	return retval;
}

struct retval open(const char *filename, int flags) {
	init_tables();
	struct retval retval;
	retval.errno = NO_ERROR;

	int current_fd = get_next_free(previous_fd);

	int result;
	int open_id = file_is_open(filename);
	if (open_id != FAILED) {
		file_descriptors[current_fd].fd = current_fd;
		file_descriptors[current_fd].pos = 0;
		file_descriptors[current_fd].open_id = open_id;
	} else {
		struct vnode* vnode;

		result = vfs_open((char *)filename, flags, 0, &vnode);
		// Throw error;

		file_descriptors[current_fd].fd = current_fd;
		file_descriptors[current_fd].pos = 0;
 		file_descriptors[current_fd].open_id = get_next_open(previous_open);
	}

	previous_fd = current_fd;

	retval.retval = (int*) current_fd;
	return retval;
}

struct retval read(int fd, void *buf, size_t buflen) {
	struct retval retval;
	retval.errno = NO_ERROR;
	retval.retval = (int*) 0;
	(void) fd;
	(void) buf;
	(void) buflen;
	return retval;
}

struct retval lseek(int fd, off_t pos, int whence) {
	struct retval retval;
	retval.errno = NO_ERROR;
	retval.retval = (off_t*) 0;
	(void) fd;
	(void) pos;
	(void) whence;
	return retval;
}

struct retval close(int fd) {
	struct retval retval;
	retval.errno = NO_ERROR;
	retval.retval = (int*) 0;
//	if (fd < previous_fd) {
//		set previous to be current.
//	} else {
//		do nothing.
//	}
	(void) fd;
	return retval;
}

struct retval dup2(int oldfd, int newfd) {
	struct retval retval;
	retval.errno = NO_ERROR;
	retval.retval = (int*) 0;
	(void) oldfd;
	(void) newfd;
	return retval;
}

int get_next_free(int previous_fd) {
	int next_free = previous_fd;
	while(next_free < IOV_MAX && file_descriptors[next_free].fd != FREE_FD) {
		next_free++;
	}

	if (next_free == IOV_MAX) {
		return FAILED;
	}

	return next_free;
}

int get_next_open(int previous_open) {
	int next_open = previous_open;
	while(next_open < OPEN_MAX && open_files[next_open].id != NOT_OPEN) {
		next_open++;
	}

	if (next_open == OPEN_MAX) {
		return FAILED;
	}

	return next_open;
}

int file_is_open(const char* filename) {
	int i = FIRST_OPEN;
	//TODO: FIX PLS. GEORGE PLS.
	while(i < OPEN_MAX && !strcmp(filename, open_files[i].filename)) {
		i++;
	}

	if (i == OPEN_MAX) {
		return FAILED;
	}

	return i;
}

void init_tables(void) {
	// TODO: Handle concurrency for open_files/file_descriptors.
	if (open_files == NULL) {
		open_files = kmalloc(sizeof(struct open_file*) * OPEN_MAX);

		int i = FIRST_OPEN;
		while (i < OPEN_MAX) {
			open_files[i].id = NOT_OPEN;
			i++;
		}
	}

	if (file_descriptors == NULL) {
		file_descriptors = kmalloc(sizeof(struct file_descriptor*) * IOV_MAX);

		int i = FIRST_FD;
		while (i < IOV_MAX) {
			file_descriptors[i].fd = FREE_FD;
			i++;
		}

		// Initialize STDOUT/STDERR/STDIN file descriptors
		file_descriptors[STDOUT].fd = STDOUT;
		file_descriptors[STDOUT].pos = 0;
		file_descriptors[STDOUT].open_id = STDOUT;

		file_descriptors[STDERR].fd = STDERR;
		file_descriptors[STDERR].pos = 0;
		file_descriptors[STDERR].open_id = STDERR;

		file_descriptors[STDIN].fd = STDIN;
		file_descriptors[STDIN].pos = 0;
		file_descriptors[STDIN].open_id = STDIN;

		previous_fd = STDIN;
	}
}
