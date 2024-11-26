#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "ipc_sync.h"
#include "warnp.h"

#include "daemonize.h"

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
	if (fprintf(f, "%jd", (intmax_t)getpid()) < 0) {
		warnp("fprintf");
		goto err1;
	}
	if (fclose(f)) {
		warnp("fclose");
		goto err0;
	}

	/* Success! */
	return (0);

err1:
	if (fclose(f))
		warnp("fclose");
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
	struct ipc_sync * IS;

	/*
	 * Create a pipe for the child to notify the parent when it has
	 * finished daemonizing.
	 */
	if ((IS = ipc_sync_init()) == NULL)
		goto err0;

	/*
	 * Fork into the parent process (which waits for a poke and exits)
	 * and the child process (which keeps going).
	 */
	switch (fork()) {
	case -1:
		/* Fork failed. */
		warnp("fork");
		goto err1;
	case 0:
		/* In child process. */
		break;
	default:
		/* In parent process; wait for the child to start. */
		if (ipc_sync_wait(IS))
			goto err1;

		/*
		 * Free resources associated with ${IS}.  These would be
		 * released by the following _exit(), but we're explicitly
		 * releasing them for the benefit of leak checkers.
		 */
		if (ipc_sync_done(IS))
			goto err0;

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

	/* Tell the parent that we've started. */
	if (ipc_sync_signal(IS))
		goto die;

	/* Clean up. */
	if (ipc_sync_done(IS))
		goto die;

	/* Success! */
	return (0);

err1:
	ipc_sync_done(IS);
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
