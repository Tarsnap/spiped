PKG=	spiped
PROGS=	spiped spipe
PUBLISH= ${PROGS} BUILDING CHANGELOG COPYRIGHT README STYLE POSIX lib libcperciva proto

.for D in ${PROGS}
${PKG}-${VERSION}/${D}/Makefile:
	echo '.POSIX:' > $@
	( cd ${D} && echo -n 'PROG=' && make -V PROG ) >> $@
	( cd ${D} && echo -n 'MAN1=' && make -V MAN1 ) >> $@
	( cd ${D} && echo -n 'SRCS=' && make -V SRCS | sed -e 's|cpusupport-config.h||' ) >> $@
	( cd ${D} && echo -n 'IDIRS=' && make -V IDIRS ) >> $@
	( cd ${D} && echo -n 'LDADD_REQ=' && make -V LDADD_REQ ) >> $@
	cat Makefile.prog >> $@
	( cd ${D} && make -V SRCS |	\
	    sed -e 's| cpusupport-config.h||' |	\
	    tr ' ' '\n' |		\
	    sed -E 's/.c$$/.o/' |	\
	    while read F; do		\
		S=`make source-$${F}`;	\
		CF=`make -V cflags-$${F}`;	\
		echo "$${F}: $${S}";	\
		echo "	\$${CC} \$${CFLAGS} \$${CFLAGS_POSIX} -D_POSIX_C_SOURCE=200809L -DCPUSUPPORT_CONFIG_FILE=\\\"cpusupport-config.h\\\" $${CF} -I .. \$${IDIRS} -c $${S} -o $${F}"; \
	    done ) >> $@
.endfor

publish: clean
	if [ -z "${VERSION}" ]; then			\
		echo "VERSION must be specified!";	\
		exit 1;					\
	fi
	if find . | grep \~; then					\
		echo "Delete temporary files before publishing!";	\
		exit 1;							\
	fi
	rm -f ${PKG}-${VERSION}.tgz
	mkdir ${PKG}-${VERSION}
	tar -cf- --exclude 'Makefile.*' --exclude Makefile --exclude .svn ${PUBLISH} | \
	    tar -xf- -C ${PKG}-${VERSION}
	cp Makefile.POSIX ${PKG}-${VERSION}/Makefile
.for D in ${PROGS}
	make ${PKG}-${VERSION}/${D}/Makefile
.endfor
	tar -cvzf ${PKG}-${VERSION}.tgz ${PKG}-${VERSION}
	rm -r ${PKG}-${VERSION}

SUBDIR=	${PROGS}
.include <bsd.subdir.mk>
