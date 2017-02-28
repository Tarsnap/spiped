#ifndef SIMPLE_SERVER_H
#define SIMPLE_SERVER_H

#include <netinet/in.h>

#include <stddef.h>
#include <stdint.h>

/**
 * simple_server(port, nconn_max, shutdown_after, callback, caller_cookie):
 * Run a server which accepts up to ${nconn_max} connections to port ${port}.
 * After receiving a message, call ${callback} and pass it the
 * ${caller_cookie}, along with the message.  Automatically shuts itself down
 * after ${shutdown_after} connections have been dropped.
 */
int simple_server(in_port_t, size_t, size_t, int (*)(void *, uint8_t *, size_t),
    void *);

#endif
