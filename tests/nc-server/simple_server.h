#ifndef SIMPLE_SERVER_H
#define SIMPLE_SERVER_H

#include <stddef.h>
#include <stdint.h>

/**
 * simple_server(sockname, nconn_max, shutdown_after, callback, caller_cookie):
 * Run a server which accepts up to ${nconn_max} connections to socket
 * ${sockname}.  After receiving a message, call ${callback} and pass it the
 * ${caller_cookie}, along with the message.  Automatically shuts itself down
 * after ${shutdown_after} connections have been dropped.
 */
int simple_server(const char *, size_t, size_t, int (*)(void *, uint8_t *,
    size_t), void *);

#endif
