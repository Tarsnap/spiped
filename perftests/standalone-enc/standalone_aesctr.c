#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "crypto_aes.h"
#include "crypto_aesctr.h"
#include "perftest.h"
#include "warnp.h"

#include "standalone.h"

static int
aesctr_init(void * cookie, uint8_t * buf, size_t buflen)
{
	size_t i;

	(void)cookie; /* UNUSED */

	/* Set the input. */
	for (i = 0; i < buflen; i++)
		buf[i] = (uint8_t)(i & 0xff);

	/* Success! */
	return (0);
}

static int
aesctr_func(void * cookie, uint8_t * buf, size_t buflen, size_t nreps)
{
	struct crypto_aes_key * k_aes = cookie;
	size_t i;

	/* Do the hashing. */
	for (i = 0; i < nreps; i++) {
		/*
		 * In proto_crypt_enc(), we would append the length to buf,
		 * then encrypt the buffer + 4 bytes of length.  For
		 * simplicity, this test does not imitate that details.
		 */
		crypto_aesctr_buf(k_aes, i, buf, buf, buflen);
	}

	/* Success! */
	return (0);
}

/**
 * standalone_aesctr(perfsizes, num_perf, nbytes_perftest, nbytes_warmup):
 * Performance test for AES-CTR.
 */
int
standalone_aesctr(const size_t * perfsizes, size_t num_perf,
    size_t nbytes_perftest, size_t nbytes_warmup)
{
	struct crypto_aes_key * k_aes;
	uint8_t kbuf[32];

	/* Report what we're doing. */
	printf("Testing AES-CTR\n");

	/* Initialize. */
	memset(kbuf, 0, 32);
	if ((k_aes = crypto_aes_key_expand(kbuf, 32)) == NULL)
		goto err0;

	/* Time the function. */
	if (perftest_buffers(nbytes_perftest, perfsizes, num_perf,
	    nbytes_warmup, 0, aesctr_init, aesctr_func, NULL, k_aes)) {
		warn0("perftest_buffers");
		goto err0;
	}

	/* Clean up. */
	crypto_aes_key_free(k_aes);

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (1);
}
