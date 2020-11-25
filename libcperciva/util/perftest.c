#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "monoclock.h"
#include "warnp.h"

#include "perftest.h"

/**
 * perftest_buffers(nbytes, sizes, nsizes, nbytes_warmup, init_func, func,
 *     cookie):
 * Time using ${func} to process ${nbytes} bytes in blocks of ${sizes}.
 * Before timing any block sizes, process ${nbytes_warmup} bytes with the
 * maximum size in ${sizes}.  Invoke callback functions as:
 *     init_func(cookie, buffer, buflen)
 *     func(cookie, buffer, buflen, nbuffers)
 * where ${buffer} is large enough to hold the maximum buffer size.  Print
 * the time and speed of processing each buffer size.
 */
int
perftest_buffers(size_t nbytes, const size_t * sizes, size_t nsizes,
    size_t nbytes_warmup,
    int init_func(void * cookie, uint8_t * buf, size_t buflen),
    int func(void * cookie, uint8_t * buf, size_t buflen, size_t num_buffers),
    void * cookie)
{
	uint8_t * buf;
	struct timeval begin, end;
	double * delta_s;
	double speed;
	size_t i;
	size_t buflen;
	size_t num_buffers;
	size_t nbuffers_warmup;
	size_t nbytes_in_buffer;
	size_t max_buflen = 0;

	/* Find the maximum buffer size. */
	for (i = 0; i < nsizes; i++) {
		if (max_buflen < sizes[i])
			max_buflen = sizes[i];
	}
	assert(max_buflen > 0);

	/* Allocate buffers. */
	if ((buf = malloc(max_buflen)) == NULL) {
		warnp("malloc");
		goto err0;
	}
	if ((delta_s = malloc(nsizes * sizeof(double))) == NULL) {
		warnp("malloc");
		goto err1;
	}

	/* Warm up */
	nbuffers_warmup = nbytes_warmup / max_buflen;
	if (init_func(cookie, buf, max_buflen))
		goto err2;
	if (func(cookie, buf, max_buflen, nbuffers_warmup))
		goto err2;

	/* Run operations. */
	for (i = 0; i < nsizes; i++) {
		/* Configure and sanity checks. */
		buflen = sizes[i];
		assert(buflen > 0);
		num_buffers = nbytes / buflen;

		/* Set up. */
		if (init_func(cookie, buf, buflen))
			goto err2;

		/* Get beginning time. */
		if (monoclock_get_cputime(&begin)) {
			warnp("monoclock_get_cputime()");
			goto err2;
		}

		/* Time actual code. */
		if (func(cookie, buf, buflen, num_buffers))
			goto err2;

		/* Get ending time. */
		if (monoclock_get_cputime(&end)) {
			warnp("monoclock_get_cputime()");
			goto err2;
		}

		/* Store time. */
		delta_s[i] = timeval_diff(begin, end);
	}

	/* Print output. */
	for (i = 0; i < nsizes; i++) {
		buflen = sizes[i];
		num_buffers = nbytes / buflen;

		/* We might not be processing an integer number of buffers. */
		nbytes_in_buffer = buflen * num_buffers;
		speed = (double)nbytes_in_buffer / 1e6 / delta_s[i];

		/* Print output. */
		printf("%zu blocks of size %zu\t%.06f s\t%.06f MB/s\n",
		    num_buffers, buflen, delta_s[i], speed);
	}

	/* Clean up. */
	free(delta_s);
	free(buf);

	/* Success! */
	return (0);

err2:
	free(delta_s);
err1:
	free(buf);
err0:
	/* Failure! */
	return (-1);
}
