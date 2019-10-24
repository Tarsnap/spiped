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

	/* Play games to avoid the compiler optimzing away strlen(). */
	size_t (* const volatile strlen_func)(const char *) = strlen;
	volatile size_t len = strlen_func(problem);
	(void)len;
}

/* Part of the pthread init. */
static void *
do_nothing(void * cookie)
{

	(void)cookie;
	return (NULL);
}

/* Problem with FreeBSD 11.0 and creating/freeing a thread. */
static void
pl_freebsd_pthread(void)
{
	pthread_t thread;

	pthread_create(&thread, NULL, do_nothing, NULL);
	pthread_join(thread, NULL);
}

#define MEMLEAKTEST(x) { #x, x }
static const struct memleaktest {
	const char * const name;
	void (* const volatile func)(void);
} tests[] = {
	MEMLEAKTEST(pl_freebsd_link_lrt),
	MEMLEAKTEST(pl_freebsd_strlen),
	MEMLEAKTEST(pl_freebsd_pthread)
};
static const int num_tests = sizeof(tests) / sizeof(tests[0]);

int
main(int argc, char * argv[])
{
	int i;

	if (argc == 2) {
		/* Run the relevant function. */
		for (i = 0; i < num_tests; i++) {
			if ((strcmp(argv[1], tests[i].name)) == 0)
				tests[i].func();
		}
	} else {
		/* Print test names. */
		for (i = 0; i < num_tests; i++)
			printf("%s\n", tests[i].name);
	}

	/* Success! */
	exit(0);
}
