# Should be sourced by `command -p sh posix-l.sh` from within a Makefile.
FIRST=YES
for LIB in rt xnet; do
	if ${CC} -l${LIB} posix-l.c 2>/dev/null; then
		if [ ${FIRST} = "NO" ]; then
			printf " ";
		fi
		printf "%s" "-l${LIB}";
		FIRST=NO;
	else
		echo "WARNING: POSIX violation: make's CC doesn't understand -l${LIB}" >/dev/stderr
	fi
	rm -f a.out
done
