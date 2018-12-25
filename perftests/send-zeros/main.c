#include <sys/socket.h>
#include <sys/time.h>

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "monoclock.h"
#include "parsenum.h"
#include "sock.h"
#include "warnp.h"

int
main(int argc, char ** argv)
{
	/* Command-line parameters. */
	const char * addr;
	size_t buflen;
	size_t count;

	/* Working variables. */
	struct sock_addr ** sas_t;
	struct timeval begin, end;
	double duration_s;
	char * buffer;
	int socket;
	size_t to_send;

	WARNP_INIT;

	/* Parse command-line arguments. */
	if (argc != 4) {
		warn0("usage: %s ADDRESS BUFLEN COUNT", argv[0]);
		goto err0;
	}
	addr = argv[1];
	if (PARSENUM(&buflen, argv[2], 1, SSIZE_MAX)) {
		warnp("parsenum");
		goto err0;
	}
	if (PARSENUM(&count, argv[3])) {
		warnp("parsenum");
		goto err0;
	}

	/* Sanity check */
	if (count == 0) {
		warnp("Can't send 0 blocks!");
		goto err0;
	}

	/* Allocate and fill buffer to send. */
	if ((buffer = malloc(buflen)) == NULL) {
		warnp("Out of memory");
		goto err0;
	}
	memset(buffer, 0, buflen);

	/* Initialize count. */
	to_send = count;

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
	if ((socket = sock_connect(sas_t)) == -1) {
		warnp("sock_connect");
		goto err2;
	}

	/* Make it blocking. */
	if (fcntl(socket, F_SETFL,
	    fcntl(socket, F_GETFL, 0) & (~O_NONBLOCK)) == -1) {
		warnp("Cannot make connection blocking");
		goto err3;
	}

	/* Get beginning time. */
	if (monoclock_get(&begin)) {
		warn0("monoclock_get");
		goto err3;
	}

	/* Send data. */
	while (to_send > 0) {
		if (write(socket, buffer, buflen) != (ssize_t)buflen) {
			warnp("write failed");
			goto err3;
		}
		to_send--;
	}

	/* We're not going to send anything else. */
	if (shutdown(socket, SHUT_WR)) {
		warnp("shutdown");
		goto err3;
	}

	/*
	 * The server should not send any data back, but attempting to read
	 * will detect when the other end of the socket is closed.
	 */
	if (read(socket, buffer, 1) != 0) {
		warnp("read");
		goto err3;
	}

	/* Get ending time. */
	if (monoclock_get(&end)) {
		warn0("monoclock_get");
		goto err3;
	}

	/* Print duration and speed. */
	duration_s = timeval_diff(begin, end);
	printf("%zu\t%zu\t%.4f\t%.2f\n", buflen, count, duration_s,
	    buflen * count / duration_s / 1e6);

	/* Clean up. */
	if (close(socket)) {
		warnp("close");
		goto err2;
	}
	sock_addr_freelist(sas_t);
	free(buffer);

	/* Success! */
	exit(0);

err3:
	close(socket);
err2:
	sock_addr_freelist(sas_t);
err1:
	free(buffer);
err0:
	/* Failure! */
	exit(1);
}
