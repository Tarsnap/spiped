#ifndef SETUIDGID_H
#define SETUIDGID_H

enum {
	SETUIDGID_SGROUP_IGNORE = 0,
	SETUIDGID_SGROUP_LEAVE_WARN,
	SETUIDGID_SGROUP_LEAVE_ERROR
};

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
int setuidgid(const char *, int leave_supplementary);

#endif
