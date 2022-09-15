#ifndef SETGROUPS_NONE_H_
#define SETGROUPS_NONE_H_

/**
 * setgroups_none(void):
 * Attempt to leave all supplementary groups.  If we do not know how to do this
 * on the platform, return 0 anyway.
 */
int setgroups_none(void);

#endif /* !SETGROUPS_NONE_H_ */
