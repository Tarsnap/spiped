#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "events.h"
#include "network.h"
#include "sock.h"
#include "warnp.h"

#include "conn.h"
#include "dispatch.h"

struct accept_state {
	int s;
	struct sock_addr * const * sas;
	int decr;
	int nofps;
	const struct proto_secret * K;
	size_t nconn;
	size_t nconn_max;
	double timeo;
	void * accept_cookie;
};

static int callback_gotconn(void *, int);

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

	/* Maybe accept more connections. */
	return (doaccept(A));
}

/* Handle an incoming connection. */
static int
callback_gotconn(void * cookie, int s)
{
	struct accept_state * A = cookie;

	/* This accept is no longer in progress. */
	A->accept_cookie = NULL;

	/* If we got a -1 descriptor, something went seriously wrong. */
	if (s == -1) {
		warnp("network_accept failed");
		goto err0;
	}

	/* We have gained a connection. */
	A->nconn += 1;

	/* Create a new connection. */
	if (conn_create(s, A->sas, A->decr, A->nofps, A->K, A->timeo,
	    callback_conndied, A)) {
		warnp("Failure setting up new connection");
		goto err1;
	}

	/* Accept another connection if we can. */
	if (doaccept(A))
		goto err0;

	/* Success! */
	return (0);

err1:
	A->nconn -= 1;
	close(s);
err0:
	/* Failure! */
	return (-1);
}

/**
 * dispatch_accept(s, sas, decr, nofps, K, nconn_max, timeo):
 * Start accepting connections on the socket ${s}.  Connect to the target
 * addresses ${sas}.  If ${decr} is 0, encrypt the outgoing connections; if
 * ${decr} is non-zero, decrypt the incoming connections.  Don't accept more
 * than ${nconn_max} connections.  If ${nofps} is non-zero, don't use perfect
 * forward secrecy.  Drop connections if the handshake or connecting to the
 * target takes more than ${timeo} seconds.
 */
int
dispatch_accept(int s, struct sock_addr * const * sas, int decr, int nofps,
    const struct proto_secret * K, size_t nconn_max, double timeo)
{
	struct accept_state * A;

	/* Bake a cookie. */
	if ((A = malloc(sizeof(struct accept_state))) == NULL)
		goto err0;
	A->s = s;
	A->sas = sas;
	A->decr = decr;
	A->nofps = nofps;
	A->K = K;
	A->nconn = 0;
	A->nconn_max = nconn_max;
	A->timeo = timeo;
	A->accept_cookie = NULL;

	/* Accept a connection. */
	if (doaccept(A))
		goto err1;

	/* Success! */
	return (0);

err1:
	free(A);
err0:
	/* Failure! */
	return (-1);
}
