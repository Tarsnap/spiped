.POSIX:
PROG=spipe
MAN1=spipe.1
SRCS=main.c pushbits.c proto_conn.c proto_crypt.c proto_handshake.c proto_pipe.c sha256.c elasticarray.c ptrheap.c timerqueue.c asprintf.c entropy.c insecure_memzero.c monoclock.c noeintr.c sock.c warnp.c cpusupport_x86_aesni.c events_immediate.c events_network.c events_network_selectstats.c events_timer.c events.c network_connect.c network_read.c network_write.c crypto_aes.c crypto_aes_aesni.c crypto_aesctr.c crypto_dh.c crypto_dh_group14.c crypto_entropy.c crypto_verify_bytes.c 
IDIRS=-I ../proto -I ../libcperciva/alg -I ../libcperciva/datastruct -I ../libcperciva/util -I ../libcperciva/cpusupport -I ../libcperciva/events -I ../libcperciva/network -I ../libcperciva/crypto
LDADD_REQ=-lcrypto -lpthread

all:	${PROG}

install:${PROG}
	mkdir -p ${BINDIR}
	cp ${PROG} ${BINDIR}/_inst.${PROG}.$$$$_ &&	\
	    strip ${BINDIR}/_inst.${PROG}.$$$$_ &&	\
	    chmod 0555 ${BINDIR}/_inst.${PROG}.$$$$_ && \
	    mv -f ${BINDIR}/_inst.${PROG}.$$$$_ ${BINDIR}/${PROG}
	if ! [ -z "${MAN1DIR}" ]; then			\
		for MPAGE in ${MAN1}; do						\
			cp $$MPAGE ${MAN1DIR}/_inst.$$MPAGE.$$$$_ &&			\
			    chmod 0444 ${MAN1DIR}/_inst.$$MPAGE.$$$$_ &&		\
			    mv -f ${MAN1DIR}/_inst.$$MPAGE.$$$$_ ${MAN1DIR}/$$MPAGE;	\
		done;									\
	fi

clean:
	rm -f ${PROG} ${SRCS:.c=.o}

${PROG}:${SRCS:.c=.o}
	${CC} -o ${PROG} ${SRCS:.c=.o} ${LDADD_EXTRA} ${LDADD_REQ} ${LDADD_POSIX}

main.o: main.c
	${CC} ${CFLAGS} ${CFLAGS_POSIX} -D_POSIX_C_SOURCE=200809L -DCPUSUPPORT_CONFIG_FILE=\"cpusupport-config.h\"  -I .. ${IDIRS} -c main.c -o main.o
pushbits.o: pushbits.c
	${CC} ${CFLAGS} ${CFLAGS_POSIX} -D_POSIX_C_SOURCE=200809L -DCPUSUPPORT_CONFIG_FILE=\"cpusupport-config.h\"  -I .. ${IDIRS} -c pushbits.c -o pushbits.o
proto_conn.o: ../proto/proto_conn.c
	${CC} ${CFLAGS} ${CFLAGS_POSIX} -D_POSIX_C_SOURCE=200809L -DCPUSUPPORT_CONFIG_FILE=\"cpusupport-config.h\"  -I .. ${IDIRS} -c ../proto/proto_conn.c -o proto_conn.o
proto_crypt.o: ../proto/proto_crypt.c
	${CC} ${CFLAGS} ${CFLAGS_POSIX} -D_POSIX_C_SOURCE=200809L -DCPUSUPPORT_CONFIG_FILE=\"cpusupport-config.h\"  -I .. ${IDIRS} -c ../proto/proto_crypt.c -o proto_crypt.o
proto_handshake.o: ../proto/proto_handshake.c
	${CC} ${CFLAGS} ${CFLAGS_POSIX} -D_POSIX_C_SOURCE=200809L -DCPUSUPPORT_CONFIG_FILE=\"cpusupport-config.h\"  -I .. ${IDIRS} -c ../proto/proto_handshake.c -o proto_handshake.o
proto_pipe.o: ../proto/proto_pipe.c
	${CC} ${CFLAGS} ${CFLAGS_POSIX} -D_POSIX_C_SOURCE=200809L -DCPUSUPPORT_CONFIG_FILE=\"cpusupport-config.h\"  -I .. ${IDIRS} -c ../proto/proto_pipe.c -o proto_pipe.o
sha256.o: ../libcperciva/alg/sha256.c
	${CC} ${CFLAGS} ${CFLAGS_POSIX} -D_POSIX_C_SOURCE=200809L -DCPUSUPPORT_CONFIG_FILE=\"cpusupport-config.h\"  -I .. ${IDIRS} -c ../libcperciva/alg/sha256.c -o sha256.o
elasticarray.o: ../libcperciva/datastruct/elasticarray.c
	${CC} ${CFLAGS} ${CFLAGS_POSIX} -D_POSIX_C_SOURCE=200809L -DCPUSUPPORT_CONFIG_FILE=\"cpusupport-config.h\"  -I .. ${IDIRS} -c ../libcperciva/datastruct/elasticarray.c -o elasticarray.o
ptrheap.o: ../libcperciva/datastruct/ptrheap.c
	${CC} ${CFLAGS} ${CFLAGS_POSIX} -D_POSIX_C_SOURCE=200809L -DCPUSUPPORT_CONFIG_FILE=\"cpusupport-config.h\"  -I .. ${IDIRS} -c ../libcperciva/datastruct/ptrheap.c -o ptrheap.o
timerqueue.o: ../libcperciva/datastruct/timerqueue.c
	${CC} ${CFLAGS} ${CFLAGS_POSIX} -D_POSIX_C_SOURCE=200809L -DCPUSUPPORT_CONFIG_FILE=\"cpusupport-config.h\"  -I .. ${IDIRS} -c ../libcperciva/datastruct/timerqueue.c -o timerqueue.o
asprintf.o: ../libcperciva/util/asprintf.c
	${CC} ${CFLAGS} ${CFLAGS_POSIX} -D_POSIX_C_SOURCE=200809L -DCPUSUPPORT_CONFIG_FILE=\"cpusupport-config.h\"  -I .. ${IDIRS} -c ../libcperciva/util/asprintf.c -o asprintf.o
entropy.o: ../libcperciva/util/entropy.c
	${CC} ${CFLAGS} ${CFLAGS_POSIX} -D_POSIX_C_SOURCE=200809L -DCPUSUPPORT_CONFIG_FILE=\"cpusupport-config.h\"  -I .. ${IDIRS} -c ../libcperciva/util/entropy.c -o entropy.o
insecure_memzero.o: ../libcperciva/util/insecure_memzero.c
	${CC} ${CFLAGS} ${CFLAGS_POSIX} -D_POSIX_C_SOURCE=200809L -DCPUSUPPORT_CONFIG_FILE=\"cpusupport-config.h\"  -I .. ${IDIRS} -c ../libcperciva/util/insecure_memzero.c -o insecure_memzero.o
monoclock.o: ../libcperciva/util/monoclock.c
	${CC} ${CFLAGS} ${CFLAGS_POSIX} -D_POSIX_C_SOURCE=200809L -DCPUSUPPORT_CONFIG_FILE=\"cpusupport-config.h\"  -I .. ${IDIRS} -c ../libcperciva/util/monoclock.c -o monoclock.o
noeintr.o: ../libcperciva/util/noeintr.c
	${CC} ${CFLAGS} ${CFLAGS_POSIX} -D_POSIX_C_SOURCE=200809L -DCPUSUPPORT_CONFIG_FILE=\"cpusupport-config.h\"  -I .. ${IDIRS} -c ../libcperciva/util/noeintr.c -o noeintr.o
sock.o: ../libcperciva/util/sock.c
	${CC} ${CFLAGS} ${CFLAGS_POSIX} -D_POSIX_C_SOURCE=200809L -DCPUSUPPORT_CONFIG_FILE=\"cpusupport-config.h\"  -I .. ${IDIRS} -c ../libcperciva/util/sock.c -o sock.o
warnp.o: ../libcperciva/util/warnp.c
	${CC} ${CFLAGS} ${CFLAGS_POSIX} -D_POSIX_C_SOURCE=200809L -DCPUSUPPORT_CONFIG_FILE=\"cpusupport-config.h\"  -I .. ${IDIRS} -c ../libcperciva/util/warnp.c -o warnp.o
cpusupport_x86_aesni.o: ../libcperciva/cpusupport/cpusupport_x86_aesni.c
	${CC} ${CFLAGS} ${CFLAGS_POSIX} -D_POSIX_C_SOURCE=200809L -DCPUSUPPORT_CONFIG_FILE=\"cpusupport-config.h\"  -I .. ${IDIRS} -c ../libcperciva/cpusupport/cpusupport_x86_aesni.c -o cpusupport_x86_aesni.o
events_immediate.o: ../libcperciva/events/events_immediate.c
	${CC} ${CFLAGS} ${CFLAGS_POSIX} -D_POSIX_C_SOURCE=200809L -DCPUSUPPORT_CONFIG_FILE=\"cpusupport-config.h\"  -I .. ${IDIRS} -c ../libcperciva/events/events_immediate.c -o events_immediate.o
events_network.o: ../libcperciva/events/events_network.c
	${CC} ${CFLAGS} ${CFLAGS_POSIX} -D_POSIX_C_SOURCE=200809L -DCPUSUPPORT_CONFIG_FILE=\"cpusupport-config.h\"  -I .. ${IDIRS} -c ../libcperciva/events/events_network.c -o events_network.o
events_network_selectstats.o: ../libcperciva/events/events_network_selectstats.c
	${CC} ${CFLAGS} ${CFLAGS_POSIX} -D_POSIX_C_SOURCE=200809L -DCPUSUPPORT_CONFIG_FILE=\"cpusupport-config.h\"  -I .. ${IDIRS} -c ../libcperciva/events/events_network_selectstats.c -o events_network_selectstats.o
events_timer.o: ../libcperciva/events/events_timer.c
	${CC} ${CFLAGS} ${CFLAGS_POSIX} -D_POSIX_C_SOURCE=200809L -DCPUSUPPORT_CONFIG_FILE=\"cpusupport-config.h\"  -I .. ${IDIRS} -c ../libcperciva/events/events_timer.c -o events_timer.o
events.o: ../libcperciva/events/events.c
	${CC} ${CFLAGS} ${CFLAGS_POSIX} -D_POSIX_C_SOURCE=200809L -DCPUSUPPORT_CONFIG_FILE=\"cpusupport-config.h\"  -I .. ${IDIRS} -c ../libcperciva/events/events.c -o events.o
network_connect.o: ../libcperciva/network/network_connect.c
	${CC} ${CFLAGS} ${CFLAGS_POSIX} -D_POSIX_C_SOURCE=200809L -DCPUSUPPORT_CONFIG_FILE=\"cpusupport-config.h\"  -I .. ${IDIRS} -c ../libcperciva/network/network_connect.c -o network_connect.o
network_read.o: ../libcperciva/network/network_read.c
	${CC} ${CFLAGS} ${CFLAGS_POSIX} -D_POSIX_C_SOURCE=200809L -DCPUSUPPORT_CONFIG_FILE=\"cpusupport-config.h\"  -I .. ${IDIRS} -c ../libcperciva/network/network_read.c -o network_read.o
network_write.o: ../libcperciva/network/network_write.c
	${CC} ${CFLAGS} ${CFLAGS_POSIX} -D_POSIX_C_SOURCE=200809L -DCPUSUPPORT_CONFIG_FILE=\"cpusupport-config.h\"  -I .. ${IDIRS} -c ../libcperciva/network/network_write.c -o network_write.o
crypto_aes.o: ../libcperciva/crypto/crypto_aes.c
	${CC} ${CFLAGS} ${CFLAGS_POSIX} -D_POSIX_C_SOURCE=200809L -DCPUSUPPORT_CONFIG_FILE=\"cpusupport-config.h\"  -I .. ${IDIRS} -c ../libcperciva/crypto/crypto_aes.c -o crypto_aes.o
crypto_aes_aesni.o: ../libcperciva/crypto/crypto_aes_aesni.c
	${CC} ${CFLAGS} ${CFLAGS_POSIX} -D_POSIX_C_SOURCE=200809L -DCPUSUPPORT_CONFIG_FILE=\"cpusupport-config.h\" ${CFLAGS_X86_AESNI} -I .. ${IDIRS} -c ../libcperciva/crypto/crypto_aes_aesni.c -o crypto_aes_aesni.o
crypto_aesctr.o: ../libcperciva/crypto/crypto_aesctr.c
	${CC} ${CFLAGS} ${CFLAGS_POSIX} -D_POSIX_C_SOURCE=200809L -DCPUSUPPORT_CONFIG_FILE=\"cpusupport-config.h\"  -I .. ${IDIRS} -c ../libcperciva/crypto/crypto_aesctr.c -o crypto_aesctr.o
crypto_dh.o: ../libcperciva/crypto/crypto_dh.c
	${CC} ${CFLAGS} ${CFLAGS_POSIX} -D_POSIX_C_SOURCE=200809L -DCPUSUPPORT_CONFIG_FILE=\"cpusupport-config.h\"  -I .. ${IDIRS} -c ../libcperciva/crypto/crypto_dh.c -o crypto_dh.o
crypto_dh_group14.o: ../libcperciva/crypto/crypto_dh_group14.c
	${CC} ${CFLAGS} ${CFLAGS_POSIX} -D_POSIX_C_SOURCE=200809L -DCPUSUPPORT_CONFIG_FILE=\"cpusupport-config.h\"  -I .. ${IDIRS} -c ../libcperciva/crypto/crypto_dh_group14.c -o crypto_dh_group14.o
crypto_entropy.o: ../libcperciva/crypto/crypto_entropy.c
	${CC} ${CFLAGS} ${CFLAGS_POSIX} -D_POSIX_C_SOURCE=200809L -DCPUSUPPORT_CONFIG_FILE=\"cpusupport-config.h\"  -I .. ${IDIRS} -c ../libcperciva/crypto/crypto_entropy.c -o crypto_entropy.o
crypto_verify_bytes.o: ../libcperciva/crypto/crypto_verify_bytes.c
	${CC} ${CFLAGS} ${CFLAGS_POSIX} -D_POSIX_C_SOURCE=200809L -DCPUSUPPORT_CONFIG_FILE=\"cpusupport-config.h\"  -I .. ${IDIRS} -c ../libcperciva/crypto/crypto_verify_bytes.c -o crypto_verify_bytes.o
