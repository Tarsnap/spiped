#!/bin/sh

### Find script directory and load helper functions.
scriptdir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
. ${scriptdir}/../tests/shared_test_functions.sh


### Project-specific constants and setup

out="perftests-output"
out_valgrind="perftests-valgrind"

# This test script requires three sockets.
src_sock=[127.0.0.1]:8001
mid_sock=[127.0.0.1]:8002
dst_sock=[127.0.0.1]:8003

# Find relative spiped binary paths.
spiped_binary=${scriptdir}/../spiped/spiped
spipe_binary=${scriptdir}/../spipe/spipe
send_zeros=${scriptdir}/send-zeros/send-zeros
recv_zeros=${scriptdir}/recv-zeros/recv-zeros

# Find system spiped
system_spiped_binary=$( find_system spiped )

# Check for required commands.
if ! command -v ${spiped_binary} >/dev/null 2>&1; then
	echo "spiped not detected; did you forget to run 'make all'?"
	exit 1
fi
if ! command -v ${spipe_binary} >/dev/null 2>&1; then
	echo "spiped not detected; did you forget to run 'make all'?"
	exit 1
fi

# Functions to help start and stop servers
. ${scriptdir}/../tests/spiped_servers.sh
. ${scriptdir}/spiped_servers_perftests.sh


### Run tests using project-specific constants
run_scenarios ${scriptdir}/??-*.sh
