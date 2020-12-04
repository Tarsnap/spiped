#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Problem with FreeBSD 11.0 merely linking with -lrt. */
static void
pl_freebsd_link_lrt(void)
{
	/* Do nothing. */
}

/*
 * Not FreeBSD's fault; the old valgrind-3.10.1 port doesn't recognize what's
 * happening when their optimized strlen() looks at 8 bytes at once.
 */
static void
pl_freebsd_strlen(void)
{
	/* More than 8 bytes, and not a multiple of 8. */
	char problem[] = {1, 2, 3, 4, 5, 6, 7, 8, 0};

	/* Play games to avoid the compiler optimizing away strlen(). */
	size_t (* const volatile strlen_func)(const char *) = strlen;
	volatile size_t len = strlen_func(problem);
	(void)len;
}

/* Problem with FreeBSD 12.1 and printf(). */
static void
pl_freebsd_printf_space(void)
{
 
	printf(" ");
}

/* Problem with FreeBSD 12.1 and printf(). */
static void
pl_freebsd_printf_space_newline(void)
{

	printf(" \n");
}

/* Part of the pthread init. */
static void *
pl_workthread_nothing(void * cookie)
{

	(void)cookie; /* UNUSED */
	return (NULL);
}

/* Problem with FreeBSD 11.0 and creating/freeing a thread. */
static void
pl_freebsd_pthread_nothing(void)
{
	pthread_t thr;
	int rc;

	if ((rc = pthread_create(&thr, NULL, pl_workthread_nothing, NULL)))
		fprintf(stderr, "pthread_create: %s", strerror(rc));
	if ((rc = pthread_join(thr, NULL)))
		fprintf(stderr, "pthread_join: %s", strerror(rc));
}

#define MEMLEAKTEST(x) { #x, x }
static const struct memleaktest {
	const char * const name;
	void (* const volatile func)(void);
} tests[] = {
	MEMLEAKTEST(pl_freebsd_link_lrt),
	MEMLEAKTEST(pl_freebsd_strlen),
	MEMLEAKTEST(pl_freebsd_printf_space),
	MEMLEAKTEST(pl_freebsd_printf_space_newline),
	MEMLEAKTEST(pl_freebsd_pthread_nothing)
};
static const int num_tests = sizeof(tests) / sizeof(tests[0]);

int
main(int argc, char * argv[])
{
	int i;

	if (argc == 2) {
		/* Run the relevant function. */
		for (i = 0; i < num_tests; i++) {
			if ((strcmp(argv[1], tests[i].name)) == 0) {
				tests[i].func();
				goto success;
			}
		}

		/* We didn't find the desired function name. */
		goto err0;
	} else {
		/* Print test names. */
		for (i = 0; i < num_tests; i++)
			printf("%s\n", tests[i].name);
	}

success:
	/* Success! */
	exit(0);

err0:
	/* Failure! */
	exit(1);
}
