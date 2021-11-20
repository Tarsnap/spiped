#!/bin/sh

### Constants
c_valgrind_min=1
cmd="${scriptdir}/pthread_create_blocking_np/test_pthread_create_blocking_np"

### Actual command
scenario_cmd() {
	# Copy a file
	setup_check_variables "test_pthread_create_blocking_np"
	${c_valgrind_cmd} ${cmd}
	echo $? > ${c_exitfile}
}
