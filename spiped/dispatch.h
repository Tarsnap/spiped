#ifndef _DISPATCH_H_
#define _DISPATCH_H_

#include <stddef.h>

/* Opaque structures. */
struct proto_secret;
struct sock_addr;

/**
 * dispatch_accept(s, tgt, rtime, sas, sa_b, decr, nopfs, requirepfs,
 *     nokeepalive, K, nconn_max, timeo, conndone):
 * Start accepting connections on the socket ${s}.  Bind outgoing address to
 * ${sa_b} if it is not NULL.  Connect to the target ${tgt}, re-resolving
 * it every ${rtime} seconds if ${rtime} > 0; on address resolution
 * failure use the most recent successfully obtained addresses, or the
 * addresses ${sas}.  If ${decr} is 0, encrypt the outgoing connections; if
 * ${decr} is non-zero, decrypt the incoming connections.  Don't accept more
 * than ${nconn_max} connections.  If ${nopfs} is non-zero, don't use perfect
 * forward secrecy.  If ${requirepfs} is non-zero, require that both ends use
 * perfect forward secrecy.  Enable transport layer keep-alives (if applicable)
 * if and only if ${nokeepalive} is zero.  Drop connections if the handshake or
 * connecting to the target takes more than ${timeo} seconds.  If
 * dispatch_request_shutdown() is called then ${conndone} is set to a non-zero
 * value as soon as there are no active connections.  Return a cookie which can
 * be passed to dispatch_shutdown() and dispatch_request_shutdown().
 */
void * dispatch_accept(int, const char *, double, struct sock_addr **,
    const struct sock_addr *, int, int, int, int,
    const struct proto_secret *, size_t, double, int *);

/**
 * dispatch_shutdown(dispatch_cookie):
 * Stop the server, free memory, and close the listening socket.
 */
void dispatch_shutdown(void *);

/**
 * dispatch_request_shutdown(dispatch_cookie):
 * Request a shutdown: Stop accepting new connections and notify once
 * every existing connection ended.
 */
void dispatch_request_shutdown(void *);

#endif /* !_DISPATCH_H_ */
