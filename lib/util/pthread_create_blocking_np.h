#ifndef _PTHREAD_CREATE_BLOCKING_NP_H_
#define _PTHREAD_CREATE_BLOCKING_NP_H_

#include <pthread.h>

/**
 * pthread_create_blocking_np(thread, attr, start_routine, arg):
 * Run pthread_create() and block until the the ${thread} has started.  The
 * thread will execute ${start_routine} with ${arg} as its sole argument.
 * When ${start_routine} finishes, make its returned value available via
 * pthread_join().  If successful, return 0; otherwise return the error number.
 */
int pthread_create_blocking_np(pthread_t * restrict,
    const pthread_attr_t * restrict, void *(*)(void *), void *);

#endif /* !_PTHREAD_CREATE_BLOCKING_NP_H_ */
