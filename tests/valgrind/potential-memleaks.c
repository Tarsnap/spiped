#include <stdlib.h>

/* Problem with FreeBSD 11.0 merely linking with -lrt. */
static void
pl_freebsd_link_lrt(void)
{
	/* Do nothing. */
}

int
main(void)
{

	/* Test potential memory leaks. */
	pl_freebsd_link_lrt();

	/* Success! */
	exit(0);
}
