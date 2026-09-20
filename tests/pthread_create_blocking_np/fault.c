#include <errno.h>
#include <pthread.h>
#include <string.h>

#include "fault.h"
#include "warnp.h"

/*
 * Compile the implementation under an alternate public symbol and route only
 * its cleanup pthread calls through these wrappers.  This keeps the regression
 * portable -- in particular, it does not depend on GNU ld's --wrap support.
 */
static int test_pthread_mutex_unlock(pthread_mutex_t *);
static int test_pthread_cond_destroy(pthread_cond_t *);
static int test_pthread_mutex_destroy(pthread_mutex_t *);

#define pthread_create_blocking_np pthread_create_blocking_np_faultable
#define pthread_mutex_unlock test_pthread_mutex_unlock
#define pthread_cond_destroy test_pthread_cond_destroy
#define pthread_mutex_destroy test_pthread_mutex_destroy
#include "../../lib/util/pthread_create_blocking_np.c"
#undef pthread_mutex_destroy
#undef pthread_cond_destroy
#undef pthread_mutex_unlock
#undef pthread_create_blocking_np

static pthread_t parent_thread;
static const char * failpoint;
static int injected;

static int
should_fail(const char * point)
{

	/* The worker also unlocks the same mutex; only fail in the parent. */
	if (!pthread_equal(pthread_self(), parent_thread))
		return (0);
	if (injected || strcmp(failpoint, point))
		return (0);

	injected = 1;
	return (1);
}

static int
test_pthread_mutex_unlock(pthread_mutex_t * mutex)
{

	if (should_fail("unlock"))
		return (EPERM);
	return (pthread_mutex_unlock(mutex));
}

static int
test_pthread_cond_destroy(pthread_cond_t * cond)
{

	if (should_fail("cond"))
		return (EBUSY);
	return (pthread_cond_destroy(cond));
}

static int
test_pthread_mutex_destroy(pthread_mutex_t * mutex)
{

	if (should_fail("mutex"))
		return (EBUSY);
	return (pthread_mutex_destroy(mutex));
}

static void *
workfunc_fault(void * cookie)
{

	return (cookie);
}

static int
run_fault(const char * point, int expect_injected)
{
	pthread_t thr;
	void * rc_join = NULL;
	int marker;
	int rc;

	parent_thread = pthread_self();
	failpoint = point;
	injected = 0;
	marker = 443;

	/*
	 * Every synthetic failure below happens after wrapped_thread() has copied
	 * ${arg} and marked itself running.  The public contract therefore requires
	 * success: the worker, not the caller, owns ${arg} at that point.
	 */
	if ((rc = pthread_create_blocking_np_faultable(&thr, NULL,
	    workfunc_fault, &marker)) != 0) {
		warn0("pthread_create_blocking_np (%s): %s",
		    point, strerror(rc));
		goto err0;
	}
	if ((rc = pthread_join(thr, &rc_join)) != 0) {
		warn0("pthread_join (%s): %s", point, strerror(rc));
		goto err0;
	}
	if (rc_join != &marker) {
		warn0("pthread join value changed under %s cleanup failure", point);
		goto err0;
	}
	if (injected != expect_injected) {
		warn0("%s cleanup failure injection mismatch", point);
		goto err0;
	}

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}

/**
 * check_faults(void):
 * Check the ownership contract when cleanup fails after the child is running.
 */
int
check_faults(void)
{

	/* Control: wrappers must leave the ordinary success path unchanged. */
	if (run_fault("none", 0))
		goto err0;

	/* These three used to return an error after the worker owned ${arg}. */
	if (run_fault("unlock", 1))
		goto err0;
	if (run_fault("cond", 1))
		goto err0;
	if (run_fault("mutex", 1))
		goto err0;

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}
