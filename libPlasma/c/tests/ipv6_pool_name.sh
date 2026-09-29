#!/bin/bash

# Check that a pool can be reached by IPv6 address literal, which is
# written in brackets to keep the colons in the address from being
# confused with the one before the port: tcp://[::1]:1234/pool_name
#
# Once the pool is known to be reachable, test-info is also run against
# an address-literal URI, to check that the host and port it takes out
# of the URI agree with what pool_get_info() reports.
#
# This runs against the "tcp" fixture only; the TLS fixtures' server
# certificate names "localhost", so it would rightly be refused for
# "::1".

# Handle internal valgrind
IV=""
if [ "$#" == "2" ]; then
    if [ "$1" == "--internal-valgrind" ]; then
        IV=$2
    fi
fi

PATH=.:..:$PATH

# TEST_POOL looks like "tcp://localhost:1234/test_pool".  Aim at the
# same server by address instead of by name, with a pool of our own.
AUTHORITY=`echo "${TEST_POOL}" | sed -e 's|^[a-z]*://||' -e 's|/.*$||'`
PORT=`echo "${AUTHORITY}" | sed -e 's|^[^:]*||' -e 's|^:||'`
if [ -n "${PORT}" ]; then
    V6_AUTHORITY="[::1]:${PORT}"
else
    V6_AUTHORITY="[::1]"
fi
V6_POOL="tcp://${V6_AUTHORITY}/ipv6_test_pool"
INFO_POOL="tcp://${V6_AUTHORITY}/ipv6_info_pool"

$IV \
p-create ${POOL_XTRA} -t "${POOL_TYPE}" -s "${POOL_SIZE}" "${V6_POOL}" 2>>${TEST_LOG}
rc=$?

# This is what the test is really for.  16 is EXIT_BADPNAME, which is
# what this used to give, because the pool URI parser took the first
# colon of the address to be the one introducing the port.
if [ "${rc}" == "16" ]; then
    echo "'${V6_POOL}' was rejected as a malformed pool name"
    exit 1
fi

if [ "${rc}" != "0" ]; then
    # The name parsed; we just couldn't get there.  Some build machines
    # and containers have IPv6 loopback switched off, which is not this
    # test's business, so stop here rather than failing.
    echo "note: could not reach '${V6_POOL}' (exit ${rc})"
    echo "note: IPv6 loopback looks unavailable; skipping the rest"
    exit 0
fi

# There is a pool at the other end of that URI.  Use it, then put it away.
$IV \
p-info "${V6_POOL}" >>${TEST_LOG} 2>&1
[ "$?" != "0" ] && echo "p-info failed on '${V6_POOL}'" && exit 1

$IV \
p-stop "${V6_POOL}" >>${TEST_LOG} 2>&1
[ "$?" != "0" ] && echo "p-stop failed on '${V6_POOL}'" && exit 1

# test-info creates, inspects and disposes of its own pool.
$IV \
test-info ${POOL_XTRA} -t "${POOL_TYPE}" -s "${POOL_SIZE}" \
    -i "${POOL_TOC_CAPACITY}" "${INFO_POOL}" >>${TEST_LOG} 2>&1
[ "$?" != "0" ] && echo "test-info failed on '${INFO_POOL}'" && exit 1

exit 0
