#ifndef STANDALONE_H_
#define STANDALONE_H_

#include <stddef.h>

/**
 * hmac_perftest(perfsizes, num_perf, nbytes_perftest, nbytes_warmup):
 * Performance test for HMAC-SHA256.
 */
int hmac_perftest(const size_t *, size_t, size_t, size_t);

/**
 * aesctr_perftest(perfsizes, num_perf, nbytes_perftest, nbytes_warmup):
 * Performance test for AES-CTR.
 */
int aesctr_perftest(const size_t *, size_t, size_t, size_t);

/**
 * aesctr_hmac_perftest(perfsizes, num_perf, nbytes_perftest, nbytes_warmup):
 * Performance test for AES-CTR followed by HMAC-SHA256.
 */
int aesctr_hmac_perftest(const size_t *, size_t, size_t, size_t);

/**
 * pce_perftest(perfsizes, num_perf, nbytes_perftest, nbytes_warmup):
 * Performance test for proto_crypt_enc().
 */
int pce_perftest(const size_t *, size_t, size_t, size_t);

/**
 * pipe_perftest(perfsizes, num_perf, nbytes_perftest, nbytes_warmup):
 * Performance test for one proto_pipe() over a socketpair.
 */
int pipe_perftest(const size_t *, size_t, size_t, size_t);

#endif /* !STANDALONE_H_ */
