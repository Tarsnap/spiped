#ifndef _SETGROUPS_NONE_H_
#define _SETGROUPS_NONE_H_

/**
 * setgroups_none(void):
 * Attempt to leave all supplementary groups.  If we do not know how to do this
 * on the platform, return 0 anyway.
 */
int setgroups_none(void);

#endif /* !_SETUPGROUPS_NONE_H_ */
