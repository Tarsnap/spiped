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
#include <grp.h>
#define HAS_SETGROUPS 1

#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__APPLE__)
/* setgroups() includes for FreeBSD, NetBSD, MacOS X. */
#include <sys/param.h>
#include <unistd.h>
#define HAS_SETGROUPS 1

#elif defined(__OpenBSD__) || defined(__osf__)
/* setgroups() includes for OpenBSD. */
#include <sys/types.h>
#include <unistd.h>
#define HAS_SETGROUPS 1

#elif defined(__sun) || defined(__hpux)
/* setgroups() includes for Solaris. */
#include <unistd.h>
#define HAS_SETGROUPS 1

#elif defined(_AIX)
/* setgroups() includes for AIX. */
#include <grp.h>
#define HAS_SETGROUPS 1

#else
/* Unknown OS; we don't know how to get setgroups(). */

#endif /* end includes for setgroups() */

#include <assert.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
 * setgroups_none():
 * Leave all supplementary groups.  If we do not know how to do this on the
 * platform, return 0 anyway.
 */
static int
setgroups_none()
{

#ifdef HAS_SETGROUPS
	if (setgroups(0, NULL))
		goto err0;
#endif

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}

/* Check if we're in any supplementary groups. */
static int
check_supplementary_groups_none()
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
		if ((ngroups = getgroups(1, grouplist)) < 0) {
			warnp("getgroups()");
			goto err0;
		}
		if (grouplist[0] != getegid())
			goto err0;
	}

	/* Success! */
	return (0);

err0:
	/* Failure! */
	return (-1);
}

/**
 * set_group(groupname):
 * Set the effective GID to the group represented by ${groupname}.
 */
static int
set_group(const char * groupname)
{
	gid_t gid;
	struct group * group_info;

	/* Find numerical GID. */
	errno = 0;
	if ((group_info = getgrnam(groupname)) == NULL) {
		warnp("getgrnam(%s)", groupname);
		goto err0;
	}
	gid = group_info->gr_gid;

	/* Set GID. */
	if (setgid(gid) != 0) {
		if (errno == EPERM)
			goto nopermission;
		warnp("Failed to set GID");
		goto err0;
	}

	/* Success! */
	return (0);

nopermission:
	errno = EPERM;

	/* Failure! */
	return (-1);

err0:
	/* Failure! */
	return (-1);
}

/**
 * set_user(username):
 * Set the effective UID to the user represented by ${username}.
 */
static int
set_user(const char * username)
{
	uid_t uid;
	struct passwd * user_info;

	/* Calculate numerical UID. */
	errno = 0;
	if ((user_info = getpwnam(username)) == NULL) {
		warn0("getpwnam(%s)", username);
		goto err0;
	}
	uid = user_info->pw_uid;

	/* Set UID. */
	if (setuid(uid) != 0) {
		if (errno == EPERM)
			goto nopermission;
		warnp("Failed to set UID");
		goto err0;
	}

	/* Success! */
	return (0);

nopermission:
	errno = EPERM;

	/* Failure! */
	return (-1);

err0:
	/* Failure! */
	return (-1);
}

/* Parses "username" or "username:groupgroup". */
static int
string_extract_user_group(const char * combined, char ** username_p,
    char ** groupname_p)
{
	size_t pos;
	size_t len;

	/* Sanity check. */
	assert(combined != NULL);

	/* Search for ":". */
	pos = strcspn(combined, ":");

	/* Extract strings, or duplicate string. */
	len = strlen(combined);

	/* Reject silly strings. */
	if (pos == 0) {
		warn0("Username is blank.");
		goto err0;
	}
	if (pos == len - 1) {
		warn0("Groupname is blank.");
		goto err0;
	}

	/* String ok, proceed. */
	if (pos != len) {
		/* Extract username. */
		if ((*username_p = malloc((pos + 1) * sizeof(char))) == NULL) {
			warnp("Failed to allocate memory");
			goto err0;
		}
		strncpy(*username_p, combined, pos);
		(*username_p)[pos] = '\0';

		/* Extract gropuname. */
		if ((*groupname_p = malloc((len-pos + 1) * sizeof(char)))
		    == NULL) {
			warnp("Failed to allocate memory");
			goto err1;
		}
		strncpy(*groupname_p, combined+pos+1, len-pos);
		(*groupname_p)[len-pos] = '\0';
	} else {
		/* Duplicate string. */
		if ((*username_p = strdup(combined)) == NULL)
			goto err0;
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
 * setuidgid(user_group_string, leave_supplementary):
 * Set the UID and optionally GID to the names given in ${user_group_string}.
 * If there is a ":" in the string, it is parsed into "username:groupname";
 * otherwise, the entire string is interpreted as a username.  The behaviour
 * with supplementary groups depends on ${leave_supplementary}:
 * - SETUIDGID_SGROUP_IGNORE: do not attempt to leave supplementary groups.
 * - SETUIDGID_SGROUP_LEAVE_WARN: attempt to leave; if it fails, give a
 *   warning but continue.
 * - SETUIDGID_SGROUP_LEAVE_ERROR: attempt to leave; if it fails, return
 *   an error.
 */
int
setuidgid(const char * opt_usergroup, int leave_supplementary)
{
	int setgroups_errno = 0;
	char * username;
	char * groupname;

	/* Get the username:groupname from the opt_user string. */
	if (string_extract_user_group(opt_usergroup, &username, &groupname)
	    != 0)
		goto err0;

	/* If requested, leave supplementary groups. */
	if (leave_supplementary != SETUIDGID_SGROUP_IGNORE) {
		/* Leave any supplementary groups. */
		if (setgroups_none() != 0) {
			setgroups_errno = errno;
			warnp("setgroups()");
			/*
			 * Do this regardless of whether the user specified
			 * SETUIDGID_SGROUP_LEAVE_ERROR or not, because if
			 * setgroups() bails with EPERM then so will setgid()
			 * and setuid().
			 */
			if (setgroups_errno == EPERM)
				goto nopermission;
			if (leave_supplementary == SETUIDGID_SGROUP_LEAVE_ERROR)
				goto err1;
		}

		/* Ensure that we have actually left the supplentary groups. */
		if (check_supplementary_groups_none() != 0) {
			warn0("Failed to leave supplementary groups");
			if (leave_supplementary == SETUIDGID_SGROUP_LEAVE_ERROR)
				goto err1;
		}
	}

	/* Set group; must be changed before user. */
	if (groupname != NULL) {
		if (set_group(groupname) != 0) {
			if (errno == EPERM)
				goto nopermission;
			warn0("Error changing group");
			goto err1;
		}
	}

	/* Set user. */
	if (username != NULL) {
		if (set_user(username) != 0) {
			if (errno == EPERM)
				goto nopermission;
			warn0("Error changing user");
			goto err1;
		}
	}

	/* Clean up. */
	free(groupname);
	free(username);

	/* Success! */
	return (0);

nopermission:
	free(groupname);
	free(username);

	errno = EPERM;

	/* Failure! */
	return (-1);

err1:
	free(groupname);
	free(username);
err0:
	/* Failure! */
	return (-1);
}
