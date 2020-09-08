#include <sys/socket.h>

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "events.h"
#include "getopt.h"
#include "graceful_shutdown.h"
#include "parsenum.h"
#include "sock.h"
#include "warnp.h"

#include "proto_conn.h"
#include "proto_crypt.h"

#include "pushbits.h"

/* Cookie with variables about the event loop and threads. */
struct events_threads {
	pthread_t threads[2];
	int conndone;
	int connection_error;
	int stopped;
};

static int
callback_conndied(void * cookie, int reason)
{
	struct events_threads * ET = cookie;

	/* Check reason. */
	switch (reason) {
	case PROTO_CONN_CLOSED:
	case PROTO_CONN_CANCELLED:
		break;
	case PROTO_CONN_CONNECT_FAILED:
		warn0("Could not connect");
		ET->connection_error = 1;
		break;
	case PROTO_CONN_HANDSHAKE_FAILED:
		warn0("Handshake failed");
		ET->connection_error = 1;
		break;
	default:
		warn0("Connection error");
		ET->connection_error = 1;
	}

	/* Shut it down if there's an error. */
	if (ET->connection_error)
		graceful_shutdown_manual();

	/* Quit event loop. */
	ET->conndone = 1;

	/* Success! */
	return (0);
}

static int
callback_graceful_shutdown(void * cookie)
{
	struct events_threads * ET = cookie;
	int rc;
	int i;

	/* Cancel the threads. */
	for (i = 0; i < 2; i++) {
		if ((rc = pthread_cancel(ET->threads[i])) != 0) {
			/*
			 * According to the POSIX standard, a Thread ID should
			 * still be valid after pthread_exit has been invoked
			 * by the thread if pthread_join() has not yet been
			 * called.  However, many platforms return ESRCH in
			 * this situation.
			 */
			if (rc != ESRCH) {
				warn0("pthread_cancel: %s", strerror(rc));
				goto err0;
			}
		}
	}

	/* Wait for the threads to finish. */
	for (i = 0; i < 2; i++) {
		if ((rc = pthread_join(ET->threads[i], NULL)) != 0) {
			warn0("pthread_join: %s", strerror(rc));
			goto err0;
		}
	}

	/* We've stopped the threads. */
	ET->stopped = 1;

	/* Shut down the main event loop. */
	ET->conndone = 1;

	/* Success! */
	return(0);

err0:
	/* Failure! */
	return (-1);
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
	struct events_threads ET;
	struct sock_addr ** sas_t;
	struct proto_secret * K;
	const char * ch;
	int s[2];
	void * conn_cookie;
	int rc;

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

	/* Initialize the "events & threads" cookie. */
	ET.conndone = 0;
	ET.connection_error = 0;
	ET.stopped = 0;

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
	    opt_j, K, opt_o, callback_conndied, &ET)) == NULL) {
		warnp("Could not set up connection");
		goto err3;
	}

	/* sas_t and s[1] are now owned by proto_conn. */
	sas_t = NULL;
	s[1] = -1;

	/* Push bits from the socket to stdout. */
	if (pushbits(s[0], STDOUT_FILENO, &ET.threads[1])) {
		warnp("Could not push bits");
		goto err4;
	}

	/* Push bits from stdin into the socket. */
	if (pushbits(STDIN_FILENO, s[0], &ET.threads[0])) {
		warnp("Could not push bits");
		goto err5;
	}

	/* Register a handler for SIGTERM. */
	if (graceful_shutdown_initialize(&callback_graceful_shutdown, &ET)) {
		warn0("Failed to start graceful_shutdown timer");
		goto err6;
	}

	/* Loop until we're done with the connection. */
	if (events_spin(&ET.conndone)) {
		warnp("Error running event loop");
		goto err4;
	}

	/* Wait for threads to finish (if necessary) */
	if (ET.stopped == 0) {
		if ((rc = pthread_join(ET.threads[0], NULL)) != 0) {
			warn0("pthread_join: %s", strerror(rc));
			goto err2;
		}
		if ((rc = pthread_join(ET.threads[1], NULL)) != 0) {
			warn0("pthread_join: %s", strerror(rc));
			goto err2;
		}
	}

	/* Clean up. */
	close(s[0]);
	free(K);

	/* Handle a connection error. */
	if (ET.connection_error)
		goto err0;

	/* Success! */
	exit(0);

err6:
	if ((rc = pthread_cancel(ET.threads[0])) != 0)
		warn0("pthread_cancel: %s", strerror(rc));
	if ((rc = pthread_join(ET.threads[0], NULL)) != 0)
		warn0("pthread_join: %s", strerror(rc));
err5:
	if ((rc = pthread_cancel(ET.threads[1])) != 0)
		warn0("pthread_cancel: %s", strerror(rc));
	if ((rc = pthread_join(ET.threads[1], NULL)) != 0)
		warn0("pthread_join: %s", strerror(rc));
err4:
	proto_conn_drop(conn_cookie, PROTO_CONN_CANCELLED);
err3:
	if (s[1] != -1)
		close(s[1]);
	close(s[0]);
err2:
	free(K);
err1:
	sock_addr_freelist(sas_t);
err0:
	/* Failure! */
	exit(1);
}
