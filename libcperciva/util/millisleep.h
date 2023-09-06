#ifndef MILLISLEEP_H_
#define MILLISLEEP_H_

#include <stddef.h>

#include <time.h>

/**
 * millisleep(msec):
 * Wait up to ${msec} milliseconds.  This duration can be interrupted by
 * signals.
 */
static inline void
millisleep(size_t msec)
{
	struct timespec ts;

	ts.tv_sec = msec / 1000;
	ts.tv_nsec = (msec % 1000) * 1000000;
	nanosleep(&ts, NULL);
}

#endif /* !MILLISLEEP_H_ */
