#ifndef PROTO_HANDSHAKE_H_
#define PROTO_HANDSHAKE_H_

/* Opaque structures. */
struct proto_keys;
struct proto_secret;

/**
 * proto_handshake(s, decr, nopfs, requirepfs, K, callback, cookie):
 * Perform a protocol handshake on socket ${s}.  If ${decr} is non-zero we are
 * at the receiving end of the connection; otherwise at the sending end.  If
 * ${nopfs} is non-zero, perform a "weak" handshake without perfect forward
 * secrecy.  If ${requirepfs} is non-zero, drop the connection if the other
 * end attempts to perform a "weak" handshake.  The shared protocol secret is
 * ${K}.  Upon completion, invoke ${callback}(${cookie}, f, r), where f
 * contains the keys needed for the forward direction and r contains the keys
 * needed for the reverse direction; or f = r = NULL if the handshake failed.
 * Return a cookie which can be passed to proto_handshake_cancel() to cancel the
 * handshake.
 */
void * proto_handshake(int, int, int, int, const struct proto_secret *,
    int (*)(void *, struct proto_keys *, struct proto_keys *), void *);

/**
 * proto_handshake_cancel(cookie):
 * Cancel the handshake for which proto_handshake() returned ${cookie}.
 */
void proto_handshake_cancel(void *);

#endif /* !PROTO_HANDSHAKE_H_ */
