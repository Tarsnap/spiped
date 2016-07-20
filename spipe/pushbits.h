#ifndef _PUSHBITS_H_
#define _PUSHBITS_H_

/**
 * pushbits(in, out):
 * Create a thread which copies data from ${in} to ${out}.
 */
int pushbits(int, int, pthread_t *);

#endif /* !_PUSHBITS_H_ */
