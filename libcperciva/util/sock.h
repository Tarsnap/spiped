#ifndef SOCK_H_
#define SOCK_H_

/**
 * Address strings are of the following forms:
 * /path/to/unix/socket
 * [ip.v4.ad.dr]:port
 * [ipv6:add::ress]:port
 * host.name:port
 */

/* Opaque address structure. */
struct sock_addr;

/**
 * sock_resolve(addr):
 * Return a NULL-terminated array of pointers to sock_addr structures.
 */
struct sock_addr ** sock_resolve(const char *);

/**
 * sock_resolve_one(addr, addport):
 * Return a single sock_addr structure, or NULL if there are no addresses.
 * Warn if there is more than one address, and return the first one.
 * If ${addport} is non-zero, use sock_addr_ensure_port() to add a port number
 * of ":0" if appropriate.
 */
struct sock_addr * sock_resolve_one(const char *, int);

/**
 * sock_listener(sa):
 * Create a socket, attempt to set SO_REUSEADDR, bind it to the socket address
 * ${sa}, mark it for listening, and mark it as non-blocking.
 */
int sock_listener(const struct sock_addr *);

/**
 * sock_connect(sas):
 * Iterate through the addresses in ${sas}, attempting to create a socket and
 * connect (blockingly).  Once connected, stop iterating, mark the socket as
 * non-blocking, and return it.
 */
int sock_connect(struct sock_addr * const *);

/**
 * sock_connect_nb(sa):
 * Create a socket, mark it as non-blocking, and attempt to connect to the
 * address ${sa}.  Return the socket (connected or in the process of
 * connecting) or -1 on error.
 */
int sock_connect_nb(const struct sock_addr *);

/**
 * sock_connect_bind_nb(sa, sa_b):
 * Create a socket, mark it as non-blocking, and attempt to connect to the
 * address ${sa}.  If ${sa_b} is not NULL, attempt to set SO_REUSEADDR on the
 * socket and bind it to ${sa_b} immediately after creating it.  Return the
 * socket (connected or in the process of connecting) or -1 on error.
 */
int sock_connect_bind_nb(const struct sock_addr *, const struct sock_addr *);

/**
 * sock_addr_free(sa):
 * Free the provided sock_addr structure.
 */
void sock_addr_free(struct sock_addr *);

/**
 * sock_addr_freelist(sas):
 * Free the provided NULL-terminated array of sock_addr structures.
 */
void sock_addr_freelist(struct sock_addr **);

#endif /* !SOCK_H_ */
