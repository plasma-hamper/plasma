#!/bin/bash

# Handle internal valgrind
IV=""
if [ "$#" == "2" ]; then
    if [ "$1" == "--internal-valgrind" ]; then
        IV=$2
    fi
fi

PATH=.:${PATH}
$IV \
test-fetch ${POOL_XTRA} -t "${POOL_TYPE}" \
    -s "${POOL_SIZE}" -i "${POOL_TOC_CAPACITY}" "${TEST_POOL}"

exit $?
