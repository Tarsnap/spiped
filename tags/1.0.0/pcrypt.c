#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/aes.h>

#include "crypto_aesctr.h"
#include "crypto_verify_bytes.h"
#include "sha256.h"
#include "sysendian.h"
#include "warnp.h"

#include "pcrypt.h"

struct proto_keys {
	AES_KEY k_aes;
	uint8_t k_hmac[32];
	uint64_t pnum;
};

/**
 * mkkeypair(kbuf):
 * Convert the 64 bytes of ${kbuf} into a protocol key structure.
 */
static struct proto_keys *
mkkeypair(uint8_t kbuf[64])
{
	struct proto_keys * k;

	/* Allocate a structure. */
	if ((k = malloc(sizeof(struct proto_keys))) == NULL)
		goto err0;

	/* Expand the AES key. */
	if (AES_set_encrypt_key(&kbuf[0], 256, &k->k_aes)) {
		warn0("Error in AES_set_encrypt_key");
		goto err1;
	}

	/* Fill in HMAC key. */
	memcpy(k->k_hmac, &kbuf[32], 32);

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
 * pcrypt_dhmac(K, nonce_l, nonce_r, dhmac_l, dhmac_r, decr):
 * Using the key file hash ${K}, and the local and remote nonces ${nonce_l}
 * and ${nonce_r}, compute the local and remote diffie-hellman parameter MAC
 * keys ${dhmac_l} and ${dhmac_r}.  If ${decr} is non-zero, "local" == "S"
 * and "remote" == "C"; otherwise the assignments are opposite.
 */
void pcrypt_dhmac(const uint8_t K[32],
    const uint8_t nonce_l[PCRYPT_NONCE_LEN],
    const uint8_t nonce_r[PCRYPT_NONCE_LEN],
    uint8_t dhmac_l[PCRYPT_DHMAC_LEN], uint8_t dhmac_r[PCRYPT_DHMAC_LEN],
    int decr)
{
	uint8_t nonce_CS[PCRYPT_NONCE_LEN * 2];
	uint8_t dk_1[PCRYPT_DHMAC_LEN * 2];

	/* Copy in nonces (in the right order). */
	memcpy(&nonce_CS[0], decr ? nonce_r : nonce_l, PCRYPT_NONCE_LEN);
	memcpy(&nonce_CS[PCRYPT_NONCE_LEN], decr ? nonce_l : nonce_r,
	    PCRYPT_NONCE_LEN);

	/* Compute dk_1. */
	PBKDF2_SHA256(K, 32, nonce_CS, PCRYPT_NONCE_LEN * 2, 1,
	    dk_1, PCRYPT_DHMAC_LEN * 2);

	/* Copy out diffie-hellman parameter MAC keys (in the right order). */
	memcpy(decr ? dhmac_r : dhmac_l, &dk_1[0], PCRYPT_DHMAC_LEN);
	memcpy(decr ? dhmac_l : dhmac_r, &dk_1[PCRYPT_DHMAC_LEN],
	    PCRYPT_DHMAC_LEN);
}

/**
 * pcrypt_dh_validate(yh_r, dhmac_r):
 * Return non-zero if the value ${yh_r} received from the remote party is not
 * correctly MACed using the diffie-hellman parameter MAC key ${dhmac_r}, or
 * if the included y value is >= the diffie-hellman group modulus.
 */
int
pcrypt_dh_validate(const uint8_t yh_r[PCRYPT_YH_LEN],
    const uint8_t dhmac_r[PCRYPT_DHMAC_LEN])
{
	uint8_t hbuf[32];

	/* Compute HMAC. */
	HMAC_SHA256_Buf(dhmac_r, PCRYPT_DHMAC_LEN, yh_r, CRYPTO_DH_PUBLEN,
	    hbuf);

	/* Check that the MAC matches. */
	if (crypto_verify_bytes(&yh_r[CRYPTO_DH_PUBLEN], hbuf, 32))
		return (1);

	/* Sanity-check the diffie-hellman value. */
	return (crypto_dh_sanitycheck(&yh_r[0]));
}

/**
 * pcrypt_dh_generate(yh_l, x, dhmac_l, nofps):
 * Using the MAC key ${dhmac_l}, generate the MACed diffie-hellman handshake
 * parameter ${yh_l}.  Store the diffie-hellman private value in ${x}.  If
 * ${nofps} is non-zero, skip diffie-hellman generation and use y = 1.
 */
int
pcrypt_dh_generate(uint8_t yh_l[PCRYPT_YH_LEN], uint8_t x[PCRYPT_X_LEN],
    const uint8_t dhmac_l[PCRYPT_DHMAC_LEN], int nofps)
{

	/* Are we skipping the diffie-hellman generation? */
	if (nofps) {
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
 * pcrypt_mkkeys(K, nonce_l, nonce_r, yh_r, x, nofps, decr, eh_c, eh_s):
 * Using the key file hash ${K}, the local and remote nonces ${nonce_l} and
 * ${nonce_r}, the remote MACed diffie-hellman handshake paramter ${yh_r},
 * and the local diffie-hellman secret ${x}, generate the keys ${eh_c} and
 * ${eh_s}.  If ${nofps} is non-zero, we are performing weak handshaking and
 * y_SC is set to 1 rather than being computed.  If ${decr} is non-zero,
 * "local" == "S" and "remote" == "C"; otherwise the assignments are opposite.
 */
int
pcrypt_mkkeys(const uint8_t K[32], const uint8_t nonce_l[PCRYPT_NONCE_LEN],
    const uint8_t nonce_r[PCRYPT_NONCE_LEN],
    const uint8_t yh_r[PCRYPT_YH_LEN], const uint8_t x[PCRYPT_X_LEN],
    int nofps, int decr,
    struct proto_keys ** eh_c, struct proto_keys ** eh_s)
{
	uint8_t nonce_y[PCRYPT_NONCE_LEN * 2 + CRYPTO_DH_KEYLEN];
	uint8_t dk_2[128];

	/* Copy in nonces (in the right order). */
	memcpy(&nonce_y[0], decr ? nonce_r : nonce_l, PCRYPT_NONCE_LEN);
	memcpy(&nonce_y[PCRYPT_NONCE_LEN], decr ? nonce_l : nonce_r,
	    PCRYPT_NONCE_LEN);

	/* Are we bypassing the diffie-hellman computation? */
	if (nofps) {
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
	PBKDF2_SHA256(K, 32, nonce_y,
	    PCRYPT_NONCE_LEN * 2 + CRYPTO_DH_KEYLEN, 1, dk_2, 128);

	/* Create key structures. */
	if ((*eh_c = mkkeypair(&dk_2[0])) == NULL)
		goto err0;
	if ((*eh_s = mkkeypair(&dk_2[64])) == NULL)
		goto err1;

	/* Success! */
	return (0);

err1:
	pcrypt_free(*eh_c);
err0:
	/* Failure! */
	return (-1);
}

/**
 * pcrypt_enc(ibuf, len, obuf, k):
 * Encrypt ${len} bytes from ${ibuf} into PCRYPT_ESZ bytes using the keys in
 * ${k}, and write the result into ${obuf}.
 */
void
pcrypt_enc(uint8_t * ibuf, size_t len, uint8_t obuf[PCRYPT_ESZ],
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
	be32enc(&obuf[PCRYPT_MAXDSZ], len);

	/* Encrypt the buffer in-place. */
	crypto_aesctr_buf(&k->k_aes, k->pnum, obuf, obuf, PCRYPT_MAXDSZ + 4);

	/* Append an HMAC. */
	be64enc(pnum_exp, k->pnum);
	HMAC_SHA256_Init(&ctx, k->k_hmac, 32);
	HMAC_SHA256_Update(&ctx, obuf, PCRYPT_MAXDSZ + 4);
	HMAC_SHA256_Update(&ctx, pnum_exp, 8);
	HMAC_SHA256_Final(&obuf[PCRYPT_MAXDSZ + 4], &ctx);

	/* Increment packet number. */
	k->pnum += 1;
}

/**
 * pcrypt_dec(ibuf, obuf, k):
 * Decrypt PCRYPT_ESZ bytes from ${ibuf} using the keys in ${k}.  If the data
 * is valid, write it into ${obuf} and return the length; otherwise, return
 * -1.
 */
ssize_t pcrypt_dec(uint8_t ibuf[PCRYPT_ESZ], uint8_t * obuf,
    struct proto_keys * k)
{
	HMAC_SHA256_CTX ctx;
	uint8_t hbuf[32];
	uint8_t pnum_exp[8];
	size_t len;

	/* Verify HMAC. */
	be64enc(pnum_exp, k->pnum);
	HMAC_SHA256_Init(&ctx, k->k_hmac, 32);
	HMAC_SHA256_Update(&ctx, ibuf, PCRYPT_MAXDSZ + 4);
	HMAC_SHA256_Update(&ctx, pnum_exp, 8);
	HMAC_SHA256_Final(hbuf, &ctx);
	if (crypto_verify_bytes(hbuf, &ibuf[PCRYPT_MAXDSZ + 4], 32))
		return (-1);

	/* Decrypt the buffer in-place. */
	crypto_aesctr_buf(&k->k_aes, k->pnum, ibuf, ibuf, PCRYPT_MAXDSZ + 4);

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
	return (len);
}

/**
 * pcrypt_free(k):
 * Free the protocol key structure ${k}.
 */
void
pcrypt_free(struct proto_keys * k)
{

	/* Free the key structure. */
	free(k);
}
