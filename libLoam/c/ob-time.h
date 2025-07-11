
/* (c)  oblong industries */

#ifndef OB_TIME_LORD
#define OB_TIME_LORD

#include "libLoam/c/ob-types.h"
#include "libLoam/c/ob-api.h"
#include "libLoam/c/ob-retorts.h"
#include "libLoam/c/ob-sys.h"      /* for struct timeval */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Returns the number of seconds since Epoch Time.  Resolution is
 * fractions of a second; exact resolution is OS-dependent.  (But on
 * UNIX-y systems is the same resolution as gettimeofday().
 * Resolution might be 15 milliseconds on Windows, or worse on
 * misbehaving virtual machines; see bug 840.)
 */
OB_LOAM_API float64 ob_current_time (void);

/**
 * Returns the number of nanoseconds since some fixed, but arbitrary,
 * point in time.  It will not be affected if the time-of-day is
 * adjusted (e. g. by NTP or by VMware Tools).  Therefore, it is a
 * good choice for measuring how long something takes, by calling it
 * before and after and taking the difference.
 *
 * Although the unit is nanoseconds, note that the resolution is
 * probably not nanoseconds... could be as coarse as 15 milliseconds
 * on Windows, for example.
 */
OB_LOAM_API unt64 ob_monotonic_time (void);

/**
 * sleeps for the specified # of micro_seconds. pass 0 to yield a
 * timeslice to other threads.
 * \note Always rounded up to next millisecond on Windows, plus
 * subject to Windows 15 millisecond resolution.  Also beware that any
 * OS may sleep for longer than you request.
 */
OB_LOAM_API ob_retort ob_micro_sleep (unt32 micro_seconds);

/**
 * Format the time, specified by \a tv, as a string into \a buf
 * (of length \a bufsiz).
 * Output buffer must be big enough to handle the expected output,
 * plus one byte for a nul terminator, plus one extra byte needed
 * internally, or the result is undefined.
 * 80 bytes is a safe output buffer size.
 *
 * The string will look something like "Dec 20, 2024 13:30:53.63 ".
 * For unknown/historical reasons, the string ends with a trailing
 * space.
 *
 * \note Although \a tv is in UTC, the string representation
 * is in local time.  Therefore, the current time zone is used
 * implicitly.
 */
OB_LOAM_API void ob_format_time (char *buf, size_t bufsiz,
                                 const struct timeval *tv);

/**
 * Format the time, specified by \a seconds since Epoch Time, as a string
 * into \a buf (of length \a bufsiz).
 *
 * The string will look something like "Dec 20, 2024 13:30:53.63 ".
 * For unknown/historical reasons, the string ends with a trailing
 * space.
 *
 * \note Although \a seconds is in UTC, the string representation
 * is in local time.  Therefore, the current time zone is used
 * implicitly.
 */
OB_LOAM_API void ob_format_time_f (char *buf, size_t bufsiz,
                                   const float64 seconds);

/**
 * Given a string \a s, formatted using ob_format_time or ob_format_time_f, set
 * the value of \a seconds since Epoch Time. Returns OB_OK on success.
 * IMPORTANT: Since ob_format_time clamps milliseconds to "%02d", precision is
 * lost. You should expect the following to be true:
 *
 *     char ctime [80];
 *     float64 echo_time, cur_time = ob_current_time ();
 *     ob_format_time_f (ctime, sizeof (ctime), cur_time);
 *     ob_strptime (ctime, &echo_time);
 *     EXPECT_LT (cur_time - echo_time, 0.01);
 *
 * The input string should look something like
 * "Dec 20, 2024 13:30:53.63".  Any trailing space will be ignored.
 *
 * \note Although \a seconds is in UTC, the string representation
 * is in local time.  Therefore, the current time zone is used
 * implicitly.
 */
OB_LOAM_API ob_retort ob_strptime (const char *s, float64 *seconds);

#ifdef __cplusplus
}
#endif

#endif /* OB_TIME_LORD */
