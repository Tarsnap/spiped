#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "perftest.h"
#include "proto_crypt.h"
#include "warnp.h"

#include "standalone.h"

/* Cookie for proto_crypt_enc(). */
struct pce {
	struct proto_keys * k;
};

static int
pce_init(void * cookie, uint8_t * buf, size_t buflen)
{
	struct pce * pce = cookie;
	uint8_t kbuf[64];
	size_t i;

	/* Set up encryption key. */
	memset(kbuf, 0, 64);
	if ((pce->k = mkkeypair(kbuf)) == NULL)
		goto err0;

	/* Set the input. */
	for (i = 0; i < buflen; i++)
		buf[i] = (uint8_t)(i & 0xff);

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}

static int
pce_func(void * cookie, uint8_t * buf, size_t buflen, size_t nreps)
{
	struct pce * pce = cookie;
	uint8_t encbuf[PCRYPT_ESZ];
	size_t i;

	/* Encrypt a bunch of times. */
	for (i = 0; i < nreps; i++)
		proto_crypt_enc(buf, buflen, encbuf, pce->k);

	/* Success! */
	return (0);
}

static int
pce_cleanup(void * cookie)
{
	struct pce * pce = cookie;

	/* Clean up. */
	proto_crypt_free(pce->k);

	/* Success! */
	return (0);
}

/**
 * standalone_pce(perfsizes, num_perf, nbytes_perftest, nbytes_warmup):
 * Performance test for proto_crypt_enc().
 */
int
standalone_pce(const size_t * perfsizes, size_t num_perf,
    size_t nbytes_perftest, size_t nbytes_warmup)
{
	struct pce pce_actual;
	struct pce * pce = &pce_actual;

	/* Report what we're doing. */
	printf("Testing proto_crypt_enc()\n");

	/* Time the function. */
	if (perftest_buffers(nbytes_perftest, perfsizes, num_perf,
	    nbytes_warmup, 0, pce_init, pce_func, pce_cleanup, pce)) {
		warn0("perftest_buffers");
		goto err0;
	}

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (1);
}
