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

.if !defined(NOLIBALL)
# Link everything to liball.a, unless they specifically ask not to use it.
LIBALL	=	${SUBDIR_DEPTH}/liball/liball.a ${SUBDIR_DEPTH}/liball/optional_mutex_normal/liball_optional_mutex_normal.a
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
