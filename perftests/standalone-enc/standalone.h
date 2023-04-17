#ifndef STANDALONE_H_
#define STANDALONE_H_

#include <stddef.h>

/**
 * standalone_hmac(perfsizes, num_perf, nbytes_perftest, nbytes_warmup):
 * Performance test for HMAC-SHA256.
 */
int standalone_hmac(const size_t *, size_t, size_t, size_t);

/**
 * standalone_aesctr(perfsizes, num_perf, nbytes_perftest, nbytes_warmup):
 * Performance test for AES-CTR.
 */
int standalone_aesctr(const size_t *, size_t, size_t, size_t);

/**
 * standalone_aesctr_hmac(perfsizes, num_perf, nbytes_perftest, nbytes_warmup):
 * Performance test for AES-CTR followed by HMAC-SHA256.
 */
int standalone_aesctr_hmac(const size_t *, size_t, size_t, size_t);

/**
 * standalone_pce(perfsizes, num_perf, nbytes_perftest, nbytes_warmup):
 * Performance test for proto_crypt_enc().
 */
int standalone_pce(const size_t *, size_t, size_t, size_t);

/**
 * standalone_transfer_noencrypt(perfsizes, num_perf, nbytes_perftest,
 *     nbytes_warmup, method):
 * Performance test for sending data through two communication endpoints with
 * no encryption.  If ${method} is 0, use two pipes; if ${method} is 1, use
 * two socketpairs.
 */
int standalone_transfer_noencrypt(const size_t *, size_t, size_t, size_t, int);

/**
 * standalone_pipe_socketpair_one(perfsizes, num_perf, nbytes_perftest,
 *     nbytes_warmup):
 * Performance test for one proto_pipe() over a socketpair.
 */
int standalone_pipe_socketpair_one(const size_t *, size_t, size_t, size_t);

#endif /* !STANDALONE_H_ */
