#!/bin/sh

# Goal of this test:
# - create a pair of spiped servers (encryption, decryption), where
#   the decryption server uses the system-installed spiped binary
# - establish a connection to the encryption spiped server
# - open one connection, send lorem-send.txt, close the connection
# - server should quit (because we gave it -1)
# - the received file should match lorem-send.txt

### Constants
ncat_output="${s_basename}-ncat-output.txt"

### Actual command
scenario_cmd() {
	if [ ! -n "${system_spiped_binary}" ]; then
		echo "SKIP; no system spiped, or it is too old"
		# Suppress warning
		setup_check_variables
		echo "-1" > ${c_exitfile}
		return;
	fi

	# Set up infrastructure.
	setup_check_variables
	setup_spiped_decryption_server ${ncat_output} 1

	setup_check_variables
	setup_spiped_encryption_server

	# Open and close a connection.
	setup_check_variables
	(
		cat ${scriptdir}/lorem-send.txt | nc 127.0.0.1 ${src_port}
		echo $? > ${c_exitfile}
	)

	# Wait for server(s) to quit.
	nc_server_stop

	setup_check_variables
	if ! cmp -s ${ncat_output} ${scriptdir}/lorem-send.txt; then
		echo 1
	else
		echo 0
	fi > ${c_exitfile}
}
