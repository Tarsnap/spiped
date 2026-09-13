#!/bin/sh

# Goal of this test:
# - confirm that explicitly supplying a zero connection timeout is not
#   mistaken for an omitted option and replaced by the 5 second default.

scenario_cmd() {
	spipe_stderr="${s_basename}.spipe.stderr"
	spiped_stderr="${s_basename}.spiped.stderr"

	setup_check "reject explicit spipe -o 0"
	if "${spipe_binary}" \
	    -t "${s_basename}.dst.sock" \
	    -k "${s_basename}.missing-key" \
	    -o 0 > /dev/null 2> "${spipe_stderr}"; then
		exitcode=0
	else
		exitcode=$?
	fi
	if [ "${exitcode}" -eq 1 ] && \
	    grep -q '^usage: spipe ' "${spipe_stderr}"; then
		echo 0 > "${c_exitfile}"
	else
		echo 1 > "${c_exitfile}"
	fi

	setup_check "reject explicit spiped -o 0"
	if "${spiped_binary}" -e -F \
	    -s "${s_basename}.src.sock" \
	    -t "${s_basename}.dst.sock" \
	    -k "${s_basename}.missing-key" \
	    -o 0 > /dev/null 2> "${spiped_stderr}"; then
		exitcode=0
	else
		exitcode=$?
	fi
	if [ "${exitcode}" -eq 1 ] && \
	    grep -q '^usage: spiped ' "${spiped_stderr}"; then
		echo 0 > "${c_exitfile}"
	else
		echo 1 > "${c_exitfile}"
	fi

	rm -f "${spipe_stderr}" "${spiped_stderr}"
}
