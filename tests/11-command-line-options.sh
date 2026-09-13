#!/bin/sh

# Goal of this test:
# - verify that options which are documented as mutually exclusive are
#   rejected even when one of them is given its default numeric value.

### Constants
c_valgrind_min=9
rtime_sock="${s_basename}-rtime.sock"
rtime_pid="${s_basename}-rtime.pid"


### Actual command
scenario_cmd() {
	setup_check "reject -r 60 with -R"
	"${spiped_binary}" -d				\
	    -s "${rtime_sock}" 			\
	    -t "${dst_sock}" 			\
	    -k /dev/null				\
	    -p "${rtime_pid}" 			\
	    -r 60 -R 2>/dev/null
	rc=$?
	expected_exitcode 1 "${rc}" > "${c_exitfile}"

	# An unfixed binary daemonizes successfully.  Clean it up so that a
	# deliberately failing regression run does not leave a process behind.
	if [ -e "${rtime_pid}" ]; then
		kill "$(cat "${rtime_pid}")" 2>/dev/null || true
		rm -f "${rtime_pid}"
	fi
	rm -f "${rtime_sock}"
}
