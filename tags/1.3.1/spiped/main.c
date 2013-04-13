#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "asprintf.h"
#include "daemonize.h"
#include "events.h"
#include "sock.h"
#include "warnp.h"

#include "dispatch.h"
#include "proto_crypt.h"

static void
usage(void)
{

	fprintf(stderr, "usage: spiped {-e | -d} -s <source socket> "
	    "-t <target socket> -k <key file>\n"
	    "    [-DfFj] [-n <max # connections>] "
	    "[-o <connection timeout>] [-p <pidfile>]\n"
	    "    [{-r <rtime> | -R}]\n");
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
	int opt_d = 0;
	int opt_D = 0;
	int opt_e = 0;
	int opt_f = 0;
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

	/* Working variables. */
	struct sock_addr ** sas_s;
	struct sock_addr ** sas_t;
	struct proto_secret * K;
	int ch;
	int s;

	WARNP_INIT;

	/* Parse the command line. */
	while ((ch = getopt(argc, argv, "dDefFjk:n:o:r:Rp:s:t:")) != -1) {
		switch (ch) {
		case 'd':
			if (opt_d || opt_e)
				usage();
			opt_d = 1;
			break;
		case 'D':
			if (opt_D)
				usage();
			opt_D = 1;
			break;
		case 'e':
			if (opt_d || opt_e)
				usage();
			opt_e = 1;
			break;
		case 'f':
			if (opt_f)
				usage();
			opt_f = 1;
			break;
		case 'F':
			if (opt_F)
				usage();
			opt_F = 1;
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
		case 'n':
			if (opt_n != 0)
				usage();
			if ((opt_n = strtoimax(optarg, NULL, 0)) == 0) {
				warn0("Invalid option: -n %s", optarg);
				exit(1);
			}
			break;
		case 'o':
			if (opt_o != 0.0)
				usage();
			if ((opt_o = strtod(optarg, NULL)) == 0.0) {
				warn0("Invalid option: -o %s", optarg);
				exit(1);
			}
			break;
		case 'p':
			if (opt_p)
				usage();
			if ((opt_p = strdup(optarg)) == NULL)
				OPT_EPARSE(ch, optarg);
			break;
		case 'r':
			if (opt_r != 0.0)
				usage();
			if ((opt_r = strtod(optarg, NULL)) == 0.0) {
				warn0("Invalid option: -r %s", optarg);
				exit(1);
			}
			break;
		case 'R':
			if (opt_R)
				usage();
			opt_R = 1;
			break;
		case 's':
			if (opt_s)
				usage();
			opt_s = optarg;
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
	if (opt_n == 0)
		opt_n = 100;
	if (opt_o == 0.0)
		opt_o = 5.0;
	if (opt_r == 0.0)
		opt_r = 60.0;

	/* Sanity-check options. */
	if (!opt_d && !opt_e)
		usage();
	if (opt_k == NULL)
		usage();
	if ((opt_n < 0) || (opt_n > 500))
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
			exit(1);
		}
	}

	/* Daemonize early if we're going to wait for DNS to be ready. */
	if (opt_D && !opt_F && daemonize(opt_p)) {
		warnp("Failed to daemonize");
		exit(1);
	}

	/* Resolve source address. */
	while ((sas_s = sock_resolve(opt_s)) == NULL) {
		if (!opt_D) {
			warnp("Error resolving socket address: %s", opt_s);
			exit(1);
		}
		sleep(1);
	}
	if (sas_s[0] == NULL) {
		warn0("No addresses found for %s", opt_s);
		exit(1);
	}

	/* Resolve target address. */
	while ((sas_t = sock_resolve(opt_t)) == NULL) {
		if (!opt_D) {
			warnp("Error resolving socket address: %s", opt_t);
			exit(1);
		}
		sleep(1);
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

	/* Create and bind a socket, and mark it as listening. */
	if (sas_s[1] != NULL)
		warn0("Listening on first of multiple addresses found for %s",
		    opt_s);
	if ((s = sock_listener(sas_s[0])) == -1)
		exit(1);

	/* Daemonize and write pid. */
	if (!opt_F && daemonize(opt_p)) {
		warnp("Failed to daemonize");
		exit(1);
	}

	/* Start accepting connections. */
	if (dispatch_accept(s, opt_t, opt_R ? 0.0 : opt_r, sas_t, opt_d,
	    opt_f, opt_j, K, opt_n, opt_o)) {
		warnp("Failed to initialize connection acceptor");
		exit(1);
	}

	/* Infinite loop handling events. */
	do {
		if (events_run()) {
			warnp("Error running event loop");
			exit(1);
		}
	} while (1);

	/* NOTREACHED */
	/*
	 * If we could reach this point, we would free memory, close sockets,
	 * and otherwise clean up here.
	 */
}
