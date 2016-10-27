#!/bin/sh

# This test script requires three ports.
src_port=8000
mid_port=8001
dst_port=8002

# Constants
out="output-tests-spiped"

# Timing commands (in seconds).
sleep_ncat_start=1
sleep_spiped_start=2
sleep_ncat_stop=1
sleep_ncat_timeout_stop=3

################################ Setup variables from the command-line

# Find script directory and load helper functions.
scriptdir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
. $scriptdir/shared_test_functions.sh

# Find relative spiped binary paths.
spiped_binary=$scriptdir/../spiped/spiped
spipe_binary=$scriptdir/../spipe/spipe

# Find system spiped if it supports -1.
system_spiped_binary=`which spiped`
if [ -n "$system_spiped_binary" ]; then
	if $system_spiped_binary -1 2>&1 >/dev/null | grep -qE "(invalid|illegal) option"; then
		# Disable test.
		system_spiped_binary=""
	fi
fi

# Check for required commands.
if ! command -v $spiped_binary >/dev/null 2>&1; then
	echo "spiped not detected; did you forget to run 'make all'?"
	exit 1
fi
if ! command -v $spipe_binary >/dev/null 2>&1; then
	echo "spiped not detected; did you forget to run 'make all'?"
	exit 1
fi
if ! command -v ncat >/dev/null 2>&1; then
	echo "ncat not detected; it is part of the nmap suite"
	echo "	(it is required for multiple sockets on the same port)"
	exit 1
fi

################################ Helper functions

check_leftover_ncat_server() {
	# Repeated testing, especially when doing ctrl-c to break out of
	# (suspected) hanging, can leave a ncat server floating around, which
	# is problematic for the next testing run.  Checking for this
	# shouldn't be necessary for normal testing (as opposed to test-script
	# development), but there's no harm in checking anyway.
	leftover=""

	# Find old ncat server on $dst_port.
	cmd="ncat -k -l.* $dst_port"
	oldpid=`ps -Aopid,command | grep "$cmd" | grep -v "grep" | awk '{print $1}'`
	if [ ! -z $oldpid ]; then
		echo "Error: Left-over server from previous run: pid= $oldpid"
		leftover="1"
	fi

	# Early exit if any previous servers found.
	if [ ! -z $leftover ]; then
		echo "Exit from left-over servers"
		exit 1
	fi
}

################################ Server setup

## setup_spiped_decryption_server(basename, use_system_spiped=0):
# Set up a spiped decryption server, translating from $mid_port to $dst_port,
# saving the exit code to $out/basename-d.exit.  Also set up an ncat server
# listening to $dst_port, saving output to $out/$basename.txt.  Uses the
# system's spiped (instead of the version in this source tree) if
# $use_system_spiped is 1.
setup_spiped_decryption_server () {
	basename=$1
	use_system_spiped=${2:-0}
	check_leftover_ncat_server

	# Select system or compiled spiped.
	if [ "$use_system_spiped" -gt 0 ]; then
		spiped_cmd=$system_spiped_binary
	else
		spiped_cmd=$spiped_binary
	fi

	# Start backend server.
	nc_pid="$(dst_port=$dst_port outfile=$out/$basename.txt sh -c \
		'ncat -k -l -o $outfile $dst_port >/dev/null 2>&1 & \
		echo ${!}' )"
	sleep $sleep_ncat_start

	# Start spiped to connect source port to backend.
	( $spiped_cmd -d \
	 	-s 127.0.0.1:$mid_port -t 127.0.0.1:$dst_port \
		-k $scriptdir/keys-blank.txt -F -1 -o 1 \
	; echo $? >> $out/$basename-d.exit ) &
	sleep $sleep_spiped_start
}

## setup_spiped_decryption_server(basename):
# Set up a spiped encryption server, translating from $src_port to $mid_port,
# saving the exit code to $out/basename-e.exit.
setup_spiped_encryption_server () {
	basename=$1

	# Start spiped to connect source port to backend.
	( $spiped_binary -e \
	 	-s 127.0.0.1:$src_port -t 127.0.0.1:$mid_port \
		-k $scriptdir/keys-blank.txt -F -1 -o 1 \
	; echo $? >> $out/$basename-e.exit ) &
	sleep $sleep_spiped_start
}

################################ Test functions

test_connection_open_close_single () {
	# Goal of this test:
	# - establish a connection to a spiped server.
	# - open a connection, but don't send anything.
	# - close the connection.
	# - server should quit (because we gave it -1).
	basename="01_connection_open_close_single"
	printf "Running test: $basename... "

	# Set up infrastructure.
	setup_spiped_decryption_server $basename

	# Run test.
	echo "" | nc 127.0.0.1 $mid_port >/dev/null
	cmd_retval=$?

	# Wait for server(s) to quit.
	sleep $sleep_ncat_stop
	kill $nc_pid
	wait

	# Check results.
	retval=$cmd_retval
	retval_d=`cat $out/$basename-d.exit`
	if [ ! "$retval_d" -eq 0 ]; then
		retval=1
	fi

	# Print PASS or FAIL, and return result.
	notify_success_or_fail $out/$basename ""
	return "$retval"
}

test_connection_open_timeout_single () {
	# Goal of this test:
	# - establish a connection to a spiped server.
	# - open a connection, but don't send anything.
	# - wait; the connection should be closed automatically (because we
	#   gave it -o 1 and $sleep_ncat_timeout_stop is longer than
	#   that plus $sleep_spiped_start).
	# - server should quit (because we gave it -1).
	basename="02_connection_open_timeout_single"
	printf "Running test: $basename... "

	# Set up infrastructure.
	setup_spiped_decryption_server $basename

	# Run test.  Awkwardly force nc to keep the connection open; the
	# simple "nc -q 2 ..." to wait 2 seconds isn't portable.
	( ( echo ""; sleep 2 ) | nc 127.0.0.1 $mid_port ) >/dev/null &

	# Wait for server(s) to quit.
	sleep $sleep_ncat_timeout_stop
	kill $nc_pid
	wait

	# Check results.
	retval=0
	retval_d=`cat $out/$basename-d.exit`
	if [ ! "$retval_d" -eq 0 ]; then
		retval=1
	fi

	# Print PASS or FAIL, and return result.
	notify_success_or_fail $out/$basename ""
	return "$retval"
}

test_connection_open_close_double () {
	# Goal of this test:
	# - create a pair of spiped servers (encryption, decryption).
	# - open two connections, but don't send anything,
	# - close one of the connections.
	# - server should quit (because we gave it -1).
	basename="03_connection_open_close_double"
	printf "Running test: $basename... "

	# Set up infrastructure.
	setup_spiped_decryption_server $basename
	setup_spiped_encryption_server $basename

	# Run test.  Awkwardly force nc to keep the connection open; the
	# simple "nc -q 2 ..." to wait 2 seconds isn't portable.
	( ( echo ""; sleep 2 ) | nc 127.0.0.1 $src_port ) &
	echo "" | nc 127.0.0.1 $src_port

	# Wait for server(s) to quit.
	sleep $sleep_ncat_stop
	kill $nc_pid
	wait

	# Check results.
	retval=0
	retval_d=`cat $out/$basename-d.exit`
	retval_e=`cat $out/$basename-e.exit`
	if [ ! "$retval_d" -eq 0 ] || [ ! "$retval_e" -eq 0 ]; then
		retval=1
	fi

	# Print PASS or FAIL, and return result.
	notify_success_or_fail $out/$basename ""
	return "$retval"
}

test_send_data_spipe () {
	# Goal of this test:
	basename="04_send_data_spipe"
	printf "Running test: $basename... "

	# Set up infrastructure.
	setup_spiped_decryption_server $basename

	# Run test.
	cat $scriptdir/lorem-send.txt | $spipe_binary \
		-t 127.0.0.1:$mid_port \
		-k $scriptdir/keys-blank.txt
	cmd_retval=$?

	# Wait for server(s) to quit.
	sleep $sleep_ncat_stop
	kill $nc_pid
	wait

	# Check results.
	retval=$cmd_retval
	retval_d=`cat $out/$basename-d.exit`
	if [ ! "$retval_d" -eq 0 ]; then
		retval=1
	fi
	if ! cmp -s $scriptdir/lorem-send.txt "$out/$basename.txt" ; then
		retval=1
	fi

	# Print PASS or FAIL, and return result.
	notify_success_or_fail $out/$basename ""
	return "$retval"
}


test_send_data_spiped () {
	# Goal of this test:
	# - create a pair of spiped servers (encryption, decryption)
	# - establish a connection to the encryption spiped server
	# - open one connection, send lorem-send.txt, close the connection
	# - server should quit (because we gave it -1)
	# - the received file should match lorem-send.txt
	basename="05_send_data_spiped"
	printf "Running test: $basename... "

	# Set up infrastructure.
	setup_spiped_decryption_server $basename
	setup_spiped_encryption_server $basename

	# Run test.
	cat $scriptdir/lorem-send.txt | nc 127.0.0.1 $src_port
	cmd_retval=$?

	# Wait for server(s) to quit.
	sleep $sleep_ncat_stop
	kill $nc_pid
	wait

	# Check results.
	retval=$cmd_retval
	retval_d=`cat $out/$basename-d.exit`
	retval_e=`cat $out/$basename-e.exit`
	if [ ! "$retval_d" -eq 0 ] || [ ! "$retval_e" -eq 0 ]; then
		retval=1
	fi
	if ! cmp -s $scriptdir/lorem-send.txt "$out/$basename.txt" ; then
		retval=1
	fi

	# Print PASS or FAIL, and return result.
	notify_success_or_fail $out/$basename ""
	return "$retval"
}


test_send_data_system_spiped () {
	# Goal of this test:
	# - create a pair of spiped servers (encryption, decryption), where
	#   the decryption server uses the system-installed spiped binary
	# - establish a connection to the encryption spiped server
	# - open one connection, send lorem-send.txt, close the connection
	# - server should quit (because we gave it -1)
	# - the received file should match lorem-send.txt
	basename="06-send-data-system_spiped"
	printf "Running test: $basename... "
	if [ ! -n "$system_spiped_binary" ]; then
		echo "SKIP; no system spiped, or it is too old"
		return;
	fi

	# Set up infrastructure.
	setup_spiped_decryption_server $basename 1
	setup_spiped_encryption_server $basename

	# Run test.
	cat $scriptdir/lorem-send.txt | nc 127.0.0.1 $src_port
	cmd_retval=$?

	# Wait for server(s) to quit.
	sleep $sleep_ncat_stop
	kill $nc_pid
	wait

	# Check results.
	retval=$cmd_retval
	retval_d=`cat $out/$basename-d.exit`
	retval_e=`cat $out/$basename-e.exit`
	if [ ! "$retval_d" -eq 0 ] || [ ! "$retval_e" -eq 0 ]; then
		retval=1
	fi
	if ! cmp -s $scriptdir/lorem-send.txt "$out/$basename.txt" ; then
		retval=1
	fi

	# Print PASS or FAIL, and return result.
	notify_success_or_fail $out/$basename ""
	if [ ! "$retval_d" -eq 0 ]; then
		echo "WARNING: Error found in system spiped"
	fi
	return "$retval"
}


####################################################

# Ensure clean test directory.
if [ -d "$out" ]; then
	rm -rf $out
fi
mkdir -p $out

# Run tests.
test_connection_open_close_single &&		\
	test_connection_open_timeout_single &&	\
	test_connection_open_close_double &&	\
	test_send_data_spipe &&			\
	test_send_data_spiped &&		\
	test_send_data_system_spiped
result=$?

# Return value to Makefile.
exit $result
