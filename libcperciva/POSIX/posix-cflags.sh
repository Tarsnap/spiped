# Should be sourced by `command -p sh posix-cflags.sh` from within a Makefile
FIRST=YES
if ! "${CC}" -D_POSIX_C_SOURCE=200809L posix-msg_nosignal.c 2>/dev/null; then
	[ ${FIRST} = "NO" ] && printf " "; FIRST=NO
	printf %s "-DPOSIXFAIL_MSG_NOSIGNAL"
	echo "WARNING: POSIX violation: <sys/socket.h> not defining MSG_NOSIGNAL" 1>&2
fi
if ! "${CC}" -D_POSIX_C_SOURCE=200809L posix-clock_realtime.c 2>/dev/null; then
	[ ${FIRST} = "NO" ] && printf " "; FIRST=NO
	printf %s "-DPOSIXFAIL_CLOCK_REALTIME"
	echo "WARNING: POSIX violation: <time.h> not defining CLOCK_REALTIME" 1>&2
fi
if ! "${CC}" -D_POSIX_C_SOURCE=200809L posix-restrict.c 2>/dev/null; then
	if "${CC}" -D_POSIX_C_SOURCE=200809L -std=c99 posix-restrict.c ; then
		[ ${FIRST} = "NO" ] && printf " "; FIRST=NO
		printf %s "-std=c99"
	else
		echo "ERROR: adding -std=c99 does not fix 'restrict'; cannot continue". 1>&2
	fi
fi
rm -f a.out
