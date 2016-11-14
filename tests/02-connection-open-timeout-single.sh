#!/bin/sh

# Goal of this test:
# - establish a connection to a spiped server.
# - open a connection, but don't send anything.
# - wait; the connection should be closed automatically (because we
#   gave it -o 1).
# - server should quit (because we gave it -1).

### Constants


### Actual command
scenario_cmd() {
	# Set up infrastructure.
	setup_check_variables
	setup_spiped_decryption_server

	# Open and close a connection.  Awkwardly force nc to keep
	# the connection open; the simple "nc -q 2 ..." to wait 2
	# seconds isn't portable.
	setup_check_variables
	(
		( echo "" ; sleep 2 ) | nc 127.0.0.1 ${mid_port} >/dev/null
		echo $? > ${c_exitfile}
	) &
	sleep 3

	# Wait for server(s) to quit.
	nc_server_stop
}
