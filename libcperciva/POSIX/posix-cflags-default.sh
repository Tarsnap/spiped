# Should be sourced by `command -p sh posix-cflags-default.sh "$PATH"`
# from within a Makefile

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
D=`dirname $0`

# Can we compile with -O2?
if ${CC} -D_POSIX_C_SOURCE=200809L -O2 $D/posix-l.c 2>/dev/null; then
	# We're fine; use existing flags
	printf %s "${CFLAGS_DEFAULT}"
else
	# Remove -O2 from CFLAGS_DEFAULT (if it's present)
	CFLAGS_DEFAULT_CHECKED="${CFLAGS_DEFAULT%%-O2*}${CFLAGS_DEFAULT#*-O2}"

	# Can we compile with -O?
	if ${CC} -D_POSIX_C_SOURCE=200809L -O $D/posix-l.c 2>/dev/null; then
		# Append -O to the flags
		if [ -n "${CLAGS_DEFAULT_CHECKED}" ]; then
			CFLAGS_DEFAULT_CHECKED="${CFLAGS_DEFAULT_CHECKED} -O"
		else
			CFLAGS_DEFAULT_CHECKED="-O"
		fi
		printf %s "${CFLAGS_DEFAULT_CHECKED}"
		echo "WARNING: POSIX violation: ${CC} does not accept -O2" 1>&2
	else
		# Use CFLAGS_DEFAULT_CHECKED without -O2 or -O
		printf %s "${CFLAGS_DEFAULT_CHECKED}"
	fi
fi

rm -f a.out
