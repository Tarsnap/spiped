#ifndef _DNSTHREAD_H_
#define _DNSTHREAD_H_

/* Opaque address structure. */
struct sock_addr;

/* Opaque thread token. */
typedef struct dnsthread_internal * DNSTHREAD;

/**
 * dnsthread_spawn(void):
 * Spawn a thread for performing address resolution.  Return a token which can
 * be passed to dnsthread_resolveone and dnsthread_kill.
 */
DNSTHREAD dnsthread_spawn(void);

/**
 * dnsthread_resolveone(T, addr, callback, cookie):
 * Using the thread for which ${T} was returned by dnsthread_spawn, resolve
 * the address ${addr}, which must be in one of the forms accepted by the
 * sock_resolve function.  If ${T} is already resolving an address, fail with
 * EALREADY.  Upon completion, invoke ${callback}(${cookie}, sas), where
 * ${sas} is a NULL-terminated array of pointers to sock_addr structures or
 * NULL on resolution failure.
 */
int dnsthread_resolveone(DNSTHREAD, const char *,
    int (*)(void *, struct sock_addr **), void *);

/**
 * dnsthread_kill(T):
 * Instruct an address resolution thread to die.  If the thread does not have
 * an address resolution operation currently pending, wait for the thread to
 * die before returning.
 */
int dnsthread_kill(DNSTHREAD);

/**
 * dnsthread_resolve(addr, callback, cookie):
 * Perform a non-blocking address resolution of ${addr}.  This function may
 * spawn a thread internally.
 */
int dnsthread_resolve(const char *,
    int (*)(void *, struct sock_addr **), void *);

#endif /* !_DNSTHREAD_H_ */
