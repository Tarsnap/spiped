#ifndef IPC_SYNC_H_
#define IPC_SYNC_H_

/**
 * Normal usage:
 * 1) ipc_sync_init()
 * 2) fork a new process
 * 3) process x: ipc_sync_wait()
 *    process y: ipc_sync_signal()
 * 4) both processes: ipc_sync_done()
 *
 * If there's a need to make _wait() or _signal() as fast as possible
 * (e.g. for a benchmark or a performance bottleneck), call the
 * relevant _prep() function before the critical section.
 */

/* Opaque structure. */
struct ipc_sync;

/**
 * ipc_sync_init(void):
 * Initialize an inter-process synchronization barrier.  This must be called
 * before forking.
 */
struct ipc_sync * ipc_sync_init(void);

/**
 * ipc_sync_wait(IS):
 * Block until ipc_sync_signal() has been called with ${IS}.
 */
int ipc_sync_wait(struct ipc_sync *);

/**
 * ipc_sync_signal(IS):
 * Indicate that ${IS} should no longer block.
 */
int ipc_sync_signal(struct ipc_sync *);

/**
 * ipc_sync_wait_prep(IS):
 * Perform any operation(s) that would speed up an upcoming call to
 * ipc_sync_wait().  Calling this function is optional.
 */
int ipc_sync_wait_prep(struct ipc_sync *);

/**
 * ipc_sync_signal_prep(IS):
 * Perform any operation(s) that would speed up an upcoming call to
 * ipc_sync_signal().  Calling this function is optional.
 */
int ipc_sync_signal_prep(struct ipc_sync *);

/**
 * ipc_sync_done(IS):
 * Free resources associated with ${IS}.
 */
int ipc_sync_done(struct ipc_sync *);

#endif /* !IPC_SYNC_H_ */
