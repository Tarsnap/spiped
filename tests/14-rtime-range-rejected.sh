#!/bin/sh

# Goal of this test:
# - confirm non-finite and excessively large DNS re-resolution intervals are
#   rejected during command-line parsing instead of reaching dispatch timers.

scenario_cmd() {
	spiped_stderr="${s_basename}.spiped.stderr"

	for rtime in Infinity 1000000001; do
		setup_check "reject spiped -r ${rtime} outside the safe interval"
		if "${spiped_binary}" -e -F \
		    -s "${s_basename}.src.sock" \
		    -t "${s_basename}.dst.sock" \
		    -k "${s_basename}.missing-key" \
		    -r "${rtime}" > /dev/null 2> "${spiped_stderr}"; then
			exitcode=0
		else
			exitcode=$?
		fi
		if [ "${exitcode}" -eq 1 ] && \
		    grep -q "Error parsing argument: -r ${rtime}" \
		        "${spiped_stderr}"; then
			echo 0 > "${c_exitfile}"
		else
			echo 1 > "${c_exitfile}"
		fi
		rm -f "${spiped_stderr}"
	done

	rm -f "${spiped_stderr}"
}
