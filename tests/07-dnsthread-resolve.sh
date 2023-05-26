#!/bin/sh

# Goal of this test:
# - ensure that dnsquery can find simple addresses

### Constants
c_valgrind_min=1
addr_output="${s_basename}-addrs.txt"
# These don't need a network connection to resolve
addrs_local="	localhost:80
	[1.2.3.4]:80
	[1:2:3:4:0:a:b:c]:80
	[1:2:3:4::a:b:c]:80
	[::1]:80
	/tmp/sock"
# These need a network connection to resolve
addrs_network="	google.ca:80
	tarsnap.com:80"

### Actual command
scenario_cmd() {
	if [ "${TEST_DNSTHREAD_ENABLE_REMOTE:-0}" -gt 0 ]
	then
		addrs="${addrs_local} ${addrs_network}"
	else
		addrs="${addrs_local}"
	fi

	for addr in ${addrs}
	do
		setup_check_variables "dnsthread-resolve ${addr}"
		${c_valgrind_cmd}				\
		"${dnsthread_resolve}" "${addr}" >> "${addr_output}"
		echo $? > "${c_exitfile}"
	done

	# Wait for server(s) to quit.
	servers_stop
}
