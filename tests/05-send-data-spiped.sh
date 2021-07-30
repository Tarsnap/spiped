#!/bin/sh

# Goal of this test:
# - create a pair of spiped servers (encryption, decryption)
# - establish a connection to the encryption spiped server
# - open one connection, send a file, close the connection
# - the received file should match the original one

### Constants
c_valgrind_min=1
ncat_output="${s_basename}-ncat-output.txt"
sendfile=${scriptdir}/lorem-send.txt

### Actual command
scenario_cmd() {
	# Set up infrastructure.
	setup_spiped_decryption_server ${ncat_output}
	setup_spiped_encryption_server

	# Open and close a connection.
	setup_check_variables "spiped send"
	(
		${nc_client_binary} ${src_sock} < ${sendfile}
		echo $? > ${c_exitfile}
	)

	# Wait for server(s) to quit.
	servers_stop

	setup_check_variables "spiped send output"
	if ! cmp -s ${ncat_output} ${sendfile}; then
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
