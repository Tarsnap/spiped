#!/bin/sh

## notify_success_or_fail (retval):
# Print "PASSED!" or "FAILED!" based on $retval.
notify_success_or_fail() {
	retval=$1

	if [ "$retval" -eq 0 ]; then
		echo "PASSED!"
	else
		echo "FAILED!"
	fi
}
