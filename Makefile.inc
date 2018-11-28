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
