#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "noeintr.h"
#include "warnp.h"

#include "daemonize.h"

/* Read and discard one byte from ${fd}, looping upon EINTR. */
static int
readbyte(int fd)
{
	char dummy;
	char done = 0;

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

/* Write the process' pid to a file called ${spid}. */
static int
writepid(const char * spid)
{
	FILE * f;

	/* Write our pid to a file. */
	if ((f = fopen(spid, "w")) == NULL) {
		warnp("fopen(%s)", spid);
		goto err0;
	}
	if (fprintf(f, "%d", getpid()) < 0) {
		warnp("fprintf");
		goto err0;
	}
	if (fclose(f)) {
		warnp("fclose");
		goto err0;
	}

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}

/**
 * daemonize(spid):
 * Daemonize and write the process ID in decimal to a file named ${spid}.
 * The parent process will exit only after the child has written its pid.
 * On success, the child will return 0; on failure, the parent will return
 * -1.
 */
int
daemonize(const char * spid)
{
	int fd[2];
	char dummy = 0;

	/*
	 * Create a pipe for the child to notify the parent when it has
	 * finished daemonizing.
	 */
	if (pipe(fd)) {
		warnp("pipe");
		goto err0;
	}

	/*
	 * Fork into the parent process (which waits for a poke and exits)
	 * and the child process (which keeps going).
	 */
	switch (fork()) {
	case -1:
		/* Fork failed. */
		warnp("fork");
		goto err2;
	case 0:
		/* In child process. */
		break;
	default:
		/*
		 * In parent process.  Close write end of pipe so that if the
		 * client dies we will notice the pipe being reset.
		 */
		while (close(fd[1])) {
			if (errno == EINTR)
				continue;
			warnp("close");
			goto err1;
		}

		/* Read and discard a byte from the read end of the pipe. */
		if (readbyte(fd[0]))
			goto err1;

		/*
		 * We don't need to read from this any more.  The pipe would
		 * be implicitly closed by the following _exit(), but we're
		 * explicitly closing it for the benefit of leak checkers.
		 */
		if (close(fd[0])) {
			warnp("close");
			goto err0;
		}

		/* We have been poked by the child.  Exit. */
		_exit(0);
	}

	/* Set ourselves to be a session leader. */
	if (setsid() == -1) {
		warnp("setsid");
		goto die;
	}

	/* Write out our pid file. */
	if (writepid(spid))
		goto die;

	/* Tell the parent to suicide. */
	if (noeintr_write(fd[1], &dummy, 1) == -1) {
		warnp("write");
		goto die;
	}

	/* Close the pipe. */
	while (close(fd[0])) {
		if (errno == EINTR)
			continue;
		warnp("close");
		goto die;
	}
	while (close(fd[1])) {
		if (errno == EINTR)
			continue;
		warnp("close");
		goto die;
	}

	/* Success! */
	return (0);

err2:
	if (close(fd[1]))
		warnp("close");
err1:
	if (close(fd[0]))
		warnp("close");
err0:
	/* Failure! */
	return (-1);

die:
	/*
	 * We're in the child and something bad happened; the parent will be
	 * notified when we die thanks to the pipe being closed.
	 */
	_exit(0);
}
