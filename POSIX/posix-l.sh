# Should be sourced by `command -p sh posix-l.sh` from within a Makefile.
for LIB in rt xnet; do
	if ${CC} -o /dev/null -l${LIB} posix-l.c 2>/dev/null; then
		echo -n -l${LIB};
		echo " ";
	else
		echo "WARNING: POSIX violation: make's CC doesn't understand -l${LIB}" >/dev/stderr
	fi
done
