#!/bin/sh

# Goal of this test:
# - create a pair of spiped servers (encryption, decryption).
# - open two connections, but don't send anything,
# - close one of the connections.
# - server should quit (because we gave it -1).

### Constants


### Actual command
scenario_cmd() {
	# Set up infrastructure.
	setup_spiped_decryption_server
	setup_spiped_encryption_server

	# Open and close a connection.  Awkwardly force nc to keep
	# the connection open; the simple "nc -q 2 ..." to wait 2
	# seconds isn't portable.
	setup_check_variables
	(
		( echo "" ; sleep 2 ) | nc 127.0.0.1 ${src_port} >/dev/null
		echo $? > ${c_exitfile}
	) &

	setup_check_variables
	(
		echo "" | nc 127.0.0.1 ${src_port} >/dev/null
		echo $? > ${c_exitfile}
	)
	sleep 3

	# Wait for server(s) to quit.
	servers_stop
}
