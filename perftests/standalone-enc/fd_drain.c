#include <stdint.h>
#include <unistd.h>

#include "fork_func.h"
#include "warnp.h"

#include "fd_drain.h"

/**
 * fd_drain(fd_p):
 * Drain bytes from the file descriptor ${*fd_p} -- passed as an (int *) -- as
 * quickly as possible.
 */
int
fd_drain(void * cookie)
{
	int fd = *((int *)cookie);
	uint8_t mybuf[FD_DRAIN_MAX_SIZE];
	ssize_t readlen;

	/* Loop until we hit EOF. */
	do {
		/*
		 * This will almost always read 1060 bytes (size of an
		 * encrypted packet, PCRYPT_ESZ in proto_crypt.h), but it's
		 * not impossible to have more than that.
		 */
		if ((readlen = read(fd, mybuf, FD_DRAIN_MAX_SIZE)) == -1) {
			warnp("read");
			goto err0;
		}
	} while (readlen != 0);

	/* Success! */
	return (0);

err0:
	/* Failure!  This value will be the pid's exit code. */
	return (1);
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
	if ((pid = fork_func(fd_drain, &fd)) == -1)
		goto err0;

	/* Success! */
	return (pid);

err0:
	/* Failure! */
	return (-1);
}
