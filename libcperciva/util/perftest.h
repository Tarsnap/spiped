#ifndef _PERFTESTS_H_
#define _PERFTESTS_H_

#include <stddef.h>
#include <stdint.h>

/**
 * perftest_buffers(nbytes, sizes, nsizes, nbytes_warmup, init_func, func,
 *     cookie):
 * Time using ${func} to process ${nbytes} bytes in blocks of ${sizes}.
 * Before timing any block sizes, process ${nbytes_warmup} bytes with the
 * maximum size in ${sizes}.  Invoke callback functions as:
 *     init_func(cookie, buffer, buflen)
 *     func(cookie, buffer, buflen, nbuffers)
 * where ${buffer} is large enough to hold the maximum buffer size.  Print
 * the time and speed of processing each buffer size.
 */
int perftest_buffers(size_t, const size_t *, size_t, size_t,
    int (*)(void *, uint8_t *, size_t),
    int (*)(void *, uint8_t *, size_t, size_t), void *);

#endif /* !_PERFTESTS_H_ */
