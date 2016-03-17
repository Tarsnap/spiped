# Used by Makefile code which generates POSIX Makefiles
.for X in ${SRCS}
source-${X:.c=.o}: ${X}
	@echo $>
cflags-${X:.c=.o}:
.endfor

# Defaults for all of cpercivaware
NO_MAN	?=	yes
WARNS	?=	6

# Make logic (bits which are separated out in individual Makefiles).
CPPFLAGS+=	${IDIRS}
LDADD	+=	${LDADD_REQ}

# CPU features compiler support detection
SRCS	+=	cpusupport-config.h
cpusupport-config.h:
	( export CFLAGS="${CFLAGS}"; export CC="${CC}"; command -p sh ../libcperciva/cpusupport/Build/cpusupport.sh ) > cpusupport-config.h
CLEANFILES+=	cpusupport-config.h
CPPFLAGS+=	-D CPUSUPPORT_CONFIG_FILE=\"cpusupport-config.h\"
