
/* (c)  oblong industries */

#ifndef SLAW_STRING_THEORY
#define SLAW_STRING_THEORY

#include "libLoam/c/ob-types.h"
#include "libLoam/c/ob-attrs.h"
#include "libLoam/c/ob-api.h"

#include "libPlasma/c/slaw.h"

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/**                 Creates a string which is the concatenation of all the
 *                  strings in sb.  If sep is non-NULL, inserts sep between
 *                  each element of sb.  (i. e. the effect is much like
 *                  "join($sep, @sb)" in Perl.)
 *
 * \param[in]       sb is a slabu which should only contain slaw strings.
 *
 * \param[in]       sep is the string to put between the elements of sb.
 *                  NULL is equivalent to the empty string.
 *
 * \return          newly allocated slaw string which is the concatenation
 *                  of the elements of sb with sep in between.
 * \return          NULL if allocation fails.
 */
OB_PLASMA_API slaw slaw_strings_join_slabu (slabu *sb, const char *sep);
OB_PLASMA_API slaw slaw_strings_join_slabu_f (slabu *sb, const char *sep);

/**                 Given a string and a separator, splits the string apart
 *                  at each occurrence of the separator, and produces a
 *                  slabu containing the resulting substrings.  This is
 *                  analogous to Perl's "split" function, and is the
 *                  opposite of slaw_strings_join_slabu().
 *
 * \param[in]       str is a NUL-terminated UTF-8 string to be split
 *
 * \param[in]       sep is a NUL-terminated UTF-8 string to be searched
 *                  for in str
 *
 * \return          a slabu of slaw strings
 */
OB_PLASMA_API slabu *slabu_of_strings_from_split (const char *str,
                                                  const char *sep);

/**
 * same as slaw_string_format, but takes a va_list
 */
OB_PLASMA_API slaw slaw_string_vformat (const char *fmt, va_list ap);

/**                 Create a new slaw string from a printf-style format.
 *
 * \param[in]       fmt is the printf-style format string
 *
 * \param[in]       ... is the printf-style arguments
 *
 * \return          formatted slaw string if successful, or NULL if not
 */
OB_PLASMA_API slaw slaw_string_format (const char *fmt, ...)
  OB_FORMAT (printf, 1, 2);

/**                 Creates a string which is the concatenation of all the
 *                  strings in strlist.  If sep is non-NULL, inserts sep between
 *                  each element of strlist.  (i. e. the effect is much like
 *                  "join($sep, @strlist)" in Perl.)
 *
 * \param[in]       strlist is a slaw list which should
 *                  only contain slaw strings.
 *
 * \param[in]       sep is the string to put between the elements of strlist.
 *                  NULL is equivalent to the empty string.
 *
 * \return          newly allocated slaw string which is the concatenation
 *                  of the elements of strlist with sep in between.
 * \return          NULL if allocation fails.
 */
OB_PLASMA_API slaw slaw_strings_join (bslaw strlist, const char *sep);
OB_PLASMA_API slaw slaw_strings_join_f (slaw strlist, const char *sep);

/**
 * Returns true if s is a slaw string and does not contain any
 * invalid byte sequences for UTF-8.
 */
OB_PLASMA_API bool slaw_string_is_valid_utf8 (bslaw s);

#ifdef __cplusplus
}
#endif

#endif /* SLAW_STRING_THEORY */
