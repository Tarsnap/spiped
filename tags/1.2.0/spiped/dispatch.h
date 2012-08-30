#ifndef _DISPATCH_H_
#define _DISPATCH_H_

#include <stdint.h>

/* Opaque structures. */
struct proto_secret;
struct sock_addr;

/**
 * dispatch_accept(s, sas, decr, nofps, K, nconn_max, timeo):
 * Start accepting connections on the socket ${s}.  Connect to the target
 * addresses ${sas}.  If ${decr} is 0, encrypt the outgoing connections; if
 * ${decr} is non-zero, decrypt the incoming connections.  Don't accept more
 * than ${nconn_max} connections.  If ${nofps} is non-zero, don't use perfect
 * forward secrecy.  Drop connections if the handshake or connecting to the
 * target takes more than ${timeo} seconds.
 */
int dispatch_accept(int, struct sock_addr * const *, int, int,
    const struct proto_secret *, size_t, double);

#endif /* !_DISPATCH_H_ */
