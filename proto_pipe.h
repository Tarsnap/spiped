#ifndef _PROTO_PIPE_H_
#define _PROTO_PIPE_H_

struct proto_keys;

/**
 * proto_pipe(s_in, s_out, decr, k, status, callback, cookie):
 * Read bytes from ${s_in} and write them to ${s_out}.  If ${decr} is non-zero
 * then use ${k} to decrypt the bytes; otherwise use ${k} to encrypt them.
 * If EOF is read, set ${status} to 0, and if an error is encountered set
 * ${status} to -1; in either case, invoke ${callback}(${cookie}).  Return a
 * cookie which can be passed to proto_pipe_cancel.
 */
void * proto_pipe(int, int, int, struct proto_keys *, int *,
    int (*)(void *), void *);

/**
 * proto_pipe_cancel(cookie):
 * Shut down the pipe created by proto_pipe for which ${cookie} was returned.
 */
void proto_pipe_cancel(void *);

#endif /* !_PROTO_PIPE_H_ */
