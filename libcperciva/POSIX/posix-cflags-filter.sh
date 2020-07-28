# Should be sourced by `command -p sh posix-cflags-filter.sh "$PATH"` from within a Makefile
# Produces a file to be sourced which edits CFLAGS

# Sanity check environment variables
if [ -z "${CC}" ]; then
	echo "\$CC is not defined!  Cannot run any compiler tests." 1>&2
	exit 1
fi
if ! [ ${PATH} = "$1" ]; then
	echo "WARNING: POSIX violation: $SHELL's command -p resets \$PATH" 1>&2
	PATH=$1
fi

# Find directory of this script and the source files
D=$(dirname $0)

if ! ${CC} -O2 $D/posix-cflags-filter.c 2>/dev/null; then
	if ${CC} $D/posix-cflags-filter.c 2>/dev/null; then
		echo 'CFLAGS_FILTERED=""'
		echo 'for OPT in $CFLAGS; do'
		echo '	if [ "$OPT" = "-O2" ]; then'
		echo '		continue'
		echo '	fi'
		echo '	CFLAGS_FILTERED="$CFLAGS $OPT"'
		echo 'done'
		echo 'CFLAGS="$CFLAGS_FILTERED"'
		echo "WARNING: POSIX violation: make's CC doesn't understand -O2" 1>&2
	fi
fi
rm -f a.out
