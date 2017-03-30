#ifndef _DAEMONIZE_H_
#define _DAEMONIZE_H_

/**
 * daemonize(spid):
 * Daemonize and write the process ID in decimal to a file named ${spid}.
 * The parent process will exit only after the child has written its pid.
 * On success, the child will return 0; on failure, the parent will return
 * -1.
 */
int daemonize(const char *);

#endif /* !_DAEMONIZE_H_ */
