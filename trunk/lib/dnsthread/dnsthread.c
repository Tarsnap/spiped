#include <sys/socket.h>

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "events.h"
#include "noeintr.h"
#include "sock.h"
#include "warnp.h"

#include "dnsthread.h"

/* Thread management structure. */
struct dnsthread_internal {
	/* Threading glue. */
	pthread_t thr;		/* Thread ID. */
	pthread_mutex_t	mtx;	/* Controls access to this structure. */
	pthread_cond_t cv;	/* Thread sleeps on this. */

	/* State management. */
	int state;		/* THREAD_* as below. */
	int wakeupsock[2];	/* Writes to [0], reads from [1]. */

	/* Address resolution variables. */
	char * addr;		/* Address to be resolved. */
	struct sock_addr ** sas;	/* Results. */
	int res_errno;		/* Errno to be passed back on failure. */

	/* Callback to occur when resolution is complete. */
	int (*callback)(void *, struct sock_addr **);	/* Callback. */
	void * cookie;		/* Cookie. */
};

/* Cookie used by dnsthread_resolve. */
struct resolve_cookie {
	int (*callback)(void *, struct sock_addr **);
	void * cookie;
	DNSTHREAD T;
};

/*
 * Thread states.  _resolveone moves the thread from SLEEPING to HASWORK;
 * workthread moves the thread from HASWORK to SLEEPING; and _kill moves the
 * thread from either state to SUICIDE.
 */
#define	THREAD_SLEEPING 0
#define THREAD_HASWORK 1
#define THREAD_SUICIDE 2

/* Callback functions used below. */
static int callback_resolveone(void * cookie);
static int callback_resolve(void *, struct sock_addr **);

/* Address resolution thread. */
static void *
workthread(void * cookie)
{
	struct dnsthread_internal * T = cookie;
	char * addr;
	struct sock_addr ** sas;
	int res_errno = 0;
	int rc;
	uint8_t zero = 0;

	/* Grab the mutex. */
	if ((rc = pthread_mutex_lock(&T->mtx)) != 0) {
		warn0("pthread_mutex_lock: %s", strerror(rc));
		exit(1);
	}

	/* Infinite loop doing work until told to suicide. */
	do {
		/*
		 * Sleep on the condition variable as long as we're in the
		 * SLEEPING state.
		 */
		while (T->state == THREAD_SLEEPING) {
			/* Sleep until we're woken up. */
			if ((rc = pthread_cond_wait(&T->cv, &T->mtx)) != 0) {
				warn0("pthread_cond_wait: %s", strerror(rc));
				exit(1);
			}
		}

		/* If we need to kill ourself, stop looping. */
		if (T->state == THREAD_SUICIDE)
			break;

		/* Grab the work. */
		addr = T->addr;

		/* Release the mutex. */
		if ((rc = pthread_mutex_unlock(&T->mtx)) != 0) {
			warn0("pthread_mutex_unlock: %s", strerror(rc));
			exit(1);
		}

		/* Perform the address resolution. */
		if ((sas = sock_resolve(addr)) == NULL)
			res_errno = errno;

		/* Grab the mutex again. */
		if ((rc = pthread_mutex_lock(&T->mtx)) != 0) {
			warn0("pthread_mutex_lock: %s", strerror(rc));
			exit(1);
		}

		/* Write the answer back. */
		T->sas = sas;
		T->res_errno = res_errno;

		/* Send a completion message. */
		if (noeintr_write(T->wakeupsock[0], &zero, 1) != 1) {
			warnp("Error writing to wakeup socket");
			exit(1);
		}

		/* Return to sleeping, unless we were instructed to die. */
		if (T->state != THREAD_SUICIDE)
			T->state = THREAD_SLEEPING;
	} while (1);

	/* Close the socket pair. */
	close(T->wakeupsock[1]);
	close(T->wakeupsock[0]);

	/* Destroy the condition variable. */
	if ((rc = pthread_cond_destroy(&T->cv)) != 0) {
		warn0("pthread_cond_destroy: %s", strerror(rc));
		exit(1);
	}

	/* Release the mutex. */
	if ((rc = pthread_mutex_unlock(&T->mtx)) != 0) {
		warn0("pthread_mutex_unlock: %s", strerror(rc));
		exit(1);
	}

	/* Destroy the mutex. */
	if ((rc = pthread_mutex_destroy(&T->mtx)) != 0) {
		warn0("pthread_mutex_destroy: %s", strerror(rc));
		exit(1);
	}

	/* Free the control structure. */
	free(T);

	/* Successful thread termination. */
	return (NULL);
}

/**
 * dnsthread_spawn(void):
 * Spawn a thread for performing address resolution.  Return a token which can
 * be passed to dnsthread_resolveone and dnsthread_kill.
 */
DNSTHREAD
dnsthread_spawn(void)
{
	struct dnsthread_internal * T;
	int rc;

	/* Allocate a thread management structure. */
	if ((T = malloc(sizeof(struct dnsthread_internal))) == NULL)
		goto err0;

	/* Create and lock a mutex. */
	if ((rc = pthread_mutex_init(&T->mtx, NULL)) != 0) {
		warn0("pthread_mutex_init: %s", strerror(rc));
		goto err1;
	}
	if ((rc = pthread_mutex_lock(&T->mtx)) != 0) {
		warn0("pthread_mutex_lock: %s", strerror(rc));
		goto err2;
	}

	/* Create state-changed condition variable. */
	if ((rc = pthread_cond_init(&T->cv, NULL)) != 0) {
		warn0("pthread_cond_init: %s", strerror(rc));
		goto err3;
	}

	/* Create wakeup socketpair. */
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, T->wakeupsock)) {
		warnp("socketpair");
		goto err4;
	}

	/* The thread starts out sleeping. */
	T->state = THREAD_SLEEPING;

	/* Create the thread. */
	if ((rc = pthread_create(&T->thr, NULL, workthread, T)) != 0) {
		warn0("pthread_create: %s", strerror(rc));
		goto err5;
	}

	/* Unlock the mutex. */
	if ((rc = pthread_mutex_unlock(&T->mtx)) != 0) {
		warn0("pthread_mutex_unlock: %s", strerror(rc));
		goto err0;
	}

	/* Success! */
	return (T);

err5:
	close(T->wakeupsock[1]);
	close(T->wakeupsock[0]);
err4:
	pthread_cond_destroy(&T->cv);
err3:
	pthread_mutex_unlock(&T->mtx);
err2:
	pthread_mutex_destroy(&T->mtx);
err1:
	free(T);
err0:
	/* Failure! */
	return (NULL);
}

/**
 * dnsthread_resolveone(T, addr, callback, cookie):
 * Using the thread for which ${T} was returned by dnsthread_spawn, resolve
 * the address ${addr}, which must be in one of the forms accepted by the
 * sock_resolve function.  If ${T} is already resolving an address, do not
 * resolve this address and instead return with errno == EALREADY.  Upon
 * completion, invoke ${callback}(${cookie}, sas), where ${sas} is a
 * NULL-terminated array of pointers to sock_addr structures or NULL on
 * resolution failure.
 */
int
dnsthread_resolveone(DNSTHREAD T, const char * addr,
    int (* callback)(void *, struct sock_addr **), void * cookie)
{
	int err = 0;
	int rc;

	/* Grab the mutex. */
	if ((rc = pthread_mutex_lock(&T->mtx)) != 0) {
		warn0("pthread_mutex_lock: %s", strerror(rc));
		goto err0;
	}

	/* If the resolver is already busy, fail. */
	if (T->state == THREAD_HASWORK) {
		err = EALREADY;
		goto ealready;
	}

	/* Duplicate the address to be resolved. */
	if ((T->addr = strdup(addr)) == NULL)
	    goto err1;

	/* Remember what callback we'll need to do eventually. */
	T->callback = callback;
	T->cookie = cookie;

	/* There is now work for the thread to do. */
	T->state = THREAD_HASWORK;

	/* Wake up the worker thread. */
	if ((rc = pthread_cond_signal(&T->cv)) != 0) {
		warn0("pthread_cond_signal: %s", strerror(rc));
		goto err1;
	}

	/* We want a callback when the worker thread pokes us. */
	if (events_network_register(callback_resolveone, T, T->wakeupsock[1],
	    EVENTS_NETWORK_OP_READ)) {
		warnp("Error registering wakeup listener");
		goto err1;
	}

ealready:
	/* Release the mutex. */
	if ((rc = pthread_mutex_unlock(&T->mtx)) != 0) {
		warn0("pthread_mutex_unlock: %s", strerror(rc));
		goto err0;
	}

	/* If err was set earlier, store the value in errno. */
	if (err)
		errno = err;

	/* Success! */
	return (0);

err1:
	pthread_mutex_unlock(&T->mtx);
err0:
	/* Failure! */
	return (-1);
}

/* Callback for dnsthread_resolveone, from wakeup socket. */
static int
callback_resolveone(void * cookie)
{
	struct dnsthread_internal * T = cookie;
	struct sock_addr ** sas;
	uint8_t zero;
	int res_errno = 0;
	int (*callback)(void *, struct sock_addr **);
	void * cb_cookie;
	int rc;

	/* Drain the byte from the socketpair. */
	if (read(T->wakeupsock[1], &zero, 1) != 1) {
		warn0("Error reading from wakeup socket");
		goto err0;
	}

	/* Grab the mutex. */
	if ((rc = pthread_mutex_lock(&T->mtx)) != 0) {
		warn0("pthread_mutex_lock: %s", strerror(rc));
		goto err0;
	}

	/* Free the (strduped) address which was to be resolved. */
	free(T->addr);

	/* Grab the result. */
	sas = T->sas;
	res_errno = T->res_errno;

	/* Grab the callback. */
	callback = T->callback;
	cb_cookie = T->cookie;

	/* Release the mutex. */
	if ((rc = pthread_mutex_unlock(&T->mtx)) != 0) {
		warn0("pthread_mutex_unlock: %s", strerror(rc));
		goto err0;
	}

	/* Perform the callback. */
	if (sas == NULL)
		errno = res_errno;
	return ((callback)(cb_cookie, sas));

err0:
	/* Failure! */
	return (-1);
}

/**
 * dnsthread_kill(T):
 * Instruct an address resolution thread to die.  If the thread does not have
 * an address resolution operation currently pending, wait for the thread to
 * die before returning.
 */
int
dnsthread_kill(DNSTHREAD T)
{
	int rc;
	int ostate;
	pthread_t thr;

	/* Lock the control structure. */
	if ((rc = pthread_mutex_lock(&T->mtx)) != 0) {
		warn0("pthread_mutex_lock: %s", strerror(rc));
		goto err0;
	}

	/* Remember what state the thread is currently in. */
	ostate = T->state;

	/* Grab the thread ID. */
	thr = T->thr;

	/* Tell the thread to die, and wake it up. */
	T->state = THREAD_SUICIDE;
	if ((rc = pthread_cond_signal(&T->cv)) != 0) {
		warn0("pthread_cond_signal: %s", strerror(rc));
		goto err1;
	}

	/* Unlock the control structure. */
	if ((rc = pthread_mutex_unlock(&T->mtx)) != 0) {
		warn0("pthread_mutex_unlock: %s", strerror(rc));
		goto err0;
	}

	/* If the thread was sleeping, wait for it to wake up and die. */
	if (ostate == THREAD_SLEEPING) {
		if ((rc = pthread_join(thr, NULL)) != 0) {
			warn0("pthread_join: %s", strerror(rc));
			goto err0;
		}
	}

	/* Success! */
	return (0);

err1:
	pthread_mutex_unlock(&T->mtx);
err0:
	/* Failure! */
	return (-1);
}

/**
 * dnsthread_resolve(addr, callback, cookie):
 * Perform a non-blocking address resolution of ${addr}.  This function may
 * spawn a thread internally.
 */
int
dnsthread_resolve(const char * addr,
    int (* callback)(void *, struct sock_addr **), void * cookie)
{
	struct resolve_cookie * R;

	/* Bake a cookie. */
	if ((R = malloc(sizeof(struct resolve_cookie))) == NULL)
		goto err0;
	R->callback = callback;
	R->cookie = cookie;

	/* Spawn a thread. */
	if ((R->T = dnsthread_spawn()) == NULL)
		goto err1;

	/* Launch the request. */
	if (dnsthread_resolveone(R->T, addr, callback_resolve, R))
		goto err2;

	/* Success! */
	return (0);

err2:
	dnsthread_kill(R->T);
err1:
	free(R);
err0:
	/* Failure! */
	return (-1);
}

/* Callback for dnsthread_resolve, from dnsthread_resolveone. */
static int
callback_resolve(void * cookie, struct sock_addr ** sas)
{
	struct resolve_cookie * R = cookie;
	int rc;

	/* Invoke the upstream callback. */
	rc = (R->callback)(R->cookie, sas);

	/* Kill the resolver thread. */
	if (dnsthread_kill(R->T))
		rc = -1;

	/* Free our cookie. */
	free(R);

	/* Return upstream return code or failure. */
	return (rc);
}
