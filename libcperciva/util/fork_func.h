#ifndef FORK_FUNC_H_
#define FORK_FUNC_H_

#include <sys/types.h>

/**
 * fork_func(func, cookie):
 * Fork and run ${func} in a new process, with ${cookie} as the sole argument.
 */
pid_t fork_func(int (*)(void *), void *);

/**
 * fork_func_wait(pid):
 * Wait for the process ${pid} to finish.  Print any error arising from ${pid}.
 * If ${pid} exited cleanly, return its exit code; otherwise, return -1.
 */
int fork_func_wait(pid_t);

#endif /* !FORK_FUNC_H_ */
