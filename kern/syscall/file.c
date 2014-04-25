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
struct retval mywrite(int fd_id, void* buf, size_t nbytes) {
	struct retval retval;
	retval.errno = NO_ERROR;
	retval.val_h = (int*) FAILED;
	retval.val_l = (int*) FAILED;

	struct file_descriptor* fd = curthread->file_descriptors[fd_id];
	if (fd == NULL) {
		retval.errno = EBADF;
		return retval;
	}

	if ((fd->flags & 3) == O_RDONLY) {
		retval.errno = EACCES;
		return retval;
	}

	struct iovec iov;
	struct uio uio_writer;
	
	size_t length;
	char *buffer = (char*)kmalloc(nbytes);
	if (buffer == NULL) {
		retval.errno = ENOMEM;
		return retval;
	}

	copyinstr((userptr_t)buf, buffer, nbytes, &length);

	lock_acquire(fd->lock);
	uio_kinit(&iov, &uio_writer, (void*) buffer, nbytes, fd->offset, UIO_WRITE);

	int err = VOP_WRITE(fd->vnode, &uio_writer);
	if (err) {
		retval.errno = err;
		return retval;
	}
	fd->offset = uio_writer.uio_offset;
	lock_release(fd->lock);

	retval.val_h = (void*)(nbytes - uio_writer.uio_resid);
	kfree(buffer);

	return retval;
}

struct retval myopen(const_userptr_t filename, int flags) {
	size_t length;
	struct retval retval;
	struct vnode* vn;
	retval.errno = NO_ERROR;
	retval.val_h = (int*) FAILED;
	retval.val_l = (int*) FAILED;
	int flag_count = 0;

	if ((flags & 3) == O_RDONLY) {
		flag_count++;
	}

	if ((flags & O_WRONLY) == O_WRONLY) {
		flag_count++;
	}

	if ((flags & O_RDWR) == O_RDWR) {
		flag_count++;
	}

	if (flag_count != 1) {
		retval.errno = EINVAL;
		return retval;
	}

	char *sys_filename = (char*)kmalloc(sizeof(char)*PATH_MAX);
	if(sys_filename == NULL) {
		retval.errno = ENOMEM;
		return retval;
	}

	int result = copyinstr(filename, sys_filename, PATH_MAX, &length);
	if (result != NO_ERROR) {
		retval.errno = result;
		return retval;
	}

	lock_acquire(curthread->fd_table_lock);
	int current_fd = get_next_free_fd();

	if (current_fd == FREE_FD) {
		retval.errno = ENFILE;
		return retval;
	}
	
	struct file_descriptor* fd = (struct file_descriptor*)kmalloc(sizeof(struct file_descriptor));
	if (fd == NULL) {
		retval.errno = ENOMEM;
		return retval;
	}

	fd->name = (char*)filename;
	fd->flags = flags;
	fd->ref_count = 1;

	if ((flags & O_APPEND) == O_APPEND) {
		struct stat stat_buffer;
		VOP_STAT(fd->vnode, &stat_buffer);

		fd->offset = stat_buffer.st_size;
	} else {
		fd->offset = 0;
	}

	fd->lock = lock_create(sys_filename);
	if (fd->lock == NULL) {
		retval.errno = ENOMEM;
		return retval;
	}

	curthread->file_descriptors[current_fd] = fd;
	curthread->previous_fd = current_fd;
	lock_release(curthread->fd_table_lock);

	result = vfs_open(sys_filename, flags, 0664, &vn);
	fd->vnode = vn;

	if (result == NO_ERROR) {
		retval.val_h = (int*) current_fd;
	} else {
		retval.errno = result;
		return retval;
	}
	return retval;
}

struct retval myread(int fd_id, void *buf, size_t nbytes) {
	struct retval retval;
	retval.errno = NO_ERROR;
	retval.val_h = (int*) FAILED;
	retval.val_l = (int*) FAILED;

	struct file_descriptor* fd = curthread->file_descriptors[fd_id];
	if (fd == NULL) {
		retval.errno = EBADF;
		return retval;
	}

	if ((fd->flags & O_WRONLY) == O_WRONLY) {
		retval.errno = EACCES;
		return retval;
	}

	struct iovec iov;
	struct uio uio_reader;
	char *buffer = (char*)kmalloc(nbytes);
	if (buffer == NULL) {
		retval.errno = ENOMEM;
		return retval;
	}

	lock_acquire(fd->lock);
	uio_kinit(&iov, &uio_reader, (void*) buffer, nbytes, fd->offset, UIO_READ);
	int err = VOP_READ(fd->vnode, &uio_reader);

	size_t length;
	copyoutstr(buffer, (userptr_t)buf, nbytes, &length);

	if (err) {
		retval.errno = err;
		return retval;
	}
	fd->offset = uio_reader.uio_offset;
	lock_release(fd->lock);

	retval.val_h = (void*)(nbytes - uio_reader.uio_resid);
	kfree(buffer);

	return retval;
}

struct retval mylseek(int fd_id, off_t pos, int whence) {
	struct retval retval;
	retval.errno = NO_ERROR;
	retval.val_h = (int*) FAILED;
	retval.val_l = (int*) FAILED;

	if (fd_id <= FREE_FD || fd_id >= OPEN_MAX) {
		retval.errno = EBADF;
		return retval;
	}

	struct file_descriptor* fd = curthread->file_descriptors[fd_id];
	if (fd == NULL) {
		retval.errno = EBADF;
		return retval;
	}

	lock_acquire(fd->lock);
	if (whence == SEEK_SET) {
		fd->offset = pos;

		off_t word = fd->offset;
		long temp = ((word >> 32) << 32);
		int low = (int)(word - temp);

		word = fd->offset;
		int high = (int)(word >> 32);

		retval.val_h = (int*) high;
		retval.val_l = (int*) low;
	} else if (whence == SEEK_CUR) {
		fd->offset += pos;

		off_t word = fd->offset;
		long temp = ((word >> 32) << 32);
		int low = (int)(word - temp);

		word = fd->offset;
		int high = (int)(word >> 32);

		retval.val_h = (int*) high;
		retval.val_l = (int*) low;
	} else if (whence == SEEK_END) {
		struct stat stat_buffer;
		VOP_STAT(fd->vnode, &stat_buffer);

		fd->offset = stat_buffer.st_size + pos;

		off_t word = fd->offset;
		long temp = ((word >> 32) << 32);
		int low = (int)(word - temp);

		word = fd->offset;
		int high = (int)(word >> 32);

		retval.val_h = (int*) high;
		retval.val_l = (int*) low;
	} else {
		retval.errno = EINVAL;
		return retval;
	}
	lock_release(fd->lock);

	return retval;
}

struct retval myclose(int fd_id) {
	struct retval retval;
	retval.errno = NO_ERROR;
	retval.val_h = (int*) FAILED;
	retval.val_l = (int*) FAILED;

	if (fd_id <= FREE_FD || fd_id >= OPEN_MAX) {
		retval.errno = EBADF;
		return retval;
	}

	lock_acquire(curthread->fd_table_lock);
	struct file_descriptor* fd = curthread->file_descriptors[fd_id];
	if (fd == NULL) {
		lock_release(curthread->fd_table_lock);
		retval.errno = EBADF;
		return retval;
	}

	vfs_close(fd->vnode);
	fd->ref_count--;

	if (fd->ref_count == 0) {
		lock_destroy(fd->lock);
		kfree(fd);
		curthread->file_descriptors[fd_id] = NULL;

		if (fd_id < curthread->previous_fd) {
			curthread->previous_fd = fd_id;
		}
	}
	lock_release(curthread->fd_table_lock);

	return retval;
}

struct retval mydup2(int oldfd_id, int newfd_id) {
	struct retval retval;
	retval.errno = NO_ERROR;
	retval.val_h = (int*) FAILED;
	retval.val_l = (int*) FAILED;

	if (oldfd_id <= FREE_FD || newfd_id <= FREE_FD || oldfd_id >= OPEN_MAX || newfd_id >= OPEN_MAX) {
		retval.errno = EBADF;
		return retval;
	}

	struct file_descriptor *fd = curthread->file_descriptors[oldfd_id];
	if (fd == NULL) {
		retval.errno = EBADF;
		return retval;
	}

	lock_acquire(fd->lock);
	struct file_descriptor *new_fd = curthread->file_descriptors[newfd_id];
	if (new_fd != NULL) {
		struct retval result = myclose(newfd_id);
		if (result.errno != NO_ERROR) {
			lock_release(fd->lock);
			return result;
		}
	}

	new_fd = (struct file_descriptor*) kmalloc(sizeof(struct file_descriptor));
	if (new_fd == NULL) {
		retval.errno = ENOMEM;
		return retval;
	}

	new_fd->flags = fd->flags;
	new_fd->lock = lock_create("" + newfd_id);
	new_fd->name = fd->name;
	new_fd->offset = fd->offset;
	new_fd->ref_count = 1;
	retval.val_h = (int*) newfd_id;
	curthread->file_descriptors[newfd_id] = new_fd;

	lock_release(fd->lock);

	struct vnode* vn;
	vfs_open(new_fd->name, new_fd->flags, 0664, &vn);
	new_fd->vnode = vn;

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

	return FAILED;
}
