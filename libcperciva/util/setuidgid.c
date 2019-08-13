/* We use non-POSIX functionality in this file. */
#undef _POSIX_C_SOURCE
#undef _XOPEN_SOURCE

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

#include <assert.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "parsenum.h"
#include "warnp.h"

#include "setuidgid.h"

/* Function prototypes related to supplementary groups. */
static int setgroups_none(void);
static int check_supplementary_groups_none(void);

/* Function prototypes related to uid and gid. */
static int set_group(const char *);
static int set_user(const char *);
static int string_extract_user_group(const char *, char **, char **);

/**
 * setgroups_none(void):
 * Attempt to leave all supplementary groups.  If we do not know how to do this
 * on the platform, return 0 anyway.
 */
static int
setgroups_none(void)
{

#ifndef NO_SETGROUPS
	if (setgroups(0, NULL)) {
		/* We handle EPERM separately; keep it in errno. */
		if (errno != EPERM)
			warnp("setgroups()");
		goto err0;
	}
#endif

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}

/* Check if we're in any supplementary groups. */
static int
check_supplementary_groups_none(void)
{
	gid_t grouplist[1];
	int ngroups;

	/* Find number of groups. */
	if ((ngroups = getgroups(0, NULL)) < 0) {
		warnp("getgroups()");
		goto err0;
	}

	/* Check group membership. */
	if (ngroups > 1)
		/* Definitely failed. */
		goto err0;
	if (ngroups == 1) {
		/*
		 * POSIX allows getgroups() to return the effective group ID,
		 * so if we're in exactly one group, we need to check that GID.
		 */
		if (getgroups(1, grouplist) != 1) {
			warnp("getgroups()");
			goto err0;
		}
		/* POSIX: getegid() shall not modify errno. */
		if (grouplist[0] != getegid())
			goto err0;
	}

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}

/* Set the GID to the group represented by ${groupname}. */
static int
set_group(const char * groupname)
{
	gid_t gid;
	struct group * group_info;

	/*
	 * Attempt to convert the group name to a group ID.  If the database
	 * lookup fails with an error, return; but if it fails without an error
	 * (indicating that it successfully found that the name does not exist)
	 * fall back to trying to parse the name as a numeric group ID.
	 */
	errno = 0;
	if ((group_info = getgrnam(groupname)) != NULL) {
		gid = group_info->gr_gid;
	} else if (errno) {
		warnp("getgrnam(\"%s\")", groupname);
		goto err0;
	} else if (PARSENUM(&gid, groupname)) {
		warn0("No such group: %s", groupname);
		goto err0;
	}

	/* Set GID. */
	if (setgid(gid)) {
		/* We handle EPERM separately; keep it in errno. */
		if (errno != EPERM)
			warnp("setgid(%lu)", (unsigned long int)gid);
		goto err0;
	}

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}

/* Set the UID to the user represented by ${username}. */
static int
set_user(const char * username)
{
	uid_t uid;
	struct passwd * user_info;

	/* See similar code in set_gid(). */
	errno = 0;
	if ((user_info = getpwnam(username)) != NULL) {
		uid = user_info->pw_uid;
	} else if (errno) {
		warnp("getpwnam(\"%s\")", username);
		goto err0;
	} else if (PARSENUM(&uid, username)) {
		warn0("No such user: %s", username);
		goto err0;
	}

	/* Set UID. */
	if (setuid(uid)) {
		/* We handle EPERM separately; keep it in errno. */
		if (errno != EPERM)
			warnp("setuid(%lu)", (unsigned long int)uid);
		goto err0;
	}

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}

/* Parses "username", ":groupname", or "username:groupname". */
static int
string_extract_user_group(const char * combined, char ** username_p,
    char ** groupname_p)
{
	size_t pos;
	size_t len;

	/* Sanity check. */
	assert(combined != NULL);

	/* Search for ":" and get string length. */
	pos = strcspn(combined, ":");
	len = strlen(combined);

	/* Reject silly strings. */
	if (pos == len - 1) {
		warn0("Empty group name: %s", combined);
		goto err0;
	}

	/* String ok, proceed. */
	if (pos == 0) {
		/* Groupname only; duplicate string. */
		if ((*groupname_p = strdup(&combined[1])) == NULL) {
			warnp("strdup()");
			goto err0;
		}
		*username_p = NULL;
	} else if (pos != len) {
		/* Extract username. */
		if ((*username_p = malloc(pos + 1)) == NULL) {
			warnp("Failed to allocate memory");
			goto err0;
		}
		memcpy(*username_p, combined, pos);
		(*username_p)[pos] = '\0';

		/* Extract groupname. */
		if ((*groupname_p = strdup(&combined[pos + 1])) == NULL) {
			warnp("strdup()");
			goto err1;
		}
	} else {
		/* Duplicate string. */
		if ((*username_p = strdup(combined)) == NULL) {
			warnp("strdup()");
			goto err0;
		}
		*groupname_p = NULL;
	}

	/* Success! */
	return (0);

err1:
	free(*username_p);
err0:
	/* Failure! */
	return (-1);
}

/**
 * setuidgid(user_group_string, leave_suppgrp):
 * Set the UID and/or GID to the names given in ${user_group_string}.  If no
 * UID or GID can be found matching those strings, treat the values as numeric
 * IDs.  Depending on the existence and position of a colon ":", the behaviour
 * is
 * - no ":" means that the string is a username.
 * - ":" in the first position means that the string is a groupname.
 * - otherwise, the string is parsed into "username:groupname".
 *
 * The behaviour with supplementary groups depends on ${leave_suppgrp}:
 * - SETUIDGID_SGROUP_IGNORE: do not attempt to leave supplementary groups.
 * - SETUIDGID_SGROUP_LEAVE_WARN: attempt to leave; if it fails, give a
 *   warning but continue.
 * - SETUIDGID_SGROUP_LEAVE_ERROR: attempt to leave; if it fails, return
 *   an error.
 */
int
setuidgid(const char * user_group_string, int leave_suppgrp)
{
	char * username;
	char * groupname;
	int saved_errno;

	/* Get the username:groupname from ${user_group_string}. */
	if (string_extract_user_group(user_group_string, &username, &groupname))
		goto err0;

	/* If requested, leave supplementary groups. */
	if (leave_suppgrp != SETUIDGID_SGROUP_IGNORE) {
		/* Attempt to leave all supplementary groups. */
		if (setgroups_none()) {
			if (leave_suppgrp == SETUIDGID_SGROUP_LEAVE_ERROR)
				goto err1;
		}

		/* Check that we have actually left all supplementary groups. */
		if (check_supplementary_groups_none()) {
			if (leave_suppgrp == SETUIDGID_SGROUP_LEAVE_ERROR) {
				warn0("Failed to leave supplementary groups");
				goto err1;
			} else {
				warn0("Warning: Failed to leave supplementary "
				    "groups");
			}
		}
	}

	/* Set group; must be changed before user. */
	if (groupname && set_group(groupname))
		goto err1;

	/* Set user. */
	if (username && set_user(username))
		goto err1;

	/* Clean up. */
	free(groupname);
	free(username);

	/* Success! */
	return (0);

err1:
	/* POSIX Issue 7 does not forbid free() from modifying errno. */
	saved_errno = errno;

	/* Clean up. */
	free(groupname);
	free(username);

	/* Restore errno. */
	errno = saved_errno;
err0:
	/* Failure! */
	return (-1);
}
