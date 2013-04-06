.POSIX:

PROGS=		spiped spipe
BINDIR_DEFAULT=	/usr/local/bin
CFLAGS_DEFAULT=	-O2

all:
	if [ -z "${CFLAGS}" ]; then			\
		CFLAGS=${CFLAGS_DEFAULT};		\
	else						\
		CFLAGS="${CFLAGS}";			\
	fi;						\
	LDADD_POSIX=`export CC=${CC}; cd POSIX && command -p sh posix-l.sh`;	\
	for D in ${PROGS}; do					\
		( cd $${D} && 					\
		    make CFLAGS="$${CFLAGS}"			\
			LDADD_POSIX="$${LDADD_POSIX}" all ) ||	\
		    exit 2;					\
	done

install: all
	if [ -z "${BINDIR}" ]; then			\
		BINDIR=${BINDIR_DEFAULT};		\
	else						\
		BINDIR="${BINDIR}";			\
	fi;						\
	for D in ${PROGS}; do				\
		( cd $${D} && make BINDIR="$${BINDIR}" install ) || exit 2;	\
	done

clean:
	for D in ${PROGS}; do				\
		( cd $${D} && make clean ) || exit 2;	\
	done
