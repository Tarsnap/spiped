#include <sys/time.h>

#include <pthread.h>
#include <string.h>

#include "millisleep.h"
#include "monoclock.h"
#include "pthread_create_blocking_np.h"
#include "warnp.h"

#include "timing.h"

#define WAIT_MS 10

#define WAIT_THREAD_AFTER_START 0x01
#define WAIT_PARENT_AFTER_BLOCK 0x02

static struct testcase {
	int options;
} tests[] = {
	{WAIT_THREAD_AFTER_START},
	{WAIT_PARENT_AFTER_BLOCK},
	{WAIT_THREAD_AFTER_START | WAIT_PARENT_AFTER_BLOCK},
	{0}
};

/* It's ok if "prev" and "next" are the same time. */
#define CHECK_TIME(prev, next) do {					\
		if (timeval_diff(prev, next) < 0) {			\
			warn0(#prev " is after " #next);		\
			goto err0;					\
		}							\
	} while (0)

/* Each test. */
struct info {
	int options;
	struct timeval parent_start;
	struct timeval parent_after_block;
	struct timeval parent_after_join;
	struct timeval thr_start;
	struct timeval thr_stop;
};

static void *
workfunc(void * cookie)
{
	struct info * info = cookie;

	/* For checking the order. */
	if (monoclock_get(&info->thr_start))
		warnp("monoclock_get");

	/* Wait if the test calls for it. */
	if (info->options & WAIT_THREAD_AFTER_START)
		millisleep(WAIT_MS);

	/* For checking the order. */
	if (monoclock_get(&info->thr_stop))
		warnp("monoclock_get");

	return (NULL);
}

static int
runner(int options1, int options2)
{
	struct info info1, info2;
	pthread_t thr1, thr2;
	int rc;

	info1.options = options1;
	info2.options = options2;

	/*
	 * Get starting time for each struct info.  Do this with two separate
	 * monoclock_get() calls, to check that monoclock increases.
	 */
	if (monoclock_get(&info1.parent_start)) {
		warnp("monoclock_get");
		goto err0;
	}
	if (monoclock_get(&info2.parent_start)) {
		warnp("monoclock_get");
		goto err0;
	}

	/* Launch thr1 and record time. */
	if ((rc = pthread_create_blocking_np(&thr1, NULL, workfunc, &info1))) {
		warn0("pthread_create_blocking_np: %s", strerror(rc));
		goto err0;
	}
	if (monoclock_get(&info1.parent_after_block)) {
		warnp("monoclock_get");
		goto err0;
	}

	/* Launch thr2 and record time. */
	if ((rc = pthread_create_blocking_np(&thr2, NULL, workfunc, &info2))) {
		warn0("pthread_create_blocking_np: %s", strerror(rc));
		goto err0;
	}
	if (monoclock_get(&info2.parent_after_block)) {
		warnp("monoclock_get");
		goto err0;
	}

	/* Wait if the test calls for it. */
	if (info1.options & WAIT_PARENT_AFTER_BLOCK)
		millisleep(WAIT_MS);
	if (info2.options & WAIT_PARENT_AFTER_BLOCK)
		millisleep(WAIT_MS);

	/* Join thr1 and record time. */
	if ((rc = pthread_join(thr1, NULL))) {
		warn0("pthread_join: %s", strerror(rc));
		goto err0;
	}
	if (monoclock_get(&info1.parent_after_join)) {
		warnp("monoclock_get");
		goto err0;
	}

	/* Join thr2 and record time. */
	if ((rc = pthread_join(thr2, NULL))) {
		warn0("pthread_join: %s", strerror(rc));
		goto err0;
	}
	if (monoclock_get(&info2.parent_after_join)) {
		warnp("monoclock_get");
		goto err0;
	}

	/* Check each thread independently. */
	CHECK_TIME(info1.parent_start, info1.parent_after_block);
	CHECK_TIME(info1.parent_after_block, info1.parent_after_join);
	CHECK_TIME(info1.thr_start, info1.thr_stop);
	CHECK_TIME(info2.parent_start, info2.parent_after_block);
	CHECK_TIME(info2.parent_after_block, info2.parent_after_join);
	CHECK_TIME(info2.thr_start, info2.thr_stop);

	/* Check inter-connectedness. */
	CHECK_TIME(info1.parent_start, info1.thr_start);
	CHECK_TIME(info1.thr_stop, info1.parent_after_join);
	CHECK_TIME(info2.parent_start, info2.thr_start);
	CHECK_TIME(info2.thr_stop, info2.parent_after_join);

	/* Check inter-connectedness between 1 and 2. */
	CHECK_TIME(info1.parent_start, info2.parent_start);
	CHECK_TIME(info1.parent_after_block, info2.parent_after_block);
	CHECK_TIME(info1.parent_after_join, info2.parent_after_join);

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}

/**
 * check_timing(void):
 * Check that pthread_create_blocking_np() results in the expected relative
 * timing.
 */
int
check_timing(void)
{
	size_t i, j;

	for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
		for (j = 0; j < sizeof(tests) / sizeof(tests[0]); j++) {
			if (runner(tests[i].options, tests[j].options)) {
				warn0("failed test %zu", i);
				goto err0;
			}
		}
	}

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}
