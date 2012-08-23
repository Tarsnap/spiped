#ifndef _CONN_H_
#define _CONN_H_

#include <stdint.h>

#include "sock.h"

/**
 * conn_accept(s, sas, decr, nofps, kfhash, nconn_max, timeo):
 * Start accepting connections on the socket ${s}.  Connect to the target
 * addresses ${sas}.  If ${decr} is 0, encrypt the outgoing connections; if
 * ${decr} is non-zero, decrypt the incoming connections.  Don't accept more
 * than ${nconn_max} connections.  If ${nofps} is non-zero, don't use perfect
 * forward secrecy.  Drop connections if the handshake or connecting to the
 * target takes more than ${timeo} seconds.
 */
int conn_accept(int, struct sock_addr * const *, int, int, uint8_t[32],
    size_t, double);

#endif /* !_CONN_H_ */
