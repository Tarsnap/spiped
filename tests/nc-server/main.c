#include <sys/time.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "millisleep.h"
#include "monoclock.h"
#include "parsenum.h"
#include "warnp.h"

#include "simple_server.h"

#define MAX_CONNECTIONS 2
#define SHUTDOWN_AFTER 1

struct nc_cookie {
	FILE * out;
	size_t bps;		/* Average bytes per second to send. */
};

/* Send a message, limited to bytes per second.  Send in bursts of 10ms. */
static int
write_bps(int sock, uint8_t * buf, size_t buflen, size_t bps)
{
	size_t remaining = buflen;
	size_t goal_send;
	size_t actual_sent = 0;
	size_t to_send;
	struct timeval orig, now;

	/* Get initial time. */
	if (monoclock_get(&orig)) {
		warn0("monoclock_get");
		goto err0;
	}

	do {
		/* Wait 10ms. */
		millisleep(10);

		/* How much data should we have sent by now? */
		if (monoclock_get(&now)) {
			warn0("monoclock_get");
			goto err0;
		}
		goal_send = (size_t)((double)bps * timeval_diff(orig, now));

		/* If clocks did something really weird, loop again. */
		if (goal_send <= actual_sent)
			continue;

		/* How data should we send to reach the time-based goal? */
		to_send = goal_send - actual_sent;
		if (to_send > remaining)
			to_send = remaining;

		/* Send a burst. */
		if (write(sock, buf, to_send) == -1) {
			warnp("write");
			goto err0;
		}

		/* Record effect of sending. */
		remaining -= to_send;
		buf += to_send;
		actual_sent += to_send;
	} while (remaining > 0);

err0:
	/* Failure! */
	return (-1);
}

/* A client sent a message. */
static int
callback_snc_response(void * cookie, uint8_t * buf, size_t buflen, int sock)
{
	struct nc_cookie * C = cookie;

	/* Write buffer to the previously-opened file. */
	if (fwrite(buf, sizeof(uint8_t), buflen, C->out) != buflen) {
		warnp("fwrite");
		goto err0;
	}

	/* Echo to the client (if applicable).  */
	if ((C->bps > 0) && (write_bps(sock, buf, buflen, C->bps)))
		goto err0;

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}

int
main(int argc, char ** argv)
{
	struct nc_cookie cookie;
	struct nc_cookie * C = &cookie;
	const char * sockname;
	const char * filename;

	WARNP_INIT;

	/* Parse command-line arguments. */
	if (argc < 3) {
		fprintf(stderr, "usage: %s ADDRESS FILENAME [ECHO_BPS]\n",
		    argv[0]);
		goto err0;
	}
	sockname = argv[1];
	filename = argv[2];
	if (argc > 3) {
		/* Allow up to 1 MB per second of echoing. */
		if (PARSENUM(&C->bps, argv[3], 0, 1000000)) {
			warnp("parsenum");
			goto err0;
		}
		if ((C->bps % 100) != 0) {
			warn0("BPS must be a multiple of 100");
			goto err0;
		}
	} else
		C->bps = 0;

	/* Open the output file; can be /dev/null. */
	if ((C->out = fopen(filename, "wb")) == NULL) {
		warnp("fopen");
		goto err0;
	}

	/* Run the server. */
	if (simple_server(sockname, MAX_CONNECTIONS, SHUTDOWN_AFTER,
	    &callback_snc_response, C)) {
		warn0("simple_server failed");
		goto err1;
	}

	/* Write the output file. */
	if (fclose(C->out) != 0) {
		warnp("fclose");
		goto err0;
	}

	/* Success! */
	exit(0);

err1:
	if (fclose(C->out))
		warnp("fclose");
err0:
	/* Failure! */
	exit(1);
}
