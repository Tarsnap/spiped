#ifndef _GRACEFUL_SHUTDOWN_H_
#define _GRACEFUL_SHUTDOWN_H_

/**
 * graceful_shutdown_initialize(callback, caller_cookie):
 * Initialize a signal handler for SIGTERM, and start a continuous 1-second
 * timer which checks if SIGTERM was given; if detected, call ${callback} and
 * give it the ${caller_cookie}.
 */
int graceful_shutdown_initialize(int (*)(void *), void *);

/**
 * graceful_shutdown_manual(void):
 * Shutdown immediately, without needing a SIGTERM.
 */
void graceful_shutdown_manual(void);

#endif /* !_GRACEFUL_SHUTDOWN_H_ */
