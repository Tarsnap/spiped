#include <stdio.h>
#include <stdlib.h>

#include "cpusupport.h"
#include "parsenum.h"
#include "warnp.h"

#include "standalone.h"

/* Smaller buffers are padded, so no point testing smaller values. */
static const size_t perfsizes[] = {1024};
static const size_t num_perf = sizeof(perfsizes) / sizeof(perfsizes[0]);
static size_t nbytes_perftest = 100000000;		/* 100 MB */
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

int
main(int argc, char * argv[])
{
	int desired_test;
	size_t multiplier;

	WARNP_INIT;

	/* Parse command line. */
	if ((argc < 2) || (argc > 3)) {
		fprintf(stderr, "usage: test_standalone_enc NUM [MULT]\n");
		exit(1);
	}
	if (PARSENUM(&desired_test, argv[1], 1, 5)) {
		warnp("parsenum");
		goto err0;
	}
	if (argc == 3) {
		/* Multiply number of bytes by the user-supplied value. */
		if (PARSENUM(&multiplier, argv[2], 1, 1000)) {
			warnp("parsenum");
			goto err0;
		}
		nbytes_perftest *= multiplier;
	}

	/* Report what we're doing. */
	print_hardware("Testing spiped speed limits");

	/* Run the desired test. */
	switch (desired_test) {
	case 1:
		if (hmac_perftest(perfsizes, num_perf,
		    nbytes_perftest, nbytes_warmup))
			goto err0;
		break;
	case 2:
		if (aesctr_perftest(perfsizes, num_perf,
		    nbytes_perftest, nbytes_warmup))
			goto err0;
		break;
	case 3:
		if (aesctr_hmac_perftest(perfsizes, num_perf,
		    nbytes_perftest, nbytes_warmup))
			goto err0;
		break;
	case 4:
		if (pce_perftest(perfsizes, num_perf,
		    nbytes_perftest, nbytes_warmup))
			goto err0;
		break;
	case 5:
		if (pipe_perftest(perfsizes, num_perf,
		    nbytes_perftest, nbytes_warmup))
			goto err0;
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
