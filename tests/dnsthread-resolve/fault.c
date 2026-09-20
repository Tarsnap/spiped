#include <errno.h>
#include <pthread.h>

#include "events.h"
#include "warnp.h"

#include "fault.h"

/*
 * Compile dnsthread.c into this test binary with wrappers around the two
 * operations whose failures matter to dnsthread_resolveone().  The wrappers
 * delegate normally except for one explicitly requested failure, so the
 * ordinary dnsthread-resolve test still exercises the production code.
 */
static int test_events_network_register(int (*)(void *), void *, int, int);
static int test_events_network_cancel(int, int);
static int test_pthread_cond_signal(pthread_cond_t *);

#define events_network_register test_events_network_register
#define events_network_cancel test_events_network_cancel
#define pthread_cond_signal test_pthread_cond_signal
#include "../../lib/dnsthread/dnsthread.c"
#undef pthread_cond_signal
#undef events_network_cancel
#undef events_network_register

static int fail_register;
static int fail_signal;
static int register_calls;
static int cancel_calls;
static int signal_calls;

static int
test_events_network_register(int (* callback)(void *), void * cookie, int s,
    int op)
{

	register_calls++;
	if (fail_register) {
		fail_register = 0;
		errno = ENOMEM;
		return (-1);
	}

	return (events_network_register(callback, cookie, s, op));
}

static int
test_events_network_cancel(int s, int op)
{

	cancel_calls++;
	return (events_network_cancel(s, op));
}

static int
test_pthread_cond_signal(pthread_cond_t * cv)
{

	signal_calls++;
	if (fail_signal) {
		fail_signal = 0;
		return (EINVAL);
	}

	return (pthread_cond_signal(cv));
}

static int
found_never(void * cookie, struct sock_addr ** sas)
{

	(void)cookie;
	(void)sas;

	/* The injected failure paths must never reach the resolver callback. */
	warn0("resolver callback ran after failed handoff");
	return (-1);
}

static void
reset_faults(void)
{

	fail_register = 0;
	fail_signal = 0;
	register_calls = 0;
	cancel_calls = 0;
	signal_calls = 0;
}

static int
check_register_failure(void)
{
	DNSTHREAD T;

	if ((T = dnsthread_spawn()) == NULL) {
		warn0("dnsthread_spawn");
		goto err0;
	}

	reset_faults();
	fail_register = 1;

	if (dnsthread_resolveone(T, "127.0.0.1:1", found_never, NULL) != -1) {
		warn0("registration failure was not propagated");
		goto err1;
	}

	/*
	 * The listener must be registered before ownership is handed to the
	 * worker.  If registration fails, there must be no signal at all.
	 */
	if ((register_calls != 1) || (signal_calls != 0) ||
	    (cancel_calls != 0) || (T->state != THREAD_SLEEPING)) {
		warn0("registration failure handed work to resolver");
		goto err1;
	}

	if (dnsthread_kill(T)) {
		warn0("dnsthread_kill");
		goto err0;
	}

	/* Success! */
	return (0);

err1:
	dnsthread_kill(T);
err0:
	/* Failure! */
	return (-1);
}

static int
check_signal_failure(void)
{
	DNSTHREAD T;

	if ((T = dnsthread_spawn()) == NULL) {
		warn0("dnsthread_spawn");
		goto err0;
	}

	reset_faults();
	fail_signal = 1;

	if (dnsthread_resolveone(T, "127.0.0.1:1", found_never, NULL) != -1) {
		warn0("signal failure was not propagated");
		goto err1;
	}

	/*
	 * Once listener registration succeeds, a failed signal must undo the
	 * registration and restore SLEEPING before returning to the caller.
	 */
	if ((register_calls != 1) || (signal_calls != 1) ||
	    (cancel_calls != 1) || fail_signal ||
	    (T->state != THREAD_SLEEPING)) {
		warn0("signal failure did not unwind resolver state");
		goto err1;
	}

	if (dnsthread_kill(T)) {
		warn0("dnsthread_kill");
		goto err0;
	}

	/* Success! */
	return (0);

err1:
	/* The one-shot signal fault is consumed before this cleanup. */
	fail_signal = 0;
	dnsthread_kill(T);
err0:
	/* Failure! */
	return (-1);
}

/**
 * check_faults(void):
 * Verify that failures before a DNS worker owns a request leave it sleeping
 * and leave no registered wakeup callback behind.
 */
int
check_faults(void)
{

	if (check_register_failure())
		goto err0;
	if (check_signal_failure())
		goto err0;

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}
