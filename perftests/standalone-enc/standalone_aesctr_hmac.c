#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "crypto_aes.h"
#include "crypto_aesctr.h"
#include "perftest.h"
#include "sha256.h"
#include "sysendian.h"
#include "warnp.h"

#include "standalone.h"

/* Cookie for HMAC_SHA256 with crypto_aesctr. */
struct aesctr_hmac_cookie {
	HMAC_SHA256_CTX * ctx;
	struct crypto_aes_key * k_aes;
};

static int
aesctr_hmac_init(void * cookie, uint8_t * buf, size_t buflen)
{
	struct aesctr_hmac_cookie * ahc = cookie;
	uint8_t kbuf[32];
	size_t i;

	/* (Re-)Initialize the context. */
	memset(kbuf, 0, 32);
	HMAC_SHA256_Init(ahc->ctx, kbuf, 32);

	/* Set the input. */
	for (i = 0; i < buflen; i++)
		buf[i] = (uint8_t)(i & 0xff);

	/* Success! */
	return (0);
}

static int
aesctr_hmac_func(void * cookie, uint8_t * buf, size_t buflen, size_t nreps)
{
	struct aesctr_hmac_cookie * ahc = cookie;
	uint8_t hbuf[32];
	uint8_t pnum_exp[8];
	size_t i;

	/* Do the hashing. */
	for (i = 0; i < nreps; i++) {
		/*
		 * In proto_crypt_enc(), we would append the length to buf,
		 * then encrypt the buffer + 4 bytes of length.  After that,
		 * we'd hash the resulting (larger) buffer.  For simplicity,
		 * this test does not imitate those details.
		 */
		crypto_aesctr_buf(ahc->k_aes, i, buf, buf, buflen);
		HMAC_SHA256_Update(ahc->ctx, buf, buflen);
		be64enc(pnum_exp, i);
		HMAC_SHA256_Update(ahc->ctx, pnum_exp, 8);
	}
	HMAC_SHA256_Final(hbuf, ahc->ctx);

	/* Success! */
	return (0);
}

/**
 * aesctr_hmac_perftest(perfsizes, num_perf, nbytes_perftest, nbytes_warmup):
 * Performance test for AES-CTR followed by HMAC-SHA256.
 */
int
aesctr_hmac_perftest(const size_t * perfsizes, size_t num_perf,
    size_t nbytes_perftest, size_t nbytes_warmup)
{
	struct aesctr_hmac_cookie aesctr_hmac_cookie;
	struct aesctr_hmac_cookie * ahc = &aesctr_hmac_cookie;
	HMAC_SHA256_CTX ctx;
	uint8_t kbuf[32];

	/* Report what we're doing. */
	printf("Testing HMAC_SHA256 with AES-CTR\n");

	/* Initialize. */
	ahc->ctx = &ctx;
	memset(kbuf, 0, 32);
	if ((ahc->k_aes = crypto_aes_key_expand(kbuf, 32)) == NULL)
		goto err0;

	/* Time the function. */
	if (perftest_buffers(nbytes_perftest, perfsizes, num_perf,
	    nbytes_warmup, 0, aesctr_hmac_init, aesctr_hmac_func, NULL, ahc)) {
		warn0("perftest_buffers");
		goto err0;
	}

	/* Clean up. */
	crypto_aes_key_free(ahc->k_aes);

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (1);
}
