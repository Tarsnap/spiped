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

#include "fd_drain.h"
#include "standalone.h"

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

struct eof_testinfo {
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

static int
pipe_callback_eof_test(void * cookie)
{
	struct eof_testinfo * info = cookie;

	/* We've reached EOF or an error. */
	info->done = 1;

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

static int
pipe_decr_eof_case(size_t complete_frames, size_t tail_len,
    int expected_status)
{
	struct eof_testinfo info;
	struct proto_keys * enc = NULL;
	struct proto_keys * dec = NULL;
	void * pipe = NULL;
	uint8_t kbuf[64];
	uint8_t inbuf[PCRYPT_MAXDSZ];
	uint8_t encbuf[PCRYPT_ESZ];
	int in[2] = {-1, -1};
	int out[2] = {-1, -1};
	size_t i;

	/* Set up matching encryption and decryption keys. */
	memset(kbuf, 0, sizeof(kbuf));
	memset(inbuf, 0, sizeof(inbuf));
	if ((enc = mkkeypair(kbuf)) == NULL)
		goto err0;
	if ((dec = mkkeypair(kbuf)) == NULL)
		goto err0;

	/* Create socket pairs for the encrypted input and cleartext output. */
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, in)) {
		warnp("socketpair");
		goto err0;
	}
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, out)) {
		warnp("socketpair");
		goto err0;
	}

	/* Start a decrypting pipe. */
	info.status = 1;
	info.done = 0;
	if ((pipe = proto_pipe(in[R], out[W], 1, dec, &info.status,
	    pipe_callback_eof_test, &info)) == NULL) {
		warn0("proto_pipe");
		goto err0;
	}

	/* Send any complete frames first. */
	for (i = 0; i < complete_frames; i++) {
		proto_crypt_enc(inbuf, sizeof(inbuf), encbuf, enc);
		if (noeintr_write(in[W], encbuf, sizeof(encbuf))
		    != (ssize_t)sizeof(encbuf)) {
			warnp("write");
			goto err0;
		}
	}

	/* Send a truncated final frame if requested. */
	if (tail_len != 0) {
		proto_crypt_enc(inbuf, sizeof(inbuf), encbuf, enc);
		if (noeintr_write(in[W], encbuf, tail_len) != (ssize_t)tail_len) {
			warnp("write");
			goto err0;
		}
	}

	/* End the encrypted input and let the pipe report its final status. */
	if (shutdown(in[W], SHUT_WR)) {
		warnp("shutdown");
		goto err0;
	}
	if (events_spin(&info.done)) {
		warnp("events_spin");
		goto err0;
	}
	if (info.status != expected_status) {
		warn0("proto_pipe EOF status: got %d, expected %d",
		    info.status, expected_status);
		goto err0;
	}

	/* Clean up. */
	proto_pipe_cancel(pipe);
	proto_crypt_free(enc);
	proto_crypt_free(dec);
	(void)close(in[R]);
	(void)close(in[W]);
	(void)close(out[R]);
	(void)close(out[W]);

	/* Success! */
	return (0);

err0:
	if (pipe != NULL)
		proto_pipe_cancel(pipe);
	proto_crypt_free(enc);
	proto_crypt_free(dec);
	if (in[R] != -1)
		(void)close(in[R]);
	if (in[W] != -1)
		(void)close(in[W]);
	if (out[R] != -1)
		(void)close(out[R]);
	if (out[W] != -1)
		(void)close(out[W]);
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

/**
 * standalone_pipe_truncated_frame():
 * Verify that decrypt-side EOF is accepted only on an encrypted packet
 * boundary.
 */
int
standalone_pipe_truncated_frame(void)
{

	/* Clean EOF before any encrypted data is valid. */
	if (pipe_decr_eof_case(0, 0, 0))
		goto err0;

	/* EOF after a complete encrypted frame is valid. */
	if (pipe_decr_eof_case(1, 0, 0))
		goto err0;

	/* Any partial encrypted frame at EOF is a connection error. */
	if (pipe_decr_eof_case(0, 1, -1))
		goto err0;
	if (pipe_decr_eof_case(1, PCRYPT_ESZ - 1, -1))
		goto err0;

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (1);
}
