#ifndef _PERFTESTS_H_
#define _PERFTESTS_H_

#include <stddef.h>
#include <stdint.h>

/**
 * perftest_buffers(nbytes, sizes, nsizes, nbytes_warmup, cputime,
 *     init_func, func, clean_func, cookie):
 * Time using ${func} to process ${nbytes} bytes in blocks of ${sizes}.
 * Before timing any block sizes, process ${nbytes_warmup} bytes with the
 * maximum size in ${sizes}.  If ${cputime} is non-zero, attempt to use
 * cpu time rather than wall-clock time.  Invoke callback functions as:
 *     init_func(cookie, buffer, buflen)
 *     func(cookie, buffer, buflen, nbuffers)
 *     clean_func(cookie)
 * where ${buffer} is large enough to hold the maximum buffer size.  Print
 * the time and speed of processing each buffer size.  ${init_func} and
 * ${clean_func} may be NULL.  If ${init_func} has completed successfully,
 * then ${clean_func} will be called if there is a subsequent error.
 */
int perftest_buffers(size_t, const size_t *, size_t, size_t, int,
    int (*)(void *, uint8_t *, size_t),
    int (*)(void *, uint8_t *, size_t, size_t),
    int (*)(void *), void *);

#endif /* !_PERFTESTS_H_ */
