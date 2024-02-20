#include <sys/wait.h>

#include <stdint.h>
#include <unistd.h>

#include "warnp.h"

#include "fork_func.h"

/**
 * fork_func(func, cookie):
 * Fork and run ${func} in a new process, with ${cookie} as the sole argument.
 */
pid_t
fork_func(int (* func)(void *), void * cookie)
{
	pid_t pid;

	/* Fork */
	switch (pid = fork()) {
	case -1:
		/* Error in fork system call. */
		warnp("fork");
		goto err0;
	case 0:
		/* In child process: Run the provided function, then exit. */
		_exit(func(cookie));
	default:
		/* In parent process: do nothing else. */
		break;
	}

	/* Success! */
	return (pid);

err0:
	/* Failure! */
	return (-1);
}

/**
 * fork_func_wait(pid):
 * Wait for the process ${pid} to finish.  Print any error arising from ${pid}.
 * If ${pid} exited cleanly, return its exit code; otherwise, return -1.
 */
int
fork_func_wait(pid_t pid)
{
	int status;
	int rc = -1;

	/* Wait for the process to finish. */
	if (waitpid(pid, &status, 0) == -1) {
		warnp("waitpid");
		goto err0;
	}

	/* Print the error status, if applicable. */
	if (WIFEXITED(status)) {
		/* Child ${pid} exited cleanly. */
		rc = WEXITSTATUS(status);
	} else {
		/*
		 * Child ${pid} did not exit cleanly; warn about the reason
		 * for the unclean exit.
		 */
		if (WIFSIGNALED(status))
			warn0("pid %jd: terminated with signal %d",
			    (intmax_t)pid, WTERMSIG(status));
		else
			warn0("pid %jd: exited for an unknown reason",
			    (intmax_t)pid);
	}

err0:
	/* Done! */
	return (rc);
}
