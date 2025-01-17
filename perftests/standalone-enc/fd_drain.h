#ifndef FD_DRAIN_H_
#define FD_DRAIN_H_

#include <sys/types.h>

/**
 * fd_drain_fork(fd):
 * Create a new process to drain bytes from the file descriptor ${fd} until it
 * receives an EOF.  Return the process ID of the new process, or -1 upon
 * error.
 */
pid_t fd_drain_fork(int);

#endif /* !FD_DRAIN_H_ */
