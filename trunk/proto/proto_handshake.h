#ifndef _PROTO_HANDSHAKE_H_
#define _PROTO_HANDSHAKE_H_

struct proto_keys;

/**
 * proto_handshake(s, decr, nofps, kfhash, callback, cookie):
 * Perform a protocol handshake on socket ${s}.  If ${decr} is non-zero we are
 * at the receiving end of the connection; otherwise at the sending end.  If
 * ${nofps} is non-zero, perform a "weak" handshake without forward perfect
 * secrecy.  The key file's hash is ${kfhash}.  Upon completion, invoke
 * ${callback}(${cookie}, f, r) where f contains the keys needed for the
 * forward direction and r contains the keys needed for the reverse direction;
 * or w = r = NULL if the handshake failed.  Return a cookie which can be
 * passed to proto_handshake_cancel to cancel the handshake.
 */
void * proto_handshake(int, int, int, const uint8_t[32],
    int (*)(void *, struct proto_keys *, struct proto_keys *), void *);

/**
 * proto_handshake_cancel(cookie):
 * Cancel the handshake for which proto_handshake returned ${cookie}.
 */
void proto_handshake_cancel(void *);

#endif /* !_PROTO_HANDSHAKE_H_ */
