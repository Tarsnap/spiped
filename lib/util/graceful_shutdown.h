#ifndef GRACEFUL_SHUTDOWN_H_
#define GRACEFUL_SHUTDOWN_H_

/**
 * graceful_shutdown_init(callback, caller_cookie):
 * Initialize a signal handler for SIGTERM, and start a continuous 1-second
 * timer which checks if SIGTERM was given; if detected, call ${callback} and
 * give it the ${caller_cookie}.  Do not retry this function upon failure.
 */
int graceful_shutdown_init(int (*)(void *), void *);

/**
 * graceful_shutdown_manual(void):
 * Shutdown immediately, without needing a SIGTERM.  This must be called from
 * the thread which called graceful_shutdown_initialize().  If a shutdown
 * has already been started, do nothing.
 */
int graceful_shutdown_manual(void);

#endif /* !GRACEFUL_SHUTDOWN_H_ */
