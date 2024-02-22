# Used by Makefile code which generates POSIX Makefiles
.for X in ${SRCS}
source-${X:.c=.o}: ${X}
	@echo $>
cflags-${X:.c=.o}:
.endfor

# Used to track the subdirectory depth (other directories append to this)
SUBDIR_DEPTH	=	..

# Expand ${IDIRS} before it is used
IDIRS	:=	${IDIRS}

# Default is no man pages
MAN	=

# The SUBDIR_DEPTH variable (defined above) initializes tracking the
# subdirectory depth, but it's not fully set at this stage in the build
# process.  Instead, we jump back to the shell and print the (finalized) value
# of SUBDIR_DEPTH.  This avoids an infinite loop because we query the Makefile
# (which does not include ../Makefile.inc), rather than Makefile.BSD (which
# would end up including this file again).
FINALIZED_SUBDIR_DEPTH	!=	${MAKE} -v SUBDIR_DEPTH

.if !defined(NOLIBALL)
# Link everything to liball.a, unless they specifically ask not to use it.
LIBALL	=	${SUBDIR_DEPTH}/liball/liball.a
.if exists(${FINALIZED_SUBDIR_DEPTH}/liball/optional_mutex_normal)
# If we have optional_mutex, add it. If appropriate, metabuild.sh will do:
#     s/optional_mutex_normal/optional_mutex_pthread/g
LIBALL	+=	${SUBDIR_DEPTH}/liball/optional_mutex_normal/liball_optional_mutex_normal.a
.endif
LDADD	+=	${LIBALL}
DPADD	+=	${LIBALL}
.endif

# Lines below this point are only relevant if running
#     make -f Makefile.BSD
# in a subdirectory; they have no influence on the generated Makefiles.

# Use POSIX standard
CFLAGS	+=	-D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700

# Make logic
CFLAGS	+=	${IDIRS}
LDADD	+=	${LDADD_REQ}
