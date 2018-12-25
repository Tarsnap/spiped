#include <sys/socket.h>

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "parsenum.h"
#include "sock.h"
#include "warnp.h"

int
main(int argc, char ** argv)
{
	/* Command-line parameters. */
	const char * addr = NULL;
	ssize_t buflen;

	/* Working variables. */
	struct sock_addr ** sas;
	int socket;
	int socket_recv;
	char * buffer;
	ssize_t r;

	WARNP_INIT;

	/* Parse command-line arguments. */
	if (argc < 3) {
		warn0("usage: %s ADDRESS BUFLEN", argv[0]);
		goto err0;
	}
	addr = argv[1];
	if (PARSENUM(&buflen, argv[2], 1, SSIZE_MAX)) {
		warnp("parsenum");
		goto err0;
	}

	/* Allocate buffer. */
	if ((buffer = malloc((size_t)buflen)) == NULL) {
		warnp("malloc");
		goto err0;
	}

	/* Resolve the address. */
	if ((sas = sock_resolve(addr)) == NULL) {
		warn0("sock_resolve");
		goto err1;
	}

	/* Create a socket, bind it, mark it as listening. */
	if ((socket = sock_listener(sas[0])) == -1) {
		warn0("sock_listener");
		goto err2;
	}

	/* Make it blocking. */
	if (fcntl(socket, F_SETFL,
	    fcntl(socket, F_GETFL, 0) & (~O_NONBLOCK)) == -1) {
		warnp("Cannot make connection blocking");
		goto err3;
	}

	/* Accept connections. */
	if ((socket_recv = accept(socket, NULL, NULL)) == -1) {
		warnp("accept");
		goto err3;
	}

	/* Receive data, but do nothing with it. */
	do {
		r = recv(socket_recv, buffer, (size_t)buflen, MSG_WAITALL);
	} while (r == buflen);
	if (r != 0) {
		warnp("recv");
		goto err4;
	}

	/* Clean up. */
	if (close(socket_recv)) {
		warnp("close");
		goto err3;
	}
	if (close(socket)) {
		warnp("close");
		goto err2;
	}
	sock_addr_freelist(sas);
	free(buffer);

	/* Success! */
	exit(0);

err4:
	close(socket_recv);
err3:
	close(socket);
err2:
	sock_addr_freelist(sas);
err1:
	free(buffer);
err0:
	/* Failure! */
	exit(1);
}
