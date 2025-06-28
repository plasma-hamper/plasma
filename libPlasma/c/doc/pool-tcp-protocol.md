# Pool TCP protocol #     {#PoolTCPProtocol}

# Overview #   {#Overview}

The pool TCP protocol is a protein-based protocol, where the client
sends a request protein, and the server replies with a response
protein.  For the exact way the proteins are sent over the wire, see
the sections on "protocol version 1" and "protocol version 0", below.

Request and response proteins have no descrips and no rude data
(except in the version special case explained in "Version Detection",
below) and have exactly two ingests.  The "op" ingest is an int32, and
for a request it is one of the op numbers shown in the table below.
For responses, it is always POOL_CMD_RESULT, except in the cases where
one of the FANCY_RESULT codes is explicitly specified.  The "args"
ingest is a list.  The number and type of elements in the list depends
on the op number, as shown in the table below.

Here is the key to the types in the table.

    'i' - int64
    'p' - protein
    'r' - ob_retort (usually int64, but may be unt64 or a list in older TCP servers)
    's' - string
    't' - pool_timestamp (as float64)
    'x' - slaw

And here is the key to the "state" field:

    I - this must be the Initial (1st) command in a connection
    P - this command is only valid after PARTICIPATE or PARTICIPATE_CREATINGLY
    F - this is the Final command in a connection (server closes
        connection after sending response)
    N - iNterruptible; if you send another request before you get a response,
        this request will terminate and you'll get responses for both requests.

Requests:

Name                            | op | state | request | response
:-------------------------------|---:|:------|:--------|:--------
POOL_CMD_CREATE                 |  0 | IF    | ssp     | r
POOL_CMD_DISPOSE                |  1 | IF    | s       | r
POOL_CMD_PARTICIPATE            |  2 | I     | sp      | r
POOL_CMD_PARTICIPATE_CREATINGLY |  3 | I     | sspp    | r
POOL_CMD_WITHDRAW               |  4 | PF    | `<none>`| r
POOL_CMD_DEPOSIT                |  5 | P     | p       | irt
POOL_CMD_NTH_PROTEIN            |  6 | P     | i       | ptr
POOL_CMD_NEXT                   |  7 | P     | i       | ptir
POOL_CMD_PROBE_FRWD             |  8 | P     | ix      | ptir
POOL_CMD_NEWEST_INDEX           |  9 | P     | `<none>`| ir
POOL_CMD_OLDEST_INDEX           | 10 | P     | `<none>`| ir
POOL_CMD_AWAIT_NEXT_SINGLE      | 11 | P     | t       | rpti
POOL_CMD_MULTI_ADD_AWAITER      | 12 | PN    | `<none>`| rpti
POOL_CMD_INFO                   | 15 | P     | i       | rp
POOL_CMD_LIST                   | 16 | IF    | `<none>`| rx
POOL_CMD_INDEX_LOOKUP           | 17 | P     | tii     | ir
POOL_CMD_PROBE_BACK             | 18 | P     | ix      | ptir
POOL_CMD_PREV                   | 19 | P     | i       | ptir
POOL_CMD_FANCY_ADD_AWAITER      | 20 | PN    | ix      | rti + rti + tip
POOL_CMD_SET_HOSE_NAME          | 21 | P     | ssi     | `<no response>`
POOL_CMD_SUB_FETCH              | 22 | P     | x       | xii
POOL_CMD_RENAME                 | 23 | IF    | ss      | r
POOL_CMD_ADVANCE_OLDEST         | 24 | P     | i       | r
POOL_CMD_SLEEP                  | 25 | IF    | s       | r
POOL_CMD_CHANGE_OPTIONS         | 27 | P     | p       | r
POOL_CMD_LIST_EX                | 28 | IF    | s       | rx
POOL_CMD_SUB_FETCH_EX           | 29 | P     | xi      | xii
POOL_CMD_STARTTLS               | 30 | I     | x       | rx
POOL_CMD_GREENHOUSE             | 31 | | | <not a real command; never sent>

Responses:

Name                         | op
:----------------------------|---:
POOL_CMD_RESULT              | 14
POOL_CMD_FANCY_RESULT_1      | 64
POOL_CMD_FANCY_RESULT_2      | 65
POOL_CMD_FANCY_RESULT_3      | 66

All requests return a single response of type POOL_CMD_RESULT, unless
otherwise noted.

Note that the options for POOL_CMD_CREATE, POOL_CMD_PARTICIPATE,
POOL_CMD_PARTICIPATE_CREATINGLY, and POOL_CMD_CHANGE_OPTIONS are
required to be sent over the wire as proteins, even though the C API
allows the options to be specified as either a protein or a map.  (But
if it is a map, the client converts it to a protein before sending it
over the wire.)

# More detailed request-response information #   {#request-response}

    POOL_CMD_CREATE - create a new pool
      s: name of pool to create
      s: type of pool to create (really "mmap" is the only choice for now)
      p: creation options (e. g. size)
    response:
      r: retort

    POOL_CMD_DISPOSE - delete a pool
      s: name of pool to delete
    response:
      r: retort

    POOL_CMD_PARTICIPATE - specify an existing pool to operate upon
      s: name of pool to participate in
      p: participate options (deprecated, and was never used anyway)
    response:
      r: retort

    POOL_CMD_PARTICIPATE_CREATINGLY - specify a pool to operate upon, and create it
      s: name of pool to create (if neccesary) and participate in
      s: type of pool to create (really "mmap" is the only choice for now)
      p: creation options (e. g. size)
      p: was reserved for participate options (ignored & probably will remain so)
    response:
      r: retort

    POOL_CMD_WITHDRAW - terminate the connection
    response:
      r: retort

    POOL_CMD_DEPOSIT - deposit a protein in the pool
      p: protein to deposit
    response:
      i: index where protein was deposited
      r: retort
      t: timestamp when protein was deposited

    POOL_CMD_NTH_PROTEIN - get a protein at a specified index
      i: index of protein to get
    response:
      p: the protein at that index
      t: the timestamp of that protein
      r: retort

    POOL_CMD_NEXT - get the protein at or after the specified index
      i: index of the protein you want to get
    response:
      p: the next protein at or after the specified index
      t: the timestamp of that protein
      i: the index of the protein
      r: retort

    POOL_CMD_PROBE_FRWD - get the next protein that matches the search pattern
      i: index of the protein where you want to start searching
      x: the pattern to match against the descrips (string or list of strings)
    response:
      p: the next protein at or after the specified index
      t: the timestamp of that protein
      i: the index of the protein
      r: retort

    POOL_CMD_NEWEST_INDEX - get the index of the pool's newest protein
    response:
      i: the index of the newest protein in the pool
      r: retort

    POOL_CMD_OLDEST_INDEX - get the index of the pool's oldest protein
    response:
      i: the index of the oldest protein in the pool
      r: retort

    POOL_CMD_AWAIT_NEXT_SINGLE† - wait for a protein to be deposited, with timeout
      t: timeout in seconds (*)
    response:
      r: retort
      p: the protein you were waiting for
      t: the timestamp of that protein
      i: the index of the protein

    (*) Note: protocol versions 0 and 1 use a timestamp value of 0 to
    indicate POOL_WAIT_FOREVER and a timestamp value of -1 to indicate
    POOL_NO_WAIT.  However, protocol verion 2 and higher use -1 for
    POOL_WAIT_FOREVER and 0 for POOL_NO_WAIT.

    POOL_CMD_MULTI_ADD_AWAITER† - wait indefinitely for a protein; can be interrupted
    response:
      r: retort
      p: the protein you were waiting for
      t: the timestamp of that protein
      i: the index of the protein

    † Both POOL_CMD_AWAIT_NEXT_SINGLE and POOL_CMD_MULTI_ADD_AWAITER have
    the property that they depend on the location of the hose as set by a
    previous operation (one of POOL_CMD_NEXT, POOL_CMD_PROBE_FRWD,
    POOL_CMD_PREV, or POOL_CMD_PROBE_BACK).
    This implicit state could be considered a bug, since most other
    network commands explicitly specify the index at which to start
    (e. g. POOL_CMD_NEXT and POOL_CMD_PROBE_FRWD).
    An alternative is to use POOL_CMD_FANCY_ADD_AWAITER instead,

    POOL_CMD_INFO - get information about the current pool
      i: number of hops (probably 0 or -1 at this point)
    response:
      r: retort
      p: protein containing information about the current pool

    POOL_CMD_LIST - list the names of the pools available on this server
    response:
      r: retort
      x: list of strings (one for each pool on the server)

    POOL_CMD_LIST_EX - list the names of some of the pools available on this server
      s: subdirectory to look in (e. g. "foo/bar/")
    response:
      r: retort
      x: list of strings (one for each pool in the subdirectory)

    POOL_CMD_INDEX_LOOKUP - get the index closest to a given timestamp
      t: timestamp to look for
      i: -1 if the the timestamp is absolute, or the base index otherwise
      i: what is meant by 'closest': 0 absolute, 1 lower, 2 higher
    response:
      i: the index closest to the given timestamp
      r: retort

    POOL_CMD_PROBE_BACK - get the previous protein that matches the search pattern
      i: index of the protein where you want to start searching
      x: the pattern to match against the descrips (string or list of strings)
    response:
      p: the matching protein
      t: the timestamp of that protein
      i: the index of the protein
      r: retort

    POOL_CMD_PREV - get the protein before the specified index
      i: index of the protein after the one you want to get
    response:
      p: the next protein before the specified index
      t: the timestamp of that protein
      i: the index of the protein
      r: retort

    POOL_CMD_SET_HOSE_NAME - specify the name of the hose and process
      s: name of hose
      s: name of program
      i: process ID
    This command cannot fail and sends no response, thus avoiding a
    round-trip latency.  The information specified is used solely for
    logging and debugging.

    POOL_CMD_FANCY_ADD_AWAITER - wait indefinitely for a protein; can be interrupted
      i: index of the protein where you want to start awaiting
      x: the pattern to match against the descrips (string or list of strings,
         or nil for no pattern)
    1st response: POOL_CMD_FANCY_RESULT_1
      r: retort
      t: the timestamp of that protein (only meaningful if r is OB_OK)
      i: the index of the protein (only meaningful if r is OB_OK)
    2nd response (only if 1st was POOL_NO_SUCH_PROTEIN): POOL_CMD_FANCY_RESULT_2
      r: retort
      t: the timestamp of that protein (only meaningful if r is OB_OK)
      i: the index of the protein (only meaningful if r is OB_OK)
    3rd response (only if 1st or 2nd was OB_OK): POOL_CMD_FANCY_RESULT_3
      t: the timestamp of that protein
      i: the index of the protein
      p: the protein you were waiting for

    POOL_CMD_RENAME - rename (or move, within a server) a pool
      s: old name of pool
      s: new name of pool
    response:
      r: retort

    POOL_CMD_ADVANCE_OLDEST - artificially advance the "oldest" index of a pool
      i: index to become the new oldest index
    response:
      r: retort

    POOL_CMD_SLEEP - temporarily free "resources" (semaphores) of an inactive pool
      s: name of pool to put to sleep
    response:
      r: retort

    POOL_CMD_CHANGE_OPTIONS - modify some of the options from pool creation
      p: new options (e. g. size)
    response:
      r: retort

    POOL_CMD_STARTTLS - Initiate Transport Layer Security on the underlying
        connection.  The reply is sent in the clear, and then after the reply
        is sent, the client begins the TLS handshake.  Once the TLS handshake
        is successful, the connection essentially resets, since none of the
        information exchanged before the TLS handshake can be trusted, since
        it was not authenticated.  However, rather than sending the entire
        88 byte chunk required to initiate the pool protocol, the first data
        the client sends over the TLS-secured connection is merely two
        bytes: the "pv" and "sv" bytes.  After that, the pool protocol handshake
        continues as normal, with the server sending "pv sv nn ..".  The
        client and server should not assume they will negotiate the same version
        of the pool protocol after TLS is initiated as they did before TLS.
        And note that once the second pool protocol handshake occurs, inside
        the TLS connection, we are once again back in the "I" state, and any
        of the supported "initial" commands (except of course STARTTLS itself)
        can be used at this point.  The server might advertise different
        commands available before and after TLS is initiated.  In particular,
        if the server wishes to require secure connections, it should only
        advertise support for the STARTTLS command, and nothing else, until
        after TLS is established.
      x: a slaw map which might contain additional information, but currently empty
    response:
      r: retort (if a failure retort, then the TLS handshake will not begin)
      x: a slaw map which might contain additional information, but currently empty

    POOL_CMD_GREENHOUSE - This is a fake command, which is never sent, so
        has no request or reponse format.  The only purpose of this
        "command" is to be a flag, when the server sends the bitmask of
        supported command to the client.  Servers compiled with
        -DGREENHOUSE will advertise that they support this command.
        Clients compiled with -DGREENHOUSE will fail with the retort
        POOL_NOT_A_GREENHOUSE_SERVER unless the server claims to support
        this command.  This prevents Greenhouse clients from talking to
        non-Greenhouse servers, without preventing any other pairings.

    POOL_CMD_SUB_FETCH - fetch part or all of one or more proteins
      x: a slaw list of slaw maps; see below
    response:
      x: a slaw list of slaw maps; see below
      i: oldest index (or negative retort for error or empty pool)
      i: newest index (or negative retort for error or empty pool)

    POOL_CMD_SUB_FETCH_EX - same as POOL_CMD_SUB_FETCH, with added "clamp" arg
      x: a slaw list of slaw maps; see below
      i: clamp indices to valid range in pool (0 = no, nonzero = yes)
    response:
      x: a slaw list of slaw maps; see below
      i: oldest index (or negative retort for error or empty pool)
      i: newest index (or negative retort for error or empty pool)

The request and response for POOL_CMD_SUB_FETCH are both lists, where
each list element represents one protein that is to be fetched.
(Therefore, the request and response lists are of the same length.)
Each list element is a map.  The map keys are kept to six characters
or less, because that is more efficient in the slaw encoding.

Each element of the request list should have the following keys:

name    | type   | description
:-------| :------| :----------
idx     | int64  | index of the protein to be fetched
des     | bool   | true if you want the protein's descrips
ing     | bool   | true if you want the protein's ingests
roff    | int64  | offset of rude data to start at, or -1 for no rude data
rbytes  | int64  | number of bytes of rude data to get, or -1 for "until end"

Each element of the response list should have the following keys:

name    | type     | description
:-------| :--------| :----------
idx     | int64    | index of the protein that was fetched
retort  | int64    | 0 (OB_OK) on success, or other retort on failure<br />(the remaining keys need not be present* on failure)
time    | float64  | protein's timestamp metadata from the pool
tbytes  | int64    | total number of bytes in the protein
dbytes  | int64    | number of bytes in the protein's descrips
ibytes  | int64    | number of bytes in the protein's ingests
rbytes  | int64    | number of bytes in the protein's rude data
ndes    | int64    | -1 if no descrips or descrips is not a list or map<br />otherwise, number of descrips
ning    | int64    | -1 if no ingests or ingests is not a list or map<br />otherwise, number of ingests
prot    | protein  | absent‡ if "des" and "ing" were false and roff was -1<br />otherwise, a (possibly trimmed) version of the<br />only if requested, and the rude data is only the<br />range requested from the original protein)

Note: all of the metadata (tbytes, dbytes, ibytes, rbytes, ndes, ning)
refers to the original protein in the pool, not the trimmed protein
that was returned.

* The current server does not actually omit these keys, but it could;
the client doesn't care one way or the other.

# Endianness #    {#endianness}

Most of the pool protocol is protein-based, and proteins self-describe
their endianness.  Each protein you send can be of an endianness of
your own choosing, but each protein you receive will be of an
endianness of the sender's choosing, so you must be prepared to swap.
Also, proteins contained inside another protein must have the same
endianness as their encapsulating protein; you can't put a
little-endian protein inside a big-endian protein.

For the few parts of the pool protocol which exist outside of
proteins, this document specifies an explicit endianness for any
multi-byte numbers.  (e. g. big-endian for the lengths in the version
0 protocol, and little-endian for the bit mask of supported commands
in the version 1 and higher protocols.)

# Versioning policy #     {#versioning-policy}

Since the handshake for version 1 and higher includes a list of the
commands that are supported, it's safe to add new commands without
incrementing the version number.  (As long as your implementation
checks whether the command is supported before issuing the command.)

However, any change to the semantics of existing commands (such as
changing the arguments they accept or return, or changing the numeric
value of \c \#defined constants which are sent over the wire) will require
incrementing the version number.

## Pool TCP protocol version 3

Pool TCP protocol version 3 is identical to version 2, except for
different values for some ob_retorts.  See the old_retorts table in
plasma-util.c which defines the numeric values for specific retorts
for specific version ranges.

## Pool TCP protocol version 2

Pool TCP protocol version 2 is identical to version 1, except that for
the POOL_CMD_AWAIT_NEXT_SINGLE command, the values for
POOL_WAIT_FOREVER and POOL_NO_WAIT are swapped.

## Pool TCP protocol version 1

Pool TCP protocol version 1 (and hopefully all later versions) begins
with a handshake to negotiate the pool TCP protocol version to use,
and the slaw encoding version to use.

After the client connects to the server port, it must send the
following 88 bytes:

    00  00  00  00  00  00  00  50  93  93  00  80  18  00  00  02
    00  00  00  10  40  00  00  04  20  00  00  01  6f  70  00  00
    08  00  00  03  00  00  00  01  40  00  00  08  20  00  00  02
    61  72  67  73  00  00  00  00  10  00  00  01  00  00  00  05
    20  00  00  02  5e  2f  5e  2f  5e  2f  5e  00  pv  sv  00  00
    00  00  00  00  00  00  00  00

Where pv is the highest pool TCP protocol version supported by the
client and sv is the highest slaw version supported by the client.
The other 86 bytes are constant.

After receiving this sequence of bytes, the server sends the following
bytes back to the client:

    pv sv nn ..

Where pv and sv are the protocol version and slaw version,
respectively, that the server proposes using.  (They must be less than
or equal to the version numbers that the client sent.)  As a special
case, pv=00 and sv=00 indicates protocol version 0, slaw version 1.
(See "Version detection", below.)  As another special case, pv=ff and
sv=ff indicates that the server does not support any version that the
client supports, and the connection cannot be made.

nn is the number of bytes that follow (not counting these first three
bytes).  ".." represents n bytes which encode a bit mask, transmitted
least significant byte first.  This bit mask contains a "1" for each
operation number that the server supports.

For example, consider a server that supports protocol version 1, slaw
version 2, and supports all operations except POOL_CMD_RESULT. I. e.,
it supports operations 0 through 12, 15, and 16. This means that the
mask of its supported operations is 019fff, so it would send the
following bytes.

    01 02 03 ff 9f 01

After this initial handshake, the client sends a request protein, and
the exchange of requests and responses continues as normal.  All that
is sent are the actual bytes of the proteins; the length is encoded in
the first oct of the protein.  The proteins must be sent in the
negotiated slaw encoding, in either endianness.

## Pool TCP protocol version 0

Pool TCP protocol version 0 only supports slaw version 1.

There is no initial handshake.  The client simply opens the port and
sends a request protein to the server, the server sends a response
protein, and so forth.

For each protein, first the length of the protein, in bytes, is sent
as a big-endian 8-byte integer.  Then the bytes of the protein are
sent, in either endianness, in slaw version 1 encoding.

Since protocol version 0 does not provide a way to communicate which
commands are supported, the client should assume a version 0 server
supports POOL_CMD_CREATE, POOL_CMD_DISPOSE, POOL_CMD_PARTICIPATE,
POOL_CMD_PARTICIPATE_CREATINGLY, POOL_CMD_WITHDRAW, POOL_CMD_DEPOSIT,
POOL_CMD_NTH_PROTEIN, POOL_CMD_NEXT, POOL_CMD_PROBE_FRWD,
POOL_CMD_NEWEST_INDEX, POOL_CMD_OLDEST_INDEX,
POOL_CMD_AWAIT_NEXT_SINGLE, POOL_CMD_MULTI_ADD_AWAITER, POOL_CMD_INFO,
and POOL_CMD_LIST.

# Version detection #     {#version-detection}

Pool TCP protocol version 1 and higher include a handshake which
allows the version to be negotiated.  This makes interoperability
straightforward when both the client and the server are version 1 or
greater.

The 88 byte handshake message used by version 1 and higher is in fact
a valid request message encoded with protocol version 0, slaw
version 1.  It requests that the pool named "^/^/^/^" be deleted, and
contains the version numbers embedded in its rude data.  Since this is
an invalid pool name since it contains a slash, a version 0 server
which receives this handshake from a version 1 or higher client will
send back an error response protein using protocol 0.  Since this
protein is significantly smaller than `2**48` bytes, the first two
bytes of the 8-byte big endian length will be 0, which means the
version 1 client will receive 0 for pv and sv, which it can use to
recognize a version 0 server.  At this point, the client should
disconnect and reconnect using protocol version 0.

On the other hand, a version 1 or higher server can detect a version 0
client if the first bytes sent do not match the 88-byte handshake.  If
the bytes do not match the handshake, it interprets the bytes as a
protocol 0 request protein.

