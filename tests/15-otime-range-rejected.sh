#!/bin/sh

# Goal of this test:
# - reject connection/handshake timeouts which cannot be represented safely
#   by the event timer, rather than converting them to an invalid deadline.

process_running() {
	kill -0 "$1" 2>/dev/null
}

check_otime_rejected() {
	otime=$1
	label=$2
	spipe_stderr="${s_basename}-${label}.stderr"

	"${spipe_binary}" \
	    -t "${dst_sock}" \
	    -k /dev/null \
	    -o "${otime}" < /dev/null > /dev/null 2> "${spipe_stderr}" &
	pid=$!

	# The repaired timer rejects the interval while proto_conn_create() is
	# setting up its connect timeout.  Bound this check so an unfixed binary
	# cannot leave the regression suite waiting on an enormous deadline.
	if wait_while 1000 process_running "${pid}"; then
		if wait "${pid}"; then
			exitcode=0
		else
			exitcode=$?
		fi
		if [ "${exitcode}" -eq 1 ] &&
		    grep -q 'Could not set up connection' "${spipe_stderr}"; then
			rc=0
		else
			rc=1
		fi
	else
		kill "${pid}" 2>/dev/null || true
		wait "${pid}" 2>/dev/null || true
		rc=1
	fi

	rm -f "${spipe_stderr}"
	return "${rc}"
}

scenario_cmd() {
	setup_check "reject infinite connection timeout"
	if check_otime_rejected Infinity inf; then
		echo 0 > "${c_exitfile}"
	else
		echo 1 > "${c_exitfile}"
	fi

	setup_check "reject huge finite connection timeout"
	if check_otime_rejected 1e308 huge; then
		echo 0 > "${c_exitfile}"
	else
		echo 1 > "${c_exitfile}"
	fi
}
