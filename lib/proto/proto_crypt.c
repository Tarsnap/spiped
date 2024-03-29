#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "crypto_aes.h"
#include "crypto_aesctr.h"
#include "crypto_verify_bytes.h"
#include "insecure_memzero.h"
#include "sha256.h"
#include "sysendian.h"
#include "warnp.h"

#include "proto_crypt.h"

struct proto_secret {
	uint8_t K[32];
};

struct proto_keys {
	struct crypto_aes_key * k_aes;
	HMAC_SHA256_CTX ctx_init;
	uint64_t pnum;
};

/**
 * mkkeypair(kbuf):
 * Convert the 64 bytes of ${kbuf} into a protocol key structure.
 */
#ifdef STANDALONE_ENC_TESTING
struct proto_keys *
mkkeypair(uint8_t kbuf[64])
#else
static struct proto_keys *
mkkeypair(uint8_t kbuf[64])
#endif
{
	struct proto_keys * k;

	/* Allocate a structure. */
	if ((k = malloc(sizeof(struct proto_keys))) == NULL)
		goto err0;

	/* Expand the AES key. */
	if ((k->k_aes = crypto_aes_key_expand(&kbuf[0], 32)) == NULL)
		goto err1;

	/* Initialize the HMAC_SHA256 context. */
	HMAC_SHA256_Init(&k->ctx_init, &kbuf[32], 32);

	/* The first packet will be packet number zero. */
	k->pnum = 0;

	/* Success! */
	return (k);

err1:
	free(k);
err0:
	/* Failure! */
	return (NULL);
}

/**
 * proto_crypt_secret(filename):
 * Read the key file ${filename} and return a protocol secret structure.
 */
struct proto_secret *
proto_crypt_secret(const char * filename)
{
	SHA256_CTX ctx;
	FILE * f;
	struct proto_secret * K;
	uint8_t buf[BUFSIZ];
	size_t lenread;

	/* Allocate a protocol secret structure. */
	if ((K = malloc(sizeof(struct proto_secret))) == NULL)
		goto err0;

	/* Open the file, or use stdin if requested. */
	if (strcmp(filename, STDIN_FILENAME) == 0) {
		f = stdin;
	} else if ((f = fopen(filename, "r")) == NULL) {
		warnp("Cannot open file: %s", filename);
		goto err1;
	}

	/* Initialize the SHA256 hash context. */
	SHA256_Init(&ctx);

	/* Read the file until we hit EOF. */
	while ((lenread = fread(buf, 1, BUFSIZ, f)) > 0)
		SHA256_Update(&ctx, buf, lenread);

	/* Did we hit EOF? */
	if (!feof(f)) {
		if (f == stdin) {
			warnp("Error reading from stdin");
		} else {
			warnp("Error reading file: %s", filename);
		}

		goto err2;
	}

	/* Close the file if it isn't stdin. */
	if ((f != stdin) && fclose(f))
		warnp("fclose");

	/* Compute the final hash and wipe context state. */
	SHA256_Final(K->K, &ctx);

	/* Success! */
	return (K);

err2:
	/* Close the file if it isn't stdin. */
	if ((f != stdin) && fclose(f))
		warnp("fclose");

	/* Wipe context state. */
	SHA256_Final(K->K, &ctx);
err1:
	proto_crypt_secret_free(K);
err0:
	/* Failure! */
	return (NULL);
}

/**
 * proto_crypt_dhmac(K, nonce_l, nonce_r, dhmac_l, dhmac_r, decr):
 * Using the protocol secret ${K}, and the local and remote nonces ${nonce_l}
 * and ${nonce_r}, compute the local and remote diffie-hellman parameter MAC
 * keys ${dhmac_l} and ${dhmac_r}.  If ${decr} is non-zero, "local" == "S"
 * and "remote" == "C"; otherwise the assignments are opposite.
 */
void
proto_crypt_dhmac(const struct proto_secret * K,
    const uint8_t nonce_l[PCRYPT_NONCE_LEN],
    const uint8_t nonce_r[PCRYPT_NONCE_LEN],
    uint8_t dhmac_l[PCRYPT_DHMAC_LEN], uint8_t dhmac_r[PCRYPT_DHMAC_LEN],
    int decr)
{
	uint8_t nonce_CS[PCRYPT_NONCE_LEN * 2];
	uint8_t dk_1[PCRYPT_DHMAC_LEN * 2];
	const uint8_t * nonce_c, * nonce_s;
	uint8_t * dhmac_c, * dhmac_s;

	/* Figure out how {c, s} maps to {l, r}. */
	nonce_c = decr ? nonce_r : nonce_l;
	dhmac_c = decr ? dhmac_r : dhmac_l;
	nonce_s = decr ? nonce_l : nonce_r;
	dhmac_s = decr ? dhmac_l : dhmac_r;

	/* Copy in nonces (in the right order). */
	memcpy(&nonce_CS[0], nonce_c, PCRYPT_NONCE_LEN);
	memcpy(&nonce_CS[PCRYPT_NONCE_LEN], nonce_s, PCRYPT_NONCE_LEN);

	/* Compute dk_1. */
	PBKDF2_SHA256(K->K, 32, nonce_CS, PCRYPT_NONCE_LEN * 2, 1,
	    dk_1, PCRYPT_DHMAC_LEN * 2);

	/* Copy out diffie-hellman parameter MAC keys (in the right order). */
	memcpy(dhmac_c, &dk_1[0], PCRYPT_DHMAC_LEN);
	memcpy(dhmac_s, &dk_1[PCRYPT_DHMAC_LEN], PCRYPT_DHMAC_LEN);
}

/**
 * is_not_one(x, len):
 * Return non-zero if the big-endian value stored at (${x}, ${len}) is not
 * equal to 1.
 */
static int
is_not_one(const uint8_t * x, size_t len)
{
	size_t i;
	char y;

	for (i = 0, y = 0; i < len - 1; i++) {
		y |= x[i];
	}

	return (y | (x[len - 1] - 1));
}

/**
 * proto_crypt_dh_validate(yh_r, dhmac_r, requirepfs):
 * Return non-zero if the value ${yh_r} received from the remote party is not
 * correctly MACed using the diffie-hellman parameter MAC key ${dhmac_r}, or
 * if the included y value is >= the diffie-hellman group modulus, or if
 * ${requirepfs} is non-zero and the included y value is 1.
 */
int
proto_crypt_dh_validate(const uint8_t yh_r[PCRYPT_YH_LEN],
    const uint8_t dhmac_r[PCRYPT_DHMAC_LEN], int requirepfs)
{
	uint8_t hbuf[32];

	/* Compute HMAC. */
	HMAC_SHA256_Buf(dhmac_r, PCRYPT_DHMAC_LEN, yh_r, CRYPTO_DH_PUBLEN,
	    hbuf);

	/* Check that the MAC matches. */
	if (crypto_verify_bytes(&yh_r[CRYPTO_DH_PUBLEN], hbuf, 32))
		return (1);

	/* Sanity-check the diffie-hellman value. */
	if (crypto_dh_sanitycheck(&yh_r[0]))
		return (1);

	/* If necessary, enforce that the diffie-hellman value is != 1. */
	if (requirepfs) {
		if (! is_not_one(&yh_r[0], CRYPTO_DH_PUBLEN))
			return (1);
	}

	/* Everything is good. */
	return (0);
}

/**
 * proto_crypt_dh_generate(yh_l, x, dhmac_l, nopfs):
 * Using the MAC key ${dhmac_l}, generate the MACed diffie-hellman handshake
 * parameter ${yh_l}.  Store the diffie-hellman private value in ${x}.  If
 * ${nopfs} is non-zero, skip diffie-hellman generation and use y = 1.
 */
int
proto_crypt_dh_generate(uint8_t yh_l[PCRYPT_YH_LEN], uint8_t x[PCRYPT_X_LEN],
    const uint8_t dhmac_l[PCRYPT_DHMAC_LEN], int nopfs)
{

	/* Are we skipping the diffie-hellman generation? */
	if (nopfs) {
		/* Set y_l to a big-endian 1. */
		memset(yh_l, 0, CRYPTO_DH_PUBLEN - 1);
		yh_l[CRYPTO_DH_PUBLEN - 1] = 1;
	} else {
		/* Generate diffie-hellman parameters x and y. */
		if (crypto_dh_generate(yh_l, x))
			goto err0;
	}

	/* Append an HMAC. */
	HMAC_SHA256_Buf(dhmac_l, PCRYPT_DHMAC_LEN, yh_l, CRYPTO_DH_PUBLEN,
	    &yh_l[CRYPTO_DH_PUBLEN]);

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}

/**
 * proto_crypt_mkkeys(K, nonce_l, nonce_r, yh_r, x, nopfs, decr, eh_c, eh_s):
 * Using the protocol secret ${K}, the local and remote nonces ${nonce_l} and
 * ${nonce_r}, the remote MACed diffie-hellman handshake parameter ${yh_r},
 * and the local diffie-hellman secret ${x}, generate the keys ${eh_c} and
 * ${eh_s}.  If ${nopfs} is non-zero, we are performing weak handshaking and
 * y_SC is set to 1 rather than being computed.  If ${decr} is non-zero,
 * "local" == "S" and "remote" == "C"; otherwise the assignments are opposite.
 */
int
proto_crypt_mkkeys(const struct proto_secret * K,
    const uint8_t nonce_l[PCRYPT_NONCE_LEN],
    const uint8_t nonce_r[PCRYPT_NONCE_LEN],
    const uint8_t yh_r[PCRYPT_YH_LEN], const uint8_t x[PCRYPT_X_LEN],
    int nopfs, int decr,
    struct proto_keys ** eh_c, struct proto_keys ** eh_s)
{
	uint8_t nonce_y[PCRYPT_NONCE_LEN * 2 + CRYPTO_DH_KEYLEN];
	uint8_t dk_2[128];
	const uint8_t * nonce_c, * nonce_s;

	/* Copy in nonces (in the right order). */
	nonce_c = decr ? nonce_r : nonce_l;
	nonce_s = decr ? nonce_l : nonce_r;
	memcpy(&nonce_y[0], nonce_c, PCRYPT_NONCE_LEN);
	memcpy(&nonce_y[PCRYPT_NONCE_LEN], nonce_s, PCRYPT_NONCE_LEN);

	/* Are we bypassing the diffie-hellman computation? */
	if (nopfs) {
		/* We sent y_l = 1, so y_SC is also 1. */
		memset(&nonce_y[PCRYPT_NONCE_LEN * 2], 0,
		    CRYPTO_DH_KEYLEN - 1);
		nonce_y[PCRYPT_NONCE_LEN * 2 + CRYPTO_DH_KEYLEN - 1] = 1;
	} else {
		/* Perform the diffie-hellman computation. */
		if (crypto_dh_compute(yh_r, x,
		    &nonce_y[PCRYPT_NONCE_LEN * 2]))
			goto err0;
	}

	/* Compute dk_2. */
	PBKDF2_SHA256(K->K, 32, nonce_y,
	    PCRYPT_NONCE_LEN * 2 + CRYPTO_DH_KEYLEN, 1, dk_2, 128);

	/* Create key structures. */
	if ((*eh_c = mkkeypair(&dk_2[0])) == NULL)
		goto err0;
	if ((*eh_s = mkkeypair(&dk_2[64])) == NULL)
		goto err1;

	/* Success! */
	return (0);

err1:
	proto_crypt_free(*eh_c);
err0:
	/* Failure! */
	return (-1);
}

/**
 * proto_crypt_enc(ibuf, len, obuf, k):
 * Encrypt ${len} bytes from ${ibuf} into PCRYPT_ESZ bytes using the keys in
 * ${k}, and write the result into ${obuf}.
 */
void
proto_crypt_enc(uint8_t * ibuf, size_t len, uint8_t obuf[PCRYPT_ESZ],
    struct proto_keys * k)
{
	HMAC_SHA256_CTX ctx;
	uint8_t pnum_exp[8];

	/* Sanity-check the length. */
	assert(len <= PCRYPT_MAXDSZ);

	/* Copy the decrypted data into the encrypted buffer. */
	memcpy(obuf, ibuf, len);

	/* Pad up to PCRYPT_MAXDSZ with zeroes. */
	memset(&obuf[len], 0, PCRYPT_MAXDSZ - len);

	/* Add the length. */
	be32enc(&obuf[PCRYPT_MAXDSZ], (uint32_t)len);

	/* Encrypt the buffer in-place. */
	crypto_aesctr_buf(k->k_aes, k->pnum, obuf, obuf, PCRYPT_MAXDSZ + 4);

	/* Copy the original (initialized) context. */
	memcpy(&ctx, &k->ctx_init, sizeof(HMAC_SHA256_CTX));

	/* Append an HMAC. */
	be64enc(pnum_exp, k->pnum);
	HMAC_SHA256_Update(&ctx, obuf, PCRYPT_MAXDSZ + 4);
	HMAC_SHA256_Update(&ctx, pnum_exp, 8);
	HMAC_SHA256_Final(&obuf[PCRYPT_MAXDSZ + 4], &ctx);

	/* Increment packet number. */
	k->pnum += 1;
}

/**
 * proto_crypt_dec(ibuf, obuf, k):
 * Decrypt PCRYPT_ESZ bytes from ${ibuf} using the keys in ${k}.  If the data
 * is valid, write it into ${obuf} and return the length; otherwise, return
 * -1.
 */
ssize_t
proto_crypt_dec(uint8_t ibuf[PCRYPT_ESZ], uint8_t * obuf,
    struct proto_keys * k)
{
	HMAC_SHA256_CTX ctx;
	uint8_t hbuf[32];
	uint8_t pnum_exp[8];
	size_t len;

	/* Copy the original (initialized) context. */
	memcpy(&ctx, &k->ctx_init, sizeof(HMAC_SHA256_CTX));

	/* Verify HMAC. */
	be64enc(pnum_exp, k->pnum);
	HMAC_SHA256_Update(&ctx, ibuf, PCRYPT_MAXDSZ + 4);
	HMAC_SHA256_Update(&ctx, pnum_exp, 8);
	HMAC_SHA256_Final(hbuf, &ctx);
	if (crypto_verify_bytes(hbuf, &ibuf[PCRYPT_MAXDSZ + 4], 32))
		return (-1);

	/* Decrypt the buffer in-place. */
	crypto_aesctr_buf(k->k_aes, k->pnum, ibuf, ibuf, PCRYPT_MAXDSZ + 4);

	/* Increment packet number. */
	k->pnum += 1;

	/* Parse length. */
	len = be32dec(&ibuf[PCRYPT_MAXDSZ]);

	/* Make sure nobody is being evil here... */
	if ((len == 0) || (len > PCRYPT_MAXDSZ))
		return (-1);

	/* Copy the bytes into the output buffer. */
	memcpy(obuf, ibuf, len);

	/* Return the decrypted length. */
	return ((ssize_t)len);
}

/**
 * proto_crypt_secret_free(K):
 * Free the protocol secret structure ${K}.
 */
void
proto_crypt_secret_free(struct proto_secret * K)
{

	/* Be compatible with free(NULL). */
	if (K == NULL)
		return;

	/* Clear secret from the memory. */
	insecure_memzero(K, sizeof(struct proto_secret));

	/* Free the key structure. */
	free(K);
}

/**
 * proto_crypt_free(k):
 * Free the protocol key structure ${k}.
 */
void
proto_crypt_free(struct proto_keys * k)
{

	/* Be compatible with free(NULL). */
	if (k == NULL)
		return;

	/* Free the AES key. */
	crypto_aes_key_free(k->k_aes);

	/* Free the key structure. */
	free(k);
}
