#include <sys/socket.h>

#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "events.h"
#include "noeintr.h"
#include "perftest.h"
#include "proto_crypt.h"
#include "proto_pipe.h"
#include "pthread_create_blocking_np.h"
#include "warnp.h"

#include "standalone.h"

/* The smallest this can be is PCRYPT_ESZ (which is 1060). */
#define MAXOUTSIZE 16384

/* Cookie for proto_pipe */
struct pipe {
	struct proto_keys * k;
	void * cancel_cookie;
	pthread_t enc_thr;
	pthread_t output_thr;
	int in[2];
	int out[2];
	int status;
	int done;
};

static int
pipe_callback_status(void * cookie)
{
	struct pipe * pipe = cookie;

	/* Was there an error? */
	if (pipe->status) {
		warn0("proto_pipe callback status: %i", pipe->status);
		return (-1);
	}

	/* We've finished. */
	pipe->done = 1;

	/* Success! */
	return (0);
}

/* Encrypt bytes sent to a socket, and send them to another socket. */
static void *
pipe_enc_thread(void * cookie)
{
	struct pipe * pipe = cookie;

	/* Create the pipe. */
	if ((pipe->cancel_cookie = proto_pipe(pipe->in[1], pipe->out[0], 0,
	    pipe->k, &pipe->status, pipe_callback_status, pipe)) == NULL)
		warn0("proto_pipe");

	/* Let events happen. */
	if (events_spin(&pipe->done))
		warnp("events_spin");

	/* Finished! */
	return (NULL);
}

/* Drain bytes from pipe->out[1] as quickly as possible. */
static void *
pipe_output_thread(void * cookie)
{
	struct pipe * pipe = cookie;
	uint8_t mybuf[MAXOUTSIZE];
	ssize_t readlen;

	/* Loop until we hit EOF. */
	do {
		/*
		 * This will almost always read 1060 bytes (size of an
		 * encrypted packet, PCRYPT_ESZ in proto_crypt.h), but it's
		 * not impossible to have more than that.
		 */
		if ((readlen = read(pipe->out[1], mybuf, MAXOUTSIZE)) == -1) {
			warnp("read");
			return (NULL);
		}
	} while (readlen != 0);

	/* Success! */
	return (NULL);
}

static int
pipe_init(void * cookie, uint8_t * buf, size_t buflen)
{
	struct pipe * pipe = cookie;
	uint8_t kbuf[64];
	size_t i;
	int rc;

	/* Sanity check for pipe_output_thread(). */
	assert(buflen <= MAXOUTSIZE);

	/* Set up encryption key. */
	memset(kbuf, 0, 64);
	if ((pipe->k = mkkeypair(kbuf)) == NULL)
		goto err0;

	/* Create socket pairs for the input and output. */
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, pipe->in)) {
		warnp("socketpair");
		goto err0;
	}
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, pipe->out)) {
		warnp("socketpair");
		goto err0;
	}

	/* Set the input. */
	for (i = 0; i < buflen; i++)
		buf[i] = (uint8_t)(i & 0xff);

	/* We haven't finished the event loop. */
	pipe->done = 0;

	/* Create the pipe threads. */
	if ((rc = pthread_create_blocking_np(&pipe->output_thr, NULL,
	    pipe_output_thread, pipe))) {
		warn0("pthread_create: %s", strerror(rc));
		goto err0;
	}
	if ((rc = pthread_create_blocking_np(&pipe->enc_thr, NULL,
	    pipe_enc_thread, pipe))) {
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
pipe_func(void * cookie, uint8_t * buf, size_t buflen, size_t nreps)
{
	struct pipe * pipe = cookie;
	size_t i;
	int rc;

	/* Send bytes. */
	for (i = 0; i < nreps; i++) {
		if (noeintr_write(pipe->in[0], buf, buflen)
		    != (ssize_t)buflen) {
			warnp("network_write");
			goto err0;
		}
	}

	/* We've finished writing stuff. */
	if (shutdown(pipe->in[0], SHUT_WR)) {
		warnp("close");
		goto err0;
	}

	/* Wait for threads to finish. */
	if ((rc = pthread_join(pipe->output_thr, NULL))) {
		warn0("pthread_join: %s", strerror(rc));
		goto err0;
	}
	if ((rc = pthread_join(pipe->enc_thr, NULL))) {
		warn0("pthread_join: %s", strerror(rc));
		goto err0;
	}

	/* Clean up. */
	if (close(pipe->out[0])) {
		warnp("close");
		goto err0;
	}

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}

static int
pipe_cleanup(void * cookie)
{
	struct pipe * pipe = cookie;

	/* Clean up. */
	proto_pipe_cancel(pipe->cancel_cookie);
	proto_crypt_free(pipe->k);

	/* Success! */
	return (0);
}

/**
 * pipe_perftest(perfsizes, num_perf, nbytes_perftest, nbytes_warmup):
 * Performance test for one proto_pipe().
 */
int
pipe_perftest(const size_t * perfsizes, size_t num_perf,
    size_t nbytes_perftest, size_t nbytes_warmup)
{
	struct pipe pipe_actual;

	/* Report what we're doing. */
	printf("Testing proto_pipe()\n");

	/* Time the function. */
	if (perftest_buffers(nbytes_perftest, perfsizes, num_perf,
	    nbytes_warmup, 0, pipe_init, pipe_func, pipe_cleanup,
	    &pipe_actual)) {
		warn0("perftest_buffers");
		goto err0;
	}

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (1);
}
