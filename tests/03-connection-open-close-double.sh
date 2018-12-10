#!/bin/sh

# Goal of this test:
# - create a pair of spiped servers (encryption, decryption).
# - open two connections, but don't send anything,
# - close one of the connections.

### Constants


### Actual command
scenario_cmd() {
	# Set up infrastructure.
	setup_spiped_decryption_server
	setup_spiped_encryption_server

	# Open and close a connection, keeping it open for 2 seconds.
	setup_check_variables
	(
		( echo "" ; sleep 2 ) |		\
			 ${nc_client_binary} ${src_sock}
		echo $? > ${c_exitfile}
	) &

	setup_check_variables
	(
		echo "" | ${nc_client_binary} ${src_sock}
		echo $? > ${c_exitfile}
	)
	sleep 3

	# Wait for server(s) to quit.
	servers_stop
}
