#!/bin/sh

# Goal of this test:
# - establish a connection to a spiped server.
# - open a connection, but don't send anything.
# - close the connection.
# - server should quit (because we gave it -1).

### Constants


### Actual command
scenario_cmd() {
	# Set up infrastructure.
	setup_spiped_decryption_server

	# Open and close a connection.
	setup_check_variables
	(
		echo "" | ${nc_client_binary} [127.0.0.1]:${mid_port}
		echo $? > ${c_exitfile}
	)

	# Wait for server(s) to quit.
	servers_stop
}
