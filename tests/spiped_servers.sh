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
	# Find old nc-server on ${dst_sock}.
	if [ -n "${nc_server_binary+set}" ]; then
		if $( has_pid "${nc_server_binary} ${dst_sock}" ); then
			echo "Error: Left-over nc-server from previous run."
			exit 1
		fi
	fi

	# Find old spiped {-d, -e} servers on {${mid_sock}, ${src_sock}}.
	if $( has_pid "spiped -d -s ${mid_sock}" ); then
		echo "Error: Left-over spiped -d from previous run."
		exit 1
	fi
	if $( has_pid "spiped -e -s ${src_sock}" ); then
		echo "Error: Left-over spiped -e from previous run."
		exit 1
	fi
}

## setup_spiped_decryption_server(nc_output=/dev/null, use_system_spiped=0,
#      use_nc=1):
# Set up a spiped decryption server, translating from ${mid_sock}
# to ${dst_sock}, saving the exit code to ${c_exitfile}.  Also set
# up a nc-server listening to ${dst_sock}, saving output to
# ${nc_output}, unless ${use_nc} is 0.  Uses the system's spiped (instead of
# the version in this source tree) if ${use_system_spiped} is 1.
setup_spiped_decryption_server () {
	nc_output=${1:-/dev/null}
	use_system_spiped=${2:-0}
	use_nc=${3:-1}
	check_leftover_servers

	# Select system or compiled spiped.
	if [ "${use_system_spiped}" -gt 0 ]; then
		this_spiped_cmd=${system_spiped_binary}
	else
		this_spiped_cmd=${spiped_binary}
	fi

	# Start backend server (if desired).
	if [ ! "${use_nc}" -eq 0 ]; then
		${nc_server_binary} ${dst_sock} ${nc_output} &
	fi

	# Start spiped to connect middle port to backend.
	${this_spiped_cmd} -d			\
		-s ${mid_sock}			\
		-t ${dst_sock}			\
		-p ${s_basename}-spiped-d.pid	\
		-k /dev/null -o 1
}

## setup_spiped_decryption_server(basename):
# Set up a spiped encryption server, translating from ${src_sock}
# to ${mid_sock}, saving the exit code to ${c_exitfile}.
setup_spiped_encryption_server () {
	# Start spiped to connect source port to middle.
	${spiped_binary} -e			\
		-s ${src_sock}			\
		-t ${mid_sock}			\
		-p ${s_basename}-spiped-e.pid	\
		-k /dev/null -o 1
}

## servers_stop():
# Stops the various servers.
servers_stop() {
	# Signal spiped servers to stop
	if [ -e ${s_basename}-spiped-e.pid ]; then
		kill `cat ${s_basename}-spiped-e.pid`
		rm ${s_basename}-spiped-e.pid
	fi
	if [ -e ${s_basename}-spiped-d.pid ]; then
		kill `cat ${s_basename}-spiped-d.pid`
		rm ${s_basename}-spiped-d.pid
	fi

	# Give servers a chance to stop without fuss.
	sleep 1

	# Waiting for servers to stop
	while $( has_pid "spiped -e -s ${src_sock}" ); do
		if [ ${VERBOSE} -ne 0 ]; then
			echo "Waiting to stop: spiped -e"
		fi
		sleep 1
	done
	while $( has_pid "spiped -d -s ${mid_sock}" ); do
		if [ ${VERBOSE} -ne 0 ]; then
			echo "Waiting to stop: spiped -d"
		fi
		sleep 1
	done
	if [ -n "${nc_server_binary+set}" ]; then
		while $( has_pid "${nc_server_binary} ${dst_sock}" ); do
			if [ ${VERBOSE} -ne 0 ]; then
				echo "Waiting to stop: ncat"
			fi
			sleep 1
		done
	fi
}
