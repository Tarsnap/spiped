#!/bin/sh

# Goal of this test:
# - create a pair of spiped servers (encryption, decryption).
# - open two connections, but don't send anything,
# - close one of the connections.

### Constants
c_valgrind_min=1
flag_1="${s_basename}-1.flag"
flag_2="${s_basename}-2.flag"


### Actual command
scenario_cmd() {
	# Set up infrastructure.
	setup_spiped_decryption_server
	setup_spiped_encryption_server

	# Open and close a connection, keeping it open for 2 seconds.
	setup_check "multi open 2 seconds close"
	(
		( echo "" ; sleep 2 ) |		\
			 ${nc_client_binary} "${src_sock}"
		echo $? > "${c_exitfile}"
		touch "${flag_1}"
	) &

	# Open another connection (before the first has closed).
	# Don't check that we've already finished the previous test.
	setup_check "multi open close" 0
	(
		echo "" | ${nc_client_binary} "${src_sock}"
		echo $? > "${c_exitfile}"
		touch "${flag_2}"
	) &

	# Wait for both connections to have closed.
	wait_while 0 test ! -e "${flag_1}"
	wait_while 0 test ! -e "${flag_2}"

	# Wait for server(s) to quit.
	servers_stop
}
