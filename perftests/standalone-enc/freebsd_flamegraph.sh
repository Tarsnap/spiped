#!/bin/sh

# To see only profile data from pipe_enc_thread:
#       ./freebsd_flamegraph.sh
#
# To see all profile data:
#       ./freebsd_flamegraph.sh --all

set -e -o noclobber -o nounset

# This command must be in the same directory; it will be invoked as "./${cmd}".
cmd="test_standalone_enc"
args="5 10"
outfilename="bench"

# Sanity check: dtrace silently truncates the execname to 19 characters, which
# this script does not handle.
numchars="$(printf "%s" "${cmd}" | wc -c)"
if [ "${numchars}" -gt "19" ]; then
	printf "%s is %d chars; that's too long for dtrace!\n"	\
	    "${cmd}" "${numchars}"
	exit 1
fi

# Should we keep all the profile data?
if [ "${1:-}" = "--all" ]; then
	KEEP_ALL=1
else
	KEEP_ALL=0
fi

# Set dtrace command; if we're not root, we must have doas(1) configured.
uid=$(id -u)
if [ "${uid}" -eq "0" ]; then
	DTRACE_CMD="dtrace"
else
	DTRACE_CMD="doas /usr/sbin/dtrace"
fi

# Clear previous data
rm -f "${outfilename}.stacks" "${outfilename}.folded" "${outfilename}.svg"
rm -f "${outfilename}.tmp"

# Get profile data and consolidate it.
${DTRACE_CMD}						\
	-x ustackframes=100				\
	-n "profile-997					\
	    /execname == \"${cmd}\" && arg1/		\
	    { @[ustack()] = count(); }"			\
	-o "${outfilename}.stacks"			\
	-c "./${cmd} ${args}"
stackcollapse.pl "${outfilename}.stacks" > "${outfilename}.folded"

# Unless otherwise specified, only keep the pipe_enc_thread part.
# If there's no pipe_enc_thread, don't do any filtering.
if grep -q pipe_enc_thread "${outfilename}.folded" ; then
	has_pipe_enc=1
else
	has_pipe_enc=0
fi
if [ "${KEEP_ALL}" -eq "0" ] && [ "${has_pipe_enc}" -gt "0" ]; then
	grep pipe_enc_thread "${outfilename}.folded" > "${outfilename}.tmp"
	mv "${outfilename}.tmp" "${outfilename}.folded"
fi

# Generate flamegraph image.
flamegraph.pl						\
	--width 1000					\
	--title "1 proto_pipe() doing encryption"	\
	--subtitle "(total of 3 threads in 1 process)"	\
	--hash						\
	"${outfilename}.folded" > "${outfilename}.svg"
