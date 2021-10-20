#ifndef _PTHREAD_CREATE_BLOCKING_H_
#define _PTHREAD_CREATE_BLOCKING_H_

#include <pthread.h>

/**
 * pthread_create_blocking(thread, attr, start_routine, arg):
 * Run pthread_create() and block until ${start_routine} has indicated that it
 * has started and finished any initialization.  The ${start_routine} must be:
 *     void * funcname(void * cookie, int init(void*), void * init_cookie)
 * When ${start_routine} is ready for the main thread to continue, it must
 * call init(init_cookie).
 */
int pthread_create_blocking(pthread_t * restrict,
    const pthread_attr_t * restrict,
    void *(*)(void *, int(void *), void *), void * restrict);

#endif /* !_PTHREAD_CREATE_BLOCKING_H_ */
