PROGS=	spiped spipe
PUBLISH= ${PROGS} BUILDING CHANGELOG COPYRIGHT README STYLE lib proto

.for D in ${PROGS}
spiped-${VERSION}/${D}/Makefile:
	echo '.POSIX:' > $@
	( cd ${D} && echo -n 'PROG=' && make -V PROG ) >> $@
	( cd ${D} && echo -n 'SRCS=' && make -V SRCS ) >> $@
	( cd ${D} && echo -n 'IDIRS=' && make -V IDIRS ) >> $@
	( cd ${D} && echo -n 'LDADD=' && make -V LDADD ) >> $@
	cat Makefile.prog >> $@
	( cd ${D} && make -V SRCS |	\
	    tr ' ' '\n' |		\
	    sed -E 's/.c$$/.o/' |	\
	    while read F; do		\
		echo -n "$${F}: ";	\
		make source-$${F};	\
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
