#!/bin/sh

### Find script directory and load helper functions.
scriptdir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
. ${scriptdir}/shared_test_functions.sh


### Project-specific constants and setup

out="${bindir}/tests-output"
out_valgrind="${bindir}/tests-valgrind"

# This test script requires three sockets.
src_sock=[127.0.0.1]:8001
mid_sock=[127.0.0.1]:8002
dst_sock=[127.0.0.1]:8003

# Find relative spiped binary paths.
spiped_binary=${scriptdir}/../spiped/spiped
spipe_binary=${scriptdir}/../spipe/spipe
nc_client_binary=${scriptdir}/../tests/nc-client/nc-client
nc_server_binary=${scriptdir}/../tests/nc-server/nc-server
dnsthread_resolve=${scriptdir}/../tests/dnsthread-resolve/dnsthread-resolve

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
. ${scriptdir}/spiped_servers.sh


### Run tests using project-specific constants
run_scenarios ${scriptdir}/??-*.sh
