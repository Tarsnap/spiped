#!/bin/sh

# Goal of this test:
# - FIXME

### Constants
c_valgrind_min=1
num_conn=3
ncat_output="${s_basename}-ncat-output.txt"
sendfile=${scriptdir}/shared_test_functions.sh


### Helper functions
run_test() {
	# Set up infrastructure.
	setup_spiped_decryption_server "${ncat_output}" 0 1 0 "${num_conn}"
	setup_spiped_encryption_server

	# Open and close a connection.
	for i in `seq ${num_conn}`; do
		echo "doing $i" 1>&2
		setup_check "11-foo $i"
		${nc_client_binary} "${src_sock}" < "${sendfile}"
		#echo "a" | ${nc_client_binary} "${src_sock}"
		echo $? > "${c_exitfile}"
	done

	# Wait for server(s) to quit.
	echo "doing servers_stop" 1>&2
	servers_stop
}

### Actual command
scenario_cmd() {
	# Run test with network sockets (as usual)
	run_test

	return
	# Save the network sockets
	src_sock_orig=${src_sock}
	mid_sock_orig=${mid_sock}
	dst_sock_orig=${dst_sock}

	# Run test with unix domain sockets
	src_sock=${s_basename}-src.sock
	mid_sock=${s_basename}-mid.sock
	dst_sock=${s_basename}-dst.sock
	run_test

	# Clean up left-over sockets
	rm "${src_sock}"
	rm "${mid_sock}"
	rm "${dst_sock}"

	# Restore the network sockets
	src_sock=${src_sock_orig}
	mid_sock=${mid_sock_orig}
	dst_sock=${dst_sock_orig}
}
