#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "parsenum.h"
#include "warnp.h"

int
main(int argc, char * argv[])
{
	int msec;
	struct timespec ts;

	WARNP_INIT;

	/* Parse command line. */
	if (argc != 2) {
		fprintf(stderr, "usage: %s milliseconds\n", argv[0]);
		goto err0;
	}
	if (PARSENUM(&msec, argv[1], 0, INT_MAX)) {
		warnp("PARSNEUM");
		goto err0;
	}

	/* Sleep for the desired time. */
	ts.tv_sec = msec / 1000;
	ts.tv_nsec = (msec % 1000) * 1000000;
	if (nanosleep(&ts, NULL)) {
		warnp("nanosleep");
		goto err0;
	}

	/* Success! */
	exit(0);

err0:
	/* Failure! */
	exit(1);
}
