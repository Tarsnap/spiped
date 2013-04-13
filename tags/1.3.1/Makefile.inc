# Used by Makefile code which generates POSIX Makefiles
.for X in ${SRCS}
source-${X:.c=.o}: ${X}
	@echo $>
.endfor

# Defaults for all kivaloo code
NO_MAN	?=	yes
WARNS	?=	6

# Make logic (bits which are separated out in individual Makefiles).
CFLAGS	+=	${IDIRS}
LDADD	+=	${LDADD_REQ}
