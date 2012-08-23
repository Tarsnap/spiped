# Program name.
PROG	=	spiped

# Library code required
LDADD	=	-lrt
LDADD	+=	-lcrypto

# spiped code
SRCS	=	main.c
SRCS	+=	conn.c

# spipe protocol
.PATH.c	:	proto
SRCS	+=	pcrypt.c
SRCS	+=	proto_handshake.c
SRCS	+=	proto_pipe.c
IDIRS	+=	-I proto

# Data structures
.PATH.c	:	lib/datastruct
SRCS	+=	elasticarray.c
SRCS	+=	ptrheap.c
SRCS	+=	timerqueue.c
IDIRS	+=	-I lib/datastruct

# Utility functions
.PATH.c	:	lib/util
SRCS	+=	asprintf.c
SRCS	+=	daemonize.c
SRCS	+=	entropy.c
SRCS	+=	monoclock.c
SRCS	+=	sock.c
SRCS	+=	warnp.c
IDIRS	+=	-I lib/util

# Event loop
.PATH.c	:	lib/events
SRCS	+=	events_immediate.c
SRCS	+=	events_network.c
SRCS	+=	events_timer.c
SRCS	+=	events.c
IDIRS	+=	-I lib/events

# Event-driven networking
.PATH.c	:	lib/network
SRCS	+=	network_accept.c
SRCS	+=	network_buf.c
SRCS	+=	network_connect.c
IDIRS	+=	-I lib/network

# Crypto code
.PATH.c	:	lib/crypto
SRCS	+=	crypto_aesctr.c
SRCS	+=	crypto_dh.c
SRCS	+=	crypto_dh_group14.c
SRCS	+=	crypto_entropy.c
SRCS	+=	crypto_verify_bytes.c
SRCS	+=	sha256.c
IDIRS	+=	-I lib/crypto

# Debugging options
CFLAGS	+=	-g
#CFLAGS	+=	-DNDEBUG
#CFLAGS	+=	-DDEBUG
#CFLAGS	+=	-pg

publish: clean
	if [ -z "${VERSION}" ]; then			\
		echo "VERSION must be specified!";	\
		exit 1;					\
	fi
	if find . | grep \~; then					\
		echo "Delete temporary files before publishing!";	\
		exit 1;							\
	fi
	rm -f ${PROG}-${VERSION}.tgz
	mkdir ${PROG}-${VERSION}
	tar -cf- --exclude 'Makefile.*' --exclude Makefile		\
	    --exclude ${PROG}-${VERSION} . | 				\
	    tar -xf- -C ${PROG}-${VERSION}
	echo '.POSIX:' > ${PROG}-${VERSION}/Makefile
	( echo -n 'PROG=' && make -V PROG ) >> ${PROG}-${VERSION}/Makefile
	( echo -n 'SRCS=' && make -V SRCS ) >> ${PROG}-${VERSION}/Makefile
	( echo -n 'IDIRS=' && make -V IDIRS ) >> ${PROG}-${VERSION}/Makefile
	( echo -n 'LDADD=' && make -V LDADD ) >> ${PROG}-${VERSION}/Makefile
	cat Makefile.prog >> ${PROG}-${VERSION}/Makefile
	( make -V SRCS |	\
	    tr ' ' '\n' |		\
	    sed -E 's/.c$$/.o/' |	\
	    while read F; do		\
		echo -n "$${F}: ";	\
		make source-$${F};	\
	    done ) >> ${PROG}-${VERSION}/Makefile
	tar -cvzf ${PROG}-${VERSION}.tgz ${PROG}-${VERSION}
	rm -r ${PROG}-${VERSION}

.include <Makefile.inc>
.include <bsd.prog.mk>
