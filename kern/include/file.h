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
#define NO_ERROR 0

struct retval {
	int errno;
	void* val;
};

struct retval mywrite(int fd, const void *buf, size_t nbytes);
struct retval myopen(const char *filename, int flags);
struct retval myread(int fd, void *buf, size_t buflen);
struct retval mylseek(int fd, off_t pos, int whence);
struct retval myclose(int fd);
struct retval mydup2(int oldfd, int newfd);


#endif /* _FILE_H_ */
