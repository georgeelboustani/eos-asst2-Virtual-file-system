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

int get_next_free_fd(int previous_fd);
int get_next_open(int previous_open);
int file_is_open(const char* filename);
void init_tables(void);

int initialised = 0;
struct open_file* open_files[OPEN_MAX];
struct file_descriptor* file_descriptors[IOV_MAX];
int previous_fd = 0;
int previous_open = 0;
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
	init_tables();
	struct retval retval;
	retval.errno = NO_ERROR;

	int current_fd = get_next_free_fd(previous_fd);

	int result;
	int open_id = file_is_open(filename);
	if (open_id > -1) {
		file_descriptors[current_fd]->fd = current_fd;
		file_descriptors[current_fd]->pos = 0;
		file_descriptors[current_fd]->open_id = open_id;
	} else {
		struct vnode* vnode;

		result = vfs_open((char *)filename, flags, 0, &vnode);
		// TODO - Throw error if needed;

		file_descriptors[current_fd]->fd = current_fd;
		file_descriptors[current_fd]->pos = 0;

		int open_id = get_next_open(previous_open);
		if (open_id > -1) {
 			file_descriptors[current_fd]->open_id = open_id;
		} else {
			// TODO - throw error too many open
		}
	}

	previous_fd = current_fd;

	retval.val = (int*) current_fd;
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

struct retval myclose(int fd) {
	struct retval retval;
	retval.errno = NO_ERROR;
	retval.val = (int*) 0;
//  update the lowest id to the one we just released
//	if (fd < previous_fd) {
//		set previous to be current.
//	} else {
//		do nothing.
//	}
	(void) fd;
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

int get_next_free_fd(int previous_fd) {
	int next_free = previous_fd;

	while(next_free < IOV_MAX) {
		if (file_descriptors[next_free] == NULL) {
			return next_free;
		}
		next_free++;
	}

	return -1;
}

int get_next_open(int previous_open) {
	int next_open = previous_open;
	
	while(next_open < OPEN_MAX) {
		if (open_files[next_open] == NULL) {
			return next_open;
		}
		next_open++;
	}

	return -1;
}

int file_is_open(const char* filename) {
	int i = 0;
	int found = 0;
	//TODO: OTIMISE PLS. GEORGE PLS.
	while(i < OPEN_MAX) {
		if (open_files[i] != NULL && strcmp(filename, open_files[i]->filename)) {
			return i;
		}
		i++;
	}

	return -1;
}

void init_tables(void) {
	if (!initialised) {
		// TODO: Handle concurrency for open_files/file_descriptors.
		
		// ---- Open Files ----

		int i = 0;
		while (i < OPEN_MAX) {
			open_files[i] = NULL;
			i++;
		}

		// ---- File Descriptors ----

		int i = 0;
		while (i < IOV_MAX) {
			file_descriptors[i] = NULL;
			i++;
		}

		// Initialize STDOUT/STDERR/STDIN file descriptors
		file_descriptors[STDIN_FILENO] = kmalloc(sizeof(struct file_descriptor));
		file_descriptors[STDIN_FILENO]->fd = STDIN_FILENO;
		file_descriptors[STDIN_FILENO]->pos = 0;
		file_descriptors[STDIN_FILENO]->open_id = STDIN_FILENO;

		file_descriptors[STDOUT_FILENO] = kmalloc(sizeof(struct file_descriptor));
		file_descriptors[STDOUT_FILENO]->fd = STDOUT_FILENO;
		file_descriptors[STDOUT_FILENO]->pos = 0;
		file_descriptors[STDOUT_FILENO]->open_id = STDOUT_FILENO;

		file_descriptors[STDERR_FILENO] = kmalloc(sizeof(struct file_descriptor));
		file_descriptors[STDERR_FILENO]->fd = STDERR_FILENO;
		file_descriptors[STDERR_FILENO]->pos = 0;
		file_descriptors[STDERR_FILENO]->open_id = STDERR_FILENO;

		previous_fd = STDERR_FILENO;

		initialised = 1;
	}
}
