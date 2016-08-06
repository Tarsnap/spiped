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
#include "sock.h"
#include "warnp.h"

#include "dispatch.h"
#include "proto_crypt.h"

volatile sig_atomic_t should_shutdown = 0;
static void * graceful_shutdown_timer_cookie = NULL;

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

/*
 * Signal handler for SIGTERM to perform a graceful shutdown.
 */
static void
graceful_shutdown_handler(int signo)
{

	(void)signo; /* UNUSED */
	should_shutdown = 1;
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

/*
 * Requests a graceful shutdown of the given cookie.
 */
int
graceful_shutdown(void * cookie)
{
	struct accept_state * A = cookie;

	/* This timer has expired. */
	graceful_shutdown_timer_cookie = NULL;

	if (should_shutdown)
		dispatch_request_shutdown(A);
	else
		graceful_shutdown_timer_cookie = events_timer_register_double(
		    graceful_shutdown, A, 1.0);

	return 0;
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
	char * opt_p = NULL;
	double opt_r = 0.0;
	int opt_R = 0;
	const char * opt_s = NULL;
	const char * opt_t = NULL;
	int opt_1 = 0;

	/* Working variables. */
	struct sock_addr ** sas_s;
	struct sock_addr ** sas_t;
	struct proto_secret * K;
	const char * ch;
	int s;
	void * dispatch_cookie = NULL;
	int conndone = 0;

	WARNP_INIT;

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
			if ((opt_p = strdup(optarg)) == NULL)
				OPT_EPARSE(ch, optarg);
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
		if (asprintf(&opt_p, "%s.pid", opt_s) == -1) {
			warnp("asprintf");
			goto err0;
		}
	}

	if (signal(SIGTERM, graceful_shutdown_handler) == SIG_ERR) {
		warnp("Failed to bind SIGTERM signal handler");
		goto err1;
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
	if (opt_D && !opt_F && daemonize(opt_p)) {
		warnp("Failed to daemonize");
		goto err1;
	}

	/* Resolve source address. */
	while ((sas_s = sock_resolve(opt_s)) == NULL) {
		if (!opt_D) {
			warnp("Error resolving socket address: %s", opt_s);
			goto err1;
		}
		sleep(1);
	}
	if (sas_s[0] == NULL) {
		warn0("No addresses found for %s", opt_s);
		goto err2;
	}

	/* Resolve target address. */
	while ((sas_t = sock_resolve(opt_t)) == NULL) {
		if (!opt_D) {
			warnp("Error resolving socket address: %s", opt_t);
			goto err2;
		}
		sleep(1);
	}
	if (sas_t[0] == NULL) {
		warn0("No addresses found for %s", opt_t);
		goto err3;
	}

	/* Load the keying data. */
	if ((K = proto_crypt_secret(opt_k)) == NULL) {
		warnp("Error reading shared secret");
		goto err3;
	}

	/* Create and bind a socket, and mark it as listening. */
	if (sas_s[1] != NULL)
		warn0("Listening on first of multiple addresses found for %s",
		    opt_s);
	if ((s = sock_listener(sas_s[0])) == -1)
		goto err4;

	/* Daemonize and write pid. */
	if (!opt_D && !opt_F && daemonize(opt_p)) {
		warnp("Failed to daemonize");
		goto err4;
	}

	/* Start accepting connections. */
	if ((dispatch_cookie = dispatch_accept(s, opt_t, opt_R ? 0.0 : opt_r,
	    sas_t, opt_d, opt_f, opt_g, opt_j, K, opt_n, opt_o, opt_1,
	    &conndone)) == NULL) {
		warnp("Failed to initialize connection acceptor");
		goto err5;
	}

	/* Check periodically whether a signal was received. */
	graceful_shutdown_timer_cookie = events_timer_register_double(
	    graceful_shutdown, dispatch_cookie, 1.0);

	/*
	 * Loop until an error occurs, or a connection closes if the
	 * command-line argument -1 was given.
	 */
	if (events_spin(&conndone)) {
		warnp("Error running event loop");
		goto err6;
	}

	/* Deregister the graceful_shutdown timer. */
	if (graceful_shutdown_timer_cookie != NULL)
		events_timer_cancel(graceful_shutdown_timer_cookie);

	/* Stop accepting connections and shut down the dispatcher. */
	dispatch_shutdown(dispatch_cookie);

	/* Shut down the events system. */
	events_shutdown();

	/* Free the protocol secret structure. */
	free(K);

	/* Free arrays of resolved addresses. */
	sock_addr_freelist(sas_t);
	sock_addr_freelist(sas_s);

	/* Free pid filename. */
	free(opt_p);

	/* Success! */
	exit(0);

err6:
	if (graceful_shutdown_timer_cookie != NULL)
		events_timer_cancel(graceful_shutdown_timer_cookie);
	dispatch_shutdown(dispatch_cookie);
err5:
	events_shutdown();
err4:
	free(K);
err3:
	sock_addr_freelist(sas_t);
err2:
	sock_addr_freelist(sas_s);
err1:
	free(opt_p);
err0:
	/* Failure! */
	exit(1);
}
