#!/bin/sh

# Goal of this test:
# - establish a connection to a spiped server.
# - send data via spipe
# - the received file should match lorem-send.txt

### Constants
ncat_output="${s_basename}-ncat-output.txt"

### Actual command
scenario_cmd() {
	# Set up infrastructure.
	setup_spiped_decryption_server ${ncat_output}

	# Send data.
	setup_check_variables
	(
		cat ${scriptdir}/lorem-send.txt | ${spipe_binary}	\
			-t ${mid_sock} -k /dev/null
		echo $? > ${c_exitfile}
	)

	# Wait for server(s) to quit.
	servers_stop

	setup_check_variables
	if ! cmp -s ${ncat_output} ${scriptdir}/lorem-send.txt; then
		if [ ${VERBOSE} -ne 0 ]; then
			printf "Test output does not match input;" 1>&2
			printf -- " output is:\n----\n" 1>&2
			cat ${ncat_output} 1>&2
			printf -- "----\n" 1>&2
		fi
		echo 1
	else
		echo 0
	fi > ${c_exitfile}
}
