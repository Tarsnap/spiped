#!/bin/sh

# Goal of this test:
# - confirm that -r and -R are mutually exclusive even when the explicitly
#   supplied rtime is the default value (60 seconds).

scenario_cmd() {
	stderr_file="${s_basename}.stderr"

	setup_check "reject -r 60 together with -R"
	if "${spiped_binary}" -e -F \
	    -s "${s_basename}.src.sock" \
	    -t "${s_basename}.dst.sock" \
	    -k "${s_basename}.missing-key" \
	    -r 60 -R > /dev/null 2> "${stderr_file}"; then
		exitcode=0
	else
		exitcode=$?
	fi

	if [ "${exitcode}" -eq 1 ] && \
	    grep -q '^usage: spiped ' "${stderr_file}"; then
		echo 0 > "${c_exitfile}"
	else
		echo 1 > "${c_exitfile}"
	fi

	rm -f "${stderr_file}"
}
