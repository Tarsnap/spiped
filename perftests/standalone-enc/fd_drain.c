#include <stdint.h>
#include <unistd.h>

#include "fork_func.h"
#include "warnp.h"

#include "fd_drain.h"

#define FD_DRAIN_MAX_SIZE 16384

/**
 * fd_drain_internal(fd_p):
 * Drain bytes from the file descriptor ${*fd_p} -- passed as an (int *) -- as
 * quickly as possible.
 */
static int
fd_drain_internal(void * cookie)
{
	int fd = *((int *)cookie);
	uint8_t mybuf[FD_DRAIN_MAX_SIZE];
	ssize_t readlen;

	/* Loop until we hit EOF or an error. */
	do {
		readlen = read(fd, mybuf, FD_DRAIN_MAX_SIZE);
	} while (readlen > 0);

	/* Check for an error. */
	if (readlen == -1) {
		warnp("read");
		goto err0;
	}

	/* Success! */
	return (0);

err0:
	/* Failure!  This value will be the pid's exit code. */
	return (1);
}

/**
 * fd_drain(fd):
 * Drain bytes from the file descriptor ${fd} as quickly as possible.
 */
int
fd_drain(int fd)
{

	/* Call internal function. */
	return (fd_drain_internal(&fd));
}

/**
 * fd_drain_fork(fd):
 * Create a new process to drain bytes from the file descriptor ${fd} until it
 * receives an EOF.  Return the process ID of the new process, or -1 upon
 * error.
 */
pid_t
fd_drain_fork(int fd)
{
	pid_t pid;

	/* Fork a new process to run the function. */
	if ((pid = fork_func(fd_drain_internal, &fd)) == -1)
		goto err0;

	/* Success! */
	return (pid);

err0:
	/* Failure! */
	return (-1);
}
