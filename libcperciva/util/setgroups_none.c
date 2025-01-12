/**
 * APISUPPORT CFLAGS: NONPOSIX_SETGROUPS
 */

/*
 * There is no setgroups() in the POSIX standard, so if we want to drop
 * supplementary groups, we need to use platform-specific code.  This must
 * happen before the regular includes, as in some cases we need to define other
 * symbols before including the relevant header.
 */
#if defined(__linux__)
/* setgroups() includes for Linux. */
#define _BSD_SOURCE 1
#define _DEFAULT_SOURCE 1

#include <grp.h>

#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__APPLE__)
/* setgroups() includes for FreeBSD, NetBSD, MacOS X. */
#include <sys/param.h>
#include <unistd.h>

#elif defined(__OpenBSD__) || defined(__osf__)
/* setgroups() includes for OpenBSD. */
#include <sys/types.h>
#include <unistd.h>

#elif defined(__sun) || defined(__hpux)
/* setgroups() includes for Solaris. */
#include <unistd.h>

#elif defined(_AIX)
/* setgroups() includes for AIX. */
#include <grp.h>

#else
/* Unknown OS; we don't know how to get setgroups(). */
#define NO_SETGROUPS

#endif /* end includes for setgroups() */

#include <stddef.h>

#include "setgroups_none.h"

/**
 * setgroups_none(void):
 * Attempt to leave all supplementary groups.  If we do not know how to do this
 * on the platform, return 0 anyway.
 */
int
setgroups_none(void)
{

#ifdef NO_SETGROUPS
	/* Not supported. */
	return (0);
#else
	return (setgroups(0, NULL));
#endif
}
