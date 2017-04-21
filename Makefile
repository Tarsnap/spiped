.POSIX:

PROGS=		spiped spipe
TESTS=		tests/nc-client tests/nc-server tests/valgrind
BINDIR_DEFAULT=	/usr/local/bin
CFLAGS_DEFAULT=	-O2

all: check-devel cpusupport-config.h
	export CFLAGS="$${CFLAGS:-${CFLAGS_DEFAULT}}";	\
	export LDADD_POSIX=`export CC="${CC}"; cd libcperciva/POSIX && command -p sh posix-l.sh "$$PATH"`;	\
	export CFLAGS_POSIX=`export CC="${CC}"; cd libcperciva/POSIX && command -p sh posix-cflags.sh "$$PATH"`;	\
	. ./cpusupport-config.h;			\
	for D in ${PROGS} ${TESTS}; do			\
		( cd $${D} && ${MAKE} all ) || exit 2;	\
	done

check-devel:
	@if [ -z ${SPIPED_DEVEL} ]; then				\
		printf "This is unstable development code.  Please";	\
		printf " consider using the official tarball:\n";	\
		printf "https://www.tarsnap.com/spiped.html\n";		\
		printf "\nIf you are certain that you want to";		\
		printf " continue, define the SPIPED_DEVEL\n";		\
		printf "environment variable.\n\n";			\
		exit 1;							\
	fi

cpusupport-config.h:
	( export CC="${CC}"; command -p sh libcperciva/cpusupport/Build/cpusupport.sh "$$PATH" ) > cpusupport-config.h

install: all
	export BINDIR=$${BINDIR:-${BINDIR_DEFAULT}};	\
	for D in ${PROGS}; do				\
		( cd $${D} && ${MAKE} install ) || exit 2;	\
	done

clean:
	rm -f cpusupport-config.h
	for D in ${PROGS} ${TESTS}; do				\
		( cd $${D} && ${MAKE} clean ) || exit 2;	\
	done

.PHONY: test
test:
	VERBOSE=1 tests/test_spiped.sh

# Developer targets: These only work with BSD make
Makefiles:
	${MAKE} -f Makefile.BSD Makefiles

publish:
	${MAKE} -f Makefile.BSD publish
