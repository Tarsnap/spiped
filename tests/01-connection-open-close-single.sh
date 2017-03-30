#!/bin/sh

# Goal of this test:
# - establish a connection to a spiped server.
# - open a connection, but don't send anything.
# - close the connection.

### Constants


### Actual command
scenario_cmd() {
	# Set up infrastructure.
	setup_spiped_decryption_server
	setup_spiped_encryption_server

	# Open and close a connection.
	setup_check_variables
	(
		echo "" | ${nc_client_binary} [127.0.0.1]:${src_port}
		echo $? > ${c_exitfile}
	)

	# Wait for server(s) to quit.
	servers_stop
}
