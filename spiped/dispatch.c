#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "dnsthread.h"
#include "events.h"
#include "network.h"
#include "sock.h"
#include "sock_util.h"
#include "warnp.h"

#include "proto_conn.h"

#include "dispatch.h"

struct accept_state {
	int s;
	const char * tgt;
	struct sock_addr ** sas;
	double rtime;
	int decr;
	int nofps;
	int requirefps;
	int nokeepalive;
	int * conndone;
	const struct proto_secret * K;
	size_t nconn;
	size_t nconn_max;
	double timeo;
	void * accept_cookie;
	void * dnstimer_cookie;
	DNSTHREAD T;
};

static int callback_gotconn(void *, int);
static int callback_resolveagain(void *);

/* Callback from address resolution. */
static int
callback_resolve(void * cookie, struct sock_addr ** sas)
{
	struct accept_state * A = cookie;

	/* If the address resolution succeeded... */
	if (sas != NULL) {
		/* Free the old addresses. */
		sock_addr_freelist(A->sas);

		/* Use the new addresses. */
		A->sas = sas;
	}

	/* Wait a while before resolving again. */
	if (events_timer_register_double(callback_resolveagain,
	    A, A->rtime) == NULL)
		goto err0;

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}

/* Timer callback to trigger another address resolution. */
static int
callback_resolveagain(void * cookie)
{
	struct accept_state * A = cookie;

	/* Re-resolve the target address. */
	return (dnsthread_resolveone(A->T, A->tgt, callback_resolve, A));
}

/* Non-blocking accept, if we can have more connections. */
static int
doaccept(struct accept_state * A)
{
	int rc = 0;

	/* If we can, accept a new connection. */
	if ((A->nconn < A->nconn_max) && (A->accept_cookie == NULL)) {
		if ((A->accept_cookie =
		    network_accept(A->s, callback_gotconn, A)) == NULL)
			rc = -1;
	}

	/* Return success/fail status. */
	return (rc);
}

/* A connection has closed.  Accept more if necessary. */
static int
callback_conndied(void * cookie)
{
	struct accept_state * A = cookie;

	/* We've lost a connection. */
	A->nconn -= 1;

	/* If requested to do so, indicate that a connection closed. */
	if (A->conndone != NULL)
		*A->conndone = 1;

	/* Maybe accept more connections. */
	return (doaccept(A));
}

/* Handle an incoming connection. */
static int
callback_gotconn(void * cookie, int s)
{
	struct accept_state * A = cookie;
	struct sock_addr ** sas;

	/* This accept is no longer in progress. */
	A->accept_cookie = NULL;

	/* If we got a -1 descriptor, something went seriously wrong. */
	if (s == -1) {
		warnp("network_accept failed");
		goto err0;
	}

	/* We have gained a connection. */
	A->nconn += 1;

	/* Duplicate the target address list. */
	if ((sas = sock_addr_duplist(A->sas)) == NULL)
		goto err1;

	/* Create a new connection. */
	if (proto_conn_create(s, sas, A->decr, A->nofps, A->requirefps,
	    A->nokeepalive, A->K, A->timeo, callback_conndied, A)) {
		warnp("Failure setting up new connection");
		goto err2;
	}

	/* Accept another connection if we can. */
	if (doaccept(A))
		goto err0;

	/* Success! */
	return (0);

err2:
	sock_addr_freelist(sas);
err1:
	A->nconn -= 1;
	close(s);
err0:
	/* Failure! */
	return (-1);
}

/**
 * dispatch_accept(s, tgt, rtime, sas, decr, nofps, requirefps, nokeepalive, K,
 *     nconn_max, timeo, conndone):
 * Start accepting connections on the socket ${s}.  Connect to the target
 * ${tgt}, re-resolving it every ${rtime} seconds if ${rtime} > 0; on address
 * resolution failure use the most recent successfully obtained addresses, or
 * the addresses ${sas}.  If ${decr} is 0, encrypt the outgoing connections; if
 * ${decr} is non-zero, decrypt the incoming connections.  Don't accept more
 * than ${nconn_max} connections.  If ${nofps} is non-zero, don't use perfect
 * forward secrecy.  If ${requirefps} is non-zero, require that both ends use
 * perfect forward secrecy.  Enable transport layer keep-alives (if applicable)
 * if and only if ${nokeepalive} is zero.  Drop connections if the handshake or
 * connecting to the target takes more than ${timeo} seconds.  If ${conndone}
 * is not NULL, set it to non-zero value when a connection closes.  Returns a
 * cookie which can be passed to dispatch_shutdown.
 */
void *
dispatch_accept(int s, const char * tgt, double rtime, struct sock_addr ** sas,
    int decr, int nofps, int requirefps, int nokeepalive,
    const struct proto_secret * K, size_t nconn_max, double timeo,
    int * conndone)
{
	struct accept_state * A;

	/* Bake a cookie. */
	if ((A = malloc(sizeof(struct accept_state))) == NULL)
		goto err0;
	A->s = s;
	A->tgt = tgt;
	A->sas = sas;
	A->rtime = rtime;
	A->decr = decr;
	A->nofps = nofps;
	A->requirefps = requirefps;
	A->nokeepalive = nokeepalive;
	A->conndone = conndone;
	A->K = K;
	A->nconn = 0;
	A->nconn_max = nconn_max;
	A->timeo = timeo;
	A->T = NULL;
	A->accept_cookie = NULL;
	A->dnstimer_cookie = NULL;

	/* If address re-resolution is enabled... */
	if (rtime > 0.0) {
		/* Launch an address resolution thread. */
		if ((A->T = dnsthread_spawn()) == NULL)
			goto err1;

		/* Re-resolve the target address after a while. */
		if ((A->dnstimer_cookie = events_timer_register_double(
		    callback_resolveagain, A, A->rtime)) == NULL)
			goto err2;
	}

	/* Accept a connection. */
	if (doaccept(A))
		goto err3;

	/* Success! */
	return (A);

err3:
	if (A->dnstimer_cookie != NULL)
		events_timer_cancel(A->dnstimer_cookie);
err2:
	if (A->T != NULL)
		dnsthread_kill(A->T);
err1:
	free(A);
err0:
	/* Failure! */
	return (NULL);
}

/**
 * dispatch_shutdown(dispatch_cookie):
 * Stops and frees memory associated with the ${dispatch_cookie}.
 */
void
dispatch_shutdown(void * dispatch_cookie)
{
	struct accept_state * A = dispatch_cookie;

	if (A->accept_cookie != NULL)
		network_accept_cancel(A->accept_cookie);
	if (A->dnstimer_cookie != NULL)
		events_timer_cancel(A->dnstimer_cookie);
	if (A->T != NULL)
		dnsthread_kill(A->T);
	free(A);
}
