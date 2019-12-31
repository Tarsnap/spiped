#include <sys/socket.h>

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "noeintr.h"
#include "warnp.h"

#include "pushbits.h"

struct push {
	uint8_t buf[BUFSIZ];
	int in;
	int out;
	/* The below are only valid during thread startup. */
	int * initialized_p;
	pthread_mutex_t * mutex_p;
	pthread_cond_t * cond_p;
};

static void
workthread_cleanup(void * cookie)
{
	struct push * P = cookie;

	/* Close the descriptor (we either hit EOF, or received a cancel). */
	close(P->in);

	/*
	 * Try to shut down the descriptor we're writing to.  Ignore ENOTSOCK,
	 * since it might, indeed, not be a socket.
	 */
	if (shutdown(P->out, SHUT_WR)) {
		if (errno != ENOTSOCK) {
			warnp("Error shutting down socket");
			exit(1);
		}
	}

	/* Free our parameters. */
	free(P);
}

/* Bit-pushing thread. */
static void *
workthread(void * cookie)
{
	struct push * P = cookie;
	ssize_t readlen;
	int rc;

	/* Set up cleanup function. */
	pthread_cleanup_push(workthread_cleanup, P);

	/* Notify that we've started. */
	if ((rc = pthread_mutex_lock(P->mutex_p)) != 0) {
		warn0("pthread_mutex_lock: %s", strerror(rc));
		exit(1);
	}
	*P->initialized_p = 1;
	if ((rc = pthread_cond_signal(P->cond_p)) != 0) {
		warn0("pthread_cond_signal: %s", strerror(rc));
		exit(1);
	}
	if ((rc = pthread_mutex_unlock(P->mutex_p)) != 0) {
		warn0("pthread_mutex_unlock: %s", strerror(rc));
		exit(1);
	}

	/* Main thread is now destroying the startup-related variables. */

	/* Infinite loop unless we hit EOF or an error. */
	do {
		/* Read data and die on error. */
		if ((readlen = read(P->in, P->buf, BUFSIZ)) == -1) {
			if (errno == EINTR)
				continue;
			warnp("Error reading");
			exit(1);
		}

		/* If we hit EOF, exit the loop. */
		if (readlen == 0)
			break;

		/* Write the data back out. */
		if (noeintr_write(P->out, &P->buf, (size_t)readlen)
		    != readlen) {
			warnp("Error writing");
			exit(1);
		}
	} while (1);

	/* Clean up. */
	pthread_cleanup_pop(1);

	/* We're done. */
	return (NULL);
}

/**
 * pushbits(in, out, thr):
 * Create a thread which copies data from ${in} to ${out} and
 * store the thread ID in ${thr}.  Wait until the thread has started.
 */
int
pushbits(int in, int out, pthread_t * thr)
{
	struct push * P;
	int rc;
	int initialized;
	pthread_mutex_t mutex;
	pthread_cond_t cond;

	/* Initialize thread startup-related variables. */
	initialized = 0;
	if (pthread_mutex_init(&mutex, NULL)) {
		warnp("pthread_mutex_init");
		goto err0;
	}
	if (pthread_cond_init(&cond, NULL)) {
		warnp("pthread_cond_init");
		goto err1;
	}

	/* Allocate structure. */
	if ((P = malloc(sizeof(struct push))) == NULL)
		goto err2;
	P->in = in;
	P->out = out;
	P->initialized_p = &initialized;
	P->mutex_p = &mutex;
	P->cond_p = &cond;

	/* Create thread. */
	if ((rc = pthread_create(thr, NULL, workthread, P)) != 0) {
		warn0("pthread_create: %s", strerror(rc));
		goto err3;
	}

	/* Wait for the thread to have started. */
	if ((rc = pthread_mutex_lock(&mutex)) != 0) {
		warn0("pthread_mutex_lock: %s", strerror(rc));

		/* Don't try to clean up if there's an error here. */
		exit(1);
	}
	while (!initialized) {
		if ((rc = pthread_cond_wait(&cond, &mutex)) != 0) {
			warn0("pthread_cond_signal: %s", strerror(rc));
			exit(1);
		}
	}
	if (pthread_mutex_unlock(&mutex)) {
		warn0("pthread_mutex_unlock: %s", strerror(rc));
		exit(1);
	}

	/* Clean up startup-related variables. */
	if ((rc = pthread_cond_destroy(&cond)) != 0) {
		warn0("pthread_cond_destroy: %s", strerror(rc));
		goto err2;
	}
	if ((rc = pthread_mutex_destroy(&mutex)) != 0) {
		warn0("pthread_mutex_destroy: %s", strerror(rc));
		goto err1;
	}

	/* Success! */
	return (0);

err3:
	free(P);
err2:
	if ((rc = pthread_cond_destroy(&cond)) != 0)
		warn0("pthread_cond_destroy: %s", strerror(rc));
err1:
	if ((rc = pthread_mutex_destroy(&mutex)) != 0)
		warn0("pthread_mutex_destroy: %s", strerror(rc));
err0:
	/* Failure! */
	return (-1);
}
