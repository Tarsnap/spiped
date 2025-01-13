#ifndef THREAD_SYNC_H_
#define THREAD_SYNC_H_

/**
 * Normal usage:
 * 1) thread_sync_init()
 * 2) start a new thread
 * 3) thread x: thread_sync_wait()
 *    thread y: thread_sync_signal()
 * 4) both threads: thread_sync_done()
 */

/* Opaque structure. */
struct thread_sync;

/**
 * thread_sync_init(void):
 * Initialize an inter-process synchronization barrier.  This must be called
 * before forking.
 */
struct thread_sync * thread_sync_init(void);

/**
 * thread_sync_wait(IS):
 * Block until thread_sync_signal() has been called with ${IS}.
 */
int thread_sync_wait(struct thread_sync *);

/**
 * thread_sync_signal(IS):
 * Indicate that ${IS} should no longer block.
 */
int thread_sync_signal(struct thread_sync *);

/**
 * thread_sync_done(IS):
 * Free resources associated with ${IS}.
 */
int thread_sync_done(struct thread_sync *);

#endif /* !THREAD_SYNC_H_ */
