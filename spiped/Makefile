# Program name.
PROG	=	spiped
MAN1	=	${PROG}.1

# Libraries which are sometimes merged into libc
LDADD	=	-lrt
#LDADD	+=	-lxnet	# Missing on FreeBSD

# Library code required
LDADD_REQ	=	-lcrypto

# spiped code
SRCS	=	main.c
SRCS	+=	dispatch.c

# spipe protocol
.PATH.c	:	../proto
SRCS	+=	proto_conn.c
SRCS	+=	proto_crypt.c
SRCS	+=	proto_handshake.c
SRCS	+=	proto_pipe.c
IDIRS	+=	-I ../proto

# Fundamental algorithms
.PATH.c	:	../lib/alg
SRCS	+=	sha256.c
IDIRS	+=	-I ../lib/alg

# Data structures
.PATH.c	:	../lib/datastruct
SRCS	+=	elasticarray.c
SRCS	+=	ptrheap.c
SRCS	+=	timerqueue.c
IDIRS	+=	-I ../lib/datastruct

# DNS resolution
.PATH.c	:	../lib/dnsthread
SRCS	+=	dnsthread.c
IDIRS	+=	-I ../lib/dnsthread

# Utility functions
.PATH.c	:	../lib/util
SRCS	+=	asprintf.c
SRCS	+=	daemonize.c
SRCS	+=	entropy.c
SRCS	+=	monoclock.c
SRCS	+=	noeintr.c
SRCS	+=	sock.c
SRCS	+=	sock_util.c
SRCS	+=	warnp.c
IDIRS	+=	-I ../lib/util

# Event loop
.PATH.c	:	../lib/events
SRCS	+=	events_immediate.c
SRCS	+=	events_network.c
SRCS	+=	events_timer.c
SRCS	+=	events.c
IDIRS	+=	-I ../lib/events

# Event-driven networking
.PATH.c	:	../lib/network
SRCS	+=	network_accept.c
SRCS	+=	network_buf.c
SRCS	+=	network_connect.c
IDIRS	+=	-I ../lib/network

# Crypto code
.PATH.c	:	../lib/crypto
SRCS	+=	crypto_aesctr.c
SRCS	+=	crypto_dh.c
SRCS	+=	crypto_dh_group14.c
SRCS	+=	crypto_entropy.c
SRCS	+=	crypto_verify_bytes.c
IDIRS	+=	-I ../lib/crypto

# Debugging options
CFLAGS	+=	-g
#CFLAGS	+=	-DNDEBUG
#CFLAGS	+=	-DDEBUG
#CFLAGS	+=	-pg

.include <bsd.prog.mk>
