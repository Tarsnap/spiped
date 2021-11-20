#include <stdlib.h>
#include <string.h>

#include "pthread_create_blocking_np.h"
#include "timing.h"
#include "warnp.h"

/* This is an arbitrary pointer to return via pthread_join(). */
static int arbitrary_value;
static void * MY_RUN_RETURN = &arbitrary_value;

static void *
workfunc(void * cookie)
{

	(void)cookie; /* UNUSED */

	/* Special value for testing; normally this would be NULL. */
	return (MY_RUN_RETURN);
}

static int
check_basic(void)
{
	pthread_t thr;
	void * rc_join = NULL;
	void * rc_join_expected;
	int rc;

	/* Try to make a thread. */
	if ((rc = pthread_create_blocking_np(&thr, NULL, workfunc, NULL))) {
		warnp("pthread_create_blocking_np: %s", strerror(rc));
		goto err0;
	}

	/* If we succeeded in making a thread, wait for it to finish. */
	if ((rc = pthread_join(thr, &rc_join))) {
		warn0("pthread_join: %s", strerror(rc));
		goto err0;
	}

	/* Check the value returned from the workfunc. */
	rc_join_expected = MY_RUN_RETURN;
	if (rc_join != rc_join_expected) {
		warn0("pthread_join expected to get %p; instead, got %p",
		    rc_join, rc_join_expected);
		goto err0;
	}

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}

int
main(int argc, char ** argv)
{

	WARNP_INIT;
	(void)argc; /* UNUSED */

	/* Check basic stuff. */
	if (check_basic())
		goto err0;

	/* Check the relative timing of points in the code. */
	if (check_timing())
		goto err0;

	/* Success! */
	exit(0);

err0:
	/* Failure! */
	exit(1);
}
