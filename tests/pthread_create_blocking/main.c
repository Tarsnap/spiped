#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "monoclock.h"
#include "pthread_create_blocking.h"
#include "warnp.h"

#define DEBUG 0

#define WAIT_MS 10

#define WAIT1 0x01
#define WAIT2 0x02
#define WAIT3 0x04

static struct testcase {
	int options;
} tests[] = {
	{WAIT1 | WAIT2 | WAIT3},
	{WAIT1 | WAIT2},
	{WAIT1 | WAIT3},
	{WAIT2 | WAIT3},
	{WAIT1},
	{WAIT2},
	{WAIT3},
	{0}
};

#if DEBUG
#define REPORT_TIME(info, tv)		\
	printf("%.08f\t%s\n", timeval_diff(info.main_start, info.tv), #tv);
#endif

/* It's ok if "prev" and "next" are the same time. */
#define CHECK_TIME(prev, next) do {					\
		if (timeval_diff(prev, next) < 0) {			\
			warn0(#prev " is after " #next);		\
			goto err0;					\
		}							\
	}								\
	while(0);

/* Each test. */
struct info {
	int options;
	struct timeval main_start;
	struct timeval main_after_block;
	struct timeval main_after_join;
	struct timeval thr_start;
	struct timeval thr_initialized;
	struct timeval thr_stop;
};

/* Wait duration can be interrupted by signals. */
static int
wait_ms(size_t msec)
{
	struct timespec ts;

	/* Try to wait for the desired duration. */
	ts.tv_sec = msec / 1000;
	ts.tv_nsec = (msec % 1000) * 1000000;
	nanosleep(&ts, NULL);

	/* Success! */
	return (0);
}

static void *
workthread(void * cookie, int init_func(void *), void * init_cookie)
{
	struct info * info = cookie;

	/* For checking the order. */
	if (monoclock_get(&info->thr_start))
		warnp("monoclock_get");

	/* Wait if the test calls for it. */
	if (info->options & WAIT1)
		wait_ms(WAIT_MS);

	/* For checking the order. */
	if (monoclock_get(&info->thr_initialized))
		warnp("monoclock_get");

	/* We've started. */
	if (init_func(init_cookie))
		warn0("init function failed");

	/* Wait if the test calls for it. */
	if (info->options & WAIT2)
		wait_ms(WAIT_MS);

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
	if (monoclock_get(&info1.main_start)) {
		warnp("monoclock_get");
		goto err0;
	}
	if (monoclock_get(&info2.main_start)) {
		warnp("monoclock_get");
		goto err0;
	}

	/* Launch thr1 and record time. */
	if ((rc = pthread_create_blocking(&thr1, NULL, workthread, &info1))) {
		warn0("pthread_create_blocking: %s", strerror(rc));
		goto err0;
	}
	if (monoclock_get(&info1.main_after_block)) {
		warnp("monoclock_get");
		goto err0;
	}

	/* Launch thr2 and record time. */
	if ((rc = pthread_create_blocking(&thr2, NULL, workthread, &info2))) {
		warn0("pthread_create_blocking: %s", strerror(rc));
		goto err0;
	}
	if (monoclock_get(&info2.main_after_block)) {
		warnp("monoclock_get");
		goto err0;
	}

	/* Wait if the test calls for it. */
	if (info1.options & WAIT3)
		wait_ms(WAIT_MS);
	if (info2.options & WAIT3)
		wait_ms(WAIT_MS);

	/* Join thr1 and record time. */
	if ((rc = pthread_join(thr1, NULL))) {
		warn0("pthread_join: %s", strerror(rc));
		goto err0;
	}
	if (monoclock_get(&info1.main_after_join)) {
		warnp("monoclock_get");
		goto err0;
	}

	/* Join thr2 and record time. */
	if ((rc = pthread_join(thr2, NULL))) {
		warn0("pthread_join: %s", strerror(rc));
		goto err0;
	}
	if (monoclock_get(&info2.main_after_join)) {
		warnp("monoclock_get");
		goto err0;
	}

#if DEBUG
	REPORT_TIME(info1, main_start);
	REPORT_TIME(info1, thr_start);
	REPORT_TIME(info1, thr_initialized);
	REPORT_TIME(info1, thr_stop);
	REPORT_TIME(info1, main_after_block);
	REPORT_TIME(info1, main_after_join);
	REPORT_TIME(info2, main_start);
	REPORT_TIME(info2, thr_start);
	REPORT_TIME(info2, thr_initialized);
	REPORT_TIME(info2, thr_stop);
	REPORT_TIME(info2, main_after_block);
	REPORT_TIME(info2, main_after_join);
#endif

	/* Check each thread independently. */
	CHECK_TIME(info1.main_start, info1.main_after_block);
	CHECK_TIME(info1.main_after_block, info1.main_after_join);
	CHECK_TIME(info1.thr_start, info1.thr_initialized);
	CHECK_TIME(info1.thr_initialized, info1.thr_stop);
	CHECK_TIME(info2.main_start, info2.main_after_block);
	CHECK_TIME(info2.main_after_block, info2.main_after_join);
	CHECK_TIME(info2.thr_start, info2.thr_initialized);
	CHECK_TIME(info2.thr_initialized, info2.thr_stop);

	/* Check inter-connectedness. */
	CHECK_TIME(info1.main_start, info1.thr_start);
	CHECK_TIME(info1.thr_initialized, info1.main_after_block);
	CHECK_TIME(info1.thr_stop, info1.main_after_join);
	CHECK_TIME(info2.main_start, info2.thr_start);
	CHECK_TIME(info2.thr_initialized, info2.main_after_block);
	CHECK_TIME(info2.thr_stop, info2.main_after_join);

	/* Check inter-connectedness between 1 and 2. */
	CHECK_TIME(info1.main_start, info2.main_start);
	CHECK_TIME(info1.main_after_block, info2.main_after_block);
	CHECK_TIME(info1.main_after_join, info2.main_after_join);

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}

int
main(int argc, char ** argv)
{
	size_t i, j;

	WARNP_INIT;
	(void)argc; /* UNUSED */

	for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
		for (j = 0; j < sizeof(tests) / sizeof(tests[0]); j++) {
#if DEBUG
			printf("Relative times for test %zu-%zu:\n", i, j);
#endif
			if (runner(tests[i].options, tests[j].options)) {
				warn0("failed test #%zu", i);
				goto err0;
			}
		}
	}

	/* Success! */
	exit(0);

err0:
	/* Failure! */
	exit(1);
}
