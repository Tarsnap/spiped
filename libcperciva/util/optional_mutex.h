#ifndef OPTIONAL_MUTEX_H_
#define OPTIONAL_MUTEX_H_

#include <pthread.h>

/**
 * optional_mutex_lock(mutex):
 * If OPTIONAL_MUTEX_PTHREAD_YES is defined, call pthread_mutex_lock();
 * if OPTIONAL_MUTEX_PTHREAD_NO is defined, do nothing.
 */
int optional_mutex_lock(pthread_mutex_t *);

/**
 * optional_mutex_unlock(mutex):
 * If OPTIONAL_MUTEX_PTHREAD_YES is defined, call pthread_mutex_unlock();
 * if OPTIONAL_MUTEX_PTHREAD_NO is defined, do nothing.
 */
int optional_mutex_unlock(pthread_mutex_t *);

#endif /* !OPTIONAL_MUTEX_H_ */
