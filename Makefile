.POSIX:

PROGS=	spipe					\
	spiped
TESTS=	perftests/recv-zeros			\
	perftests/send-zeros			\
	perftests/standalone-enc		\
	tests/dnsthread-resolve			\
	tests/nc-client				\
	tests/nc-server				\
	tests/pthread_create_blocking_np	\
	tests/pushbits				\
	tests/valgrind
BINDIR_DEFAULT=	/usr/local/bin
CFLAGS_DEFAULT=	-O2
LIBCPERCIVA_DIR=	libcperciva
TEST_CMD=	tests/test_spiped.sh

### Shared code between Tarsnap projects.

all:	toplevel
	export CFLAGS="$${CFLAGS:-${CFLAGS_DEFAULT}}";	\
	. ./posix-flags.sh;				\
	. ./cpusupport-config.h;			\
	. ./cflags-filter.sh;				\
	export HAVE_BUILD_FLAGS=1;			\
	for D in ${PROGS} ${TESTS}; do			\
		( cd $${D} && ${MAKE} all ) || exit 2;	\
	done

.PHONY:	toplevel
toplevel:	cflags-filter.sh cpusupport-config.h	\
		liball posix-flags.sh

# For "loop-back" building of a subdirectory
buildsubdir: toplevel
	. ./posix-flags.sh;				\
	. ./cpusupport-config.h;			\
	. ./cflags-filter.sh;				\
	export HAVE_BUILD_FLAGS=1;			\
	cd ${BUILD_SUBDIR} && ${MAKE} ${BUILD_TARGET}

# For "loop-back" building of the library
.PHONY: liball
liball: cflags-filter.sh cpusupport-config.h posix-flags.sh
	. ./posix-flags.sh;				\
	. ./cpusupport-config.h;			\
	. ./cflags-filter.sh;				\
	export HAVE_BUILD_FLAGS=1;			\
	( cd liball && make all ) || exit 2;

posix-flags.sh:
	if [ -d ${LIBCPERCIVA_DIR}/POSIX/ ]; then			\
		export CC="${CC}";					\
		cd ${LIBCPERCIVA_DIR}/POSIX;				\
		printf "export \"LDADD_POSIX=";				\
		command -p sh posix-l.sh "$$PATH";			\
		printf "\"\n";						\
		printf "export \"CFLAGS_POSIX=";			\
		command -p sh posix-cflags.sh "$$PATH";			\
		printf "\"\n";						\
	fi > $@
	if [ ! -s $@ ]; then						\
		printf "#define POSIX_COMPATIBILITY_NOT_CHECKED 1\n";	\
	fi >> $@

cflags-filter.sh:
	if [ -d ${LIBCPERCIVA_DIR}/POSIX/ ]; then			\
		export CC="${CC}";					\
		cd ${LIBCPERCIVA_DIR}/POSIX;				\
		command -p sh posix-cflags-filter.sh "$$PATH";		\
	fi > $@
	if [ ! -s $@ ]; then						\
		printf "# Compiler understands normal flags; ";		\
		printf "nothing to filter out\n";			\
	fi >> $@

cpusupport-config.h:
	if [ -d ${LIBCPERCIVA_DIR}/cpusupport/ ]; then			\
		export CC="${CC}";					\
		command -p sh						\
		    ${LIBCPERCIVA_DIR}/cpusupport/Build/cpusupport.sh	\
		    "$$PATH";						\
	fi > $@
	if [ ! -s $@ ]; then						\
		printf "#define CPUSUPPORT_NONE 1\n";			\
	fi >> $@

install:	all
	export BINDIR=$${BINDIR:-${BINDIR_DEFAULT}};	\
	for D in ${PROGS}; do				\
		( cd $${D} && ${MAKE} install ) || exit 2;	\
	done

clean:
	rm -f cflags-filter.sh cpusupport-config.h posix-flags.sh
	for D in liball ${PROGS} ${TESTS}; do			\
		( cd $${D} && ${MAKE} clean ) || exit 2;	\
	done

.PHONY:	test test-clean
test:	all
	${TEST_CMD}

test-clean:
	rm -rf tests-output/ tests-valgrind/

# Developer targets: These only work with BSD make
Makefiles:
	${MAKE} -f Makefile.BSD Makefiles

publish:
	${MAKE} -f Makefile.BSD publish
