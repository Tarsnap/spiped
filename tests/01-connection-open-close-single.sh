#!/bin/sh

# Goal of this test:
# - establish a connection to a spiped server.
# - open a connection, but don't send anything.
# - close the connection.
# - do the above for network sockets and unix domain sockets

### Constants


### Helper functions
run_test() {
	# Set up infrastructure.
	setup_spiped_decryption_server
	setup_spiped_encryption_server

	# Open and close a connection.
	setup_check_variables
	(
		echo "" | ${nc_client_binary} ${src_sock}
		echo $? > ${c_exitfile}
	)

	# Wait for server(s) to quit.
	servers_stop
}

### Actual command
scenario_cmd() {
	# Run test with network sockets (as usual)
	run_test

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
	rm ${src_sock}
	rm ${mid_sock}
	rm ${dst_sock}

	# Restore the network sockets
	src_sock=${src_sock_orig}
	mid_sock=${mid_sock_orig}
	dst_sock=${dst_sock_orig}
}
