#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include "noeintr.h"
#include "warnp.h"

#include "ipc_sync.h"

/* Convention for read/write ends of a pipe. */
#define R 0
#define W 1

struct ipc_sync {
	int fd[2];
};

/* Read and discard one byte from ${fd}, looping upon EINTR. */
static inline int
readbyte(int fd)
{
	char dummy;
	char done = 0;

	/* Loop until done. */
	do {
		switch (read(fd, &dummy, 1)) {
		case -1:
			/* Anything other than EINTR is bad. */
			if (errno != EINTR) {
				warnp("read");
				goto err0;
			}

			/* Otherwise, loop and read again. */
			break;
		case 0:
			warn0("Unexpected EOF in pipe");
			goto err0;
		case 1:
			/* Expected value; quit the loop. */
			done = 1;
		}
	} while (!done);

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}

/* Write one byte from ${fd}, looping upon EINTR. */
static inline int
writebyte(int fd)
{
	char dummy = 0;

	/* Do the write. */
	if (noeintr_write(fd, &dummy, 1) == -1) {
		warnp("write");
		goto err0;
	}

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}

/* close(), but looping upon EINTR. */
static inline int
ipc_sync_close(int fd)
{

	/* Loop until closed. */
	while (close(fd)) {
		if (errno == EINTR)
			continue;
		warnp("close");
		goto err0;
	}

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}

/* Close the unneeded read or write end of the pipe. */
static int
ipc_sync_prep(struct ipc_sync * IS, int num)
{

	/* Bail if there's nothing to do. */
	if (IS->fd[num] == -1)
		return (0);

	/*
	 * Close the indicated end of pipe so that if the other process dies,
	 * we will notice the pipe being reset.
	 */
	if (ipc_sync_close(IS->fd[num]))
		goto err0;
	IS->fd[num] = -1;

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}

/**
 * ipc_sync_init(void):
 * Initialize an inter-process synchronization barrier.  This must be called
 * before forking.
 */
struct ipc_sync *
ipc_sync_init(void)
{
	struct ipc_sync * IS;

	/* Allocate the cookie. */
	if ((IS = malloc(sizeof(struct ipc_sync))) == NULL) {
		warnp("malloc");
		goto err0;
	}

	/* Set up synchronization. */
	if (pipe(IS->fd)) {
		warnp("pipe");
		goto err1;
	}

	/* Success! */
	return (IS);

err1:
	free(IS);
err0:
	/* Failure! */
	return (NULL);
}

/**
 * ipc_sync_wait(IS):
 * Block until ipc_sync_signal() has been called with ${IS}.
 */
int
ipc_sync_wait(struct ipc_sync * IS)
{

	/* Ensure that we've closed the write end. */
	if (ipc_sync_prep(IS, W))
		return (-1);

	/* Read and discard a byte from the read end of the pipe. */
	return (readbyte(IS->fd[R]));
}

/**
 * ipc_sync_signal(IS):
 * Indicate that ${IS} should no longer block.
 */
int
ipc_sync_signal(struct ipc_sync * IS)
{

	/* Ensure that we've closed the read end. */
	if (ipc_sync_prep(IS, R))
		return (-1);

	/* Write a byte to the write end of the pipe. */
	return (writebyte(IS->fd[W]));
}

/**
 * ipc_sync_wait_prep(IS):
 * Perform any operation(s) that would speed up an upcoming call to
 * ipc_sync_wait().  Calling this function is optional.
 */
int
ipc_sync_wait_prep(struct ipc_sync * IS)
{

	/* Ensure that we've closed the write end. */
	return (ipc_sync_prep(IS, W));
}

/**
 * ipc_sync_signal_prep(IS):
 * Perform any operation(s) that would speed up an upcoming call to
 * ipc_sync_signal().  Calling this function is optional.
 */
int
ipc_sync_signal_prep(struct ipc_sync * IS)
{

	/* Ensure that we've closed the read end. */
	return (ipc_sync_prep(IS, R));
}

/**
 * ipc_sync_done(IS):
 * Free resources associated with ${IS}.
 */
int
ipc_sync_done(struct ipc_sync * IS)
{

	/* Close any open file descriptors. */
	if ((IS->fd[W] != -1) && ipc_sync_close(IS->fd[W]))
		goto err1;
	if ((IS->fd[R] != -1) && ipc_sync_close(IS->fd[R]))
		goto err2;

	/* Clean up. */
	free(IS);

	/* Success! */
	return (0);

err2:
	if (IS->fd[R] != -1)
		ipc_sync_close(IS->fd[R]);
err1:
	free(IS);

	/* Failure! */
	return (-1);
}
