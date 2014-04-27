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
#include <proc.h>

#define FAILED -1
#define FREE_FD -1
#define TWO_BITS 3

int get_next_free_fd(void);

/*
 * Add your file-related functions here ...
 */
struct retval mywrite(int fd_id, void* buf, size_t nbytes) {
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
	lock_release(curthread->fd_table_lock);

	if ((fd->flags & TWO_BITS) == O_RDONLY) {
		retval.errno = EACCES;
		return retval;
	}

	// char *buffer = (char*)kmalloc(nbytes);
	// if (buffer == NULL) {
	// 	retval.errno = ENOMEM;
	// 	return retval;
	// }
	
	lock_acquire(fd->lock);

	struct iovec iov;
	struct uio uio_writer;

	uio_kinit(&iov, &uio_writer, (void*) buf, nbytes, fd->offset, UIO_WRITE);
	uio_writer.uio_segflg = UIO_USERSPACE;
	uio_writer.uio_space = curthread->t_proc->p_addrspace;

	// size_t length;	
	// int result = copyinstr((userptr_t)buf, buffer, nbytes, &length);
	// if (result != NO_ERROR) {
	// 	lock_release(fd->lock);
	// 	retval.errno = result;
	// 	return retval;
	// }

	int err = VOP_WRITE(fd->vnode, &uio_writer);
	if (err) {
		lock_release(fd->lock);
		retval.errno = err;
		return retval;
	}
	fd->offset += nbytes - uio_writer.uio_resid;
	lock_release(fd->lock);

	retval.val_h = (void*)(nbytes - uio_writer.uio_resid);
	//kfree(buffer);

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

	if ((flags & TWO_BITS) == O_RDONLY) {
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
	
	struct file_descriptor* fd = (struct file_descriptor*)kmalloc(sizeof(struct file_descriptor));
	if (fd == NULL) {
		retval.errno = ENOMEM;
		return retval;
	}

	fd->name = (char*)filename;
	fd->flags = flags;
	fd->ref_count = 0;

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

	lock_acquire(curthread->fd_table_lock);
	int current_fd = get_next_free_fd();

	if (current_fd == FREE_FD) {
		// TODO clean up
		lock_release(curthread->fd_table_lock);
		retval.errno = ENFILE;
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
		// TODO - clean up
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

	if (fd_id <= FREE_FD || fd_id >= OPEN_MAX) {
		retval.errno = EBADF;
		return retval;
	}

//	char *buffer = (char*)kmalloc(nbytes);
//	if (buffer == NULL) {
//		retval.errno = ENOMEM;
//		return retval;
//	}

	lock_acquire(curthread->fd_table_lock);
	struct file_descriptor* fd = curthread->file_descriptors[fd_id];
	if (fd == NULL) {
		lock_release(curthread->fd_table_lock);
		retval.errno = EBADF;
		return retval;
	}
	lock_release(curthread->fd_table_lock);

	if ((fd->flags & O_WRONLY) == O_WRONLY) {
		retval.errno = EACCES;
		return retval;
	}

	struct iovec iov;
	struct uio uio_reader;
	uio_kinit(&iov, &uio_reader, (void*) buf, nbytes, fd->offset, UIO_READ);
	uio_reader.uio_segflg = UIO_USERSPACE;
	uio_reader.uio_space = curthread->t_proc->p_addrspace;
	
	lock_acquire(fd->lock);
	
	int err = VOP_READ(fd->vnode, &uio_reader);
	if (err) {
		lock_release(fd->lock);
		retval.errno = err;
		return retval;
	}

//	err = copyout(buffer, (userptr_t)buf, nbytes);
//	if (err) {
//		lock_release(fd->lock);
//		retval.errno = err;
//		return retval;
//	}

	fd->offset += nbytes - uio_reader.uio_resid;
	retval.val_h = (void*)(nbytes - uio_reader.uio_resid);
	
	lock_release(fd->lock);
//	kfree(buffer);

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
	int new_position = 0;
	if (whence == SEEK_SET) {
		new_position = pos;
	} else if (whence == SEEK_CUR) {
		new_position = fd->offset + pos;
	} else if (whence == SEEK_END) {
		struct stat stat_buffer;
		int result = VOP_STAT(fd->vnode, &stat_buffer);
		if (result) {
			lock_release(fd->lock);
			retval.errno = result;
			return retval;
		}

		new_position = stat_buffer.st_size + pos;
	} else {
		lock_release(fd->lock);
		retval.errno = EINVAL;
		return retval;
	}

	if (new_position < 0) {
		lock_release(fd->lock);
		retval.errno = EINVAL;
		return retval;
	}

	fd->offset = new_position;

	off_t word = fd->offset;
	long temp = ((word >> 32) << 32);
	int low = (int)(word - temp);
	word = fd->offset;
	int high = (int)(word >> 32);
	retval.val_h = (int*) high;
	retval.val_l = (int*) low;

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

	struct file_descriptor* fd = curthread->file_descriptors[fd_id];
	if (fd == NULL) {
		retval.errno = EBADF;
		return retval;
	}

	lock_acquire(fd->lock);

	if (fd->ref_count == 0) {
		vfs_close(fd->vnode);
		curthread->file_descriptors[fd_id] = NULL;
		lock_release(fd->lock);
		kfree(fd);

		if (fd_id < curthread->previous_fd) {
			curthread->previous_fd = fd_id;
		}
	} else {
		fd->ref_count--;
		lock_release(fd->lock);
	}

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

	if (oldfd_id == newfd_id) {
		retval.errno = NO_ERROR;
		return retval;
	}

	lock_acquire(fd->lock);
	struct file_descriptor *new_fd = curthread->file_descriptors[newfd_id];
	if (new_fd != NULL) {
		lock_release(fd->lock);
		struct retval result = myclose(newfd_id);

		if (result.errno != NO_ERROR) {
			return result;
		}

		lock_acquire(fd->lock);
	}

	new_fd = fd;
	new_fd->ref_count++;

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
