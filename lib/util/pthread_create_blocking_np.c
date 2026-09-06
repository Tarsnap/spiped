#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "warnp.h"

#include "pthread_create_blocking_np.h"

struct wrapped_cookie {
	/* User-supplied. */
	void * (*start_routine)(void *);
	void * arg;

	/* Synchronization. */
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int running;		/* 1 if the thread has started. */

	/* In case the synchronization fails. */
	int rc_sync;		/* non-zero if synchronization failed. */
};

/* Routine which is executed by pthread_create(). */
static void *
wrapped_thread(void * cookie)
{
	struct wrapped_cookie * U = cookie;
	void * (*start_routine)(void *);
	void * arg;
	int rc;

	/*
	 * Copy the user-supplied parameters, because U will not be valid
	 * after we signal the parent thread that we have started running.
	 */
	start_routine = U->start_routine;
	arg = U->arg;

	/* Lock mutex. */
	if ((rc = pthread_mutex_lock(&U->mutex)))
		goto err1;

	/* Set the "running" flag and signal the condition variable. */
	U->running = 1;
	if ((rc = pthread_cond_signal(&U->cond)))
		goto err2;

	/* Unlock mutex.  This allows the parent thread to free U. */
	if ((rc = pthread_mutex_unlock(&U->mutex)))
		goto err1;

	/* Run the desired routine. */
	return (start_routine(arg));

err2:
	/*
	 * Don't record any errors in this clean-up; the existing value of rc
	 * is more important.
	 */
	pthread_mutex_unlock(&U->mutex);
err1:
	/* If there was an error, then we can still write to U. */
	U->rc_sync = rc;

	/* Failure! */
	return (NULL);
}

/**
 * pthread_create_blocking_np(thread, attr, start_routine, arg):
 * Run pthread_create() and block until the the ${thread} has started.  The
 * thread will execute ${start_routine} with ${arg} as its sole argument.
 * When ${start_routine} finishes, make its returned value available via
 * pthread_join().  If successful, return 0; otherwise return the error number.
 */
int
pthread_create_blocking_np(pthread_t * restrict thread,
    const pthread_attr_t * restrict attr,
    void * (*start_routine)(void *), void * restrict arg)
{
	struct wrapped_cookie * U;
	int rc;
	int rc_cleanup;

	/*
	 * Allocate our cookie and store parameters.  The C standard does not
	 * require that variables with automatic storage duration are
	 * accessible by other threads; POSIX (and thus pthreads) does provide
	 * that guarantee, but out of an abundance of caution we take the
	 * option of using malloc in case this code ends up running on a
	 * non-POSIX system.
	 */
	if ((U = malloc(sizeof(struct wrapped_cookie))) == NULL) {
		rc = errno;
		goto err0;
	}
	U->start_routine = start_routine;
	U->arg = arg;

	/* Initialize synchronization-related variables. */
	if ((rc = pthread_mutex_init(&U->mutex, NULL)) != 0)
		goto err1;
	if ((rc = pthread_cond_init(&U->cond, NULL)) != 0)
		goto err2;
	U->running = 0;
	U->rc_sync = 0;

	/* Lock mutex. */
	if ((rc = pthread_mutex_lock(&U->mutex)) != 0)
		goto err3;

	/* Create the thread. */
	if ((rc = pthread_create(thread, attr, wrapped_thread, U)) != 0) {
		/*
		 * Don't record any pthread_mutex_unlock() error here; the
		 * error from pthread_create() is more important.
		 */
		pthread_mutex_unlock(&U->mutex);
		goto err3;
	}

	/* Wait for the thread to have started, then unlock the mutex. */
	while (!U->running) {
		/* Wait until signalled. */
		if ((rc = pthread_cond_wait(&U->cond, &U->mutex)) != 0)
			goto err5;

		/* Quit if there was an error in the synchronization. */
		if ((rc = U->rc_sync) != 0)
			goto err5;
	}

	/*
	 * The thread is running, so it owns ${arg} and will run
	 * ${start_routine} to completion.  From here we must report
	 * success: this function promises that on failure the provided
	 * routine was not run, and a caller which frees ${arg} after an
	 * error return would be freeing memory the thread is still using.
	 *
	 * Anything which fails below is one of our own synchronization
	 * variables.  The caller cannot act on that, so warn and carry on.
	 */
	if ((rc_cleanup = pthread_mutex_unlock(&U->mutex)) != 0) {
		warn0("pthread_mutex_unlock: %s", strerror(rc_cleanup));

		/* Do not destroy a mutex which we might still hold. */
		goto done;
	}
	if ((rc_cleanup = pthread_cond_destroy(&U->cond)) != 0) {
		warn0("pthread_cond_destroy: %s", strerror(rc_cleanup));
		goto done;
	}
	if ((rc_cleanup = pthread_mutex_destroy(&U->mutex)) != 0)
		warn0("pthread_mutex_destroy: %s", strerror(rc_cleanup));

done:
	/* Clean up. */
	free(U);

	/* Success! */
	return (0);

err5:
	/*
	 * Don't record any errors in this clean-up; the existing value of rc
	 * is more important.
	 */
	pthread_mutex_unlock(&U->mutex);
	pthread_cancel(*thread);
	pthread_join(*thread, NULL);
err3:
	pthread_cond_destroy(&U->cond);
err2:
	pthread_mutex_destroy(&U->mutex);
err1:
	free(U);
err0:
	/* Failure! */
	return (rc);
}
