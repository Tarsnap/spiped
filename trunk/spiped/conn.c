#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "events.h"
#include "network.h"
#include "sock.h"
#include "warnp.h"

#include "proto_handshake.h"
#include "proto_pipe.h"
#include "proto_crypt.h"

#include "conn.h"

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

struct conn_state {
	struct accept_state * A;
	int s;
	int t;
	void * connect_cookie;
	void * connect_timeout_cookie;
	void * handshake_cookie;
	void * handshake_timeout_cookie;
	struct proto_keys * k_f;
	struct proto_keys * k_r;
	void * pipe_f;
	void * pipe_r;
	int stat_f;
	int stat_r;
};

static int callback_gotconn(void *, int);
static int callback_connect_done(void *, int);
static int callback_connect_timeout(void *);
static int callback_handshake_done(void *, struct proto_keys *,
    struct proto_keys *);
static int callback_handshake_timeout(void *);
static int callback_pipestatus(void *);

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

/* Start a handshake. */
static int
starthandshake(struct conn_state * C, int s, int decr)
{

	/* Start the handshake timer. */
	if ((C->handshake_timeout_cookie = events_timer_register_double(
	    callback_handshake_timeout, C, C->A->timeo)) == NULL)
		goto err0;

	/* Start the handshake. */
	if ((C->handshake_cookie = proto_handshake(s, decr, C->A->nofps,
	    C->A->K, callback_handshake_done, C)) == NULL)
		goto err1;

	/* Success! */
	return (0);

err1:
	events_timer_cancel(C->handshake_timeout_cookie);
	C->handshake_timeout_cookie = NULL;
err0:
	/* Failure! */
	return (-1);
}

/* Launch the two pipes. */
static int
launchpipes(struct conn_state * C)
{

	/* Create two pipes. */
	if ((C->pipe_f = proto_pipe(C->s, C->t, C->A->decr, C->k_f,
	    &C->stat_f, callback_pipestatus, C)) == NULL)
		goto err0;
	if ((C->pipe_r = proto_pipe(C->t, C->s, !C->A->decr, C->k_r,
	    &C->stat_r, callback_pipestatus, C)) == NULL)
		goto err0;

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}

/* Drop a connection. */
static int
dropconn(struct conn_state * C)
{
	int rc = 0;

	/* Close the incoming connection. */
	C->A->nconn -= 1;
	close(C->s);

	/* Close the outgoing connection if it is open. */
	if (C->t != -1)
		close(C->t);

	/* Stop connecting if a connection is in progress. */
	if (C->connect_cookie != NULL)
		network_connect_cancel(C->connect_cookie);

	/* Stop handshaking if a handshake is in progress. */
	if (C->handshake_cookie != NULL)
		proto_handshake_cancel(C->handshake_cookie);

	/* Kill timeouts if they are pending. */
	if (C->connect_timeout_cookie != NULL)
		events_timer_cancel(C->connect_timeout_cookie);
	if (C->handshake_timeout_cookie != NULL)
		events_timer_cancel(C->handshake_timeout_cookie);

	/* Free protocol keys. */
	proto_crypt_free(C->k_f);
	proto_crypt_free(C->k_r);

	/* Shut down pipes. */
	if (C->pipe_f != NULL)
		proto_pipe_cancel(C->pipe_f);
	if (C->pipe_r != NULL)
		proto_pipe_cancel(C->pipe_r);

	/* Accept more connections if we need to. */
	if (doaccept(C->A))
		rc = -1;

	/* Free the connection cookie. */
	free(C);

	/* Return success/fail status. */
	return (rc);
}

/* Handle an incoming connection. */
static int
callback_gotconn(void * cookie, int s)
{
	struct accept_state * A = cookie;
	struct conn_state * C;

	/* This accept is no longer in progress. */
	A->accept_cookie = NULL;

	/* We have gained a connection. */
	A->nconn += 1;

	/* Bake a cookie for this connection. */
	if ((C = malloc(sizeof(struct conn_state))) == NULL)
		goto err1;
	C->A = A;
	C->s = s;
	C->t = -1;
	C->connect_cookie = NULL;
	C->handshake_cookie = NULL;
	C->connect_timeout_cookie = C->handshake_timeout_cookie = NULL;
	C->k_f = C->k_r = NULL;
	C->pipe_f = C->pipe_r = NULL;
	C->stat_f = C->stat_r = 1;

	/* Start the connect timer. */
	if ((C->connect_timeout_cookie = events_timer_register_double(
	    callback_connect_timeout, C, C->A->timeo)) == NULL)
		goto err2;

	/* Connect to target. */
	if ((C->connect_cookie =
	    network_connect(A->sas, callback_connect_done, C)) == NULL)
		goto err3;

	/* If we're decrypting, start the handshake. */
	if (C->A->decr) {
		if (starthandshake(C, C->s, C->A->decr))
			goto err4;
	}

	/* Accept another connection if we can. */
	if (doaccept(A))
		goto err0;

	/* Success! */
	return (0);

err4:
	network_connect_cancel(C->connect_cookie);
err3:
	events_timer_cancel(C->connect_timeout_cookie);
err2:
	free(C);
err1:
	A->nconn -= 1;
	close(s);
err0:
	/* Failure! */
	return (-1);
}

/* We have connected to the target. */
static int
callback_connect_done(void * cookie, int t)
{
	struct conn_state * C = cookie;

	/* This connection attempt is no longer pending. */
	C->connect_cookie = NULL;

	/* We beat the clock. */
	events_timer_cancel(C->connect_timeout_cookie);
	C->connect_timeout_cookie = NULL;

	/* Did we manage to connect? */
	if ((C->t = t) == -1)
		return (dropconn(C));

	/* If we're encrypting, start the handshake. */
	if (!C->A->decr) {
		if (starthandshake(C, C->t, C->A->decr))
			goto err1;
	}

	/* If we have connections and keys, start shuttling data. */
	if ((C->t != -1) && (C->k_f != NULL) && (C->k_r != NULL)) {
		if (launchpipes(C))
			goto err1;
	}

	/* Success! */
	return (0);

err1:
	dropconn(C);

	/* Failure! */
	return (-1);
}

/* Connecting to the target took too long. */
static int
callback_connect_timeout(void * cookie)
{
	struct conn_state * C = cookie;

	/* This timeout is no longer pending. */
	C->connect_timeout_cookie = NULL;

	/* Drop the connection. */
	return (dropconn(C));
}

/* We have performed the protocol handshake. */
static int
callback_handshake_done(void * cookie, struct proto_keys * f,
    struct proto_keys * r)
{
	struct conn_state * C = cookie;

	/* The handshake is no longer in progress. */
	C->handshake_cookie = NULL;

	/* We beat the clock. */
	events_timer_cancel(C->handshake_timeout_cookie);
	C->handshake_timeout_cookie = NULL;

	/* If the protocol handshake failed, drop the connection. */
	if ((f == NULL) && (r == NULL))
		return (dropconn(C));

	/* We should have two keys. */
	assert(f != NULL);
	assert(r != NULL);

	/* Record the keys so we can free them later. */
	C->k_f = f;
	C->k_r = r;

	/* If we have connections and keys, start shuttling data. */
	if ((C->t != -1) && (C->k_f != NULL) && (C->k_r != NULL)) {
		if (launchpipes(C))
			goto err1;
	}

	/* Success! */
	return (0);

err1:
	dropconn(C);

	/* Failure! */
	return (-1);
}

/* The protocol handshake took too long. */
static int
callback_handshake_timeout(void * cookie)
{
	struct conn_state * C = cookie;

	/* This timeout is no longer pending. */
	C->handshake_timeout_cookie = NULL;

	/* Drop the connection. */
	return (dropconn(C));
}

/* The status of one of the directions has changed. */
static int
callback_pipestatus(void * cookie)
{
	struct conn_state * C = cookie;

	/* If we have an error in either direction, kill the connection. */
	if ((C->stat_f == -1) || (C->stat_r == -1))
		return (dropconn(C));

	/* If both directions have been shut down, kill the connection. */
	if ((C->stat_f == 0) && (C->stat_r == 0))
		return (dropconn(C));

	/* Nothing to do. */
	return (0);
}

/**
 * conn_accept(s, sas, decr, nofps, K, nconn_max, timeo):
 * Start accepting connections on the socket ${s}.  Connect to the target
 * addresses ${sas}.  If ${decr} is 0, encrypt the outgoing connections; if
 * ${decr} is non-zero, decrypt the incoming connections.  Don't accept more
 * than ${nconn_max} connections.  If ${nofps} is non-zero, don't use perfect
 * forward secrecy.  Drop connections if the handshake or connecting to the
 * target takes more than ${timeo} seconds.
 */
int
conn_accept(int s, struct sock_addr * const * sas, int decr, int nofps,
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
