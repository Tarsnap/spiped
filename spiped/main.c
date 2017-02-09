#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "asprintf.h"
#include "daemonize.h"
#include "events.h"
#include "getopt.h"
#include "graceful_shutdown.h"
#include "sock.h"
#include "warnp.h"

#include "dispatch.h"
#include "proto_crypt.h"

static struct spiped_cookie {
	struct sock_addr ** sas_s;
	struct sock_addr ** sas_t;
	struct proto_secret * K;
	char * pidfilename;
} spiped_cookie_allocated;

static void
usage(void)
{

	fprintf(stderr,
	    "usage: spiped {-e | -d} -s <source socket> "
	    "-t <target socket> -k <key file>\n"
	    "    [-DFj] [-f | -g] [-n <max # connections>] "
	    "[-o <connection timeout>]\n"
	    "    [-p <pidfile>] [-r <rtime> | -R] [-1]\n"
	    "       spiped -v\n");
	exit(1);
}

static int
callback_graceful_shutdown(void * dispatch_cookie)
{

	dispatch_request_shutdown(dispatch_cookie);

	/* Success! */
	return(0);
}

/*
 * Signal handler for SIGINT to perform a hard shutdown.
 */
static void
diediedie_handler(int signo)
{

	(void)signo; /* UNUSED */
	_exit(0);
}

int
main(int argc, char * argv[])
{
	/* Command-line parameters. */
	int opt_d = 0;
	int opt_D = 0;
	int opt_e = 0;
	int opt_f = 0;
	int opt_g = 0;
	int opt_F = 0;
	int opt_j = 0;
	const char * opt_k = NULL;
	intmax_t opt_n = 0;
	double opt_o = 0.0;
	const char * opt_p = NULL;
	double opt_r = 0.0;
	int opt_R = 0;
	const char * opt_s = NULL;
	const char * opt_t = NULL;
	int opt_1 = 0;

	/* Working variables. */
	const char * ch;
	int s;
	void * dispatch_cookie = NULL;
	int conndone = 0;
	struct spiped_cookie * S = &spiped_cookie_allocated;

	WARNP_INIT;

	/* Initialize the working cookie. */
	S->sas_s = NULL;
	S->sas_t = NULL;
	S->K = NULL;
	S->pidfilename = NULL;

	/* Parse the command line. */
	while ((ch = GETOPT(argc, argv)) != NULL) {
		GETOPT_SWITCH(ch) {
		GETOPT_OPT("-d"):
			if (opt_d || opt_e)
				usage();
			opt_d = 1;
			break;
		GETOPT_OPT("-D"):
			if (opt_D)
				usage();
			opt_D = 1;
			break;
		GETOPT_OPT("-e"):
			if (opt_d || opt_e)
				usage();
			opt_e = 1;
			break;
		GETOPT_OPT("-f"):
			if (opt_f)
				usage();
			opt_f = 1;
			break;
		GETOPT_OPT("-F"):
			if (opt_F)
				usage();
			opt_F = 1;
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
		GETOPT_OPTARG("-n"):
			if (opt_n != 0)
				usage();
			if ((opt_n = strtoimax(optarg, NULL, 0)) == 0) {
				warn0("Invalid option: -n %s", optarg);
				exit(1);
			}
			if ((opt_n <= 0) || (opt_n > 500)) {
				warn0("The parameter to -n must be between 1 and 500\n");
				exit(1);
			}
			break;
		GETOPT_OPTARG("-o"):
			if (opt_o != 0.0)
				usage();
			if ((opt_o = strtod(optarg, NULL)) == 0.0) {
				warn0("Invalid option: -o %s", optarg);
				exit(1);
			}
			break;
		GETOPT_OPTARG("-p"):
			if (opt_p)
				usage();
			opt_p = optarg;
			break;
		GETOPT_OPTARG("-r"):
			if (opt_r != 0.0)
				usage();
			if ((opt_r = strtod(optarg, NULL)) == 0.0) {
				warn0("Invalid option: -r %s", optarg);
				exit(1);
			}
			break;
		GETOPT_OPT("-R"):
			if (opt_R)
				usage();
			opt_R = 1;
			break;
		GETOPT_OPTARG("-s"):
			if (opt_s)
				usage();
			opt_s = optarg;
			break;
		GETOPT_OPTARG("-t"):
			if (opt_t)
				usage();
			opt_t = optarg;
			break;
		GETOPT_OPT("-v"):
			fprintf(stderr, "spiped @VERSION@\n");
			exit(0);
		GETOPT_OPT("-1"):
			if (opt_1 != 0)
				usage();
			opt_1 = 1;
			break;
		GETOPT_MISSING_ARG:
			warn0("Missing argument to %s\n", ch);
			usage();
		GETOPT_DEFAULT:
			warn0("illegal option -- %s\n", ch);
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
	if (opt_n == 0)
		opt_n = 100;
	if (opt_o == 0.0)
		opt_o = 5.0;
	if (opt_r == 0.0)
		opt_r = 60.0;

	/* Sanity-check options. */
	if (!opt_d && !opt_e)
		usage();
	if (opt_f && opt_g)
		usage();
	if (opt_k == NULL)
		usage();
	if (!(opt_o > 0.0))
		usage();
	if ((opt_r != 60.0) && opt_R)
		usage();
	if (opt_s == NULL)
		usage();
	if (opt_t == NULL)
		usage();

	/* Figure out where our pid should be written. */
	if (opt_p == NULL) {
		if (asprintf(&S->pidfilename, "%s.pid", opt_s) == -1) {
			warnp("asprintf");
			goto err0;
		}
	} else {
		if ((S->pidfilename = strdup(opt_p)) == NULL) {
			warnp("Out of memory");
			goto err0;
		}
	}

	/* Check whether we are running as init (e.g., inside a container). */
	if (getpid() == 1) {
		/* https://github.com/docker/docker/issues/7086 */
		warn0("WARNING: Applying workaround for Docker signal-handling bug");

		/* Bind an explicit signal handler for SIGINT. */
		if (signal(SIGINT, diediedie_handler) == SIG_ERR) {
			warnp("Failed to bind SIGINT signal handler");
		}
	}

	/* Daemonize early if we're going to wait for DNS to be ready. */
	if (opt_D && !opt_F && daemonize(S->pidfilename)) {
		warnp("Failed to daemonize");
		goto err1;
	}

	/* Resolve source address. */
	while ((S->sas_s = sock_resolve(opt_s)) == NULL) {
		if (!opt_D) {
			warnp("Error resolving socket address: %s", opt_s);
			goto err1;
		}
		sleep(1);
	}
	if (S->sas_s[0] == NULL) {
		warn0("No addresses found for %s", opt_s);
		goto err2;
	}

	/* Resolve target address. */
	while ((S->sas_t = sock_resolve(opt_t)) == NULL) {
		if (!opt_D) {
			warnp("Error resolving socket address: %s", opt_t);
			goto err2;
		}
		sleep(1);
	}
	if (S->sas_t[0] == NULL) {
		warn0("No addresses found for %s", opt_t);
		goto err3;
	}

	/* Load the keying data. */
	if ((S->K = proto_crypt_secret(opt_k)) == NULL) {
		warnp("Error reading shared secret");
		goto err3;
	}

	/* Create and bind a socket, and mark it as listening. */
	if (S->sas_s[1] != NULL)
		warn0("Listening on first of multiple addresses found for %s",
		    opt_s);
	if ((s = sock_listener(S->sas_s[0])) == -1)
		goto err4;

	/* Daemonize and write pid. */
	if (!opt_D && !opt_F && daemonize(S->pidfilename)) {
		warnp("Failed to daemonize");
		goto err4;
	}

	/* Start accepting connections. */
	if ((dispatch_cookie = dispatch_accept(s, opt_t, opt_R ? 0.0 : opt_r,
	    S->sas_t, opt_d, opt_f, opt_g, opt_j, S->K, (size_t)opt_n, opt_o,
	    opt_1, &conndone)) == NULL) {
		warnp("Failed to initialize connection acceptor");
		goto err5;
	}

	/* Register a handler for SIGTERM. */
	if (graceful_shutdown_initialize(&callback_graceful_shutdown,
	    dispatch_cookie)) {
		warn0("Failed to start graceful_shutdown timer");
		goto err6;
	}

	/*
	 * Loop until an error occurs, or a connection closes if the
	 * command-line argument -1 was given.
	 */
	if (events_spin(&conndone)) {
		warnp("Error running event loop");
		goto err6;
	}

	/* Stop accepting connections and shut down the dispatcher. */
	dispatch_shutdown(dispatch_cookie);

	/* Shut down the events system. */
	events_shutdown();

	/* Free the protocol secret structure. */
	free(S->K);

	/* Free arrays of resolved addresses. */
	sock_addr_freelist(S->sas_t);
	sock_addr_freelist(S->sas_s);

	/* Free pid filename. */
	free(S->pidfilename);

	/* Success! */
	exit(0);

err6:
	dispatch_shutdown(dispatch_cookie);
err5:
	events_shutdown();
err4:
	free(S->K);
err3:
	sock_addr_freelist(S->sas_t);
err2:
	sock_addr_freelist(S->sas_s);
err1:
	free(S->pidfilename);
err0:
	/* Failure! */
	exit(1);
}
