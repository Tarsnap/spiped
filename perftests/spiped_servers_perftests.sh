#!/bin/sh

## check_leftover_servers_perftests():
# Repeated testing, especially when doing ctrl-c to break out of (suspected)
# hanging, can leave a process(es) floating around, which is problematic for
# the next testing run.  Checking for this shouldn't be necessary for normal
# testing (as opposed to test-script development), but there's no harm in
# checking anyway.  Once identified, the user is notified (instead of
# automatically removing them) -- a failing test will require manual attention
# anyway.
check_leftover_servers_perftests() {
	# Call original one
	check_leftover_servers

	# Find old recv-zeros on ${dst_port}.
	if $( has_pid "${recv_zeros} ${dst_sock}" ); then
		echo "Error: Left-over recv-zeros from previous run."
		exit 1
	fi
}

## servers_stop():
# Stops the various servers.
servers_stop_perftests() {
	# Call original one
	servers_stop

	while $( has_pid "${recv_zeros} ${dst_sock}" ); do
		if [ ${VERBOSE} -ne 0 ]; then
			echo "Waiting to stop: ncat"
		fi
		sleep 1
	done
}
