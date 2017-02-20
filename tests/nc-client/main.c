#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "events.h"
#include "network.h"
#include "sock.h"
#include "warnp.h"

struct senddata {
	char * buffer;
	ssize_t nchars;
	int socket;
	int conndone;
	void * connect_cookie;
	void * write_cookie;
};

/* Forward declaration. */
static int callback_wrote(void *, ssize_t);

/* Send data from stdin to a socket, or close the connection. */
static int
send_input(void * cookie)
{
	struct senddata * send = (struct senddata *)cookie;
	size_t len = 0;

	/* Read data from stdin. */
	if ((send->nchars = getline(&send->buffer, &len, stdin)) != -1) {
		/* Send data to server. */
		if ((send->write_cookie = network_write(send->socket,
		    (uint8_t *)send->buffer, (size_t)send->nchars,
		    (size_t)send->nchars, callback_wrote, cookie)) == NULL) {
			warn0("network_write failure");
			goto err0;
		}
	} else {
		/* Close connection without sending anything. */
		send->conndone = 1;

		/* If we didn't get an EOF, then there was an error. */
		if (!feof(stdin)) {
			warn0("getline(..., ..., stdin)");
			goto err0;
		}
	}

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}

/* Finished writing data; look for more from stdin. */
static int
callback_wrote(void * cookie, ssize_t lenwrit)
{
	struct senddata * send = (struct senddata *)cookie;

	/* We are no longer writing. */
	send->write_cookie = NULL;

	/* Check that we wrote all our data. */
	if (lenwrit != send->nchars) {
		warn0("Mismatch between data sent and data requested to send");
		goto err0;
	}

	/* Clear the buffer that was used. */
	free(send->buffer);
	send->buffer = NULL;

	return send_input(cookie);

err0:
	/* Failure! */
	return (-1);
}

/* Got a connection; look for data from stdin. */
static int
callback_connected(void * cookie, int socket)
{
	struct senddata * send = (struct senddata *)cookie;

	/* We are no longer connecting. */
	send->connect_cookie = NULL;

	/* Check that the connection did not fail. */
	if (socket == -1) {
		warn0("failed to connect");
		goto err0;
	}

	/* Record socket for future use. */
	send->socket = socket;

	return send_input(cookie);

err0:
	/* Failure! */
	return (-1);
}

int
main(int argc, char ** argv)
{
	/* Command-line parameter. */
	const char * addr;

	/* Working variables. */
	struct sock_addr ** sas_t;
	struct senddata send_allocated;
	struct senddata * send = &send_allocated;

	WARNP_INIT;

	/* Parse command-line arguments. */
	if (argc < 2) {
		fprintf(stderr, "%s ADDRESS\n", argv[0]);
		goto err0;
	}
	addr = argv[1];

	/* Initialize cookie. */
	send->buffer = NULL;
	send->conndone = 0;
	send->connect_cookie = NULL;
	send->write_cookie = NULL;

	/* Resolve target address. */
	if ((sas_t = sock_resolve(addr)) == NULL) {
		warnp("Error resolving socket address: %s", addr);
		goto err1;
	}
	if (sas_t[0] == NULL) {
		warn0("No addresses found for %s", addr);
		goto err2;
	}

	/* Connect to target. */
	if ((send->connect_cookie = network_connect(sas_t, callback_connected,
	    send)) == NULL) {
		warn0("Error connecting");
		goto err2;
	}

	/* Loop until we're done with the connection. */
	if (events_spin(&send->conndone)) {
		warn0("Error running event loop");
		goto err3;
	}

	/* Clean up. */
	events_shutdown();
	sock_addr_freelist(sas_t);
	free(send->buffer);

	/* Success! */
	exit(0);

err3:
	if (send->connect_cookie != NULL)
		network_connect_cancel(send->connect_cookie);
	if (send->write_cookie != NULL)
		network_write_cancel(send->write_cookie);
	events_shutdown();
err2:
	sock_addr_freelist(sas_t);
err1:
	free(send->buffer);
err0:
	/* Failure! */
	exit(1);
}
