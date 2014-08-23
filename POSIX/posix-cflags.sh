# Should be sourced by `command -p sh posix-cflags.sh` from within a Makefile
if ! ${CC} -D_POSIX_C_SOURCE=200809L posix-msg_nosignal.c 2>/dev/null; then
	echo "-DPOSIXFAIL_MSG_NOSIGNAL"
	echo "WARNING: POSIX violation: <sys/socket.h> not defining MSG_NOSIGNAL" >/dev/stderr
fi
rm -f a.out
