#ifndef _PROTO_CONN_H_
#define _PROTO_CONN_H_

/* Opaque structures. */
struct proto_secret;
struct sock_addr;

/* Reason why the connection was dropped. */
enum {
	PROTO_CONN_CLOSED = 0,		/* Normal exit */
	PROTO_CONN_CANCELLED,		/* Exit triggered by client code */
	PROTO_CONN_CONNECT_FAILED,	/* Could not connect */
	PROTO_CONN_HANDSHAKE_FAILED,	/* Handshake failed */
	PROTO_CONN_ERROR,		/* Unspecified reason */
};

/**
 * proto_conn_create(s, sas, decr, nopfs, requirepfs, nokeepalive, K, timeo,
 *     callback_dead, cookie):
 * Create a connection with one end at ${s} and the other end connecting to
 * the target addresses ${sas}.  If ${decr} is 0, encrypt the outgoing data;
 * if ${decr} is nonzero, decrypt the incoming data.  If ${nopfs} is non-zero,
 * don't use perfect forward secrecy.  If ${requirepfs} is non-zero, drop
 * the connection if the other end tries to disable perfect forward secrecy.
 * Enable transport layer keep-alives (if applicable) on both sockets if and
 * only if ${nokeepalive} is zero.  Drop the connection if the handshake or
 * connecting to the target takes more than ${timeo} seconds.  When the
 * connection is dropped, invoke ${callback_dead}(${cookie}).  Free ${sas}
 * once it is no longer needed.  Return a cookie which can be passed to
 * proto_conn_drop.
 */
void * proto_conn_create(int, struct sock_addr **, int, int, int, int,
    const struct proto_secret *, double, int (*)(void *, int), void *);

/**
 * proto_conn_drop(conn_cookie, reason):
 * Drop connection and frees memory associated with ${conn_cookie}, due to
 * ${reason}.  Return success or failure.
 */
int proto_conn_drop(void *, int);

#endif /* !_PROTO_CONN_H_ */
