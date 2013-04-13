#include <sys/socket.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "asprintf.h"
#include "events.h"
#include "sha256.h"
#include "sock.h"
#include "warnp.h"

#include "proto_conn.h"
#include "proto_crypt.h"

#include "pushbits.h"

static int
callback_conndied(void * cookie)
{

	(void)cookie; /* UNUSED */

	/* We're done! */
	exit(0);
}

static void
usage(void)
{

	fprintf(stderr, "usage: spipe -t <target socket> -k <key file>"
	    " [-fj] [-o <connection timeout>]\n");
	exit(1);
}

/* Simplify error-handling in command-line parse loop. */
#define OPT_EPARSE(opt, arg) do {					\
	warnp("Error parsing argument: -%c %s", opt, arg);		\
	exit(1);							\
} while (0)

int
main(int argc, char * argv[])
{
	/* Command-line parameters. */
	int opt_f = 0;
	int opt_j = 0;
	const char * opt_k = NULL;
	double opt_o = 0.0;
	const char * opt_t = NULL;

	/* Working variables. */
	struct sock_addr ** sas_t;
	struct proto_secret * K;
	int ch;
	int s[2];

	WARNP_INIT;

	/* Parse the command line. */
	while ((ch = getopt(argc, argv, "fjk:o:t:")) != -1) {
		switch (ch) {
		case 'f':
			if (opt_f)
				usage();
			opt_f = 1;
			break;
		case 'j':
			if (opt_j)
				usage();
			opt_j = 1;
			break;
		case 'k':
			if (opt_k)
				usage();
			opt_k = optarg;
			break;
		case 'o':
			if (opt_o != 0.0)
				usage();
			if ((opt_o = strtod(optarg, NULL)) == 0.0) {
				warn0("Invalid option: -o %s", optarg);
				exit(1);
			}
			break;
		case 't':
			if (opt_t)
				usage();
			opt_t = optarg;
			break;
		default:
			usage();
		}
	}

	/* We should have processed all the arguments. */
	if (argc != optind)
		usage();

	/* Set defaults. */
	if (opt_o == 0.0)
		opt_o = 5.0;

	/* Sanity-check options. */
	if (opt_k == NULL)
		usage();
	if (!(opt_o > 0.0))
		usage();
	if (opt_t == NULL)
		usage();

	/* Resolve target address. */
	if ((sas_t = sock_resolve(opt_t)) == NULL) {
		warnp("Error resolving socket address: %s", opt_t);
		exit(1);
	}
	if (sas_t[0] == NULL) {
		warn0("No addresses found for %s", opt_t);
		exit(1);
	}

	/* Load the keying data. */
	if ((K = proto_crypt_secret(opt_k)) == NULL) {
		warnp("Error reading shared secret");
		exit(1);
	}

	/*
	 * Create a socket pair to push bits through.  The spiped protocol
	 * code expects to be handed a socket to read/write bits to, and our
	 * stdin/stdout might not be sockets (in fact, almost certainly aren't
	 * sockets); so we'll hand one end of the socket pair to the spiped
	 * protocol code and shuttle bits between stdin/stdout and the other
	 * end of the socket pair ourselves.
	 */
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, s)) {
		warnp("socketpair");
		exit(1);
	}

	/* Set up a connection. */
	if (proto_conn_create(s[1], sas_t, 0, opt_f, opt_j, K, opt_o,
	    callback_conndied, NULL)) {
		warnp("Could not set up connection");
		exit(1);
	}

	/* Push bits from stdin into the socket. */
	if (pushbits(STDIN_FILENO, s[0]) || pushbits(s[0], STDOUT_FILENO)) {
		warnp("Could not push bits");
		exit(1);
	}

	/* Loop until we die. */
	do {
		if (events_run()) {
			warnp("Error running event loop");
			exit(1);
		}
	} while(1);

	/* NOTREACHED */
	/*
	 * If we could reach this point, we would free memory, close sockets,
	 * and otherwise clean up here.
	 */
}
