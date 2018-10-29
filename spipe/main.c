#include <sys/socket.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "events.h"
#include "getopt.h"
#include "parsenum.h"
#include "sock.h"
#include "warnp.h"

#include "proto_conn.h"
#include "proto_crypt.h"

#include "pushbits.h"

static int
callback_conndied(void * cookie)
{
	int * conndone = cookie;

	/* Quit event loop. */
	*conndone = 1;

	/* Success! */
	return (0);
}

static void
usage(void)
{

	fprintf(stderr,
	    "usage: spipe -t <target socket> -k <key file>"
	    " [-f | -g] [-j]\n"
	    "    [-o <connection timeout>]\n"
	    "       spipe -v\n");
	exit(1);
}

/* Simplify error-handling in command-line parse loop. */
#define OPT_EPARSE(opt, arg) do {					\
	warnp("Error parsing argument: %s %s", opt, arg);		\
	exit(1);							\
} while (0)

int
main(int argc, char * argv[])
{
	/* Command-line parameters. */
	int opt_f = 0;
	int opt_g = 0;
	int opt_j = 0;
	const char * opt_k = NULL;
	int opt_o_set = 0;
	double opt_o = 0.0;
	const char * opt_t = NULL;

	/* Working variables. */
	struct sock_addr ** sas_t;
	struct proto_secret * K;
	const char * ch;
	int s[2];
	int conndone = 0;
	void * conn_cookie;

	WARNP_INIT;

	/* Parse the command line. */
	while ((ch = GETOPT(argc, argv)) != NULL) {
		GETOPT_SWITCH(ch) {
		GETOPT_OPT("-f"):
			if (opt_f)
				usage();
			opt_f = 1;
			break;
		GETOPT_OPT("-g"):
			if (opt_g)
				usage();
			opt_g = 1;
			break;
		GETOPT_OPT("-j"):
			if (opt_j)
				usage();
			opt_j = 1;
			break;
		GETOPT_OPTARG("-k"):
			if (opt_k)
				usage();
			opt_k = optarg;
			break;
		GETOPT_OPTARG("-o"):
			if (opt_o_set)
				usage();
			opt_o_set = 1;
			if (PARSENUM(&opt_o, optarg, 0, INFINITY))
				OPT_EPARSE(ch, optarg);
			break;
		GETOPT_OPTARG("-t"):
			if (opt_t)
				usage();
			opt_t = optarg;
			break;
		GETOPT_OPT("-v"):
			fprintf(stderr, "spipe @VERSION@\n");
			exit(0);
		GETOPT_MISSING_ARG:
			warn0("Missing argument to %s", ch);
			usage();
		GETOPT_DEFAULT:
			warn0("illegal option -- %s", ch);
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	/* We should have processed all the arguments. */
	if (argc != 0)
		usage();
	(void)argv; /* argv is not used beyond this point. */

	/* Set defaults. */
	if (opt_o == 0.0)
		opt_o = 5.0;

	/* Sanity-check options. */
	if (opt_f && opt_g)
		usage();
	if (opt_k == NULL)
		usage();
	if (!(opt_o > 0.0))
		usage();
	if (opt_t == NULL)
		usage();

	/* Resolve target address. */
	if ((sas_t = sock_resolve(opt_t)) == NULL) {
		warnp("Error resolving socket address: %s", opt_t);
		goto err0;
	}
	if (sas_t[0] == NULL) {
		warn0("No addresses found for %s", opt_t);
		goto err1;
	}

	/* Load the keying data. */
	if ((K = proto_crypt_secret(opt_k)) == NULL) {
		warnp("Error reading shared secret");
		goto err1;
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
		goto err2;
	}

	/* Set up a connection. */
	if ((conn_cookie = proto_conn_create(s[1], sas_t, 0, opt_f, opt_g,
	    opt_j, K, opt_o, callback_conndied, &conndone)) == NULL) {
		warnp("Could not set up connection");
		goto err2;
	}

	/* Push bits from stdin into the socket. */
	if (pushbits(STDIN_FILENO, s[0])) {
		warnp("Could not push bits");
		goto err3;
	}

	/* Push bits from the socket to stdout. */
	if (pushbits(s[0], STDOUT_FILENO)) {
		warnp("Could not push bits");
		goto err3;
	}

	/* Loop until we're done with the connection. */
	if (events_spin(&conndone)) {
		warnp("Error running event loop");
		exit(1);
	}

	/* Clean up. */
	events_shutdown();
	free(K);

	/* Success! */
	exit(0);

err3:
	proto_conn_drop(conn_cookie);
	sas_t = NULL;
	events_shutdown();
err2:
	free(K);
err1:
	sock_addr_freelist(sas_t);
err0:
	/* Failure! */
	exit(1);
}
