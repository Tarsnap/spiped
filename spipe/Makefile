.POSIX:
# AUTOGENERATED FILE, DO NOT EDIT
PROG=spipe
MAN1=spipe.1
SRCS=main.c pushbits.c
IDIRS=-I../libcperciva/crypto -I../libcperciva/events -I../libcperciva/util -I../lib/proto -I../lib/util
LDADD_REQ=-lcrypto -lpthread
SUBDIR_DEPTH=..
RELATIVE_DIR=spipe
LIBALL=../liball/liball.a ../liball/optional_mutex_pthread/liball_optional_mutex_pthread.a

all:
	if [ -z "$${HAVE_BUILD_FLAGS}" ]; then \
		cd ${SUBDIR_DEPTH}; \
		${MAKE} BUILD_SUBDIR=${RELATIVE_DIR} \
		    BUILD_TARGET=${PROG} buildsubdir; \
	else \
		${MAKE} ${PROG}; \
	fi

install:${PROG}
	mkdir -p ${BINDIR}
	cp ${PROG} ${BINDIR}/_inst.${PROG}.$$$$_ &&	\
	    strip ${BINDIR}/_inst.${PROG}.$$$$_ &&	\
	    chmod 0555 ${BINDIR}/_inst.${PROG}.$$$$_ && \
	    mv -f ${BINDIR}/_inst.${PROG}.$$$$_ ${BINDIR}/${PROG}
	if ! [ -z "${MAN1DIR}" ]; then			\
		mkdir -p ${MAN1DIR};			\
		for MPAGE in ${MAN1}; do						\
			cp $$MPAGE ${MAN1DIR}/_inst.$$MPAGE.$$$$_ &&			\
			    chmod 0444 ${MAN1DIR}/_inst.$$MPAGE.$$$$_ &&		\
			    mv -f ${MAN1DIR}/_inst.$$MPAGE.$$$$_ ${MAN1DIR}/$$MPAGE;	\
		done;									\
	fi

clean:
	rm -f ${PROG} ${SRCS:.c=.o}

${PROG}:${SRCS:.c=.o} ${LIBALL}
	${CC} -o ${PROG} ${SRCS:.c=.o} ${LIBALL} ${LDFLAGS} ${LDADD_EXTRA} ${LDADD_REQ} ${LDADD_POSIX}

main.o: main.c ../libcperciva/events/events.h ../libcperciva/util/getopt.h ../lib/util/graceful_shutdown.h ../libcperciva/util/parsenum.h ../libcperciva/util/sock.h ../libcperciva/util/sock_util.h ../libcperciva/util/warnp.h ../lib/proto/proto_conn.h ../lib/proto/proto_crypt.h ../libcperciva/crypto/crypto_dh.h pushbits.h
	${CC} ${CFLAGS_POSIX} -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -DCPUSUPPORT_CONFIG_FILE=\"cpusupport-config.h\" -DAPISUPPORT_CONFIG_FILE=\"apisupport-config.h\" -I.. ${IDIRS} ${CPPFLAGS} ${CFLAGS} -c main.c -o main.o
pushbits.o: pushbits.c ../libcperciva/util/noeintr.h ../lib/util/pthread_create_blocking_np.h ../libcperciva/util/warnp.h pushbits.h
	${CC} ${CFLAGS_POSIX} -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -DCPUSUPPORT_CONFIG_FILE=\"cpusupport-config.h\" -DAPISUPPORT_CONFIG_FILE=\"apisupport-config.h\" -I.. ${IDIRS} ${CPPFLAGS} ${CFLAGS} -c pushbits.c -o pushbits.o
