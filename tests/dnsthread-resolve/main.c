#include <stdio.h>
#include <stdlib.h>

#include "events.h"
#include "sock.h"
#include "sock_util.h"
#include "warnp.h"

#include "dnsthread.h"

#define MAX_ADDRS 8
static int doneloop = 0;

static int
found(void * cookie, struct sock_addr ** sas)
{
	char * addr;
	int i;

	(void)cookie; /* UNUSED */

	/* Sanity check. */
	if (sas == NULL)
		goto err0;

	/* Print each address. */
	for (i = 0; i < MAX_ADDRS; i++) {
		if (sas[i] == NULL)
			break;

		/* Extract address and print it. */
		if ((addr = sock_addr_prettyprint(sas[i])) == NULL) {
			warn0("sock_addr_prettyprint()");
			goto err1;
		}
		printf("%s\n", addr);

		/* Clean up. */
		free(addr);
	}

	/* Clean up. */
	sock_addr_freelist(sas);

	/* Quit event loop. */
	doneloop = 1;

	/* Success! */
	return (0);

err1:
	sock_addr_freelist(sas);
err0:
	/* Failure! */
	return (-1);
}

int
main(int argc, char ** argv)
{

	WARNP_INIT;

	/* Usage. */
	if (argc < 2) {
		fprintf(stderr, "usage: dnsthread-resolve ADDRESS\n");
		goto err0;
	}

	/* Look for address given. */
	if (dnsthread_resolve(argv[1], found, NULL)) {
		warn0("fail dns");
		goto err0;
	}

	/* Loop until we're done. */
	if (events_spin(&doneloop)) {
		warn0("Error running event loop");
		goto err0;
	}

	/* Success! */
	exit(0);

err0:
	/* Failure! */
	exit(1);
}
