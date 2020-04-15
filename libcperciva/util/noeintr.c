#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <unistd.h>

#include "noeintr.h"

/**
 * noeintr_write(d, buf, nbytes):
 * Write ${nbytes} bytes of data from ${buf} to the file descriptor ${d} per
 * the write(2) system call, but looping until completion if interrupted by
 * a signal.  Return ${nbytes} on success or -1 on error.
 */
ssize_t
noeintr_write(int d, const void * buf, size_t nbytes)
{
	const uint8_t * p = buf;
	size_t len = nbytes;
	ssize_t lenwrit;

	/* Implementation-defined: Don't allow oversized writes. */
	assert(nbytes <= SSIZE_MAX);

	/* Loop until we have no data left to write. */
	while (len > 0) {
		if ((lenwrit = write(d, p, len)) == -1) {
			/* EINTR is harmless. */
			if (errno == EINTR)
				continue;

			/* Anything else isn't. */
			goto err0;
		}

		/* Sanity check. */
		assert(lenwrit >= 0);
		assert(lenwrit <= (ssize_t)len);

		/*
		 * We might have done a partial write; advance the buffer
		 * pointer and adjust the remaining write length.
		 */
		p += (size_t)lenwrit;
		len -= (size_t)lenwrit;
	}

	/* Success! */
	return (ssize_t)(nbytes);

err0:
	/* Failure! */
	return (-1);
}
