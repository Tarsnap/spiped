#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "asprintf.h"
#include "daemonize.h"
#include "events.h"
#include "sha256.h"
#include "sock.h"
#include "warnp.h"

#include "conn.h"

static void
usage(void)
{

	fprintf(stderr, "usage: spiped {-e | -d} -s <source socket> "
	    "-t <target socket> -k <key file>\n"
	    "    [-f] [-n <max # connections>] "
	    "[-o <connection timeout>] [-p <pidfile>]\n");
	exit(1);
}

/* Compute the SHA256 hash of the file contents. */
static void
SHA256File(const char * filename, uint8_t hbuf[32])
{
	FILE * f;
	SHA256_CTX ctx;
	uint8_t buf[BUFSIZ];
	size_t lenread;

	/* Open the file. */
	if ((f = fopen(filename, "r")) == NULL) {
		warnp("Cannot open file: %s", filename);
		exit(1);
	}

	/* Initialize the SHA256 hash context. */
	SHA256_Init(&ctx);

	/* Read the file until we hit EOF. */
	while ((lenread = fread(buf, 1, BUFSIZ, f)) > 0)
		SHA256_Update(&ctx, buf, lenread);

	/* Did we hit EOF? */
	if (! feof(f)) {
		warnp("Error reading file: %s", filename);
		exit(1);
	}

	/* Close the file. */
	fclose(f);

	/* Compute the final hash. */
	SHA256_Final(hbuf, &ctx);
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
	int opt_e = 0;
	int opt_f = 0;
	const char * opt_k = NULL;
	intmax_t opt_n = 0;
	double opt_o = 0.0;
	char * opt_p = NULL;
	const char * opt_s = NULL;
	const char * opt_t = NULL;

	/* Working variables. */
	struct sock_addr ** sas_s;
	struct sock_addr ** sas_t;
	uint8_t kfhash[32];
	int ch;
	int s;

	WARNP_INIT;

	/* Parse the command line. */
	while ((ch = getopt(argc, argv, "defk:n:o:p:s:t:")) != -1) {
		switch (ch) {
		case 'd':
			if (opt_d || opt_e)
				usage();
			opt_d = 1;
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

	/* Sanity-check options. */
	if (!opt_d && !opt_e)
		usage();
	if (opt_k == NULL)
		usage();
	if ((opt_n < 0) || (opt_n > 500))
		usage();
	if (!(opt_o > 0.0))
		usage();
	if (opt_s == NULL)
		usage();
	if (opt_t == NULL)
		usage();

	/* Resolve source address. */
	if ((sas_s = sock_resolve(opt_s)) == NULL) {
		warnp("Error resolving socket address: %s", opt_s);
		exit(1);
	}
	if (sas_s[0] == NULL) {
		warn0("No addresses found for %s", opt_s);
		exit(1);
	}

	/* Resolve target address. */
	if ((sas_t = sock_resolve(opt_t)) == NULL) {
		warnp("Error resolving socket address: %s", opt_t);
		exit(1);
	}
	if (sas_t[0] == NULL) {
		warn0("No addresses found for %s", opt_t);
		exit(1);
	}

	/* Compute the SHA256 of the key file. */
	SHA256File(opt_k, kfhash);

	/* Create and bind a socket, and mark it as listening. */
	if (sas_s[1] != NULL)
		warn0("Listening on first of multiple addresses found for %s",
		    opt_s);
	if ((s = sock_listener(sas_s[0])) == -1)
		exit(1);

	/* Daemonize and write pid. */
	if (opt_p == NULL) {
		if (asprintf(&opt_p, "%s.pid", opt_s) == -1) {
			warnp("asprintf");
			exit(1);
		}
	}
	if (daemonize(opt_p)) {
		warnp("Failed to daemonize");
		exit(1);
	}

	/* Start accepting connections. */
	if (conn_accept(s, sas_t, opt_d, opt_f, kfhash, opt_n, opt_o)) {
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
