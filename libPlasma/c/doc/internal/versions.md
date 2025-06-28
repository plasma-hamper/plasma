# Plasma Versioning

_This document describes the historical versioning of Slaw, our TCP
protocol, and pools. In all of these cases, the earlier versions of
each have not been in use since around 2010/2011. The only potential
place I can think of where these versions may still be in use is the
original optical/dot tracking pipeline._

-----

In order for libPlasma to be backwards-compatible with older versions
of itself, we use several version numbering schemes to keep track of
incompatible changes.  This is a bit confusing, because we use several
different version numbering schemes to version different ascpects of
libPlasma.


Slaw version
============

This is the version number for the binary encoding used for slaw.  The
versions recognized by modern libPlasma are version 1 and version 2.
(There were some even older versions before version 1, and the
skipping of version 0 is a nod to that, but they were never officially
numbered and are no longer supported.)

The two supported slaw encodings are documented in slaw-v1.md and
slaw-v2.md.

The main flaw of slaw version 1 is that it was only 4-byte aligned,
which meant that 64-bit numbers might be unaligned.  Although x86
scalar instructions do not require their operands to be aligned, x86
vector instructions do.  So, when using a compiler like gcc that can
perform autovectorization, this would sometimes result in program
crashes due to unaligned access.  (And in theory, this could be even
more of a problem on non-x86 architectures where even scalar
instructions require aligned operands.)

Slaw version 2 is 8-byte aligned, and therefore solved this problem.
Slaw version 2 also supports NUL bytes in the middle of strings, which
slaw version 1 did not support.  The slaw version 2 encoding is also
simpler and easier to parse.  (In this author's opinion, although she
is biased since she is also the author of the slawv2 encoding.)


TCP protocol version
====================

This is the version number for the protocol used to talk to the pool
server.

The legacy protocol is version 0, and requires the use of slaw
version 1.  Since protocol version 0 was not explicitly numbered
and did not support a mechanism to negotiate the protocol version
number, a rather sneaky mechanism is used to disambiguate protocol
0 from later protocols.

Protocol 1 and later support an explicit, straightforward mechanism
for negotiating the protocol version.  They also support specifying
the slaw version independently of the protocol version.  And they
support discovery of which commands are implemented.  Therefore,
adding (or even removing) a command generally doesn't require creating
a new protocol version.  It's only necessary to increment the version
when the behavior of existing commands is changed.  (This was done
when the timestamp and retort encoding changed, for example.)

For complete documentation of all versions of the TCP pool protocol,
including the versioning policy and the version negotiation mechanism,
see pool-tcp-protocol-internal.md.


mmap pool version
=================

This is the version number for the format of the mmap pool "backing
file".

There are currently two versions, 0 and 1.  Version 0 is also called
"green-room compatible", since the libPlamsa in the green-room branch
only supports version 0.  Version 1 is also called "resizable", since
only version 1 pools can be resized after they are created.

In practice, the mmap pool version is entangled with the pool
directory version, although in theory it might be possible to version
them separately.


pool directory version
======================

This version number indicates how the pools directory is laid out.
(i. e. what the files are named and where they are located)

For bizarre historical reasons, the pool directory versions are 4
and 5.  They are also given symbolic names by #defines in
pool_impl.h: version 4 is POOL_DIRECTORY_VERSION_CONFIG_IN_FILE
and version 5 is POOL_DIRECTORY_VERSION_CONFIG_IN_MMAP.

In both types, there is a pool.conf file which contains configuration
information for the pool, including the pool directory version
number.  In version 4, there is also an mmap.conf file which contains
additional information specific to the mmap pool implementation.  In
version 5, mmap.conf is gone, and the information that used to be in
it has been moved directly into the mmap backing file.

The name of the mmap backing file itself also changed between
versions.  In version 4, it is named "foo.mmap-pool", where foo is the
name of the pool.  This was weird, because it was the only file inside
the pool directory whose name depended on the name of the pool
directory itself.  And it made moving and renaming pools less
convenient, because the file inside had to be renamed, too.  So in
version 5, the backing file is simply called "mmap-pool", regardless
of the pool name.

Because these changes affected the interface between the mmap pool
code and the generic pool code, rather than just one of them
independently, both the mmap pool version and the pool directory
version had to be incremented.  Therefore, pool directory version 4
implies mmap pool version 0, and pool directory version 5 implies mmap
pool version 1.
