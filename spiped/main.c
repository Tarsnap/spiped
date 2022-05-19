#include <math.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "asprintf.h"
#include "daemonize.h"
#include "events.h"
#include "getopt.h"
#include "graceful_shutdown.h"
#include "parsenum.h"
#include "setuidgid.h"
#include "sock.h"
#include "warnp.h"

#include "dispatch.h"
#include "proto_crypt.h"

static void
usage(void)
{

	fprintf(stderr,
	    "usage: spiped {-e | -d} -s <source socket> "
	    "-t <target socket> [-b <bind address>] -k <key file>\n"
	    "    [-DFj] [-f | -g] [-n <max # connections>] "
	    "[-o <connection timeout>]\n"
	    "    [-p <pidfile>] [-r <rtime> | -R] [--syslog]\n"
	    "    [-u {<username> | <:groupname> | <username:groupname>}]\n"
	    "       spiped -v\n");
	exit(1);
}

static int
callback_graceful_shutdown(void * dispatch_cookie)
{

	dispatch_request_shutdown(dispatch_cookie);

	/* Success! */
	return (0);
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

/* Simplify error-handling in command-line parse loop. */
#define OPT_EPARSE(opt, arg) do {					\
	warnp("Error parsing argument: %s %s", opt, arg);		\
	exit(1);							\
} while (0)

int
main(int argc, char * argv[])
{
	/* Command-line parameters. */
	const char * opt_b = NULL;
	int opt_d = 0;
	int opt_D = 0;
	int opt_e = 0;
	int opt_f = 0;
	int opt_g = 0;
	int opt_F = 0;
	int opt_j = 0;
	const char * opt_k = NULL;
	int opt_n_set = 0;
	size_t opt_n = 0;
	int opt_o_set = 0;
	double opt_o = 0.0;
	const char * opt_p = NULL;
	int opt_r_set = 0;
	double opt_r = 0.0;
	int opt_R = 0;
	int opt_syslog = 0;
	const char * opt_s = NULL;
	const char * opt_t = NULL;
	const char * opt_u = NULL;

	/* Working variables. */
	char * bind_addr = NULL;
	struct sock_addr * sa_b = NULL;
	struct sock_addr ** sas_b = NULL;
	struct sock_addr ** sas_s;
	struct sock_addr ** sas_t;
	struct proto_secret * K;
	const char * ch;
	char * pidfilename = NULL;
	int s;
	void * dispatch_cookie = NULL;
	int conndone = 0;

	WARNP_INIT;

	/* Parse the command line. */
	while ((ch = GETOPT(argc, argv)) != NULL) {
		GETOPT_SWITCH(ch) {
		GETOPT_OPTARG("-b"):
			if (opt_b)
				usage();
			opt_b = optarg;
			break;
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
			if (opt_n_set)
				usage();
			opt_n_set = 1;
			if (PARSENUM(&opt_n, optarg))
				OPT_EPARSE(ch, optarg);
			if (opt_n == 0)
				opt_n = SIZE_MAX;
			break;
		GETOPT_OPTARG("-o"):
			if (opt_o_set)
				usage();
			opt_o_set = 1;
			if (PARSENUM(&opt_o, optarg, 0, INFINITY))
				OPT_EPARSE(ch, optarg);
			break;
		GETOPT_OPTARG("-p"):
			if (opt_p)
				usage();
			opt_p = optarg;
			break;
		GETOPT_OPTARG("-r"):
			if (opt_r_set)
				usage();
			opt_r_set = 1;
			if (PARSENUM(&opt_r, optarg, 0, INFINITY))
				OPT_EPARSE(ch, optarg);
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
		GETOPT_OPT("--syslog"):
			if (opt_syslog)
				usage();
			opt_syslog = 1;
			break;
		GETOPT_OPTARG("-t"):
			if (opt_t)
				usage();
			opt_t = optarg;
			break;
		GETOPT_OPTARG("-u"):
			if (opt_u != NULL)
				usage();
			opt_u = optarg;
			break;
		GETOPT_OPT("-v"):
			fprintf(stderr, "spiped @VERSION@\n");
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
	if (!opt_n_set)
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

	/*
	 * A limit of SIZE_MAX connections is equivalent to any larger limit;
	 * we'll be unable to allocate memory for socket bookkeeping before we
	 * reach either.
	 */
	if (opt_n > SIZE_MAX)
		opt_n = SIZE_MAX;

	/* Figure out where our pid should be written. */
	if (asprintf(&pidfilename, (opt_p != NULL) ? "%s" : "%s.pid",
	    (opt_p != NULL) ? opt_p : opt_s) == -1) {
		warnp("asprintf");
		goto err0;
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
	if (opt_D && !opt_F) {
		if (daemonize(pidfilename)) {
			warnp("Failed to daemonize");
			goto err1;
		}

		/* Send to syslog (if applicable). */
		if (opt_syslog)
			warnp_syslog(1);
	}

	/* Resolve bind address. */
	if (opt_b) {
		char * a = strchr(opt_b, ':');
		char * b = strrchr(opt_b, ':');
		char * c = strrchr(opt_b, ']');
		/* If there is no ':', it's ipv4 address (or Unix domain socket). */
		if (b == NULL) {
			if (asprintf(&bind_addr, "%s:0", opt_b) == -1) {
			    warnp("asprintf");
			    goto err2;
			}
		}
		/* If there are two different ':', it's ipv6 address. */
		else if (a != b) {
			/* If there is no ']', we need to add '[]' and ':0'. */
			if (c == NULL) {
				if (asprintf(&bind_addr, "[%s]:0", opt_b) == -1) {
				    warnp("asprintf");
				    goto err2;
				}
			}
			/* If there is nothing after ']', we need to add ':0'. */
			else if (c[1] == '\0') {
				if (asprintf(&bind_addr, "%s:0", opt_b) == -1) {
				    warnp("asprintf");
				    goto err2;
				}
			}
		}
		while ((sas_b = sock_resolve(bind_addr)) == NULL) {
			if (!opt_D) {
				warnp("Error resolving socket address: %s", bind_addr);
				goto err2;
			}
			sleep(1);
		}
		if ((sa_b = sas_b[0]) == NULL) {
			warn0("No address found for %s", bind_addr);
			goto err3;
		}
	}

	/* Resolve source address. */
	while ((sas_s = sock_resolve(opt_s)) == NULL) {
		if (!opt_D) {
			warnp("Error resolving socket address: %s", opt_s);
			goto err3;
		}
		sleep(1);
	}
	if (sas_s[0] == NULL) {
		warn0("No addresses found for %s", opt_s);
		goto err4;
	}

	/* Resolve target address. */
	while ((sas_t = sock_resolve(opt_t)) == NULL) {
		if (!opt_D) {
			warnp("Error resolving socket address: %s", opt_t);
			goto err4;
		}
		sleep(1);
	}
	if (sas_t[0] == NULL) {
		warn0("No addresses found for %s", opt_t);
		goto err5;
	}

	/* Load the keying data. */
	if ((K = proto_crypt_secret(opt_k)) == NULL) {
		warnp("Error reading shared secret");
		goto err5;
	}

	/* Create and bind a socket, and mark it as listening. */
	if (sas_s[1] != NULL)
		warn0("Listening on first of multiple addresses found for %s",
		    opt_s);
	if ((s = sock_listener(sas_s[0])) == -1)
		goto err6;

	/* Daemonize and write pid. */
	if (!opt_D && !opt_F) {
		if (daemonize(pidfilename)) {
			warnp("Failed to daemonize");
			goto err7;
		}
		/* Send to syslog (if applicable). */
		if (opt_syslog)
			warnp_syslog(1);
	}

	/* Drop privileges (if applicable). */
	if (opt_u && setuidgid(opt_u, SETUIDGID_SGROUP_LEAVE_WARN)) {
		warnp("Failed to drop privileges");
		goto err7;
	}

	/* Start accepting connections. */
	if ((dispatch_cookie = dispatch_accept(s, opt_t, opt_R ? 0.0 : opt_r,
	    sas_t, sa_b, opt_d, opt_f, opt_g, opt_j, K, opt_n, opt_o,
	    &conndone)) == NULL) {
		warnp("Failed to initialize connection acceptor");
		goto err7;
	}

	/* dispatch is now maintaining sas_t and s. */
	sas_t = NULL;
	s = -1;

	/* Register a handler for SIGTERM. */
	if (graceful_shutdown_initialize(&callback_graceful_shutdown,
	    dispatch_cookie)) {
		warn0("Failed to start graceful_shutdown timer");
		goto err8;
	}

	/*
	 * Loop until an error occurs, or a connection closes if the
	 * command-line argument -1 was given.
	 */
	if (events_spin(&conndone)) {
		warnp("Error running event loop");
		goto err8;
	}

	/* Stop accepting connections and shut down the dispatcher. */
	dispatch_shutdown(dispatch_cookie);

	/* Free the protocol secret structure. */
	proto_crypt_secret_free(K);

	/* Free arrays of resolved addresses. */
	sock_addr_freelist(sas_s);
	sock_addr_freelist(sas_b);

	/* Free bind address. */
	free(bind_addr);

	/* Free pid filename. */
	free(pidfilename);

	/* Success! */
	exit(0);

err8:
	dispatch_shutdown(dispatch_cookie);
err7:
	if (s != -1)
		close(s);
err6:
	proto_crypt_secret_free(K);
err5:
	sock_addr_freelist(sas_t);
err4:
	sock_addr_freelist(sas_s);
err3:
	sock_addr_freelist(sas_b);
err2:
	free(bind_addr);
err1:
	free(pidfilename);
err0:
	/* Failure! */
	exit(1);
}
