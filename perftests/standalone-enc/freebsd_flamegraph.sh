#!/bin/sh

# To see only profile data from pipe_enc_thread:
#       ./freebsd_flamegraph.sh
#
# To see all profile data:
#       ./freebsd_flamegraph.sh --all

set -e -o noclobber -o nounset

outfilename="bench"

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

# Get profile data and consolidate it.
${DTRACE_CMD}						\
	-x ustackframes=100				\
	-n 'profile-997 /execname == "test_standalone_enc" && arg1/ { @[ustack()] = count(); }'	\
	-o "${outfilename}.stacks"			\
	-c './test_standalone_enc 5 10'
stackcollapse.pl "${outfilename}.stacks" > "${outfilename}.folded"

# Unless otherwise specified, only keep the pipe_enc_thread part.
if [ "${KEEP_ALL}" -eq "0" ]; then
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
