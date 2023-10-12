/*
 * Including a .c file is unusual, but setgroups_none.c contains a number of
 * system-specific header includes, which we do not want to duplicate here.
 */
#include "../../util/setgroups_none.c"

int
main(void)
{

	(void)setgroups(0, NULL);

	/* Success! */
	return (0);
}
