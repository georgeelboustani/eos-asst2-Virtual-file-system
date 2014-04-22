/*
 * Declarations for file handle and file table management.
 */

#ifndef _FILE_H_
#define _FILE_H_

/*
 * Contains some file-related maximum length constants
 */
#include <limits.h>

/*
 * Put your function declarations and data types here ...
 */
#define NO_ERROR -1

struct retval {
	int errno;
	void* retval;
};

struct retval write(int fd, const void *buf, size_t nbytes);
struct retval open(const char *filename, int flags);
struct retval read(int fd, void *buf, size_t buflen);
struct retval lseek(int fd, off_t pos, int whence);
struct retval close(int fd);
struct retval dup2(int oldfd, int newfd);


#endif /* _FILE_H_ */
