#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "warnp.h"

#include "pthread_create_blocking.h"

enum status {
	INITIALIZE_BEGIN,
	INITIALIZE_DONE,
	SHUTDOWN_BEGIN,
	SHUTDOWN_DONE
};

struct start_cookie {
	/* Normal arguments for pthread. */
	void *(*start_routine)(void *, int(void *), void *);
	void * cookie;		/* This is called "arg" in the POSIX API. */

	/* Only valid during thread startup. */
	enum status * status_p;
	pthread_mutex_t * mutex_p;
	pthread_cond_t * cond_p;
};

/* Will be called by the client code. */
static int
initialized_func(void * cookie)
{
	struct start_cookie * U = cookie;
	int rc;

	/* Lock mutex. */
	if ((rc = pthread_mutex_lock(U->mutex_p))) {
		warn0("pthread_mutex_lock: %s", strerror(rc));
		goto err0;
	}

	/* Bail if the master thread says we should. */
	if (*U->status_p == SHUTDOWN_BEGIN) {
		warn0("in initialized_func, SHUTDOWN_BEGIN");
		goto err1;
	}

	/* Indicate that the client has finished initializing. */
	*U->status_p = INITIALIZE_DONE;
	if ((rc = pthread_cond_signal(U->cond_p)) != 0) {
		warn0("pthread_cond_signal: %s", strerror(rc));
		goto err1;
	}

	/* Unlock mutex. */
	if ((rc = pthread_mutex_unlock(U->mutex_p))) {
		warn0("pthread_mutex_unlock: %s", strerror(rc));
		goto err0;
	}

	/* Success! */
	return (0);

err1:
	/* This function will not access U after releasing the mutex. */
	*U->status_p = SHUTDOWN_DONE;
	if ((rc = pthread_cond_signal(U->cond_p))) {
		warn0("pthread_cond_signal: %s", strerror(rc));
		goto err1;
	}
	if ((rc = pthread_mutex_unlock(U->mutex_p))) {
		warn0("pthread_mutex_unlock: %s", strerror(rc));
		goto err0;
	}
err0:
	/* Failure! */
	return (-1);
}

static void *
wrapped_start(void * cookie)
{
	struct start_cookie * U = cookie;

	/* Call the supplied function with the extra function and cookie. */
	return (U->start_routine(U->cookie, initialized_func, U));
}

/**
 * pthread_create_blocking(thread, attr, start_routine, arg):
 * Run pthread_create() and block until ${start_routine} has indicated that it
 * has started and finished any initialization.  The ${start_routine} must be:
 *     void * funcname(void * cookie, int init(void*), void * init_cookie)
 * When ${start_routine} is ready for the main thread to continue, it must
 * call init(init_cookie).
 */
int
pthread_create_blocking(pthread_t * restrict thread,
    const pthread_attr_t * restrict attr,
    void *(*start_routine)(void *, int(void *), void *),
    void * restrict arg)
{
	struct start_cookie * U;
	int rc;
	enum status status = INITIALIZE_BEGIN;
	pthread_mutex_t mutex;
	pthread_cond_t cond;

	/* Allocate our cookie. */
	if ((U = malloc(sizeof(struct start_cookie))) == NULL) {
		warnp("malloc");
		goto err0;
	}
	U->cookie = arg;
	U->start_routine = start_routine;
	U->status_p = &status;

	/* Initialize thread startup-related variables. */
	if ((rc = pthread_mutex_init(&mutex, NULL)) != 0) {
		warn0("pthread_mutex_init: %s", strerror(rc));
		goto err1;
	}
	if ((rc = pthread_cond_init(&cond, NULL)) != 0) {
		warnp("pthread_cond_init: %s", strerror(rc));
		goto err2;
	}
	U->mutex_p = &mutex;
	U->cond_p = &cond;

	/* Lock mutex. */
	if ((rc = pthread_mutex_lock(&mutex)) != 0) {
		warn0("pthread_mutex_lock: %s", strerror(rc));
		*U->status_p = SHUTDOWN_BEGIN;
		goto err4;
	}

	/* Create the thread. */
	if ((rc = pthread_create(thread, attr, wrapped_start, U)) != 0) {
		warn0("pthread_create: %s", strerror(rc));

		if (pthread_mutex_unlock(&mutex))
			warn0("pthread_mutex_unlock: %s", strerror(rc));
		goto err3;
	}

	/* Wait for the thread to have started. */
	while (status == INITIALIZE_BEGIN) {
		if ((rc = pthread_cond_wait(&cond, &mutex)) != 0) {
			warn0("pthread_cond_signal: %s", strerror(rc));
			*U->status_p = SHUTDOWN_BEGIN;
			goto err5;
		}
	}
	if (pthread_mutex_unlock(&mutex)) {
		warn0("pthread_mutex_unlock: %s", strerror(rc));
		*U->status_p = SHUTDOWN_BEGIN;
		goto err4;
	}

	/* Clean up startup-related variables. */
	if ((rc = pthread_cond_destroy(&cond)) != 0) {
		warn0("pthread_cond_destroy: %s", strerror(rc));
		*U->status_p = SHUTDOWN_BEGIN;
		goto err2;
	}
	if ((rc = pthread_mutex_destroy(&mutex)) != 0) {
		warn0("pthread_mutex_destroy: %s", strerror(rc));
		*U->status_p = SHUTDOWN_BEGIN;
		goto err1;
	}
	free(U);

	/* Success! */
	return (0);

err5:
	if (pthread_mutex_unlock(&mutex))
		warn0("pthread_mutex_unlock: %s", strerror(rc));
err4:
	if ((rc = pthread_cancel(*thread)) != 0) {
		if (rc != ESRCH)
			warn0("pthread_cancel: %s", strerror(rc));
	}
	if ((rc = pthread_join(*thread, NULL)) != 0)
		warn0("pthread_join: %s", strerror(rc));
err3:
	if ((rc = pthread_cond_destroy(&cond)) != 0)
		warn0("pthread_cond_destroy: %s", strerror(rc));
err2:
	if ((rc = pthread_mutex_destroy(&mutex)) != 0)
		warn0("pthread_mutex_destroy: %s", strerror(rc));
err1:
	free(U);
err0:
	/* Failure! */
	return (-1);
}
