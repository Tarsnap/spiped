#!/bin/sh

# Goal of this test:
# - enforce the documented mutual exclusion between -r <rtime> and -R.
# - cover the two values which used to bypass the check because option
#   validation compared opt_r against its default instead of opt_r_set.
# - verify that either option still passes option validation by itself.

### Constants
c_valgrind_min=1
missing_key="${s_basename}-missing-key"

### Actual command
scenario_cmd() {
	rm -f "${missing_key}"

	# These combinations are forbidden by the manual page.  A missing key is
	# deliberate: before the fix, the option check was bypassed and execution
	# continued far enough to report a key-read failure instead of usage.
	setup_check "spiped rejects -r 60 -R"
	${c_valgrind_cmd} "${spiped_binary}" -e -F			\
	    -s "${src_sock}" -t "${dst_sock}" -k "${missing_key}"	\
	    -r 60 -R 2>&1 | grep -q "^usage: spiped "
	echo $? > "${c_exitfile}"

	setup_check "spiped rejects -R -r 60"
	${c_valgrind_cmd} "${spiped_binary}" -e -F			\
	    -s "${src_sock}" -t "${dst_sock}" -k "${missing_key}"	\
	    -R -r 60 2>&1 | grep -q "^usage: spiped "
	echo $? > "${c_exitfile}"

	setup_check "spiped rejects -r 0 -R"
	${c_valgrind_cmd} "${spiped_binary}" -e -F			\
	    -s "${src_sock}" -t "${dst_sock}" -k "${missing_key}"	\
	    -r 0 -R 2>&1 | grep -q "^usage: spiped "
	echo $? > "${c_exitfile}"

	setup_check "spiped rejects -R -r 0"
	${c_valgrind_cmd} "${spiped_binary}" -e -F			\
	    -s "${src_sock}" -t "${dst_sock}" -k "${missing_key}"	\
	    -R -r 0 2>&1 | grep -q "^usage: spiped "
	echo $? > "${c_exitfile}"

	# Controls: each option by itself remains valid at the option-validation
	# layer and therefore reaches the deliberately missing key.
	setup_check "spiped accepts -r 60 option by itself"
	${c_valgrind_cmd} "${spiped_binary}" -e -F			\
	    -s "${src_sock}" -t "${dst_sock}" -k "${missing_key}"	\
	    -r 60 2>&1 | grep -q "Error reading shared secret"
	echo $? > "${c_exitfile}"

	setup_check "spiped accepts -R option by itself"
	${c_valgrind_cmd} "${spiped_binary}" -e -F			\
	    -s "${src_sock}" -t "${dst_sock}" -k "${missing_key}"	\
	    -R 2>&1 | grep -q "Error reading shared secret"
	echo $? > "${c_exitfile}"
}
