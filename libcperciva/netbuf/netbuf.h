#ifndef NETBUF_H_
#define NETBUF_H_

#include <stddef.h>
#include <stdint.h>

/* Opaque types. */
struct network_ssl_ctx;
struct netbuf_read;
struct netbuf_write;

/**
 * netbuf_read_init(s):
 * Create and return a buffered reader attached to socket ${s}.  The caller
 * is responsible for ensuring that no attempts are made to read from said
 * socket except via the returned reader.
 */
struct netbuf_read * netbuf_read_init(int);

/**
 * netbuf_read_peek(R, data, datalen):
 * Set ${data} to point to the currently buffered data in the reader ${R}; set
 * ${datalen} to the number of bytes buffered.
 */
void netbuf_read_peek(struct netbuf_read *, uint8_t **, size_t *);

/**
 * netbuf_read_wait(R, len, callback, cookie):
 * Wait until ${R} has ${len} or more bytes of data buffered or an error
 * occurs; then invoke ${callback}(${cookie}, status) with status set to 0
 * if the data is available, -1 on error, or 1 on EOF.
 */
int netbuf_read_wait(struct netbuf_read *, size_t,
    int (*)(void *, int), void *);

/**
 * netbuf_read_wait_cancel(R):
 * Cancel any in-progress wait on the reader ${R}.  Do not invoke the callback
 * associated with the wait.
 */
void netbuf_read_wait_cancel(struct netbuf_read *);

/**
 * netbuf_read_consume(R, len):
 * Advance the reader pointer for the reader ${R} by ${len} bytes.
 */
void netbuf_read_consume(struct netbuf_read *, size_t);

/**
 * netbuf_read_free(R):
 * Free the reader ${R}.  Note that an indeterminate amount of data may have
 * been buffered and will be lost.
 */
void netbuf_read_free(struct netbuf_read *);

/**
 * netbuf_write_init(s, fail_callback, fail_cookie):
 * Create and return a buffered writer attached to socket ${s}.  The caller
 * is responsible for ensuring that no attempts are made to write to said
 * socket except via the returned writer until netbuf_write_free() is called.
 * If a write fails, ${fail_callback} will be invoked with the parameter
 * ${fail_cookie}.
 */
struct netbuf_write * netbuf_write_init(int, int (*)(void *), void *);

/**
 * netbuf_write_reserve(W, len):
 * Reserve ${len} bytes of space in the buffered writer ${W} and return a
 * pointer to the buffer.  This operation must be followed by a call to
 * netbuf_write_consume() before the next call to _reserve() or _write() and
 * before a callback could be made into netbuf_write() (i.e., before control
 * returns to the event loop).
 */
uint8_t * netbuf_write_reserve(struct netbuf_write *, size_t);

/**
 * netbuf_write_consume(W, len):
 * Consume a reservation previously made by netbuf_write_reserve(); the value
 * ${len} must be <= the value passed to netbuf_write_reserve().
 */
int netbuf_write_consume(struct netbuf_write *, size_t);

/**
 * netbuf_write_write(W, buf, buflen):
 * Write ${buflen} bytes from the buffer ${buf} via the buffered writer ${W}.
 */
int netbuf_write_write(struct netbuf_write *, const uint8_t *, size_t);

/**
 * netbuf_write_free(W):
 * Free the writer ${W}.
 */
void netbuf_write_free(struct netbuf_write *);

/**
 * netbuf_ssl_read_init(ssl):
 * Behave as netbuf_read_init() but take an SSL context instead.
 */
struct netbuf_read * netbuf_ssl_read_init(struct network_ssl_ctx *);

/**
 * netbuf_ssl_write_init(ssl, fail_callback, fail_cookie):
 * Behave as netbuf_write_init() but take an SSL context instead.
 */
struct netbuf_write * netbuf_ssl_write_init(struct network_ssl_ctx *,
    int (*)(void *), void *);

#endif /* !NETBUF_H_ */
