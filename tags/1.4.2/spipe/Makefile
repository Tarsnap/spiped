# Program name.
PROG	=	spipe
MAN1	=	${PROG}.1

# Libraries which are sometimes merged into libc
LDADD	=	-lrt
#LDADD	+=	-lxnet	# Missing on FreeBSD

# Library code required
LDADD_REQ	=	-lcrypto
LDADD_REQ	+=	-lpthread

# spipe code
SRCS	=	main.c
SRCS	+=	pushbits.c

# spipe protocol
.PATH.c	:	../proto
SRCS	+=	proto_conn.c
SRCS	+=	proto_crypt.c
SRCS	+=	proto_handshake.c
SRCS	+=	proto_pipe.c
IDIRS	+=	-I ../proto

# Fundamental algorithms
.PATH.c	:	../libcperciva/alg
SRCS	+=	sha256.c
IDIRS	+=	-I ../libcperciva/alg

# Data structures
.PATH.c	:	../libcperciva/datastruct
SRCS	+=	elasticarray.c
SRCS	+=	ptrheap.c
SRCS	+=	timerqueue.c
IDIRS	+=	-I ../libcperciva/datastruct

# Utility functions
.PATH.c	:	../libcperciva/util
SRCS	+=	asprintf.c
SRCS	+=	entropy.c
SRCS	+=	insecure_memzero.c
SRCS	+=	monoclock.c
SRCS	+=	noeintr.c
SRCS	+=	sock.c
SRCS	+=	warnp.c
IDIRS	+=	-I ../libcperciva/util

# CPU features detection
.PATH.c	:	../libcperciva/cpusupport
SRCS	+=	cpusupport_x86_aesni.c
IDIRS	+=	-I ../libcperciva/cpusupport

# Event loop
.PATH.c	:	../libcperciva/events
SRCS	+=	events_immediate.c
SRCS	+=	events_network.c
SRCS	+=	events_network_selectstats.c
SRCS	+=	events_timer.c
SRCS	+=	events.c
IDIRS	+=	-I ../libcperciva/events

# Event-driven networking
.PATH.c	:	../libcperciva/network
SRCS	+=	network_connect.c
SRCS	+=	network_read.c
SRCS	+=	network_write.c
IDIRS	+=	-I ../libcperciva/network

# Crypto code
.PATH.c	:	../libcperciva/crypto
SRCS	+=	crypto_aes.c
SRCS	+=	crypto_aes_aesni.c
SRCS	+=	crypto_aesctr.c
SRCS	+=	crypto_dh.c
SRCS	+=	crypto_dh_group14.c
SRCS	+=	crypto_entropy.c
SRCS	+=	crypto_verify_bytes.c
IDIRS	+=	-I ../libcperciva/crypto

crypto_aes_aesni.o: crypto_aes_aesni.c cpusupport-config.h
	. ./cpusupport-config.h; ${CC} ${CFLAGS} $${CFLAGS_X86_AESNI} -c $< -o $@
cflags-crypto_aes_aesni.o=$${CFLAGS_X86_AESNI}

# Debugging options
CFLAGS	+=	-g
#CFLAGS	+=	-DNDEBUG
#CFLAGS	+=	-DDEBUG
#CFLAGS	+=	-pg

.include <bsd.prog.mk>
