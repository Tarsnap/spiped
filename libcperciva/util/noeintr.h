#ifndef _NOEINTR_H_
#define _NOEINTR_H_

#include <unistd.h>

/**
 * noeintr_write(d, buf, nbytes):
 * Write ${nbytes} bytes of data from ${buf} to the file descriptor ${d} per
 * the write(2) system call, but looping until completion if interrupted by
 * a signal.  Return ${nbytes} on success or -1 on error.
 */
ssize_t noeintr_write(int, const void *, size_t);

#endif /* !_NOEINTR_H_ */
