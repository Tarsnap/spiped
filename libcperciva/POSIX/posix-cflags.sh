# Should be sourced by `command -p sh posix-cflags.sh "$PATH"` from within a Makefile
if ! [ ${PATH} = "$1" ]; then
	echo "WARNING: POSIX violation: $SHELL's command -p resets \$PATH" 1>&2
	PATH=$1
fi
FIRST=YES
if ! ${CC} -D_POSIX_C_SOURCE=200809L posix-msg_nosignal.c 2>/dev/null; then
	[ ${FIRST} = "NO" ] && printf " "; FIRST=NO
	printf %s "-DPOSIXFAIL_MSG_NOSIGNAL"
	echo "WARNING: POSIX violation: <sys/socket.h> not defining MSG_NOSIGNAL" 1>&2
fi
if ! ${CC} -D_POSIX_C_SOURCE=200809L posix-clock_realtime.c 2>/dev/null; then
	[ ${FIRST} = "NO" ] && printf " "; FIRST=NO
	printf %s "-DPOSIXFAIL_CLOCK_REALTIME"
	echo "WARNING: POSIX violation: <time.h> not defining CLOCK_REALTIME" 1>&2
fi
if ! ${CC} -D_POSIX_C_SOURCE=200809L posix-restrict.c 2>/dev/null; then
	echo "WARNING: POSIX violation: ${CC} does not accept 'restrict' keyword" 1>&2
	if ${CC} -D_POSIX_C_SOURCE=200809L -std=c99 posix-restrict.c 2>/dev/null; then
		[ ${FIRST} = "NO" ] && printf " "; FIRST=NO
		printf %s "-std=c99"
	fi
fi
rm -f a.out
