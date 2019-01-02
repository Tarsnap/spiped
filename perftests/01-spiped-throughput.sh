#!/bin/sh

### Constants
total=$( echo "2^24" | bc )
reps=$( seq 3 )

# Output log files
output_net_direct=${s_basename}-direct-net.data
output_file_direct=${s_basename}-direct-file.data
output_net_spiped=${s_basename}-spiped-net.data
output_file_spiped=${s_basename}-spiped-file.data

# Sockets
src_sock_net=[127.0.0.1]:8001
src_sock_file="${PWD}/${s_basename}-src.sock"
mid_sock_net=[127.0.0.1]:8002
mid_sock_file="${PWD}/${s_basename}-mid.sock"
dst_sock_net=[127.0.0.1]:8003
dst_sock_file="${PWD}/${s_basename}-dst.sock"

# Write metadata in python's YAML format for plotting
write_metadata() {
	mb=$((${total}/1000000))
	printf "[METADATA]\n"
	printf "title = Direct vs. spiped, sending ${mb} MB\n"
	printf "basenames ="
	for filename in ${output_net_direct} ${output_file_direct}	\
			${output_net_spiped} ${output_file_spiped} ; do
		printf " $( basename ${filename} )"
	done
	printf "\n"
	printf "shortnames = direct-net direct-file spiped-net spiped-file\n"
	printf "columns = blocksize count time_(s) speed_Mb/s\n"
}

# Test bandwidth without spiped
run_direct() {
	blocksize=$1
	outfile=$2
	this_sock=$3

	# Set up server(s)
	check_leftover_servers_perftests
	setup_check_variables
	${recv_zeros} ${this_sock} ${blocksize} &
	sleep 1

	# Send data
	count=$((${total}/${blocksize}))
	${send_zeros} ${this_sock} ${blocksize} ${count} >> ${outfile}
	echo $? > ${c_exitfile}

	# Wait for server(s) to quit.
	servers_stop_perftests
}

# Test bandwidth with spiped
run_through_spiped() {
	blocksize=$1
	outfile=$2
	src_sock=$3
	mid_sock=$4
	dst_sock=$5

	# Set up server(s)
	check_leftover_servers_perftests
	setup_check_variables
	${recv_zeros} ${dst_sock} ${blocksize} &
	sleep 1
	setup_spiped_decryption_server /dev/null 0 0
	setup_spiped_encryption_server

	# Send data
	count=$((${total}/${blocksize}))
	${send_zeros} ${src_sock} ${blocksize} ${count} >> ${outfile}
	echo $? > ${c_exitfile}

	# Wait for server(s) to quit.
	servers_stop_perftests
}

# For a given blocksize, time the direct and spiped connections
run_set() {
	blocksize=$1

	# Direct connection, file
	run_direct ${blocksize} ${output_file_direct} ${dst_sock_file}
	rm ${dst_sock_file}

	# Direct connection, network
	run_direct ${blocksize} ${output_net_direct} ${dst_sock_net}

	# Connect via two spiped processes, file
	run_through_spiped ${blocksize} ${output_file_spiped}		\
		${src_sock_file} ${mid_sock_file} ${dst_sock_file}
	rm ${src_sock_file}
	rm ${mid_sock_file}
	rm ${dst_sock_file}

	# Connect via two spiped processes, network
	run_through_spiped ${blocksize} ${output_net_spiped}		\
		${src_sock_net} ${mid_sock_net} ${dst_sock_net}
}

### Actual command
scenario_cmd() {
	# Write metadata for plotting
	write_metadata > ${s_basename}.meta

	# Save the network sockets
	src_sock_orig=${src_sock}
	mid_sock_orig=${mid_sock}
	dst_sock_orig=${dst_sock}

	# Run tests
	for rep in ${reps}; do
		for blocksize in 1024 2048 4096 8192 16384 32768 65536; do
			run_set ${blocksize}
			printf "."
		done
	done
	printf " "

	# Restore the network sockets
	src_sock=${src_sock_orig}
	mid_sock=${mid_sock_orig}
	dst_sock=${dst_sock_orig}
}
