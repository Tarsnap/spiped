#ifndef _SETUIDGID_H_
#define _SETUIDGID_H_

/* How should we treat supplementary groups? */
enum {
	SETUIDGID_SGROUP_IGNORE = 0,
	SETUIDGID_SGROUP_LEAVE_WARN,
	SETUIDGID_SGROUP_LEAVE_ERROR
};

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
int setuidgid(const char *, int);

#endif /* !_SETUIDGID_H_ */
