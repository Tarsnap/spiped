#ifndef _DAEMONIZE_H_
#define _DAEMONIZE_H_

/**
 * daemonize(spid):
 * Daemonize and write the process ID in decimal to a file named ${spid}.
 */
int daemonize(const char *);

#endif /* !_DAEMONIZE_H_ */
