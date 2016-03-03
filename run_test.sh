#!/bin/bash

TEST_CMD='./mem_test'

handle_return_code ()
{
	if [[ $1 -eq 0 ]]; then
		echo "OK";
	else
		echo "FAIL";
	fi;
}

do_test ()
{
	echo -en " > $1\t"
	"$TEST_CMD" "$1" > /dev/null
	handle_return_code $?
}

do_tests ()
{
	do_test 'stack'
	do_test 'stack-exec'
	do_test 'malloc-rw'
	do_test 'malloc-rw-x'
	do_test 'mmap-rw'
	do_test 'mmap-rw-x'
	do_test 'mmap-rwx'
	do_test 'shmget-rw'
	do_test 'shmget-rw-x'
	do_test 'shmget-rwx'
	echo ""
}

# ------------------------------------------------------------------------ #

if [ ! -f ${TEST_CMD} ]; then
	echo "Test command ${TEST_CMD} not found! Try run 'make' first"
	exit 1
fi

echo """Legend:
OK - code executed from memory (it's bad)
FAIL - can't execute code from memory (it's good)
"""

echo "==== bit: RW ===="
execstack -c "$TEST_CMD"
do_tests

echo "==== bit: RWE ==== (mark binary as requiring executable stack)"
execstack -s "$TEST_CMD"
do_tests


