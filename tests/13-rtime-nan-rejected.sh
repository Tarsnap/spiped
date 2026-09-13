#!/bin/sh

# Goal of this test:
# - confirm NaN is rejected by PARSENUM instead of reaching dispatch as a
#   non-positive re-resolution interval.

scenario_cmd() {
	spiped_stderr="${s_basename}.spiped.stderr"

	setup_check "reject spiped -r NaN during parse"
	if "${spiped_binary}" -e -F \
	    -s "${s_basename}.src.sock" \
	    -t "${s_basename}.dst.sock" \
	    -k "${s_basename}.missing-key" \
	    -r NaN > /dev/null 2> "${spiped_stderr}"; then
		exitcode=0
	else
		exitcode=$?
	fi
	if [ "${exitcode}" -eq 1 ] && \
	    grep -q 'Error parsing argument: -r NaN' "${spiped_stderr}"; then
		echo 0 > "${c_exitfile}"
	else
		echo 1 > "${c_exitfile}"
	fi

	rm -f "${spiped_stderr}"
}
