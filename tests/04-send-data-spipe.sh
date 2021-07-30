#!/bin/sh

# Goal of this test:
# - establish a connection to a spiped server.
# - send data via spipe
# - the received file should match the original one
# - also, check that we quit immediately if we can't connect

### Constants
c_valgrind_min=1
ncat_output="${s_basename}-ncat-output.txt"
bad_target_stderr="${s_basename}-bad-target-stderr.txt"
sendfile=${scriptdir}/lorem-send.txt

### Actual command
scenario_cmd() {
	# Set up infrastructure.
	setup_spiped_decryption_server ${ncat_output}

	# Send data.
	setup_check_variables "spipe send"
	(
		${c_valgrind_cmd} ${spipe_binary}		\
			-t ${mid_sock} -k /dev/null		\
			< ${sendfile}
		echo $? > ${c_exitfile}
	)

	# Wait for server(s) to quit.
	servers_stop

	# Check output.  This must be after nc-server has stopped, to
	# ensure that no data is buffered and not yet written to disk.
	setup_check_variables "spipe send output"
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

	# Disable normal valgrind use: the "spipe fail bad target" test
	# invokes an `exit(1)` in pushbits.c, which does not attempt to
	# clean up after itself.
	c_valgrind_min=2

	# Should quit immediately if the target fails to connect.
	setup_check_variables "spipe fail bad target"
	${c_valgrind_cmd} ${spipe_binary}		\
		-t /this-is-a-fake.socket -k /dev/null	\
		2> "${bad_target_stderr}"
	expected_exitcode 1 $? > ${c_exitfile}
}
