#include <sys/types.h>

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "events.h"
#include "network.h"

#include "netbuf.h"
#include "netbuf_ssl_internal.h"

/*
 * Set to NULL here; initialized by netbuf_ssl if SSL is being used.  This
 * allows us to avoid needing to link libssl into binaries which aren't
 * going to be using SSL.
 */
void * (* netbuf_read_ssl_func)(struct network_ssl_ctx *, uint8_t *, size_t,
    size_t, int (*)(void *, ssize_t), void *) = NULL;
void (* netbuf_read_ssl_cancel_func)(void *) = NULL;

/* Buffered reader structure. */
struct netbuf_read {
	/* Reader state. */
	int s;				/* Source socket for reads... */
	struct network_ssl_ctx * ssl;	/* ... unless we're using this. */
	int (* callback)(void *, int);	/* Callback for _wait. */
	void * cookie;			/* Cookie for _wait. */
	void * read_cookie;		/* From network_read. */
	void * immediate_cookie;	/* From events_immediate_register. */

	/* Buffer state. */
	uint8_t * buf;			/* Current read buffer. */
	size_t buflen;			/* Length of buf. */
	size_t bufpos;			/* Position of read pointer in buf. */
	size_t datalen;			/* Position of write pointer in buf. */
};

static int callback_success(void *);
static int callback_read(void *, ssize_t);

/**
 * netbuf_read_init(s):
 * Create and return a buffered reader attached to socket ${s}.  The caller
 * is responsible for ensuring that no attempts are made to read from said
 * socket except via the returned reader.
 */
struct netbuf_read *
netbuf_read_init(int s)
{

	/* Call the real function (without SSL). */
	return (netbuf_read_init2(s, NULL));
}

/**
 * netbuf_read_init2(s, ssl):
 * Behave like netbuf_read_init() if ${ssl} is NULL.  If the SSL context
 * ${ssl} is not NULL, use it and ignore ${s}.
 */
struct netbuf_read *
netbuf_read_init2(int s, struct network_ssl_ctx * ssl)
{
	struct netbuf_read * R;

	/* Bake a cookie. */
	if ((R = malloc(sizeof(struct netbuf_read))) == NULL)
		goto err0;
	R->s = s;
	R->ssl = ssl;
	R->read_cookie = NULL;
	R->immediate_cookie = NULL;

	/* Allocate buffer. */
	R->buflen = 4096;
	if ((R->buf = malloc(R->buflen)) == NULL)
		goto err1;
	R->bufpos = 0;
	R->datalen = 0;

	/* Success! */
	return (R);

err1:
	free(R);
err0:
	/* Failure! */
	return (NULL);
}

/**
 * netbuf_read_peek(R, data, datalen):
 * Set ${data} to point to the currently buffered data in the reader ${R}; set
 * ${datalen} to the number of bytes buffered.
 */
void
netbuf_read_peek(struct netbuf_read * R, uint8_t ** data, size_t * datalen)
{

	/* Point at current buffered data. */
	*data = &R->buf[R->bufpos];
	*datalen = R->datalen - R->bufpos;
}

/* Ensure that ${R} can store at least ${len} bytes. */
static int
netbuf_read_resize_buffer(struct netbuf_read * R, size_t len)
{
	uint8_t * nbuf;
	size_t nbuflen;

	/* Compute new buffer size. */
	nbuflen = R->buflen * 2;
	if (nbuflen < len)
		nbuflen = len;

	/* Allocate new buffer. */
	if ((nbuf = malloc(nbuflen)) == NULL)
		goto err0;

	/* Copy data into new buffer. */
	memcpy(nbuf, &R->buf[R->bufpos], R->datalen - R->bufpos);

	/* Free old buffer and use new buffer. */
	free(R->buf);
	R->buf = nbuf;
	R->buflen = nbuflen;
	R->datalen -= R->bufpos;
	R->bufpos = 0;

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}

/**
 * netbuf_read_wait(R, len, callback, cookie):
 * Wait until ${R} has ${len} or more bytes of data buffered or an error
 * occurs; then invoke ${callback}(${cookie}, status) with status set to 0
 * if the data is available, -1 on error, or 1 on EOF.
 */
int
netbuf_read_wait(struct netbuf_read * R, size_t len,
    int (* callback)(void *, int), void * cookie)
{

	/* Sanity-check: We shouldn't be reading already. */
	assert(R->read_cookie == NULL);
	assert(R->immediate_cookie == NULL);

	/* Record parameters for future reference. */
	R->callback = callback;
	R->cookie = cookie;

	/* If we have enough data already, schedule a callback. */
	if (R->datalen - R->bufpos >= len) {
		if ((R->immediate_cookie =
		    events_immediate_register(callback_success, R, 0)) == NULL)
			goto err0;
		else
			goto done;
	}

	/* Resize the buffer if needed. */
	if ((R->buflen < len) && netbuf_read_resize_buffer(R, len))
		goto err0;

	/* Move data to start of buffer if needed. */
	if (R->buflen - R->bufpos < len) {
		memmove(R->buf, &R->buf[R->bufpos], R->datalen - R->bufpos);
		R->datalen -= R->bufpos;
		R->bufpos = 0;
	}

	/* Read data into the buffer. */
	if (R->ssl) {
		if ((R->read_cookie = (netbuf_read_ssl_func)(R->ssl,
		    &R->buf[R->datalen], R->buflen - R->datalen,
		    R->bufpos + len - R->datalen, callback_read, R)) == NULL)
			goto err0;
	} else {
		if ((R->read_cookie = network_read(R->s, &R->buf[R->datalen],
		    R->buflen - R->datalen, R->bufpos + len - R->datalen,
		    callback_read, R)) == NULL)
			goto err0;
	}

done:
	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}

/* Perform immediate callback for netbuf_read_wait. */
static int
callback_success(void * cookie)
{
	struct netbuf_read * R = cookie;

	/* Sanity-check: We should be expecting this callback. */
	assert(R->immediate_cookie != NULL);

	/* This callback is no longer pending. */
	R->immediate_cookie = NULL;

	/* Perform callback. */
	return ((R->callback)(R->cookie, 0));
}

/* Callback for a completed network read. */
static int
callback_read(void * cookie, ssize_t lenread)
{
	struct netbuf_read * R = cookie;

	/* Sanity-check: We should be reading. */
	assert(R->read_cookie != NULL);

	/* This callback is no longer pending. */
	R->read_cookie = NULL;

	/* Did the read fail? */
	if (lenread < 0)
		goto failed;

	/* Did we hit EOF? */
	if (lenread == 0)
		goto eof;

	/* We've got more data. */
	R->datalen += (size_t)lenread;

	/* Perform callback. */
	return ((R->callback)(R->cookie, 0));

eof:
	/* Perform EOF callback. */
	return ((R->callback)(R->cookie, 1));

failed:
	/* Perform failure callback. */
	return ((R->callback)(R->cookie, -1));
}

/**
 * netbuf_read_wait_cancel(R):
 * Cancel any in-progress wait on the reader ${R}.  Do not invoke the callback
 * associated with the wait.
 */
void
netbuf_read_wait_cancel(struct netbuf_read * R)
{

	/* If we have an in-progress read, cancel it. */
	if (R->read_cookie != NULL) {
		if (R->ssl)
			(netbuf_read_ssl_cancel_func)(R->read_cookie);
		else
			network_read_cancel(R->read_cookie);
		R->read_cookie = NULL;
	}

	/* If we have an immediate callback pending, cancel it. */
	if (R->immediate_cookie != NULL) {
		events_immediate_cancel(R->immediate_cookie);
		R->immediate_cookie = NULL;
	}
}

/**
 * netbuf_read_consume(R, len):
 * Advance the reader pointer for the reader ${R} by ${len} bytes.
 */
void
netbuf_read_consume(struct netbuf_read * R, size_t len)
{

	/* Sanity-check: We can't consume data we don't have. */
	assert(R->datalen - R->bufpos >= len);

	/* Advance the buffer pointer. */
	R->bufpos += len;
}

/**
 * netbuf_read_free(R):
 * Free the reader ${R}.  Note that an indeterminate amount of data may have
 * been buffered and will be lost.
 */
void
netbuf_read_free(struct netbuf_read * R)
{

	/* Behave consistently with free(NULL). */
	if (R == NULL)
		return;

	/* Can't free a reader which is busy. */
	assert(R->read_cookie == NULL);
	assert(R->immediate_cookie == NULL);

	/* Free the buffer and the reader. */
	free(R->buf);
	free(R);
}
