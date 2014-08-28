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

# CPU features compiler support detection
SRCS	+=	cpusupport-config.h
cpusupport-config.h:
	( export CFLAGS="${CFLAGS}"; cd ../libcperciva/cpusupport/Build && command -p sh cpusupport.sh ) > cpusupport-config.h
CLEANFILES+=	cpusupport-config.h
CFLAGS	+=	-I . -D CPUSUPPORT_CONFIG_FILE=\"cpusupport-config.h\"
