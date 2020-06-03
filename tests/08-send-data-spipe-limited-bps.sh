#!/bin/sh

# Goal of this test:
# - ensure that spipe can close the sending side of the TCP
#   connection, while the receiving side keeps working.

### Constants
c_valgrind_min=1
echo_limited_output="${s_basename}-echo-limited-output.txt"

### Actual command
scenario_cmd() {
	# Send a file through a rate-limited echo server.  Echo
	# server is limited to 1000 bytes per second.
	setup_check_variables "spipe send rate-limited"
	setup_spiped_decryption_server /dev/null 0 1 1000
	cat ${scriptdir}/lorem-send.txt				\
		| ${c_valgrind_cmd}				\
		${spipe_binary} -t ${mid_sock} -k /dev/null	\
		> ${echo_limited_output}
	echo $? > ${c_exitfile}

	setup_check_variables "spipe send rate-limited output"
	cmp -s ${scriptdir}/lorem-send.txt ${echo_limited_output}
	echo $? > ${c_exitfile}

	# Clean up
	servers_stop
}
