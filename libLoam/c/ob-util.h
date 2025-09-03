
/* (c)  oblong industries */

#ifndef OB_UTIL_SOLAR
#define OB_UTIL_SOLAR

/**
 * Admittedly ob-util.h isn't a very descriptive name.  The intent is
 * for this file to hold misc libLoam functions that don't warrant
 * their own header file.  Eventually, when they reach critical mass,
 * they can migrate out into other headers, so this is sort of an
 * incubator.
 */

#include "libLoam/c/ob-types.h"
#include "libLoam/c/ob-api.h"
#include "libLoam/c/ob-retorts.h"
#include "libLoam/c/ob-attrs.h"

#include <string.h> /* for memset */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Generate a UUID as a string (32 hex digits, 4 dashes, 1 terminating
 * NUL)
 */
OB_LOAM_API ob_retort ob_generate_uuid (char uu[37]);

/**
 * Stores away a copy of the original program arguments.
 */
OB_LOAM_API void ob_set_program_arguments (int argc, const char *const *argv);

/**
 * Fetch the original argc and argv that this program was invoked
 * with.  ARGC will be set to 0, and ARGV to NULL, if no program
 * arguments were set.  \note Just like the arguments to main,
 * argv[argc] == NULL.
 */
OB_LOAM_API void ob_get_program_arguments (int *argc, const char *const **argv);

/**
 * Given a user name as a string, looks up the numeric user id and
 * puts it in *uid.
 */
OB_LOAM_API ob_retort ob_uid_from_name (const char *name, int *uid);

/**
 * Given a group name as a string, looks up the numeric group id and
 * puts it in *gid.
 */
OB_LOAM_API ob_retort ob_gid_from_name (const char *name, int *gid);

/**
 * Does nothing.
 */
OB_LOAM_API void ob_nop (void);

/**
 * Does nothing with one or more arguments of any type.  This is
 * useful if you are calling a function like write() that complains
 * (gcc compiler warning) if you ignore the return value, but you
 * really want to ignore it anyway.  (Like you are already in error
 * handling code and there's really nothing further you can do if
 * there's an error in the error handler.)
 */
OB_LOAM_API void ob_ignore (int64 foo, ...);

/**
 * Fetch the current user's username, or "unknown" on error.
 * Returns a value suitable for use with ob_uid_from_name.
 * On Windows, this is %USERNAME%; on Unix, it's `id -un`.
 */
OB_LOAM_API const char *ob_get_user_name (void);

/**
 * Return the name of the current program; i. e. basename of argv[0]
 */
OB_LOAM_API const char *ob_get_prog_name (void);

/**
 * Sets the name returned by ob_get_prog_name().  An example of why
 * you would want to do this is if the program is an interpreter, and
 * you want the name to show up as the name of the script
 * (e. g. "foo.rb") instead of the name of the interpreter
 * (e. g. "ruby").
 */
OB_LOAM_API void ob_set_prog_name (const char *newname);

/**
 * Sets environment variable \a name to \a value.
 */
OB_LOAM_API ob_retort ob_setenv (const char *name, const char *value);

/**
 * Unsets environment variable \a name.
 */
OB_LOAM_API ob_retort ob_unsetenv (const char *name);

/**
 * Appends \a value to the list of directories in environment variable \a name.
 * If the list was not empty, uses OB_PATH_CHAR to join the two.
 */
OB_LOAM_API ob_retort ob_append_env_list (const char *name, const char *value);

/**
 * Prepends \a value to the list of directories in environment variable \a name.
 * If the list was not empty, uses OB_PATH_CHAR to join the two.
 */
OB_LOAM_API ob_retort ob_prepend_env_list (const char *name, const char *value);

/**
 * pthread_self() is portable, but on both Linux and Mac it returns an
 * address, which is long (as many as 16 hex digits if we are compiled
 * 64-bit).  We'd like a smaller number to identify a thread.  On
 * Linux, there is the tid, which lives in the same namespace as pids
 * (since on Linux, threads are just a special kind of process).  On
 * Mac, there is the mach port for the thread.  In both cases, these
 * numbers tend to be 4 or 5 decimal digits, which is a bit easier for
 * a human reading a log file to do pattern matching on.  As an added
 * bonus, and to avoid cluttering the logs of single-threaded
 * programs, we identify the main thread specially.  The downside is
 * that all of this is horribly, horribly nonportable.
 */
OB_LOAM_API void ob_get_small_integer_thread_id (unt32 *tid, bool *is_main);

/**
 * Returns true if we are running under Valgrind.
 */
OB_LOAM_API OB_CONST bool ob_running_under_valgrind (void);

/**
 * Overwrites the specified memory, and, if running under Valgrind,
 * marks it as undefined.  This is a good way to assert that the memory
 * should not be read.
 */
OB_LOAM_API void *ob_make_undefined (void *addr, size_t len);

/**
 * Zeroes out the given structure.
 */
#define OB_CLEAR(x) memset (&(x), 0, sizeof (x))

/**
 * Marks the given structure as undefined.
 */
#define OB_INVALIDATE(x) ob_make_undefined ((void *) &(x), sizeof (x))

/**
 * Size of the array required by ob_fmt_hex_line().
 *
 * (68 characters, plus room for a terminating NUL character.)
 */
#define OB_HEX_LINE_LEN 69

/**
 * Formats up to 16 bytes from \a src as a line of hex plus ASCII,
 * in the same format produced by the hd(1) command.  (Although not
 * including the file offset that hd(1) puts at the beginning of the
 * line.)  There are eight hex bytes, an additional space, another
 * eight hex bytes, and then sixteen ASCII bytes, where bytes
 * outside the ASCII range are represented with a period.
 *
 * The result is written into \a dst and terminated with a NUL
 * character.
 *
 * If \a srcLen is greater than 16, it is treated as if it was 16.
 */
OB_LOAM_API void ob_fmt_hex_line (char        dst[OB_HEX_LINE_LEN],
                                  const unt8 *src,
                                  size_t      srcLen);

#ifdef __cplusplus
}
#endif

#endif /* OB_UTIL_SOLAR */
