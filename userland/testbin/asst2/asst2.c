#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <err.h>
#include <errno.h>

#define MAX_BUF 500
char teststr[] = "The quick brown fox jumped over the lazy dog.";
char buf[MAX_BUF];
char newbuf[MAX_BUF];

int
main(int argc, char * argv[])
{
	int fd, r, i, j , k;
	(void) argc;
	(void) argv;

	pid_t p = getpid();
	printf("PID of this process is: %d", p);

	printf("\n**********\n* File Tester\n");

	snprintf(buf, MAX_BUF, "**********\n* write() works for stdout\n");
	write(1, buf, strlen(buf));
	snprintf(buf, MAX_BUF, "**********\n* write() works for stderr\n");
	write(2, buf, strlen(buf));

	printf("**********\n* opening new file \"test.file\"\n");
	fd = open("test.file", O_RDWR | O_CREAT );
	printf("* open() got fd %d\n", fd);
	if (fd < 0) {
		printf("ERROR opening file: %s\n", strerror(errno));
		exit(1);
	}

	printf("* writing test string\n");  
	r = write(fd, teststr, strlen(teststr));
	printf("* wrote %d bytes\n", r);
	if (r < 0) {
		printf("ERROR writing file: %s\n", strerror(errno));
		exit(1);
	}

	printf("* writing test string again\n");  
	r = write(fd, teststr, strlen(teststr));
	printf("* wrote %d bytes\n", r);
	if (r < 0) {
		printf("ERROR writing file: %s\n", strerror(errno));
		exit(1);
	}
	printf("* closing file\n");  
	close(fd);

	printf("**********\n* opening old file \"test.file\"\n");
	fd = open("test.file", O_RDONLY);
	printf("* open() got fd %d\n", fd);
	if (fd < 0) {
		printf("ERROR opening file: %s\n", strerror(errno));
		exit(1);
	}

	printf("* reading entire file into buffer \n");
	i = 0;
	do  {  
		printf("* attemping read of %d bytes\n", MAX_BUF -i);
		r = read(fd, &buf[i], MAX_BUF - i);
		printf("* read %d bytes\n", r);
		i += r;
	} while (i < MAX_BUF && r > 0);

	printf("* reading complete\n");
	if (r < 0) {
		printf("ERROR reading file: %s\n", strerror(errno));
		exit(1);
	}
	k = j = 0;
	r = strlen(teststr);
	do {
		if (buf[k] != teststr[j]) {
			printf("ERROR  file contents mismatch\n");
			exit(1);
		}
		k++;
		j = k % r;
	} while (k < i);
	printf("* file content okay\n");

	printf("**********\n* testing lseek\n");
	r = lseek(fd, 5, SEEK_SET);
	if (r < 0) {
		printf("ERROR lseek: %s\n", strerror(errno));
		exit(1);
	}

	printf("* reading 10 bytes of file into buffer \n");
	i = 0;
	do  {  
		printf("* attemping read of %d bytes\n", 10 - i );
		r = read(fd, &buf[i], 10 - i);
		printf("* read %d bytes\n", r);
		i += r;
	} while (i < 10 && r > 0);
	printf("* reading complete\n");
	if (r < 0) {
		printf("ERROR reading file: %s\n", strerror(errno));
		exit(1);
	}

	k = 0;
	j = 5;
	r = strlen(teststr);
	do {
		if (buf[k] != teststr[j]) {
			printf("ERROR  file contents mismatch\n");
			exit(1);
		}
		k++;
		j = (k + 5)% r;
	} while (k < 5);

	printf("* file lseek  okay\n");

	/* Duping */
	printf("**********\n* testing dup2 with new fd\n");
	int unused_fd = 15;
	r = dup2(fd, unused_fd);
	if (r == 15) {
		printf("* reading 10 bytes of original file into buffer \n");
		i = 0;
		do  {
			printf("* attemping read of %d bytes\n", 10 - i );
			r = read(fd, &buf[i], 10 - i);
			printf("* read %d bytes\n", r);
			i += r;
		} while (i < 10 && r > 0);
		printf("* reading 10 bytes of new file into buffer \n");
		i = 0;
		do  {
			printf("* attemping read of %d bytes\n", 10 - i );
			r = read(unused_fd, &newbuf[i], 10 - i);
			printf("* read %d bytes\n", r);
			i += r;
		} while (i < 10 && r > 0);
		i = 0;
		while (i < 10) {
			if (buf[i] == newbuf[i]) {
				printf("* data at %d is the same: %c\n", i, buf[i]);
			} else {
				printf("ERROR data at %d is different\n", i);
				printf("buf[%d] = %c compared to newbuf[%d] = %c\n", i, buf[i], i, newbuf[i]);
			}
			i++;
		}
	} else {
		printf("ERROR dup2 failed, return fd: %d is different from parameter: %d\n", r, unused_fd);
	}

	printf("**********\n* testing dup2 with existing fd\n");
	int used_fd = 15;
	r = dup2(fd, used_fd);
	if (r == 15) {
		printf("* reading 10 bytes of original file into buffer \n");
		i = 0;
		do  {
			printf("* attemping read of %d bytes\n", 10 - i );
			r = read(fd, &buf[i], 10 - i);
			printf("* read %d bytes\n", r);
			i += r;
		} while (i < 10 && r > 0);
		printf("* reading 10 bytes of new file into buffer \n");
		i = 0;
		do  {
			printf("* attemping read of %d bytes\n", 10 - i );
			r = read(used_fd, &newbuf[i], 10 - i);
			printf("* read %d bytes\n", r);
			i += r;
		} while (i < 10 && r > 0);
		i = 0;
		while (i < 10) {
			if (buf[i] == newbuf[i]) {
				printf("* data at %d is the same: %c\n", i, buf[i]);
			} else {
				printf("ERROR data at %d is different\n", i);
				printf("buf[%d] = %c compared to newbuf[%d] = %c\n", i, buf[i], i, newbuf[i]);
			}
			i++;
		}
	} else {
		printf("ERROR dup2 failed, return fd: %d is different from parameter: %d\n", r, used_fd);
	}

	printf("* dup2() success! YOU ARE AWESOME!\n");

	printf("* closing file\n");
	close(fd);

	/* Forking */
	printf("**********\n* testing fork\n");

	pid_t parent = 0;
	pid_t child = 0;
	int status;
	pid_t childID = fork();
	if (childID >= 0) {
		printf("* Forking success\n");
		if (childID == 0) {
			printf("* Forked, in child\n");
			child = getpid();
			printf("* Exiting child\n");
			int i = 0;
			while (i < 10000000) {
				i++;
			}
			exit(0);
		} else {
			printf("* Forked, in parent\n");
			parent = getpid();
			printf("* Checking on child exitcode\n");
			pid_t pid = waitpid(childID, &status, 0);
			printf("* Child: %d exited with exitcode: %d\n", pid, status);
		}
	} else {
		printf("* Forking failed\n");
	}
	printf("parent pid: %d, child pid: %d\n", parent, child);

	return 0;
}
