#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "perftest.h"
#include "sha256.h"
#include "sysendian.h"
#include "warnp.h"

#include "standalone.h"

static int
hmac_init(void * cookie, uint8_t * buf, size_t buflen)
{
	HMAC_SHA256_CTX * ctx = cookie;
	uint8_t kbuf[32];
	size_t i;

	/* (Re-)Initialize the context. */
	memset(kbuf, 0, 32);
	HMAC_SHA256_Init(ctx, kbuf, 32);

	/* Set the input. */
	for (i = 0; i < buflen; i++)
		buf[i] = (uint8_t)(i & 0xff);

	/* Success! */
	return (0);
}

static int
hmac_func(void * cookie, uint8_t * buf, size_t buflen, size_t nreps)
{
	HMAC_SHA256_CTX * ctx = cookie;
	uint8_t hbuf[32];
	uint8_t pnum_exp[8];
	size_t i;

	/* Do the hashing. */
	for (i = 0; i < nreps; i++) {
		HMAC_SHA256_Update(ctx, buf, buflen);

		/* Hash the iteration number as well. */
		be64enc(pnum_exp, i);
		HMAC_SHA256_Update(ctx, pnum_exp, 8);
	}
	HMAC_SHA256_Final(hbuf, ctx);

	/* Success! */
	return (0);
}

/**
 * standalone_hmac(perfsizes, num_perf, nbytes_perftest, nbytes_warmup):
 * Performance test for HMAC-SHA256.
 */
int
standalone_hmac(const size_t * perfsizes, size_t num_perf,
    size_t nbytes_perftest, size_t nbytes_warmup)
{
	HMAC_SHA256_CTX ctx;

	/* Report what we're doing. */
	printf("Testing HMAC_SHA256 with iteration numbers\n");

	/* Time the function. */
	if (perftest_buffers(nbytes_perftest, perfsizes, num_perf,
	    nbytes_warmup, 0, hmac_init, hmac_func, NULL, &ctx)) {
		warn0("perftest_buffers");
		goto err0;
	}

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (1);
}
