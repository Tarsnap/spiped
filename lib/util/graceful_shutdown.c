#include <signal.h>
#include <stddef.h>

#include "events.h"
#include "warnp.h"

#include "graceful_shutdown.h"

/* Data from parent code. */
static int (* begin_shutdown)(void *);
static void (* sighandler_sigterm_orig)(int);
static void * caller_cookie;
static void * timer_cookie;

/* Flag to show that SIGTERM was received. */
static volatile sig_atomic_t should_shutdown = 0;

/* Signal handler for SIGTERM to perform a graceful shutdown. */
static void
graceful_shutdown_handler(int signo)
{

	(void)signo; /* UNUSED */
	should_shutdown = 1;
}

/* Requests a graceful shutdown of the caller via the cookie info. */
static int
graceful_shutdown(void * cookie)
{

	(void)cookie; /* UNUSED */

	/* This timer has expired. */
	timer_cookie = NULL;

	/* Use the callback function, or schedule another check in 1 second. */
	if (should_shutdown) {
		if (begin_shutdown(caller_cookie) != 0) {
			warn0("Failed to begin shutdown");
			goto err0;
		}

		/* Restore original SIGTERM handler. */
		if (signal(SIGTERM, sighandler_sigterm_orig) == SIG_ERR)
			warnp("Failed to restore original SIGTERM handler");
	} else {
		if ((timer_cookie = events_timer_register_double(
		    graceful_shutdown, NULL, 1.0)) == NULL) {
			warnp("Failed to register the graceful shutdown timer");
			goto err0;
		}
	}

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}

/**
 * graceful_shutdown_initialize(callback, caller_cookie):
 * Initializes a signal handler for SIGTERM, and starts a continuous 1-second
 * timer which checks if SIGTERM was given; if detected, calls ${callback} and
 * gives it the ${caller_cookie}.
 */
int
graceful_shutdown_initialize(int (* begin_shutdown_parent)(void *),
    void * caller_cookie_parent)
{

	/* Record callback data. */
	begin_shutdown = begin_shutdown_parent;
	caller_cookie = caller_cookie_parent;

	/* Start signal handler and save the original one. */
	if ((sighandler_sigterm_orig = signal(SIGTERM,
	    graceful_shutdown_handler)) == SIG_ERR) {
		warnp("signal");
		goto err0;
	}

	/* Periodically check whether a signal was received. */
	if ((timer_cookie = events_timer_register_double(
	    graceful_shutdown, NULL, 1.0)) == NULL) {
		warnp("Failed to register the graceful shutdown timer");
		goto err0;
	}

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}
