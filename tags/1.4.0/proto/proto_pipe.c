#include <sys/types.h>
#include <sys/socket.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "network.h"

#include "proto_crypt.h"

#include "proto_pipe.h"

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
	void * read_cookie;
	void * write_cookie;
	ssize_t wlen;
};

static int callback_pipe_read(void *, ssize_t);
static int callback_pipe_write(void *, ssize_t);

/**
 * proto_pipe(s_in, s_out, decr, k, status, callback, cookie):
 * Read bytes from ${s_in} and write them to ${s_out}.  If ${decr} is non-zero
 * then use ${k} to decrypt the bytes; otherwise use ${k} to encrypt them.
 * If EOF is read, set ${status} to 0, and if an error is encountered set
 * ${status} to -1; in either case, invoke ${callback}(${cookie}).  Return a
 * cookie which can be passed to proto_pipe_cancel.
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

	/* Start reading. */
	if (P->decr) {
		if ((P->read_cookie = network_read(P->s_in, P->ebuf,
		    PCRYPT_ESZ, PCRYPT_ESZ, callback_pipe_read, P)) == NULL)
			goto err1;
	} else {
		if ((P->read_cookie = network_read(P->s_in, P->dbuf,
		    PCRYPT_MAXDSZ, 1, callback_pipe_read, P)) == NULL)
			goto err1;
	}

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
callback_pipe_read(void * cookie, ssize_t len)
{
	struct pipe_cookie * P = cookie;

	/* This read is no longer in progress. */
	P->read_cookie = NULL;

	/* Did we read EOF? */
	if (len == 0)
		goto eof;

	/* Did the read fail? */
	if (len == -1)
		goto fail;

	/* Did a packet read end prematurely? */
	if ((P->decr) && (len < PCRYPT_ESZ))
		goto fail;

	/* Encrypt or decrypt the data. */
	if (P->decr) {
		if ((P->wlen = proto_crypt_dec(P->ebuf, P->dbuf, P->k)) == -1)
			goto fail;
	} else {
		proto_crypt_enc(P->dbuf, len, P->ebuf, P->k);
		P->wlen = PCRYPT_ESZ;
	}

	/* Write the encrypted or decrypted data. */
	if (P->decr) {
		if ((P->write_cookie = network_write(P->s_out, P->dbuf,
		    P->wlen, P->wlen, callback_pipe_write, P)) == NULL)
			goto err0;
	} else {
		if ((P->write_cookie = network_write(P->s_out, P->ebuf,
		    P->wlen, P->wlen, callback_pipe_write, P)) == NULL)
			goto err0;
	}

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
	if (P->decr) {
		if ((P->read_cookie = network_read(P->s_in, P->ebuf,
		    PCRYPT_ESZ, PCRYPT_ESZ, callback_pipe_read, P)) == NULL)
			goto err0;
	} else {
		if ((P->read_cookie = network_read(P->s_in, P->dbuf,
		    PCRYPT_MAXDSZ, 1, callback_pipe_read, P)) == NULL)
			goto err0;
	}

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
 * Shut down the pipe created by proto_pipe for which ${cookie} was returned.
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
