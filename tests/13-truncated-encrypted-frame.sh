#!/bin/sh

# Goal of this test:
# - exercise proto_pipe() decrypt-side EOF handling directly
# - accept EOF only on an encrypted packet boundary
# - reject a truncated final encrypted packet as a connection error

### Constants
c_valgrind_min=1
standalone_enc=${scriptdir}/../perftests/standalone-enc/test_standalone_enc

### Actual command
scenario_cmd() {
	setup_check "truncated encrypted frame EOF"
	(
		${c_valgrind_cmd} "${standalone_enc}" 8
		echo $? > "${c_exitfile}"
	)
}
