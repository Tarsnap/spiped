.POSIX:

PROGS=	spipe					\
	spiped
LIBS=	liball					\
	liball/optional_mutex_normal		\
	liball/optional_mutex_pthread
TESTS=	perftests/recv-zeros			\
	perftests/send-zeros			\
	perftests/standalone-enc		\
	tests/dnsthread-resolve			\
	tests/msleep				\
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

.PHONY:	all
all:	toplevel
	export CFLAGS="$${CFLAGS:-${CFLAGS_DEFAULT}}";	\
	. ./posix-flags.sh;				\
	. ./cpusupport-config.h;			\
	. ./cflags-filter.sh;				\
	. ./apisupport-config.h;			\
	export HAVE_BUILD_FLAGS=1;			\
	for D in ${PROGS} ${TESTS}; do			\
		( cd $${D} && ${MAKE} all ) || exit 2;	\
	done

.PHONY:	toplevel
toplevel:	apisupport-config.h cflags-filter.sh	\
		cpusupport-config.h libs		\
		posix-flags.sh

# For "loop-back" building of a subdirectory
.PHONY:	buildsubdir
buildsubdir: toplevel
	. ./posix-flags.sh;				\
	. ./cpusupport-config.h;			\
	. ./cflags-filter.sh;				\
	. ./apisupport-config.h;			\
	export HAVE_BUILD_FLAGS=1;			\
	cd ${BUILD_SUBDIR} && ${MAKE} ${BUILD_TARGET}

# For "loop-back" building of libraries
.PHONY:	libs
libs: apisupport-config.h cflags-filter.sh cpusupport-config.h posix-flags.sh
	. ./posix-flags.sh;				\
	. ./cpusupport-config.h;			\
	. ./cflags-filter.sh;				\
	. ./apisupport-config.h;			\
	export HAVE_BUILD_FLAGS=1;			\
	for D in ${LIBS}; do				\
		( cd $${D} && make all ) || exit 2;	\
	done

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

apisupport-config.h:
	if [ -d ${LIBCPERCIVA_DIR}/apisupport/ ]; then			\
		export CC="${CC}";					\
		command -p sh						\
		    ${LIBCPERCIVA_DIR}/apisupport/Build/apisupport.sh	\
		    "$$PATH";						\
	else								\
		:;							\
	fi > $@

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

.PHONY:	install
install:	all
	export BINDIR=$${BINDIR:-${BINDIR_DEFAULT}};	\
	for D in ${PROGS}; do				\
		( cd $${D} && ${MAKE} install ) || exit 2;	\
	done

.PHONY:	clean
clean:	test-clean
	rm -f apisupport-config.h cflags-filter.sh cpusupport-config.h posix-flags.sh
	for D in ${LIBS} ${PROGS} ${TESTS}; do			\
		( cd $${D} && ${MAKE} clean ) || exit 2;	\
	done

.PHONY:	test
test:	all
	${TEST_CMD}

.PHONY:	test-clean
test-clean:
	rm -rf tests-output/ tests-valgrind/

# Developer targets: These only work with BSD make
.PHONY:	Makefiles
Makefiles:
	${MAKE} -f Makefile.BSD Makefiles

.PHONY:	publish
publish:
	${MAKE} -f Makefile.BSD publish
