#ifndef _PROTO_CONN_H_
#define _PROTO_CONN_H_

/* Opaque structures. */
struct proto_secret;
struct sock_addr;

/**
 * proto_conn_create(s, sas, decr, nofps, K, timeo, callback_dead, cookie):
 * Create a connection with one end at ${s} and the other end connecting to
 * the target addresses ${sas}.  If ${decr} is 0, encrypt the outgoing data;
 * if ${decr} is nonzero, decrypt the outgoing data.  If ${nofps} is non-zero,
 * don't use perfect forward secrecy.  Drop the connection if the handshake or
 * connecting to the target takes more than ${timeo} seconds.  When the
 * connection is dropped, invoke ${callback_dead}(${cookie}).
 */
int proto_conn_create(int, struct sock_addr * const *, int, int,
    const struct proto_secret *, double, int (*)(void *), void *);

#endif /* !_CONN_H_ */
