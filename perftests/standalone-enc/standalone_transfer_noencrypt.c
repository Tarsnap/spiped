#include <sys/socket.h>

#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "noeintr.h"
#include "perftest.h"
#include "pthread_create_blocking_np.h"
#include "thread_sync.h"
#include "warnp.h"

#include "standalone.h"

/* The smallest this can be is PCRYPT_ESZ (which is 1060). */
#define MAXOUTSIZE 16384

/* Ends of pipes or sockets. */
#define W 1
#define R 0

/* Cookie for proto_pipe */
struct shared {
	pthread_t data_thr;
	pthread_t output_thr;
	int in[2];
	int out[2];
	struct thread_sync * finished;
	int method;
};

/* Transfer bytes from ->in to ->out. */
static void *
transfer_data(void * cookie)
{
	struct shared * shared = cookie;
	uint8_t mybuf[MAXOUTSIZE];
	ssize_t readlen;

	/* Loop until we hit EOF. */
	do {
		/* Get data from ->in, and break on EOF. */
		if ((readlen = read(shared->in[R], mybuf, MAXOUTSIZE)) == -1) {
			warnp("read");
			goto err0;
		}
		if (readlen == 0)
			break;

		/* Send data to ->out. */
		if (noeintr_write(shared->out[W], mybuf, (size_t)readlen)
		    != readlen) {
			warnp("network_write");
			goto err0;
		}
	} while (1);

	/* When ->in received an EOF, close ->out. */
	if (close(shared->out[W])) {
		warnp("close");
		goto err0;
	}

err0:
	/* Finished! */
	return (NULL);
}

/* Drain bytes from ->out as quickly as possible. */
static void *
drain_output(void * cookie)
{
	struct shared * shared = cookie;
	uint8_t mybuf[MAXOUTSIZE];
	ssize_t readlen;

	/* Loop until we hit EOF. */
	do {
		/* Read from ->out. */
		if ((readlen = read(shared->out[R], mybuf, MAXOUTSIZE)) == -1) {
			warnp("read");
			goto err0;
		}
	} while (readlen != 0);

	/* Notify that we've finished. */
	if (thread_sync_signal(shared->finished))
		goto err0;

err0:
	/* Finished! */
	return (NULL);
}

static int
perftest_init(void * cookie, uint8_t * buf, size_t buflen)
{
	struct shared * shared = cookie;
	size_t i;
	int rc;

	/* Sanity checks. */
	assert(buflen <= MAXOUTSIZE);

	/* Set up thread_sync. */
	if ((shared->finished = thread_sync_init()) == NULL)
		goto err0;

	/* Create communication endpoints. */
	if (shared->method == 0) {
		/* Use pipes. */
		if (pipe(shared->in)) {
			warnp("pipe");
			goto err0;
		}
		if (pipe(shared->out)) {
			warnp("pipe");
			goto err0;
		}
	} else if (shared->method == 1) {
		/* Use socketpairs. */
		if (socketpair(AF_UNIX, SOCK_STREAM, 0, shared->in)) {
			warnp("socketpair");
			goto err0;
		}
		if (socketpair(AF_UNIX, SOCK_STREAM, 0, shared->out)) {
			warnp("socketpair");
			goto err0;
		}
	} else
		goto err0;

	/* Set the input. */
	for (i = 0; i < buflen; i++)
		buf[i] = (uint8_t)(i & 0xff);

	/* Create the threads. */
	if ((rc = pthread_create_blocking_np(&shared->output_thr, NULL,
	    drain_output, shared))) {
		warn0("pthread_create: %s", strerror(rc));
		goto err0;
	}
	if ((rc = pthread_create_blocking_np(&shared->data_thr, NULL,
	    transfer_data, shared))) {
		warn0("pthread_create: %s", strerror(rc));
		goto err0;
	}

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}

static int
perftest_func(void * cookie, uint8_t * buf, size_t buflen, size_t nreps)
{
	struct shared * shared = cookie;
	size_t i;

	/* Send bytes. */
	for (i = 0; i < nreps; i++) {
		if (noeintr_write(shared->in[W], buf, buflen)
		    != (ssize_t)buflen) {
			warnp("network_write");
			goto err0;
		}
	}

	/* We've finished sending data. */
	if (close(shared->in[W])) {
		warnp("close");
		goto err0;
	}

	/* Wait until transfer_data has finished. */
	if (thread_sync_wait(shared->finished))
		goto err0;

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}

static int
perftest_cleanup(void * cookie)
{
	struct shared * shared = cookie;
	int rc;

	/* Wait for threads to finish. */
	if ((rc = pthread_join(shared->data_thr, NULL))) {
		warn0("pthread_join: %s", strerror(rc));
		goto err0;
	}
	if ((rc = pthread_join(shared->output_thr, NULL))) {
		warn0("pthread_join: %s", strerror(rc));
		goto err0;
	}

	/* Clean up communication. */
	if (close(shared->out[R])) {
		warnp("close");
		goto err0;
	}
	if (close(shared->in[R])) {
		warnp("close");
		goto err0;
	}
	if (thread_sync_done(shared->finished))
		goto err0;

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}

/**
 * standalone_transfer_noencrypt(perfsizes, num_perf, nbytes_perftest,
 *     nbytes_warmup, method):
 * Performance test for sending data through two communication endpoints with
 * no encryption.  If ${method} is 0, use two pipes; if ${method} is 1, use
 * two socketpairs.
 */
int
standalone_transfer_noencrypt(const size_t * perfsizes, size_t num_perf,
    size_t nbytes_perftest, size_t nbytes_warmup, int method)
{
	struct shared shared_actual;
	const char * method_str;

	/* Report what we're doing. */
	if (method == 0)
		method_str = "two pipes";
	else if (method == 1)
		method_str = "two socketpairs";
	else {
		warn0("method not recognized");
		goto err0;
	}
	printf("Testing sending data over %s, no encryption\n", method_str);

	/* Record the communication method. */
	shared_actual.method = method;

	/* Time the function. */
	if (perftest_buffers(nbytes_perftest, perfsizes, num_perf,
	    nbytes_warmup, 0, perftest_init, perftest_func, perftest_cleanup,
	    &shared_actual)) {
		warn0("perftest_buffers");
		goto err0;
	}

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (1);
}
