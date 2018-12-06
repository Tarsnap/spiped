#!/bin/sh

## check_leftover_servers():
# Repeated testing, especially when doing ctrl-c to break out of (suspected)
# hanging, can leave a nc_server or spiped process(es) floating around, which
# is problematic for the next testing run.  Checking for this shouldn't be
# necessary for normal testing (as opposed to test-script development), but
# there's no harm in checking anyway.  Once identified, the user is notified
# (instead of automatically removing them) -- a failing test will require
# manual attention anyway.
check_leftover_servers() {
	# Find old nc-server on ${dst_port}.
	if $( has_pid "${nc_server_binary} ${dst_port}" ); then
		echo "Error: Left-over nc-server from previous run."
		exit 1
	fi

	# Find old spiped {-d, -e} servers on {${mid_port}, ${src_port}}.
	if $( has_pid "spiped -d -s \[127.0.0.1\]:${mid_port}" ); then
		echo "Error: Left-over spiped -d from previous run."
		exit 1
	fi
	if $( has_pid "spiped -e -s \[127.0.0.1\]:${src_port}" ); then
		echo "Error: Left-over spiped -e from previous run."
		exit 1
	fi
}

## setup_spiped_decryption_server(nc_output=/dev/null, use_system_spiped=0):
# Set up a spiped decryption server, translating from ${mid_port}
# to ${dst_port}, saving the exit code to ${c_exitfile}.  Also set
# up a nc-server listening to ${dst_port}, saving output to
# ${nc_output}.  Uses the system's spiped (instead of the
# version in this source tree) if ${use_system_spiped} is 1.
setup_spiped_decryption_server () {
	nc_output=${1:-/dev/null}
	use_system_spiped=${2:-0}
	check_leftover_servers

	# Select system or compiled spiped.
	if [ "${use_system_spiped}" -gt 0 ]; then
		this_spiped_cmd=${system_spiped_binary}
	else
		this_spiped_cmd=${spiped_binary}
	fi

	# Start backend server.
	${nc_server_binary} ${dst_port} ${nc_output} &
	nc_pid=${!}

	# Start spiped to connect middle port to backend.
	${this_spiped_cmd} -d			\
		-s [127.0.0.1]:${mid_port}	\
		-t [127.0.0.1]:${dst_port}	\
		-p ${s_basename}-spiped-d.pid	\
		-k /dev/null -o 1
}

## setup_spiped_decryption_server(basename):
# Set up a spiped encryption server, translating from ${src_port}
# to ${mid_port}, saving the exit code to ${c_exitfile}.
setup_spiped_encryption_server () {
	# Start spiped to connect source port to middle.
	${spiped_binary} -e			\
		-s [127.0.0.1]:${src_port}	\
		-t [127.0.0.1]:${mid_port}	\
		-p ${s_basename}-spiped-e.pid	\
		-k /dev/null -o 1
}

## servers_stop():
# Stops the various servers.
servers_stop() {
	# Signal spiped servers to stop
	if [ -e ${s_basename}-spiped-e.pid ]; then
		kill `cat ${s_basename}-spiped-e.pid`
	fi
	if [ -e ${s_basename}-spiped-d.pid ]; then
		kill `cat ${s_basename}-spiped-d.pid`
	fi

	# Give servers a chance to stop without fuss.
	sleep 1

	# Waiting for servers to stop
	while $( has_pid "spiped -e -s \[127.0.0.1\]:${src_port}" ); do
		if [ ${VERBOSE} -ne 0 ]; then
			echo "Waiting to stop: spiped -e"
		fi
		sleep 1
	done
	while $( has_pid "spiped -d -s \[127.0.0.1\]:${mid_port}" ); do
		if [ ${VERBOSE} -ne 0 ]; then
			echo "Waiting to stop: spiped -d"
		fi
		sleep 1
	done
	while $( has_pid "${nc_server_binary} ${dst_port}" ); do
		if [ ${VERBOSE} -ne 0 ]; then
			echo "Waiting to stop: ncat"
		fi
		sleep 1
	done
}
