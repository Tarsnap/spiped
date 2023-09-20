#ifdef OPTIONAL_MUTEX_PTHREAD_YES
#include <pthread.h>
#endif

#include "optional_mutex.h"

/* Sanity check OPTIONAL_MUTEX_PTHREAD_* defines. */
#if defined(OPTIONAL_MUTEX_PTHREAD_YES) && defined(OPTIONAL_MUTEX_PTHREAD_NO)
#error "You must not define both OPTIONAL_MUTEX_PTHREAD_YES "	\
    "and OPTIONAL_MUTEX_PTHREAD_NO"
#elif !(defined(OPTIONAL_MUTEX_PTHREAD_YES) ||			\
    defined(OPTIONAL_MUTEX_PTHREAD_NO))
#error "You must define either OPTIONAL_MUTEX_PTHREAD_YES "	\
    "or OPTIONAL_MUTEX_PTHREAD_NO"
#endif

#ifdef OPTIONAL_MUTEX_PTHREAD_YES
/* Clang normally warns if you have a mutex held at the end of a function. */
#ifdef __clang__
_Pragma("clang diagnostic push")
_Pragma("clang diagnostic ignored \"-Wthread-safety-analysis\"")
#endif
#endif

/**
 * optional_mutex_lock(mutex):
 * If OPTIONAL_MUTEX_PTHREAD_YES is defined, call pthread_mutex_lock();
 * if OPTIONAL_MUTEX_PTHREAD_NO is defined, do nothing.
 */
int
optional_mutex_lock(pthread_mutex_t * mutex)
{

#ifdef OPTIONAL_MUTEX_PTHREAD_YES
	return (pthread_mutex_lock(mutex));
#else
	(void)mutex; /* UNUSED */
	return (0);
#endif
}

/**
 * optional_mutex_unlock(mutex):
 * If OPTIONAL_MUTEX_PTHREAD_YES is defined, call pthread_mutex_unlock();
 * if OPTIONAL_MUTEX_PTHREAD_NO is defined, do nothing.
 */
int
optional_mutex_unlock(pthread_mutex_t * mutex)
{

#ifdef OPTIONAL_MUTEX_PTHREAD_YES
	return (pthread_mutex_unlock(mutex));
#else
	(void)mutex; /* UNUSED */
	return (0);
#endif
}

#ifdef OPTIONAL_MUTEX_PTHREAD_YES
#ifdef __clang__
_Pragma("clang diagnostic pop")
#endif
#endif
