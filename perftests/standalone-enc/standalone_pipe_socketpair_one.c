#include <sys/socket.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "events.h"
#include "fork_func.h"
#include "noeintr.h"
#include "perftest.h"
#include "proto_crypt.h"
#include "proto_pipe.h"
#include "warnp.h"

#include "standalone.h"

/* The smallest this can be is PCRYPT_ESZ (which is 1060). */
#define FD_DRAIN_MAX_SIZE 16384

/* Ends of socketpairs (convention, not a firm requirement). */
#define R 0
#define W 1

/* Cookie for proto_pipe */
struct pipeinfo {
	struct proto_keys * k;
	pid_t out_pid;
	pid_t enc_pid;
	int in[2];
	int out[2];
	int status;
	int done;
};

static int
pipe_callback_status(void * cookie)
{
	struct pipeinfo * pipeinfo = cookie;

	/* Was there an error? */
	if (pipeinfo->status) {
		warn0("proto_pipe callback status: %d", pipeinfo->status);
		return (-1);
	}

	/* We've finished. */
	pipeinfo->done = 1;

	/* Success! */
	return (0);
}

/* Encrypt bytes sent to a socket, and send them to another socket. */
static int
pipe_enc(void * cookie)
{
	struct pipeinfo * pipeinfo = cookie;
	void * cancel_cookie;

	/* Create the pipe. */
	if ((cancel_cookie = proto_pipe(pipeinfo->in[R], pipeinfo->out[W], 0,
	    pipeinfo->k, &pipeinfo->status, pipe_callback_status, pipeinfo))
	    == NULL) {
		warn0("proto_pipe");
		goto err0;
	}

	/* Let events happen. */
	if (events_spin(&pipeinfo->done))
		warnp("events_spin");

	/* Clean up the pipe. */
	proto_pipe_cancel(cancel_cookie);

	/* Success! */
	return (0);

err0:
	/* Failure!  This value will be the pid's exit code. */
	return (1);
}

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
 * fd_drain_fork(fd):
 * Create a new process to drain bytes from the file descriptor ${fd} until it
 * receives an EOF.  Return the process ID of the new process, or -1 upon
 * error.
 */
static pid_t
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

static int
pipe_init(void * cookie, uint8_t * buf, size_t buflen)
{
	struct pipeinfo * pipeinfo = cookie;
	uint8_t kbuf[64];
	size_t i;

	/* Set up encryption key. */
	memset(kbuf, 0, 64);
	if ((pipeinfo->k = mkkeypair(kbuf)) == NULL)
		goto err0;

	/* Create socket pairs for the input and output. */
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, pipeinfo->in)) {
		warnp("socketpair");
		goto err0;
	}
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, pipeinfo->out)) {
		warnp("socketpair");
		goto err0;
	}

	/* Set the input. */
	for (i = 0; i < buflen; i++)
		buf[i] = (uint8_t)(i & 0xff);

	/* We haven't finished the event loop. */
	pipeinfo->done = 0;

	/* Create the pipe processes. */
	if ((pipeinfo->out_pid = fd_drain_fork(pipeinfo->out[R])) == -1)
		goto err0;
	if ((pipeinfo->enc_pid = fork_func(pipe_enc, pipeinfo)) == -1)
		goto err0;

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}

static int
pipe_func(void * cookie, uint8_t * buf, size_t buflen, size_t nreps)
{
	struct pipeinfo * pipeinfo = cookie;
	size_t i;

	/* Send bytes. */
	for (i = 0; i < nreps; i++) {
		if (noeintr_write(pipeinfo->in[W], buf, buflen)
		    != (ssize_t)buflen) {
			warnp("network_write");
			goto err0;
		}
	}

	/* We've finished writing stuff. */
	if (shutdown(pipeinfo->in[W], SHUT_WR)) {
		warnp("shutdown");
		goto err0;
	}

	/* Wait for the processes to finish. */
	if (fork_func_wait(pipeinfo->enc_pid))
		goto err0;
	if (fork_func_wait(pipeinfo->out_pid))
		goto err0;

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}

static int
pipe_cleanup(void * cookie)
{
	struct pipeinfo * pipeinfo = cookie;

	/* Clean up encryption key. */
	proto_crypt_free(pipeinfo->k);

	/* Clean up sockets. */
	if (close(pipeinfo->in[W])) {
		warnp("close");
		goto err0;
	}
	if (close(pipeinfo->in[R])) {
		warnp("close");
		goto err0;
	}
	if (close(pipeinfo->out[W])) {
		warnp("close");
		goto err0;
	}
	if (close(pipeinfo->out[R])) {
		warnp("close");
		goto err0;
	}

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}

/**
 * standalone_pipe_socketpair_one(perfsizes, num_perf, nbytes_perftest,
 *     nbytes_warmup):
 * Performance test for one proto_pipe() over a socketpair.
 */
int
standalone_pipe_socketpair_one(const size_t * perfsizes, size_t num_perf,
    size_t nbytes_perftest, size_t nbytes_warmup)
{
	struct pipeinfo pipeinfo_actual;

	/* Report what we're doing. */
	printf("Testing one proto_pipe() over a socketpair\n");

	/* Time the function. */
	if (perftest_buffers(nbytes_perftest, perfsizes, num_perf,
	    nbytes_warmup, 0, pipe_init, pipe_func, pipe_cleanup,
	    &pipeinfo_actual)) {
		warn0("perftest_buffers");
		goto err0;
	}

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (1);
}
