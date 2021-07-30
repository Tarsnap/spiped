#!/bin/sh

# Goal of this test:
# - ensure that spipe can close the sending side of the TCP
#   connection, while the receiving side keeps working.

### Constants
c_valgrind_min=1
echo_limited_output="${s_basename}-echo-limited-output.txt"
sendfile=${scriptdir}/shared_test_functions.sh

### Actual command
scenario_cmd() {
	# Set up an echo server, limited to 1000 bytes per second.
	setup_spiped_decryption_server /dev/null 0 1 1000

	# Send a file through the rate-limited echo server.
	setup_check_variables "spipe send rate-limited"
	${c_valgrind_cmd}					\
		${spipe_binary} -t ${mid_sock} -k /dev/null	\
		< ${sendfile}					\
		> ${echo_limited_output}
	echo $? > ${c_exitfile}

	setup_check_variables "spipe send rate-limited output"
	cmp -s ${sendfile} ${echo_limited_output}
	echo $? > ${c_exitfile}

	# Clean up
	servers_stop
}
