
/* (c)  oblong industries */

/* Miscellaneous notes & stuff:    [intentionally not a Doxygen comment]
 *
 * - Logging to pools is possible (implemented via the "log to function"
 *   capability here) but is implemented in libPlasma/c, since libLoam
 *   doesn't know anything about pools.  See libPlasma/c/pool-log.h
 *
 * - Controlling logging via environment variable is very well-defined
 *   and fleshed out, as documented below.  Logging options can also
 *   be set under program control, and all the functions and structures
 *   you need to do that are defined in this header file.  But, they are
 *   very low-level and only sparsely documented.  There should be a
 *   higher-level API for controlling logging.  Currently, the closest
 *   thing to this, which is very minimal and ad-hoc, is a couple of
 *   functions at the end of libPlasma/c/pool_cmd.[ch].  They are mostly
 *   meant for use by plasma utilities, but could be called by other
 *   programs, or could be used as a source of inspiration for writing
 *   additional functions.
 *
 * - The "begin usage" and "end usage" comments below are used by
 *   generate-ob-log-usage.hs to auto-generate the usage message in
 *   ob-log.c.
 */

/**
 * \file
 *
 * C API for low-level logging.
 *
 * There are six pre-defined log levels: OBLV_BUG, OBLV_ERROR,
 * OBLV_DEPRECATION, OBLV_WARN, OBLV_INFO, and OBLV_DEBUG. Besides these
 * predefined levels, you can also define your own levels if you have
 * special needs, since a log level is actually just a structure.
 *
 * For the six predefined levels, it's possible to configure their
 * behavior via the OB_LOG environment variable.
 *
 * <!-- begin usage -->
 *
 * The OB_LOG environment variable consists of space-separated
 * tokens, like this:
 *
 * LEVEL [option [option ...]] [LEVEL [option [option ...]] ...]
 *
 * Where LEVEL is one of: BUG, ERROR, DEPRECATION, WARN, INFO, or DEBUG
 *
 * And options are:
 *   (flag-like options can be prefixed with "no" to turn them off)
 *
 * stderr - log to standard error
 *
 * ./file/name - log to the named file (must contain a slash)
 *
 * nofd - don't log to previously specified file or stderr
 *
 * [no]syslog - log to syslog
 *
 * [no]stack - include stack trace when printing message
 *
 * [no]time - print time along with message
 *
 * [no]full - print full filename and line number with message
 *
 * [no]base - print file basename and line number with message
 *
 * [no]code - print 64-bit "code" (see below) with message
 *
 * [no]pid - print process ID with message
 *
 * [no]prog - print program name with message
 *
 * [no]tid - print thread ID with message
 *
 * none - turn off logging for all codes (same as "????????????????=off")
 *
 * +pattern - always turn on logging from source files that match
 *            "pattern" (which is a glob pattern) regardless of
 *            the code matching (below)
 *
 * -pattern - always turn off logging from source files that match
 *            "pattern" (which is a glob pattern) regardless of
 *            the code matching (below)
 *
 *            The glob pattern is matched against __FILE__, which is
 *            usually only the basename; see
 *            https://lists.oblong.com/pipermail/dev/2013-January/000310.html
 *
 * xxxx????=on - turn on logging for the specified range of codes
 *
 * xxxx????=off - turn off logging for the specified range of codes
 *
 * xxxx????=\<count\> - log at most "count" messages for the specified
 * range of codes
 *
 * xxxx????:\<count\> - log at most "count" instances of *each* individual
 * code within the specified range of codes
 *
 * In the above, "xxxx????" indicates a string of hex digits, where a
 * digit may also be replaced by a question mark.  (But question marks
 * may only appear in trailing positions, not anywhere in the string.)
 * This is a way of specifying a 64-bit "code", optionally with some
 * number (although a multiple of four) of low-order bits masked out.
 * Because of hierarchical nature of codes, this lets you control
 * behavior for an entire module at a time.
 *
 * If not modified by the OB_LOG environment variable, the default is
 * for log messages to include the "code" if one if given, or the file
 * basename and line number if the "code" is zero.  The thread ID is
 * printed for threads which are not the main thread.  The
 * "programming" log levels (BUG and DEPRECATION) print stack traces.
 * All log levels print to stderr, except DEBUG, which is disabled by
 * default.
 *
 * Note that the DEBUG level is disabled by virtue of not going to any
 * destination, but all codes are enabled for it.  Therefore, if you
 * enable debug messages with something like "DEBUG stderr", you will
 * get all debug messages!  If you only want to print a few debug
 * messages, you need to use "none" before enabling the codes you
 * want.  For example:
 *
 * OB_LOG="DEBUG stderr none 20108???=on"
 *
 * <!-- end usage -->
 *
 * Most logging can be done with ob_log() and ob_log_loc().
 * ob_log_loc() requires you to pass in a filename and line number
 * (useful if, for example, you really want to log the location of
 * whoever called you, rather than your own location), but for the
 * most common case, ob_log() is a convenience macro which calls
 * ob_log_loc() with __FILE__ and __LINE__ automatically.
 *
 * You must also supply a log level (which can be one of the six
 * predefined levels, or a pointer to an ob_log_level structure.  You
 * can also optionally supply a "code" (or 0 if you don't want to
 * supply a unique code).  A "code" is a unique 64-bit identifier
 * which indicates which message is being logged.  There are at least
 * three benefits to specifying such an identifier for each of your
 * log messages:
 *
 * 1. Allows individual messages to be turned on or off from the
 *    OB_LOG environment variable.  And since the bits in "code"
 *    are allocated hierarchically, by using a mask you can also
 *    enable or disable logging for entire modules at once.
 *
 * 2. Log messages can be suppressed once they have been printed
 *    a certain number of times, to avoid spamming the user.  The
 *    unique code identifies messages for this purpose.
 *
 * 3. The code can optionally be printed with the message, which may
 *    make customer support easier, since they can quote a specific
 *    number, rather than reading back the text of the message and
 *    possibly getting it wrong.
 *
 * Log messages are printf formatted, and the formatting is only done
 * if the message is going to be printed.  (This only concerns you if
 * you are using the dubious "%n" format, or if you are concerned
 * about the efficiency of suppressed messages.)  As a matter of
 * style, log messages should end in a newline-- however, if the
 * newline is missing, it will be added for you.  If a message
 * contains internal newlines, it will be formatted as a multiline
 * message, where each line will be prefixed with "(m/n): ", where n
 * is the total number of lines in the message, and n is the current
 * line number, between 1 and n, inclusive.
 */

#ifndef OB_LOG_ROLLING
#define OB_LOG_ROLLING

#include "libLoam/c/ob-attrs.h"
#include "libLoam/c/ob-retorts.h"
#include "libLoam/c/ob-api.h"

#include <string.h> /* for strerror */
#include <errno.h>  /* for errno itself */
#include <stdarg.h> /* for va_list */
#include <stdlib.h> /* for EXIT_FAILURE */

#ifdef __cplusplus
extern "C" {
#endif


/**
 * Our own drop-in replacement for the standard function perror().
 * Has two advantages over the standard version:
 * <ol>
 * <li> Uses error_report(), thus we get file and line, and
 *      all the other benefits of the logging system
 * <li> Does not modify errno (which standard perror can and does do)
 * </ol>
 */
#define ob_perror(prefix)                                                      \
  do                                                                           \
    {                                                                          \
      int save_errno__ = errno; /* avoid changing errno by printing it */      \
      error_report ("%s: %s\n", prefix, strerror (save_errno__));              \
      errno = save_errno__;                                                    \
    }                                                                          \
  while (0)


/**
 * Like ob_perror(), but allows a format string and variable arguments.
 * Useful if you want to include a filename or other information.
 */
#define ob_perrorf(fmt, ...)                                                   \
  do                                                                           \
    {                                                                          \
      int save_errno__ = errno; /* avoid changing errno by printing it */      \
      error_report (fmt ": %s\n", __VA_ARGS__, strerror (save_errno__));       \
      errno = save_errno__;                                                    \
    }                                                                          \
  while (0)


/**
 * This is a convenience for wrapping calls to close, fclose, unlink,
 * munmap, etc. (which return 0 for success and return nonzero and set
 * errno on failure) which often nobody bothers to check.
 */
#define CHECK_POSIX_ERROR(what)                                                \
  do                                                                           \
    {                                                                          \
      int posixRetCode = (what);                                               \
      if (posixRetCode != 0)                                                   \
        {                                                                      \
          int save_errno__ = errno; /* avoid changing errno by printing it */  \
          error_report ("'%s' failed with '%s'\n", #what,                      \
                        strerror (save_errno__));                              \
          errno = save_errno__;                                                \
        }                                                                      \
    }                                                                          \
  while (0)


/**
 * This is a convenience for wrapping calls to pthread_create,
 * pthread_join, etc., which return 0 for success and return an error
 * number on failure.  (In contrast to many other POSIX functions,
 * which put the error number in errno rather than returning it.)
 */
#define CHECK_PTHREAD_ERROR(what)                                              \
  do                                                                           \
    {                                                                          \
      int pthrRetCode = (what);                                                \
      if (pthrRetCode != 0)                                                    \
        error_report ("'%s' failed with '%s'\n", #what,                        \
                      strerror (pthrRetCode));                                 \
    }                                                                          \
  while (0)


/**
 * A convenience macro which does nothing if the ob_retort passed to
 * it is OB_OK, or is a success code (positive).  If the retort is an
 * error code (negative), terminates the program with a helpful error
 * message.
 */
#define OB_DIE_ON_ERROR(x)                                                     \
  do                                                                           \
    {                                                                          \
      ob_retort retort__ = (x);                                                \
      if (retort__ < 0)                                                        \
        error_exit ("fatal error in '%s': %s\n", #x,                           \
                    ob_error_string (retort__));                               \
    }                                                                          \
  while (0)


/**
 * Like assert(), but:
 *  - uses Oblong logging
 *  - is always checked (does not depend on NDEBUG)
 */
#define OBSERT(x)                                                              \
  do                                                                           \
    {                                                                          \
      if (!(x))                                                                \
        OB_FATAL_BUG ("assertion failure:\n%s\n", #x);                         \
    }                                                                          \
  while (0)


/**
 * Version of ob_perror() that takes a logging code.
 */
#define OB_PERROR_CODE(code, prefix)                                           \
  do                                                                           \
    {                                                                          \
      int save_errno__ = errno; /* avoid changing errno by printing it */      \
      OB_LOG_ERROR_CODE (code, "%s: %s\n", prefix, strerror (save_errno__));   \
      errno = save_errno__;                                                    \
    }                                                                          \
  while (0)


/**
 * Version of ob_perrorf() that takes a logging code.
 */
#define OB_PERRORF_CODE(code, fmt, ...)                                        \
  do                                                                           \
    {                                                                          \
      int save_errno__ = errno; /* avoid changing errno by printing it */      \
      OB_LOG_ERROR_CODE (code, fmt ": %s\n", __VA_ARGS__,                      \
                         strerror (save_errno__));                             \
      errno = save_errno__;                                                    \
    }                                                                          \
  while (0)


/**
 * Version of CHECK_POSIX_ERROR() that takes a logging code.
 */
#define OB_CHECK_POSIX_CODE(code, what)                                        \
  do                                                                           \
    {                                                                          \
      int posixRetCode = (what);                                               \
      if (posixRetCode != 0)                                                   \
        {                                                                      \
          int save_errno__ = errno; /* avoid changing errno by printing it */  \
          OB_LOG_ERROR_CODE (code, "'%s' failed with '%s'\n", #what,           \
                             strerror (save_errno__));                         \
          errno = save_errno__;                                                \
        }                                                                      \
    }                                                                          \
  while (0)


/**
 * Version of CHECK_PTHREAD_ERROR() that takes a logging code.
 */
#define OB_CHECK_PTHREAD_CODE(code, what)                                      \
  do                                                                           \
    {                                                                          \
      int pthrRetCode = (what);                                                \
      if (pthrRetCode != 0)                                                    \
        OB_LOG_ERROR_CODE (code, "'%s' failed with '%s'\n", #what,             \
                           strerror (pthrRetCode));                            \
    }                                                                          \
  while (0)


/**
 * Version of OB_DIE_ON_ERROR() that takes a logging code.
 */
#define OB_DIE_ON_ERR_CODE(code, x)                                            \
  do                                                                           \
    {                                                                          \
      ob_retort retort__ = (x);                                                \
      if (retort__ < 0)                                                        \
        OB_FATAL_ERROR_CODE (code, "fatal error in '%s': %s\n", #x,            \
                             ob_error_string (retort__));                      \
    }                                                                          \
  while (0)


/**
 * Version of OBSERT() that takes a logging code.
 */
#define OBSERT_CODE(code, x)                                                   \
  do                                                                           \
    {                                                                          \
      if (!(x))                                                                \
        OB_FATAL_BUG_CODE (code, "assertion failure:\n%s\n", #x);              \
    }                                                                          \
  while (0)


/**
 * Log to the specified file descriptor
 */
#define OB_DST_FD (1 << 1)

/**
 * Log to syslog()
 */
#define OB_DST_SYSLOG (1 << 2)

/**
 * Call the specified callback
 */
#define OB_DST_CALLBACK (1 << 3)

/**
 * Report this message via valgrind (and cause the valgrind run to fail)
 * XXX: This feature is not currently supported, due to valgrind changes.
 */
#define OB_DST_VALGRIND (1 << 4)

/**
 * Print out a stack trace along with the message
 */
#define OB_FLG_STACK_TRACE (1 << 30)

/**
 * Print out date and time along with the message
 */
#define OB_FLG_SHOW_TIME (1 << 29)

/**
 * Print the line number and full filename
 */
#define OB_FLG_SHOW_WHERE_FULL (1 << 28)

/**
 * Print the line number and file basename
 */
#define OB_FLG_SHOW_WHERE (1 << 27)

/**
 * Print the location code, if nonzero
 */
#define OB_FLG_SHOW_CODE (1 << 26)

/**
 * Print the location code, if nonzero, or line number and
 * file basename if code is zero
 */
#define OB_FLG_SHOW_CODE_OR_WHERE (1 << 25)

/**
 * Print the process ID
 */
#define OB_FLG_SHOW_PID (1 << 24)

/**
 * Print the program name
 */
#define OB_FLG_SHOW_PROG (1 << 23)

/**
 * Print the thread id
 */
#define OB_FLG_SHOW_TID (1 << 22)

/**
 * Print the thread id if it is not the main thread
 */
#define OB_FLG_SHOW_TID_NONMAIN (1 << 21)

typedef struct ob_log_rule
{
  int64 count;
  int64 maxcount;
  unt64 code;
  unt8 matchbits;  // 0-64
  bool uniquely;   // true means keep individual counts for each code matched
  bool notify;     // true means print a message when suppression kicks on
  struct ob_log_rule *next;     // normal chaining mechanism
  struct ob_log_rule *uniques;  // new rules created when "uniquely" is true
} ob_log_rule;

struct ob_log_file_rule;
typedef struct ob_log_file_rule ob_log_file_rule;

// Specify foreground and background colors as combinations of red, green,
// and blue (thus there are 8 colors).  If you want to set the foreground
// or background color, you must also OR in OB_FOREGROUND_ENABLE or
// OB_BACKGROUND_ENABLE (because otherwise there's no way to differentiate
// black from "I'm not specifying a color").
#ifdef _MSC_VER
// See http://msdn.microsoft.com/en-us/library/ms682013(VS.85).aspx
#define OB_FOREGROUND_BLUE    0x0001
#define OB_FOREGROUND_GREEN   0x0002
#define OB_FOREGROUND_RED     0x0004
#define OB_BACKGROUND_BLUE    0x0010
#define OB_BACKGROUND_GREEN   0x0020
#define OB_BACKGROUND_RED     0x0040
// {FOREGROUND,BACKGROUND}_{RED,GREEN,BLUE} are system-defined on Windows
// (in lower 16 bits, so we start above there)
#define OB_FOREGROUND_ENABLE 0x10000
#define OB_BACKGROUND_ENABLE 0x20000
#else  // UNIX, where we assume a terminal that takes ANSI color codes
#define OB_FOREGROUND_ENABLE  0x3000
#define OB_FOREGROUND_RED     0x3100
#define OB_FOREGROUND_GREEN   0x3200
#define OB_FOREGROUND_BLUE    0x3400
#define OB_BACKGROUND_ENABLE  0x0040
#define OB_BACKGROUND_RED     0x0041
#define OB_BACKGROUND_GREEN   0x0042
#define OB_BACKGROUND_BLUE    0x0044
#endif

typedef struct ob_log_level
{
  int32 flags;        // combination of OB_DST_* and OB_FLG_* flags
  int32 color;        // combination of FOREGROUND_* and BACKGROUND_* flags
  int32 sl_priority;  // syslog facility and severity, for OB_DST_SYSLOG
  int32 fd;           // file desciptor for OB_DST_FD
  char prefix[24];    // prefix string to indicate log level
  void (*callback) (const struct ob_log_level *lvl, unt64 code, float64 t,
                    int64 lnum, int64 lcount, const char *file, int line,
                    const char *msg, const char *fmtdmsg, const char *stack);
  void *cookie;  // for use by callback
  ob_log_rule *rules;
  ob_log_file_rule *frules;
} ob_log_level;

OB_LOAM_API extern ob_log_level oblv_bug, oblv_error, oblv_deprecation,
  oblv_wrnu, oblv_info, oblv_dbug;

/**
 * A programming error
 */
#define OBLV_BUG (&oblv_bug)

/**
 * An error caused by the user, or something external (file, network)
 */
#define OBLV_ERROR (&oblv_error)

/**
 * A warning about use of a deprecated function
 */
#define OBLV_DEPRECATION (&oblv_deprecation)

/**
 * A warning caused by the user, or something external (file, network)
 */
#define OBLV_WARN (&oblv_wrnu)

/**
 * Information (you might want to know, but you could safely ignore)
 */
#define OBLV_INFO (&oblv_info)

/**
 * Debugging (very verbose, normally not printed)
 */
#define OBLV_DEBUG (&oblv_dbug)

// old names, for compatibility
#define OBLV_ERRP (&oblv_bug)
#define OBLV_ERRU (&oblv_error)
#define OBLV_WRNP (&oblv_deprecation)
#define OBLV_WRNU (&oblv_wrnu)
#define OBLV_DBUG (&oblv_dbug)

/**
 * Same as ob_log_loc(), but the variable arguments are specified
 * as a va_list.  This allows you to write your own functions that
 * wrap the logging API.
 */
OB_LOAM_API void ob_log_loc_v (const char *file, int lineno, ob_log_level *lvl,
                               unt64 code, const char *fmt, va_list ap);

/**
 * Logs a message to the specified level \a lvl.  The message is
 * computed by formatting \a fmt as a printf-style format string, in
 * conjunction with the variable arguments that follow.  This
 * formatting is only done if \a lvl is enabled to go to some
 * destination, so the formatting overhead is avoided for log levels
 * which are disabled.  \a code is a unique identifier for this
 * message (see logging documentation for how these codes are
 * allocated and used), or 0 if you don't wish to specify a code.  \a
 * file and \a lineno are the location in the source code where this
 * log message came from; normally these are supplied automatically by
 * calling the ob_log() macro.  With the default log settings, \a file
 * and \a lineno are only printed if \a code is 0, but this behavior
 * can be configured.
 *
 * The formatted message is allowed to contain internal newlines, in
 * which case each line will be printed with an "(m/n)" prefix
 * indicating its position in the multiline message.  The message is
 * encouraged to end in a newline, but one will automatically be
 * provided if it is missing.
 */
OB_LOAM_API void ob_log_loc (const char *file, int lineno, ob_log_level *lvl,
                             unt64 code, const char *fmt, ...)
  OB_FORMAT (printf, 5, 6);

/**
 * Logs a message exactly like ob_log_loc(), and then terminates
 * the process in a way specified by \a exitcode.  If \a exitcode
 * is 0 or greater (although we recommend not using 0 since
 * that would look like success), ob_log_loc_fatal() calls
 * exit() with \a exitcode as the argument.  If \a exitcode is
 * less than 0, ob_log_loc_fatal() calls ob_abort_func, which
 * is typically abort().
 */
OB_LOAM_API OB_NORETURN void ob_log_loc_fatal (const char *file, int lineno,
                                               ob_log_level *lvl, unt64 code,
                                               int exitcode, const char *fmt,
                                               ...) OB_FORMAT (printf, 6, 7);

/**
 * Wrapper for ob_log_loc() which automatically supplies the file and
 * line number, but takes the same arguments starting at \a lvl.
 */
#define ob_log(...) ob_log_loc (__FILE__, __LINE__, __VA_ARGS__)

/**
 * Wrapper for ob_log_loc_fatal() which automatically supplies the
 * file and line number, but takes the same arguments starting at \a
 * lvl.
 */
#define ob_log_fatal(...) ob_log_loc_fatal (__FILE__, __LINE__, __VA_ARGS__)

/**
 * Conventional way of indicating programming errors.
 */
#define OB_LOG_BUG_CODE(code, ...) ob_log (OBLV_BUG, code, __VA_ARGS__)

/** Indicate that a function is deprecated. */
#define OB_LOG_DEPRECATION_CODE(code, ...)                                     \
  ob_log (OBLV_DEPRECATION, code, __VA_ARGS__)

/**
 * Conventional ways to log runtime messages which are not programming
 * errors.
 */
#define OB_LOG_ERROR_CODE(code, ...) ob_log (OBLV_ERROR, code, __VA_ARGS__)
#define OB_LOG_WARNING_CODE(code, ...) ob_log (OBLV_WRNU, code, __VA_ARGS__)
#define OB_LOG_INFO_CODE(code, ...) ob_log (OBLV_INFO, code, __VA_ARGS__)
#define OB_LOG_DEBUG_CODE(code, ...) ob_log (OBLV_DBUG, code, __VA_ARGS__)

/**
 * A variant of OB_LOG_BUG_CODE that will cause the program to abort().
 * Use ob_log_fatal instead with a positive exit code if you want to
 * terminate the program normally (via exit ()).
 */
#define OB_FATAL_BUG_CODE(code, ...)                                           \
  ob_log_fatal (OBLV_BUG, code, -1, __VA_ARGS__)

/**
 * A variant of OB_LOG_ERROR_CODE that will cause the program to exit
 * with a status of EXIT_FAILURE. Use ob_log_fatal instead if you want
 * to specify the exit code, or if you want to abort ().
 */
#define OB_FATAL_ERROR_CODE(code, ...)                                         \
  ob_log_fatal (OBLV_ERROR, code, EXIT_FAILURE, __VA_ARGS__)

/**
 * Convenient ways of logging with predefined categories. Use
 * OB_LOG_BUG for programming errors, and one of the other levels
 * otherwise.
 */
//@{
#define OB_FATAL_BUG(...) OB_FATAL_BUG_CODE (0, __VA_ARGS__)
#define OB_LOG_BUG(...) OB_LOG_BUG_CODE (0, __VA_ARGS__)
#define OB_LOG_DEPRECATION(...) OB_LOG_DEPRECATION_CODE (0, __VA_ARGS__)
#define OB_FATAL_ERROR(...) OB_FATAL_ERROR_CODE (0, __VA_ARGS__)
#define OB_LOG_ERROR(...) OB_LOG_ERROR_CODE (0, __VA_ARGS__)
#define OB_LOG_WARNING(...) OB_LOG_WARNING_CODE (0, __VA_ARGS__)
#define OB_LOG_INFO(...) OB_LOG_INFO_CODE (0, __VA_ARGS__)
#define OB_LOG_DEBUG(...) OB_LOG_DEBUG_CODE (0, __VA_ARGS__)
//@}

/**
 * Causes the message with the specified code to never be printed.
 * This is great for tests that test error conditions.
 */
OB_LOAM_API ob_retort ob_suppress_message (ob_log_level *lvl, unt64 code);

/**
 * Returns true if \a lvl is enabled; i. e. it is going to any
 * destination at all.  This shouldn't be needed very often, but is
 * primarily for the case where the arguments to a debugging log
 * statement are expensive to compute, so you want to avoid computing
 * them if the log statement will be a nop.
 *
 * Basically, this advice:
 * http://www.slf4j.org/faq.html#logging_performance
 * applies here, too.  Since ob_log uses "parameterized logging" (in
 * the form of printf-style formatting), the message is not actually
 * formatted unless it is going somewhere.  So as long as the
 * arguments themselves are cheap to compute, there's no reason to
 * wrap the log statement in a check.  But if an argument is expensive
 * to compute, like if you want to call slaw_spew_overview_to_string()
 * to print a slaw in a debugging message, then it's worth wrapping it
 * in a check of ob_log_is_level_enabled().
 */
static inline bool ob_log_is_level_enabled (const ob_log_level *lvl)
{
  int32 flags = lvl->flags;
  return (0 != (flags & (OB_DST_FD | OB_DST_SYSLOG | OB_DST_CALLBACK
                         | OB_DST_VALGRIND)));
}

/**
 * Returns true if \a file, \a lvl, or \a code is enabled; This is a
 * more detailed check to determine if a given message should be logged.
 * Like ob_log_is_level_enabled, this shouldn't be needed very often, but
 * is primarily for the case where the arguments to a debugging log
 * statement are expensive to compute, so you want to avoid computing
 * them if the log statement will be a nop. See ob_log_is_level_enabled for
 * more detail.
 */
OB_LOAM_API bool ob_log_is_enabled (const char *file, ob_log_level *lvl,
                                    unt64 code);

typedef void (*ob_abort_func_t) (void);

/**
 * This function pointer is called when ob_log_fatal() is called with
 * an \a exitcode of -1.  For other exitcodes, ob_log_fatal() calls
 * exit(), which you can hook into with atexit().  Since atexit()
 * functions are not called by abort(), ob_abort_func provides a way
 * to perform cleanup even if ob_log_fatal() calls abort().  \note
 * This is a deep dark magic sort of thing that most users shouldn't
 * need.
 */
OB_LOAM_API extern ob_abort_func_t ob_abort_func;

/**
 * Adds a file-name-matching rule to the specified log level \a lvl.
 * If a file-name-matching rule matches, then the log message is
 * either printed (if \a print is true) or not printed (if \a print is
 * false) without ever consulting the code-based rules.  The
 * code-based rules are only used if no file-name rule matches.
 *
 * \a pattern is a glob pattern which is compared against the "full"
 * (i. e. potentially with directory names) source file name of the
 * file which is logging the message.
 */
OB_LOAM_API ob_retort ob_log_add_file_rule (ob_log_level *lvl,
                                            const char *pattern, bool print);

/**
 * Add the code-based rule \a rul to level \a lvl.
 */
OB_LOAM_API ob_retort ob_log_add_rule (ob_log_level *lvl, ob_log_rule rul);

// key to location codes:
//
// Each of these directories should contain a file "LogCodeAssignments.txt"
// that explains how the "??????" portion is further divided up.
//
// To find the next available code within a particular file, run
// libLoam/c/next-code.pl on that file.
//
// XXX: This is primarily yovo-centric.  (Although it includes some things
// that used to be in yovo that have now moved elsewhere.)  Since the top
// 32-bits is 0 for all of these codes for yovo, perhaps we should assign
// other values for the top 32 bits for code in other repositories like
// perception and mezzanine.
//
// 0x00000000 = unknown location
// 0x10?????? = libLoam/c
// 0x11?????? = libLoam/c++
// 0x12?????? = libBasement
// 0x20?????? = libPlasma/c
// 0x21?????? = libPlasma/c++
// 0x22?????? = libPlasma/bench
// 0x23?????? = libPlasma/guile
// 0x24?????? = libPlasma/ruby
// 0x25?????? = system/libProtist
// 0x26?????? = libPlasma/zeroconf
// 0x40?????? = libMedia
// 0x41?????? = libMedia/GStreamer
// 0x50?????? = libNoodoo
// 0x51?????? = libImpetus
// 0x52?????? = libAfferent
// 0x54?????? = libTwillig
// 0x55?????? = libSplotch
// 0x56?????? = libResource
// 0x57?????? = libPix
// 0x58?????? = libNoodoo2
// 0x59?????? = libTwillig2
// 0x60?????? = projects/libCthulhu                 (obsolete)
// 0x61?????? = libGanglia
// 0x62?????? = libOuija
// 0x63?????? = projects/libStaging
// 0x70?????? = system/perception/libRetro          (in perception repo)
// 0x71?????? = system/perception/libPipeline/c++   (in perception repo)
// 0x72?????? = system/perception/libPipeline/utils (in perception repo)
// 0x73?????? = system/perception/oldapps           (in perception repo)
// 0x74?????? = projects/perks                      (in perception repo)
// 0x7f?????? = camspew                             (in perception repo)
// 0x83?????? = projects/quartermaster
// 0x84?????? = AVAILABLE
// 0x85?????? = AVAILABLE
// 0x86?????? = AVAILABLE                           (was projects/tapestry)
// 0x87?????? = AVAILABLE
// 0x88?????? = projects/video
// 0x93?????? = projects/bgt                        (in mezzanine repo)
// 0xA0?????? = hs-plasma

// These are just for backwards compatibility.  There are better-named
// macros above that do the same things.
/**
 * Convenient error reporting.  Each routine is basically printf(),
 * prepended with the file name and line number.
 *
 *   info_report() creates a message by prepending "info:", the file
 *   name, and the line number to the string specified by its
 *   printf-like arguments.  It prints this error message to the
 *   current log destination.
 *
 *   error_report() is like info_report but does not prepend "info:".
 *
 *   error_exit() is like error_report but calls exit(1).
 *
 *   error_abort() is like error_report calls abort().  This creates a
 *   crash dump if the calling environment allows it.
 */

#define info_report(...) ob_log (OBLV_INFO, 0, __VA_ARGS__)
#define error_report(...) ob_log (OBLV_ERROR, 0, __VA_ARGS__)
#define ob_dbg_printf(...) ob_log (OBLV_DBUG, 0, __VA_ARGS__)
#define error_exit(...) ob_log_fatal (OBLV_ERROR, 0, EXIT_FAILURE, __VA_ARGS__)
#define error_abort(...) ob_log_fatal (OBLV_BUG, 0, -1, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* OB_LOG_ROLLING */
