#ifndef NETBUF_SSL_H_
#define NETBUF_SSL_H_

#include <stdint.h>
#include <unistd.h>

/* Opaque type. */
struct network_ssl_ctx;

/*
 * Function pointers defined in netbuf_read and netbuf_write; we set them
 * from our _init functions in order to avoid unnecessary linkage.
 */
extern void * (* netbuf_read_ssl_func)(struct network_ssl_ctx *, uint8_t *,
    size_t, size_t, int (*)(void *, ssize_t), void *);
extern void (* netbuf_read_ssl_cancel_func)(void *);
extern void * (* netbuf_write_ssl_func)(struct network_ssl_ctx *,
    const uint8_t *, size_t, size_t, int (*)(void *, ssize_t), void *);
extern void (* netbuf_write_ssl_cancel_func)(void *);

/**
 * netbuf_read_init2(s, ssl):
 * Behave like netbuf_read_init if ${ssl} is NULL.  If the SSL context ${ssl}
 * is not NULL, use it and ignore ${s}.
 */
struct netbuf_read * netbuf_read_init2(int, struct network_ssl_ctx *);

/**
 * netbuf_write_init2(s, ssl, fail_callback, fail_cookie):
 * Behave like netbuf_write_init if ${ssl} is NULL.  If the SSL context ${ssl}
 * is not NULL, use it and ignore ${s}.
 */
struct netbuf_write * netbuf_write_init2(int, struct network_ssl_ctx *,
    int (*)(void *), void *);

#endif /* !NETBUF_SSL_H_ */
