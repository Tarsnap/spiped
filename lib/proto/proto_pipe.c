#include <sys/types.h>
#include <sys/socket.h>

#include <stdint.h>
#include <stdlib.h>

#include "network.h"

#include "proto_crypt.h"

#include "proto_pipe.h"

/* Maximum size of data to output in a single callback_pipe_read() call. */
#define OUTBUFSIZE (8 * PCRYPT_ESZ)

struct pipe_cookie {
	int (* callback)(void *);
	void * cookie;
	int * status;
	int s_in;
	int s_out;
	int decr;
	struct proto_keys * k;
	uint8_t dbuf[PCRYPT_MAXDSZ];
	uint8_t ebuf[PCRYPT_ESZ];
	uint8_t * inbuf;
	uint8_t outbuf[OUTBUFSIZE];
	void * read_cookie;
	void * write_cookie;
	ssize_t wlen;
	size_t minread;
	size_t full_buflen;
};

static int callback_pipe_read(void *, ssize_t);
static int callback_pipe_write(void *, ssize_t);

/**
 * proto_pipe(s_in, s_out, decr, k, status, callback, cookie):
 * Read bytes from ${s_in} and write them to ${s_out}.  If ${decr} is non-zero
 * then use ${k} to decrypt the bytes; otherwise use ${k} to encrypt them.
 * If EOF is read, set ${status} to 0, and if an error is encountered set
 * ${status} to -1; in either case, invoke ${callback}(${cookie}).  Return a
 * cookie which can be passed to proto_pipe_cancel().
 */
void *
proto_pipe(int s_in, int s_out, int decr, struct proto_keys * k,
    int * status, int (* callback)(void *), void * cookie)
{
	struct pipe_cookie * P;

	/* Bake a cookie. */
	if ((P = malloc(sizeof(struct pipe_cookie))) == NULL)
		goto err0;
	P->callback = callback;
	P->cookie = cookie;
	P->status = status;
	P->s_in = s_in;
	P->s_out = s_out;
	P->decr = decr;
	P->k = k;
	P->read_cookie = NULL;
	P->write_cookie = NULL;

	/* Set the minimum number of bytes to read. */
	P->minread = P->decr ? PCRYPT_ESZ : 1;

	/* Set the number of bytes in a full buffer. */
	P->full_buflen = P->decr ? PCRYPT_ESZ : PCRYPT_MAXDSZ;

	/* Set up input pointer. */
	P->inbuf = P->decr ? P->ebuf : P->dbuf;

	/* Start reading. */
	if ((P->read_cookie = network_read(P->s_in, P->inbuf, P->full_buflen,
	    P->minread, callback_pipe_read, P)) == NULL)
		goto err1;

	/* Success! */
	return (P);

err1:
	free(P);
err0:
	/* Failure! */
	return (NULL);
}

/* Some data has been read. */
static int
callback_pipe_read(void * cookie, ssize_t slen)
{
	struct pipe_cookie * P = cookie;
	uint8_t * inbuf;
	size_t inlen;
	size_t inpos = 0;
	size_t outpos = 0;
	size_t loop_inlen;
	ssize_t loop_outlen;

	/* This read is no longer in progress. */
	P->read_cookie = NULL;

	/* Did we read EOF? */
	if (slen == 0)
		goto eof;

	/* Did the read fail? */
	if (slen == -1)
		goto fail;

	/* Sanity check and convert length. */
	if (slen < 0)
		goto fail;
	inlen = (size_t)slen;

	/* Get data. */
	inbuf = &P->inbuf[inpos];

	/* Process as many packets as possible. */
	while (inlen > 0) {
		/* Stop processing if we don't have space for more output. */
		if (outpos + P->full_buflen > OUTBUFSIZE)
			break;

		/* How many bytes should we process this time? */
		loop_inlen = (inlen > P->full_buflen) ? P->full_buflen : inlen;

		/*
		 * If we don't have enough data to decrypt, leave it until the
		 * next time callback_pipe_read() is called.
		 */
		if ((P->decr) && (loop_inlen < PCRYPT_ESZ))
			break;

		/* Encrypt or decrypt the data. */
		if (P->decr) {
			if ((loop_outlen = proto_crypt_dec(&inbuf[inpos],
			    &P->outbuf[outpos], P->k)) == -1)
				goto fail;
		} else {
			proto_crypt_enc(&inbuf[inpos], loop_inlen,
			    &P->outbuf[outpos], P->k);
			loop_outlen = PCRYPT_ESZ;
		}

		/* We've processed this data. */
		inlen -= loop_inlen;
		inpos += loop_inlen;
		outpos += (size_t)loop_outlen;
	}

	/* Write the encrypted or decrypted data. */
	P->wlen = (ssize_t)outpos;
	if ((P->write_cookie = network_write(P->s_out, P->outbuf,
	    (size_t)P->wlen, (size_t)P->wlen, callback_pipe_write,
	    P)) == NULL)
		goto err0;

	/* Success! */
	return (0);

fail:
	/* Record that this connection is broken. */
	*(P->status) = -1;

	/* Inform the upstream that our status has changed. */
	return ((P->callback)(P->cookie));

eof:
	/* We aren't going to write any more. */
	shutdown(P->s_out, SHUT_WR);

	/* Record that we have reached EOF. */
	*(P->status) = 0;

	/* Inform the upstream that our status has changed. */
	return ((P->callback)(P->cookie));

err0:
	/* Failure! */
	return (-1);
}

static int
callback_pipe_write(void * cookie, ssize_t len)
{
	struct pipe_cookie * P = cookie;

	/* This write is no longer in progress. */
	P->write_cookie = NULL;

	/* Did we fail to write everything? */
	if (len < P->wlen)
		goto fail;

	/* Launch another read. */
	if ((P->read_cookie = network_read(P->s_in, P->inbuf, P->full_buflen,
	    P->minread, callback_pipe_read, P)) == NULL)
		goto err0;

	/* Success! */
	return (0);

fail:
	/* Record that this connection is broken. */
	*(P->status) = -1;

	/* Inform the upstream that our status has changed. */
	return ((P->callback)(P->cookie));

err0:
	/* Failure! */
	return (-1);
}

/**
 * proto_pipe_cancel(cookie):
 * Shut down the pipe created by proto_pipe() for which ${cookie} was returned.
 */
void
proto_pipe_cancel(void * cookie)
{
	struct pipe_cookie * P = cookie;

	/* If a read or write is in progress, cancel it. */
	if (P->read_cookie)
		network_read_cancel(P->read_cookie);
	if (P->write_cookie)
		network_write_cancel(P->write_cookie);

	/* Free the cookie. */
	free(P);
}
