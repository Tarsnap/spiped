PROGS=	spiped spipe
PUBLISH= ${PROGS} BUILDING CHANGELOG COPYRIGHT README STYLE POSIX lib proto

.for D in ${PROGS}
spiped-${VERSION}/${D}/Makefile:
	echo '.POSIX:' > $@
	( cd ${D} && echo -n 'PROG=' && make -V PROG ) >> $@
	( cd ${D} && echo -n 'MAN1=' && make -V MAN1 ) >> $@
	( cd ${D} && echo -n 'SRCS=' && make -V SRCS ) >> $@
	( cd ${D} && echo -n 'IDIRS=' && make -V IDIRS ) >> $@
	( cd ${D} && echo -n 'LDADD_REQ=' && make -V LDADD_REQ ) >> $@
	cat Makefile.prog >> $@
	( cd ${D} && make -V SRCS |	\
	    tr ' ' '\n' |		\
	    sed -E 's/.c$$/.o/' |	\
	    while read F; do		\
		S=`make source-$${F}`;	\
		echo "$${F}: $${S}";	\
		echo "	\$${CC} \$${CFLAGS} -D_POSIX_C_SOURCE=200809L \$${IDIRS} -c $${S} -o $${F}"; \
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
	rm -f spiped-${VERSION}.tgz
	mkdir spiped-${VERSION}
	tar -cf- --exclude 'Makefile.*' --exclude Makefile --exclude .svn ${PUBLISH} | \
	    tar -xf- -C spiped-${VERSION}
	cp Makefile.POSIX spiped-${VERSION}/Makefile
.for D in ${PROGS}
	make spiped-${VERSION}/${D}/Makefile
.endfor
	tar -cvzf spiped-${VERSION}.tgz spiped-${VERSION}
	rm -r spiped-${VERSION}

SUBDIR=	${PROGS}
.include <bsd.subdir.mk>
