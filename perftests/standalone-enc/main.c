#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpusupport.h"
#include "crypto_aes.h"
#include "crypto_aesctr.h"
#include "parsenum.h"
#include "perftest.h"
#include "proto_crypt.h"
#include "sha256.h"
#include "sysendian.h"
#include "warnp.h"

/* Smaller buffers are padded, so no point testing smaller values. */
static const size_t perfsizes[] = {1024};
static const size_t num_perf = sizeof(perfsizes) / sizeof(perfsizes[0]);
static const size_t nbytes_perftest = 100000000;	/* 100 MB */
static const size_t nbytes_warmup = 10000000;		/* 10 MB */

/* Print a string, then whether or not we're using hardware instructions. */
static void
print_hardware(const char * str)
{

	/* Inform the user of the general topic... */
	printf("%s", str);

	/* ... and whether we're using hardware acceleration or not. */
#if defined(CPUSUPPORT_CONFIG_FILE)
#if defined(CPUSUPPORT_X86_SHANI) && defined(CPUSUPPORT_X86_SSSE3)
	if (cpusupport_x86_shani() && cpusupport_x86_ssse3())
		printf(" using hardware SHANI");
	else
#endif
#if defined(CPUSUPPORT_X86_SSE2)
	if (cpusupport_x86_sse2())
		printf(" using hardware SSE2");
	else
#endif
#if defined(CPUSUPPORT_ARM_SHA256)
	if (cpusupport_arm_sha256())
		printf(" using hardware SHA256");
	else
#endif
		printf(" using software SHA");

#if defined(CPUSUPPORT_X86_AESNI)
	if (cpusupport_x86_aesni())
		printf(" and hardware AESNI.\n");
	else
#endif
		printf(" and software AES.\n");
#else
	printf(" with unknown hardware acceleration status.\n");
#endif /* CPUSUPPORT_CONFIG_FILE */
}

static int
hmac_init(void * cookie, uint8_t * buf, size_t buflen)
{
	HMAC_SHA256_CTX * ctx = cookie;
	uint8_t kbuf[32];
	size_t i;

	/* (Re-)Initialize the context. */
	memset(kbuf, 0, 32);
	HMAC_SHA256_Init(ctx, kbuf, 32);

	/* Set the input. */
	for (i = 0; i < buflen; i++)
		buf[i] = (uint8_t)(i & 0xff);

	/* Success! */
	return (0);
}

static int
hmac_func(void * cookie, uint8_t * buf, size_t buflen, size_t nreps)
{
	HMAC_SHA256_CTX * ctx = cookie;
	uint8_t hbuf[32];
	uint8_t pnum_exp[8];
	size_t i;

	/* Do the hashing. */
	for (i = 0; i < nreps; i++) {
		HMAC_SHA256_Update(ctx, buf, buflen);

		/* Hash the iteration number as well. */
		be64enc(pnum_exp, i);
		HMAC_SHA256_Update(ctx, pnum_exp, 8);
	}
	HMAC_SHA256_Final(hbuf, ctx);

	/* Success! */
	return (0);
}

static int
hmac_perftest(void)
{
	HMAC_SHA256_CTX ctx;

	/* Report what we're doing. */
	printf("Testing HMAC_SHA256 with iteration numbers\n");

	/* Time the function. */
	if (perftest_buffers(nbytes_perftest, perfsizes, num_perf,
	    nbytes_warmup, 0, hmac_init, hmac_func, NULL, &ctx)) {
		warn0("perftest_buffers");
		goto err0;
	}

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (1);
}

static int
aesctr_init(void * cookie, uint8_t * buf, size_t buflen)
{
	size_t i;

	(void)cookie; /* UNUSED */

	/* Set the input. */
	for (i = 0; i < buflen; i++)
		buf[i] = (uint8_t)(i & 0xff);

	/* Success! */
	return (0);
}

static int
aesctr_func(void * cookie, uint8_t * buf, size_t buflen, size_t nreps)
{
	struct crypto_aes_key * k_aes = cookie;
	size_t i;

	/* Do the hashing. */
	for (i = 0; i < nreps; i++) {
		/*
		 * In proto_crypt_enc(), we would append the length to buf,
		 * then encrypt the buffer + 4 bytes of length.  For
		 * simplicity, this test does not imitate that details.
		 */
		crypto_aesctr_buf(k_aes, i, buf, buf, buflen);
	}

	/* Success! */
	return (0);
}

static int
aesctr_perftest(void)
{
	struct crypto_aes_key * k_aes;
	uint8_t kbuf[32];

	/* Report what we're doing. */
	printf("Testing AES-CTR\n");

	/* Initialize. */
	memset(kbuf, 0, 32);
	if ((k_aes = crypto_aes_key_expand(kbuf, 32)) == NULL)
		goto err0;

	/* Time the function. */
	if (perftest_buffers(nbytes_perftest, perfsizes, num_perf,
	    nbytes_warmup, 0, aesctr_init, aesctr_func, NULL, k_aes)) {
		warn0("perftest_buffers");
		goto err0;
	}

	/* Clean up. */
	crypto_aes_key_free(k_aes);

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (1);
}

/* Cookie for HMAC_SHA256 with crypto_aesctr. */
struct aesctr_hmac_cookie {
	HMAC_SHA256_CTX * ctx;
	struct crypto_aes_key * k_aes;
};

static int
aesctr_hmac_init(void * cookie, uint8_t * buf, size_t buflen)
{
	struct aesctr_hmac_cookie * ahc = cookie;
	uint8_t kbuf[32];
	size_t i;

	/* (Re-)Initialize the context. */
	memset(kbuf, 0, 32);
	HMAC_SHA256_Init(ahc->ctx, kbuf, 32);

	/* Set the input. */
	for (i = 0; i < buflen; i++)
		buf[i] = (uint8_t)(i & 0xff);

	/* Success! */
	return (0);
}

static int
aesctr_hmac_func(void * cookie, uint8_t * buf, size_t buflen, size_t nreps)
{
	struct aesctr_hmac_cookie * ahc = cookie;
	uint8_t hbuf[32];
	uint8_t pnum_exp[8];
	size_t i;

	/* Do the hashing. */
	for (i = 0; i < nreps; i++) {
		/*
		 * In proto_crypt_enc(), we would append the length to buf,
		 * then encrypt the buffer + 4 bytes of length.  After that,
		 * we'd hash the resulting (larger) buffer.  For simplicity,
		 * this test does not imitate those details.
		 */
		crypto_aesctr_buf(ahc->k_aes, i, buf, buf, buflen);
		HMAC_SHA256_Update(ahc->ctx, buf, buflen);
		be64enc(pnum_exp, i);
		HMAC_SHA256_Update(ahc->ctx, pnum_exp, 8);
	}
	HMAC_SHA256_Final(hbuf, ahc->ctx);

	/* Success! */
	return (0);
}

static int
aesctr_hmac_perftest(void)
{
	struct aesctr_hmac_cookie aesctr_hmac_cookie;
	struct aesctr_hmac_cookie * ahc = &aesctr_hmac_cookie;
	HMAC_SHA256_CTX ctx;
	uint8_t kbuf[32];

	/* Report what we're doing. */
	printf("Testing HMAC_SHA256 with AES-CTR\n");

	/* Initialize. */
	ahc->ctx = &ctx;
	memset(kbuf, 0, 32);
	if ((ahc->k_aes = crypto_aes_key_expand(kbuf, 32)) == NULL)
		goto err0;

	/* Time the function. */
	if (perftest_buffers(nbytes_perftest, perfsizes, num_perf,
	    nbytes_warmup, 0, aesctr_hmac_init, aesctr_hmac_func, NULL, ahc)) {
		warn0("perftest_buffers");
		goto err0;
	}

	/* Clean up. */
	crypto_aes_key_free(ahc->k_aes);

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (1);
}

/* Cookie for proto_crypt_enc(). */
struct pce {
	struct proto_keys * k;
};

static int
pce_init(void * cookie, uint8_t * buf, size_t buflen)
{
	struct pce * pce = cookie;
	uint8_t kbuf[64];
	size_t i;

	/* Set up encryption key. */
	memset(kbuf, 0, 64);
	if ((pce->k = mkkeypair(kbuf)) == NULL)
		goto err0;

	/* Set the input. */
	for (i = 0; i < buflen; i++)
		buf[i] = (uint8_t)(i & 0xff);

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}

static int
pce_func(void * cookie, uint8_t * buf, size_t buflen, size_t nreps)
{
	struct pce * pce = cookie;
	uint8_t encbuf[PCRYPT_ESZ];
	size_t i;

	/* Encrypt a bunch of times. */
	for (i = 0; i < nreps; i++)
		proto_crypt_enc(buf, buflen, encbuf, pce->k);

	/* Success! */
	return (0);
}

static int
pce_cleanup(void * cookie)
{
	struct pce * pce = cookie;

	/* Clean up. */
	proto_crypt_free(pce->k);

	/* Success! */
	return (0);
}

static int
pce_perftest(void)
{
	struct pce pce_actual;
	struct pce * pce = &pce_actual;

	/* Report what we're doing. */
	printf("Testing proto_crypt_enc()\n");

	/* Time the function. */
	if (perftest_buffers(nbytes_perftest, perfsizes, num_perf,
	    nbytes_warmup, 0, pce_init, pce_func, pce_cleanup, pce)) {
		warn0("perftest_buffers");
		goto err0;
	}

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (1);
}

int
main(int argc, char * argv[])
{
	int desired_test;

	WARNP_INIT;

	/* Parse command line. */
	if (argc != 2) {
		fprintf(stderr, "usage: test_standalone_enc NUM\n");
		exit(1);
	}
	if (PARSENUM(&desired_test, argv[1], 1, 4)) {
		warnp("parsenum");
		goto err0;
	}

	/* Report what we're doing. */
	print_hardware("Testing spiped speed limits");

	/* Run the desired test. */
	switch(desired_test) {
	case 1:
		hmac_perftest();
		break;
	case 2:
		aesctr_perftest();
		break;
	case 3:
		aesctr_hmac_perftest();
		break;
	case 4:
		pce_perftest();
		break;
	default:
		warn0("invalid test number");
		goto err0;
	}

	/* Success! */
	exit(0);

err0:
	/* Failure! */
	exit(1);
}
