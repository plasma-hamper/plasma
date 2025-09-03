**libLoam** is a library which contains basic portability and utility functions.

|header file|description                                                      |
|-----------|-----------------------------------------------------------------|
|[ob-api.h](ob-api.h)       | Macros for Windows DLL stuff.                                         |
|[ob-atomic.h](ob-atomic.h) | Atomic operations on integers and pointers.                           |
|[ob-attrs.h](ob-attrs.h)   | Macros to allow using GCC's `__attribute__` syntax in a portable way. |
|[ob-dirs.h](ob-dirs.h)     | Functions for operating on directories (such as recursive `mkdir` and `rmdir`, and path manipulation functions) and also functions for getting standard directory locations (such as pools directory, temporary directory, etc.). |
|[ob-endian.h](ob-endian.h) | Byte-swapping functions.  When possible, these are just wrappers around GCC's `__builtin_bswap` functions. |
|[ob-file.h](ob-file.h)     | File-related functions, such as reading an entire file at once, creating temporary files, etc. |
|[ob-hash.h](ob-hash.h)     | Various hash functions. |
|[ob-log.h](ob-log.h)       | Flexible logging facility. |
|[ob-math.h](ob-math.h)     | Cross-platform math functions. |
|[ob-pthread.h](ob-pthread.h) | On POSIX systems, this just includes `<pthread.h>`.  On Windows systems, includes a (partial) emulation of pthreads. |
|[ob-rand.h](ob-rand.h)     | Functions to generate pseudo-random numbers using Mersenne Twister.  Also contains `ob_truly_random()`, which uses platform-specific functions to produce cryptographic-grade random numbers.|
|[ob-retorts.h](ob-retorts.h) | A facility for managing error codes (which libLoam refers to as "[retorts](doc/extending-ob-retort.md)").|
|[ob-string.h](ob-string.h) | Functions for manipulating strings in C. |
|[ob-sys.h](ob-sys.h) | Includes a bunch of POSIX header files.  On Windows, does some stuff that gives a somewhat similar result.|
|[ob-thread.h](ob-thread.h) | Some additional thread functions that provide some additional functionality beyond `ob-pthread.h`.|
|[ob-time.h](ob-time.h) | Functions for getting the current time, sleeping, and formatting time.  `ob_current_time()` returns a fractional number of seconds since the UNIX epoch, as a `float64` (aka `double`).  `ob_monotonic_time()` returns the number of nanoseconds since an arbitrary point in time, as a 64-bit integer (`unt64`).|
|[ob-types.h](ob-types.h) | Contains cross-platform fixed-width types, such as `int32` and `unt64`.  (These days, this functionality is mostly duplicated by `<stdint.h>`.)  Also contains structs for complex, vector, and multivector types.  (All of the numeric types supported by libPlasma.)|
|[ob-util.h](ob-util.h) | Some totally miscellaneous stuff that doesn't fit anywhere else.  One example is `ob_generate_uuid()`.|
|[ob-vers.h](ob-vers.h) | Functions for getting information about the current system.  You can get things like the g-speak version number and the yobuild version number (neither of which mean anything in the current state that Plasma is in).  You can also get things like the OS version, compiler version, and CPU model.  There is a companion utility, [ob-version](ob-version.pod), which will print most of this stuff out.|
