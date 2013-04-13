#include <sys/socket.h>

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "events.h"
#include "network.h"
#include "sock.h"

#include "proto_handshake.h"
#include "proto_pipe.h"
#include "proto_crypt.h"

#include "proto_conn.h"

struct conn_state {
	int (* callback_dead)(void *);
	void * cookie;
	struct sock_addr ** sas;
	int decr;
	int nofps;
	int nokeepalive;
	const struct proto_secret * K;
	double timeo;
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

static int callback_connect_done(void *, int);
static int callback_connect_timeout(void *);
static int callback_handshake_done(void *, struct proto_keys *,
    struct proto_keys *);
static int callback_handshake_timeout(void *);
static int callback_pipestatus(void *);

/* Start a handshake. */
static int
starthandshake(struct conn_state * C, int s, int decr)
{

	/* Start the handshake timer. */
	if ((C->handshake_timeout_cookie = events_timer_register_double(
	    callback_handshake_timeout, C, C->timeo)) == NULL)
		goto err0;

	/* Start the handshake. */
	if ((C->handshake_cookie = proto_handshake(s, decr, C->nofps,
	    C->K, callback_handshake_done, C)) == NULL)
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
	int on = C->nokeepalive ? 0 : 1;

	/*
	 * Attempt to turn keepalives on or off as requested.  We ignore
	 * failures here since the sockets might not be of a type for which
	 * SO_KEEPALIVE is valid -- it is a socket level option, but protocol
	 * specific.  In particular, it has no sensible meaning for UNIX
	 * sockets.
	 */
	(void)setsockopt(C->s, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));
	(void)setsockopt(C->t, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));

	/* Create two pipes. */
	if ((C->pipe_f = proto_pipe(C->s, C->t, C->decr, C->k_f,
	    &C->stat_f, callback_pipestatus, C)) == NULL)
		goto err0;
	if ((C->pipe_r = proto_pipe(C->t, C->s, !C->decr, C->k_r,
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
	int rc;

	/* Close the incoming connection. */
	close(C->s);

	/* Close the outgoing connection if it is open. */
	if (C->t != -1)
		close(C->t);

	/* Stop connecting if a connection is in progress. */
	if (C->connect_cookie != NULL)
		network_connect_cancel(C->connect_cookie);

	/* Free the target addresses if we haven't already done so. */
	sock_addr_freelist(C->sas);

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

	/* Notify the upstream that we've dropped a connection. */
	rc = (C->callback_dead)(C->cookie);

	/* Free the connection cookie. */
	free(C);

	/* Return success/fail status. */
	return (rc);
}

/**
 * proto_conn_create(s, sas, decr, nofps, nokeepalive, K, timeo,
 *     callback_dead, cookie):
 * Create a connection with one end at ${s} and the other end connecting to
 * the target addresses ${sas}.  If ${decr} is 0, encrypt the outgoing data;
 * if ${decr} is nonzero, decrypt the outgoing data.  If ${nofps} is non-zero,
 * don't use perfect forward secrecy.  Enable transport layer keep-alives (if
 * applicable) on both sockets if and only if ${nokeepalive} is zero.  Drop the
 * connection if the handshake or connecting to the target takes more than
 * ${timeo} seconds.  When the connection is dropped, invoke
 * ${callback_dead}(${cookie}).  Free ${sas} once it is no longer needed.
 */
int
proto_conn_create(int s, struct sock_addr ** sas, int decr, int nofps,
    int nokeepalive, const struct proto_secret * K, double timeo,
    int (* callback_dead)(void *), void * cookie)
{
	struct conn_state * C;

	/* Bake a cookie for this connection. */
	if ((C = malloc(sizeof(struct conn_state))) == NULL)
		goto err0;
	C->callback_dead = callback_dead;
	C->cookie = cookie;
	C->sas = sas;
	C->decr = decr;
	C->nofps = nofps;
	C->nokeepalive = nokeepalive;
	C->K = K;
	C->timeo = timeo;
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
	    callback_connect_timeout, C, C->timeo)) == NULL)
		goto err1;

	/* Connect to target. */
	if ((C->connect_cookie =
	    network_connect(C->sas, callback_connect_done, C)) == NULL)
		goto err2;

	/* If we're decrypting, start the handshake. */
	if (C->decr) {
		if (starthandshake(C, C->s, C->decr))
			goto err3;
	}

	/* Success! */
	return (0);

err3:
	network_connect_cancel(C->connect_cookie);
err2:
	events_timer_cancel(C->connect_timeout_cookie);
err1:
	free(C);
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

	/* Don't need the target address any more. */
	sock_addr_freelist(C->sas);
	C->sas = NULL;

	/* We beat the clock. */
	events_timer_cancel(C->connect_timeout_cookie);
	C->connect_timeout_cookie = NULL;

	/* Did we manage to connect? */
	if ((C->t = t) == -1)
		return (dropconn(C));

	/* If we're encrypting, start the handshake. */
	if (!C->decr) {
		if (starthandshake(C, C->t, C->decr))
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

	/*
	 * We could free C->sas here, but from a semantic point of view it
	 * could still be in use by the not-yet-cancelled connect operation.
	 * Instead, we free it in dropconn, after cancelling the connect.
	 */

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
