#!/bin/sh

# Goal of this test:
# - establish a connection to a spiped server.
# - send data via spipe
# - server should quit (because we gave it -1).
# - the received file should match lorem-send.txt

### Constants
ncat_output="${s_basename}-ncat-output.txt"

### Actual command
scenario_cmd() {
	# Set up infrastructure.
	setup_check_variables
	setup_spiped_decryption_server ${ncat_output}

	# Send data.
	setup_check_variables
	(
		cat ${scriptdir}/lorem-send.txt | ${spipe_binary}	\
			-t [127.0.0.1]:${mid_port} -k /dev/null
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
