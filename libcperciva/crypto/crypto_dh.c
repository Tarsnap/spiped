#include <stdint.h>
#include <string.h>

#include <openssl/bn.h>
#include <openssl/err.h>

#include "warnp.h"

#include "crypto_dh_group14.h"
#include "crypto_entropy.h"

#include "crypto_dh.h"

static int blinded_modexp(uint8_t r[CRYPTO_DH_PUBLEN], BIGNUM * a,
    const uint8_t priv[CRYPTO_DH_PRIVLEN]);

/* Big-endian representation of 2^256. */
static const uint8_t two_exp_256[] = {
	0x01,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
};

/*
 * Big-endian representation of the group #14 modulus with its low byte
 * decremented, i.e. (p - 1) where p = crypto_dh_group14.
 */
static const uint8_t group14_minus_one[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xc9, 0x0f, 0xda, 0xa2, 0x21, 0x68, 0xc2, 0x34,
	0xc4, 0xc6, 0x62, 0x8b, 0x80, 0xdc, 0x1c, 0xd1,
	0x29, 0x02, 0x4e, 0x08, 0x8a, 0x67, 0xcc, 0x74,
	0x02, 0x0b, 0xbe, 0xa6, 0x3b, 0x13, 0x9b, 0x22,
	0x51, 0x4a, 0x08, 0x79, 0x8e, 0x34, 0x04, 0xdd,
	0xef, 0x95, 0x19, 0xb3, 0xcd, 0x3a, 0x43, 0x1b,
	0x30, 0x2b, 0x0a, 0x6d, 0xf2, 0x5f, 0x14, 0x37,
	0x4f, 0xe1, 0x35, 0x6d, 0x6d, 0x51, 0xc2, 0x45,
	0xe4, 0x85, 0xb5, 0x76, 0x62, 0x5e, 0x7e, 0xc6,
	0xf4, 0x4c, 0x42, 0xe9, 0xa6, 0x37, 0xed, 0x6b,
	0x0b, 0xff, 0x5c, 0xb6, 0xf4, 0x06, 0xb7, 0xed,
	0xee, 0x38, 0x6b, 0xfb, 0x5a, 0x89, 0x9f, 0xa5,
	0xae, 0x9f, 0x24, 0x11, 0x7c, 0x4b, 0x1f, 0xe6,
	0x49, 0x28, 0x66, 0x51, 0xec, 0xe4, 0x5b, 0x3d,
	0xc2, 0x00, 0x7c, 0xb8, 0xa1, 0x63, 0xbf, 0x05,
	0x98, 0xda, 0x48, 0x36, 0x1c, 0x55, 0xd3, 0x9a,
	0x69, 0x16, 0x3f, 0xa8, 0xfd, 0x24, 0xcf, 0x5f,
	0x83, 0x65, 0x5d, 0x23, 0xdc, 0xa3, 0xad, 0x96,
	0x1c, 0x62, 0xf3, 0x56, 0x20, 0x85, 0x52, 0xbb,
	0x9e, 0xd5, 0x29, 0x07, 0x70, 0x96, 0x96, 0x6d,
	0x67, 0x0c, 0x35, 0x4e, 0x4a, 0xbc, 0x98, 0x04,
	0xf1, 0x74, 0x6c, 0x08, 0xca, 0x18, 0x21, 0x7c,
	0x32, 0x90, 0x5e, 0x46, 0x2e, 0x36, 0xce, 0x3b,
	0xe3, 0x9e, 0x77, 0x2c, 0x18, 0x0e, 0x86, 0x03,
	0x9b, 0x27, 0x83, 0xa2, 0xec, 0x07, 0xa2, 0x8f,
	0xb5, 0xc5, 0x5d, 0xf0, 0x6f, 0x4c, 0x52, 0xc9,
	0xde, 0x2b, 0xcb, 0xf6, 0x95, 0x58, 0x17, 0x18,
	0x39, 0x95, 0x49, 0x7c, 0xea, 0x95, 0x6a, 0xe5,
	0x15, 0xd2, 0x26, 0x18, 0x98, 0xfa, 0x05, 0x10,
	0x15, 0x72, 0x8e, 0x5a, 0x8a, 0xac, 0xaa, 0x68,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe
};

/**
 * blinded_modexp(r, a, priv):
 * Compute ${r} = ${a}^(2^258 + ${priv}), where ${r} and ${priv} are treated
 * as big-endian integers; and avoid leaking timing data in this process.
 */
static int
blinded_modexp(uint8_t r[CRYPTO_DH_PUBLEN], BIGNUM * a,
    const uint8_t priv[CRYPTO_DH_PRIVLEN])
{
	BIGNUM * two_exp_256_bn;
	BIGNUM * priv_bn;
	uint8_t blinding[CRYPTO_DH_PRIVLEN];
	BIGNUM * blinding_bn;
	BIGNUM * priv_blinded;
	BIGNUM * m_bn;
	BN_CTX * ctx;
	BIGNUM * r1;
	BIGNUM * r2;
	int rlen;

	/* Construct 2^256 in BN representation. */
	if ((two_exp_256_bn = BN_bin2bn(two_exp_256, 33, NULL)) == NULL) {
		warn0("%s", ERR_error_string(ERR_get_error(), NULL));
		goto err0;
	}

	/* Construct 2^258 + ${priv} in BN representation. */
	if ((priv_bn = BN_bin2bn(priv, CRYPTO_DH_PRIVLEN, NULL)) == NULL) {
		warn0("%s", ERR_error_string(ERR_get_error(), NULL));
		goto err1;
	}
	if ((!BN_add(priv_bn, priv_bn, two_exp_256_bn)) ||
	    (!BN_add(priv_bn, priv_bn, two_exp_256_bn)) ||
	    (!BN_add(priv_bn, priv_bn, two_exp_256_bn)) ||
	    (!BN_add(priv_bn, priv_bn, two_exp_256_bn))) {
		warn0("%s", ERR_error_string(ERR_get_error(), NULL));
		goto err2;
	}

	/* Generate blinding exponent. */
	if (crypto_entropy_read(blinding, CRYPTO_DH_PRIVLEN))
		goto err2;
	if ((blinding_bn = BN_bin2bn(blinding,
	    CRYPTO_DH_PRIVLEN, NULL)) == NULL) {
		warn0("%s", ERR_error_string(ERR_get_error(), NULL));
		goto err2;
	}
	if (!BN_add(blinding_bn, blinding_bn, two_exp_256_bn)) {
		warn0("%s", ERR_error_string(ERR_get_error(), NULL));
		goto err3;
	}

	/* Generate blinded exponent. */
	if ((priv_blinded = BN_new()) == NULL) {
		warn0("%s", ERR_error_string(ERR_get_error(), NULL));
		goto err3;
	}
	if (!BN_sub(priv_blinded, priv_bn, blinding_bn)) {
		warn0("%s", ERR_error_string(ERR_get_error(), NULL));
		goto err4;
	}

	/* Construct group #14 modulus in BN representation. */
	if ((m_bn = BN_bin2bn(crypto_dh_group14, 256, NULL)) == NULL) {
		warn0("%s", ERR_error_string(ERR_get_error(), NULL));
		goto err4;
	}

	/* Allocate BN context. */
	if ((ctx = BN_CTX_new()) == NULL) {
		warn0("%s", ERR_error_string(ERR_get_error(), NULL));
		goto err5;
	}

	/* Allocate space for storing results of exponentiations. */
	if ((r1 = BN_new()) == NULL) {
		warn0("%s", ERR_error_string(ERR_get_error(), NULL));
		goto err6;
	}
	if ((r2 = BN_new()) == NULL) {
		warn0("%s", ERR_error_string(ERR_get_error(), NULL));
		goto err7;
	}

	/* Perform modular exponentiations. */
	if (!BN_mod_exp(r1, a, blinding_bn, m_bn, ctx)) {
		warn0("%s", ERR_error_string(ERR_get_error(), NULL));
		goto err8;
	}
	if (!BN_mod_exp(r2, a, priv_blinded, m_bn, ctx)) {
		warn0("%s", ERR_error_string(ERR_get_error(), NULL));
		goto err8;
	}

	/* Compute final result and export to big-endian integer format. */
	if (!BN_mod_mul(r1, r1, r2, m_bn, ctx)) {
		warn0("%s", ERR_error_string(ERR_get_error(), NULL));
		goto err8;
	}
	rlen = BN_num_bytes(r1);
	if (rlen < 0) {
		warn0("Unexpected error in OpenSSL");
		goto err8;
	}
	if (rlen > CRYPTO_DH_PUBLEN) {
		warn0("Exponent result too large!");
		goto err8;
	}
	memset(r, 0, CRYPTO_DH_PUBLEN - (size_t)rlen);
	BN_bn2bin(r1, &r[CRYPTO_DH_PUBLEN - (size_t)rlen]);

	/* Free space allocated by BN_new. */
	BN_clear_free(r2);
	BN_clear_free(r1);

	/* Free context allocated by BN_CTX_new. */
	BN_CTX_free(ctx);

	/* Free space allocated by BN_bin2bn. */
	BN_free(m_bn);

	/* Free space allocated by BN_new. */
	BN_clear_free(priv_blinded);

	/* Free space allocated by BN_bin2bn. */
	BN_clear_free(blinding_bn);
	BN_clear_free(priv_bn);
	BN_free(two_exp_256_bn);

	/* Success! */
	return (0);

err8:
	BN_clear_free(r2);
err7:
	BN_clear_free(r1);
err6:
	BN_CTX_free(ctx);
err5:
	BN_free(m_bn);
err4:
	BN_clear_free(priv_blinded);
err3:
	BN_clear_free(blinding_bn);
err2:
	BN_clear_free(priv_bn);
err1:
	BN_free(two_exp_256_bn);
err0:
	/* Failure! */
	return (-1);
}

/**
 * crypto_dh_generate_pub(pub, priv):
 * Compute ${pub} equal to 2^(2^258 + ${priv}) in Diffie-Hellman group #14.
 */
int
crypto_dh_generate_pub(uint8_t pub[CRYPTO_DH_PUBLEN],
    const uint8_t priv[CRYPTO_DH_PRIVLEN])
{
	BIGNUM * two;

	/* Generate BN representation for 2. */
	if ((two = BN_new()) == NULL) {
		warn0("%s", ERR_error_string(ERR_get_error(), NULL));
		goto err0;
	}
	if (!BN_set_word(two, 2)) {
		warn0("%s", ERR_error_string(ERR_get_error(), NULL));
		goto err1;
	}

	/* Compute pub = two^(2^258 + priv). */
	if (blinded_modexp(pub, two, priv))
		goto err1;

	/* Free storage allocated by BN_new. */
	BN_free(two);

	/* Success! */
	return (0);

err1:
	BN_free(two);
err0:
	/* Failure! */
	return (-1);
}

/**
 * crypto_dh_generate(pub, priv):
 * Generate a 256-bit private key ${priv}, and compute ${pub} equal to
 * 2^(2^258 + ${priv}) mod p where p is the Diffie-Hellman group #14 modulus.
 * Both values are stored as big-endian integers.
 */
int
crypto_dh_generate(uint8_t pub[CRYPTO_DH_PUBLEN],
    uint8_t priv[CRYPTO_DH_PRIVLEN])
{

	/* Generate a random private key. */
	if (crypto_entropy_read(priv, CRYPTO_DH_PRIVLEN))
		goto err0;

	/* Compute the public key. */
	if (crypto_dh_generate_pub(pub, priv))
		goto err0;

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}

/**
 * crypto_dh_compute(pub, priv, key):
 * In the Diffie-Hellman group #14, compute ${pub}^(2^258 + ${priv}) and
 * write the result into ${key}.  All values are big-endian.  Note that the
 * value ${pub} is the public key produced by the call to crypto_dh_generate()
 * made by the *other* participant in the key exchange.
 */
int
crypto_dh_compute(const uint8_t pub[CRYPTO_DH_PUBLEN],
    const uint8_t priv[CRYPTO_DH_PRIVLEN], uint8_t key[CRYPTO_DH_KEYLEN])
{
	BIGNUM * a;

	/* Convert ${pub} into BN representation. */
	if ((a = BN_bin2bn(pub, CRYPTO_DH_PUBLEN, NULL)) == NULL) {
		warn0("%s", ERR_error_string(ERR_get_error(), NULL));
		goto err0;
	}

	/* Compute key = pub^(2^258 + priv). */
	if (blinded_modexp(key, a, priv))
		goto err1;

	/* Free storage allocated by BN_bin2bn. */
	BN_free(a);

	/* Success! */
	return (0);

err1:
	BN_free(a);
err0:
	/* Failure! */
	return (-1);
}

/**
 * crypto_dh_sanitycheck(pub):
 * Sanity-check the Diffie-Hellman public value ${pub} by checking that it
 * is in the range (1, p - 1), where p is the group #14 modulus.  The values
 * 0, 1, and p - 1 produce degenerate shared secrets that are trivially
 * computable by the other party without using the private exponent, so
 * reject them as well as values >= p.  Return 0 if sane, -1 if insane.
 */
int
crypto_dh_sanitycheck(const uint8_t pub[CRYPTO_DH_PUBLEN])
{
	size_t i;
	uint8_t acc;
	uint8_t cmp;

	/* Reject values >= the group modulus. */
	if (memcmp(pub, crypto_dh_group14, 256) >= 0)
		return (-1);

	/*
	 * Reject 0 and 1.  These are byte-wise constant-time checks: 0 is
	 * detected by an OR accumulating to zero, and 1 by an XOR/XOR-OR
	 * comparison against the big-endian encoding of 1.
	 */
	acc = pub[0];
	for (i = 1; i < CRYPTO_DH_PUBLEN; i++)
		acc |= pub[i];
	if (acc == 0)
		return (-1);

	cmp = pub[CRYPTO_DH_PUBLEN - 1] ^ 1;
	for (i = 0; i < CRYPTO_DH_PUBLEN - 1; i++)
		cmp |= pub[i];
	if (cmp == 0)
		return (-1);

	/* Reject p - 1. */
	if (memcmp(pub, group14_minus_one, 256) == 0)
		return (-1);

	/* Value is sane. */
	return (0);
}
