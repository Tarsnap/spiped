#!/bin/sh

# Goal of this test:
# - establish a connection to a spiped server.
# - send data via spipe
# - the received file should match lorem-send.txt

### Constants
c_valgrind_min=1
ncat_output="${s_basename}-ncat-output.txt"

### Actual command
scenario_cmd() {
	# Set up infrastructure.
	setup_spiped_decryption_server ${ncat_output}

	# Send data.
	setup_check_variables "spipe send"
	(
		${c_valgrind_cmd} ${spipe_binary}		\
			-t ${mid_sock} -k /dev/null		\
			< ${scriptdir}/lorem-send.txt
		echo $? > ${c_exitfile}
	)

	# Wait for server(s) to quit.
	servers_stop

	# Check output.  This must be after nc-server has stopped, to
	# ensure that no data is buffered and not yet written to disk.
	setup_check_variables "spipe send output"
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
