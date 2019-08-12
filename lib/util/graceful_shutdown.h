#ifndef _GRACEFUL_SHUTDOWN_H_
#define _GRACEFUL_SHUTDOWN_H_

/**
 * graceful_shutdown_initialize(callback, caller_cookie):
 * Initializes a signal handler for SIGTERM, and starts a continuous 1-second
 * timer which checks if SIGTERM was given; if detected, calls ${callback} and
 * gives it the ${caller_cookie}.
 */
int graceful_shutdown_initialize(int (*)(void *), void *);

#endif /* !_GRACEFUL_SHUTDOWN_H_ */
