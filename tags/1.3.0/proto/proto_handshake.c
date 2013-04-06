#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "crypto_entropy.h"
#include "network.h"

#include "proto_crypt.h"

#include "proto_handshake.h"

struct handshake_cookie {
	int (* callback)(void *, struct proto_keys *, struct proto_keys *);
	void * cookie;
	int s;
	int decr;
	int nofps;
	const struct proto_secret * K;
	uint8_t nonce_local[PCRYPT_NONCE_LEN];
	uint8_t nonce_remote[PCRYPT_NONCE_LEN];
	uint8_t dhmac_local[PCRYPT_DHMAC_LEN];
	uint8_t dhmac_remote[PCRYPT_DHMAC_LEN];
	uint8_t x[PCRYPT_X_LEN];
	uint8_t yh_local[PCRYPT_YH_LEN];
	uint8_t yh_remote[PCRYPT_YH_LEN];
	void * read_cookie;
	void * write_cookie;
};

static int callback_nonce_write(void *, ssize_t);
static int callback_nonce_read(void *, ssize_t);
static int gotnonces(struct handshake_cookie *);
static int dhread(struct handshake_cookie *);
static int callback_dh_read(void *, ssize_t);
static int dhwrite(struct handshake_cookie *);
static int callback_dh_write(void *, ssize_t);
static int handshakedone(struct handshake_cookie *);

/* The handshake failed.  Call back and clean up. */
static int
handshakefail(struct handshake_cookie * H)
{
	int rc;

	/* Cancel any pending network read or write. */
	if (H->read_cookie != NULL)
		network_read_cancel(H->read_cookie);
	if (H->write_cookie != NULL)
		network_write_cancel(H->write_cookie);

	/* Perform the callback. */
	rc = (H->callback)(H->cookie, NULL, NULL);

	/* Free the cookie. */
	free(H);

	/* Return status from callback. */
	return (rc);
}

/**
 * proto_handshake(s, decr, nofps, K, callback, cookie):
 * Perform a protocol handshake on socket ${s}.  If ${decr} is non-zero we are
 * at the receiving end of the connection; otherwise at the sending end.  If
 * ${nofps} is non-zero, perform a "weak" handshake without forward perfect
 * secrecy.  The shared protocol secret is ${K}.  Upon completion, invoke
 * ${callback}(${cookie}, f, r) where f contains the keys needed for the
 * forward direction and r contains the keys needed for the reverse direction;
 * or w = r = NULL if the handshake failed.  Return a cookie which can be
 * passed to proto_handshake_cancel to cancel the handshake.
 */
void *
proto_handshake(int s, int decr, int nofps, const struct proto_secret * K,
    int (* callback)(void *, struct proto_keys *, struct proto_keys *),
    void * cookie)
{
	struct handshake_cookie * H;

	/* Bake a cookie. */
	if ((H = malloc(sizeof(struct handshake_cookie))) == NULL)
		goto err0;
	H->callback = callback;
	H->cookie = cookie;
	H->s = s;
	H->decr = decr;
	H->nofps = nofps;
	H->K = K;

	/* Generate a 32-byte connection nonce. */
	if (crypto_entropy_read(H->nonce_local, 32))
		goto err1;

	/* Send our nonce. */
	if ((H->write_cookie = network_write(s, H->nonce_local, 32, 32,
	    callback_nonce_write, H)) == NULL)
		goto err1;

	/* Read the other party's nonce. */
	if ((H->read_cookie = network_read(s, H->nonce_remote, 32, 32,
	    callback_nonce_read, H)) == NULL)
		goto err2;

	/* Success! */
	return (H);

err2:
	network_write_cancel(H->write_cookie);
err1:
	free(H);
err0:
	/* Failure! */
	return (NULL);
}

/* We've written our nonce. */
static int
callback_nonce_write(void * cookie, ssize_t len)
{
	struct handshake_cookie * H = cookie;

	/* This write is no longer pending. */
	H->write_cookie = NULL;

	/* Did we successfully write? */
	if (len < 32)
		return (handshakefail(H));

	/* If the nonce read is also done, move on to the next step. */
	if (H->read_cookie == NULL)
		return (gotnonces(H));

	/* Nothing to do. */
	return (0);
}

/* We've read a nonce. */
static int
callback_nonce_read(void * cookie, ssize_t len)
{
	struct handshake_cookie * H = cookie;

	/* This read is no longer pending. */
	H->read_cookie = NULL;

	/* Did we successfully read? */
	if (len < 32)
		return (handshakefail(H));

	/* If the nonce write is also done, move on to the next step. */
	if (H->write_cookie == NULL)
		return (gotnonces(H));

	/* Nothing to do. */
	return (0);
}

/* We have two nonces.  Start the DH exchange. */
static int
gotnonces(struct handshake_cookie * H)
{

	/* Compute the diffie-hellman parameter MAC keys. */
	proto_crypt_dhmac(H->K, H->nonce_local, H->nonce_remote,
	    H->dhmac_local, H->dhmac_remote, H->decr);

	/*
	 * If we're the server, we need to read the client's diffie-hellman
	 * parameter.  If we're the client, we need to generate and send our
	 * diffie-hellman parameter.
	 */
	if (H->decr)
		return (dhread(H));
	else
		return (dhwrite(H));

	/* NOTREACHED */
}

/* Read a diffie-hellman parameter. */
static int
dhread(struct handshake_cookie * H)
{

	/* Read the remote signed diffie-hellman parameter. */
	if ((H->read_cookie = network_read(H->s, H->yh_remote, PCRYPT_YH_LEN,
	    PCRYPT_YH_LEN, callback_dh_read, H)) == NULL)
		goto err0;

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}

/* We have read a diffie-hellman parameter. */
static int
callback_dh_read(void * cookie, ssize_t len)
{
	struct handshake_cookie * H = cookie;

	/* This read is no longer pending. */
	H->read_cookie = NULL;

	/* Did we successfully read? */
	if (len < PCRYPT_YH_LEN)
		return (handshakefail(H));

	/* Is the value we read valid? */
	if (proto_crypt_dh_validate(H->yh_remote, H->dhmac_remote))
		return (handshakefail(H));

	/*
	 * If we're the server, we need to send our diffie-hellman parameter
	 * next.  If we're the client, move on to the final computation.
	 */
	if (H->decr)
		return (dhwrite(H));
	else
		return (handshakedone(H));

	/* NOTREACHED */
}

/* Generate and write a diffie-hellman parameter. */
static int
dhwrite(struct handshake_cookie * H)
{

	/* Generate a signed diffie-hellman parameter. */
	if (proto_crypt_dh_generate(H->yh_local, H->x, H->dhmac_local, 
	    H->nofps))
		goto err0;

	/* Write our signed diffie-hellman parameter. */
	if ((H->write_cookie = network_write(H->s, H->yh_local, PCRYPT_YH_LEN,
	    PCRYPT_YH_LEN, callback_dh_write, H)) == NULL)
		goto err0;

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}

/* We have written our diffie-hellman parameter. */
static int
callback_dh_write(void * cookie, ssize_t len)
{
	struct handshake_cookie * H = cookie;

	/* This write is no longer pending. */
	H->write_cookie = NULL;

	/* Did we successfully write? */
	if (len < PCRYPT_YH_LEN)
		return (handshakefail(H));

	/*
	 * If we're the server, move on to the final computation.  If we're
	 * the client, we need to read the server's parameter next.
	 */
	if (H->decr)
		return (handshakedone(H));
	else
		return (dhread(H));

	/* NOTREACHED */
}

/* We've got all the bits; do the final computation and callback. */
static int
handshakedone(struct handshake_cookie * H)
{
	struct proto_keys * c;
	struct proto_keys * s;
	int rc;

	/* Sanity-check: There should be no callbacks in progress. */
	assert(H->read_cookie == NULL);
	assert(H->write_cookie == NULL);

	/* Perform the final computation. */
	if (proto_crypt_mkkeys(H->K, H->nonce_local, H->nonce_remote,
	    H->yh_remote, H->x, H->nofps, H->decr, &c, &s))
		goto err0;

	/* Perform the callback. */
	rc = (H->callback)(H->cookie, c, s);

	/* Free the cookie. */
	free(H);

	/* Return status code from callback. */
	return (rc);

err0:
	/* Failure! */
	return (-1);
}

/**
 * proto_handshake_cancel(cookie):
 * Cancel the handshake for which proto_handshake returned ${cookie}.
 */
void
proto_handshake_cancel(void * cookie)
{
	struct handshake_cookie * H = cookie;

	/* Cancel any in-progress network operations. */
	if (H->read_cookie != NULL)
		network_read_cancel(H->read_cookie);
	if (H->write_cookie != NULL)
		network_write_cancel(H->write_cookie);

	/* Free the cookie. */
	free(H);
}
