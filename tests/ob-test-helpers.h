
/* (c)  oblong industries */

#ifndef ob_test_helpers_h
#define ob_test_helpers_h

#include <stdlib.h>
#include <libLoam/c/ob-file.h>

/* Given the the path to the current source directory,
 * and the name of a test file relative to the current source directory,
 * and an output buffer,
 * return a path to that file that can be opened,
 * even in out-of-tree builds.
 * buf must be a buffer big enough to hold the path.
 */
static char *ob_test_source_relative3 (const char *abssrcdir,
                                       const char *fname,
                                       char *buf, size_t buf_len)
{
  snprintf (buf, buf_len, "%s/%s", abssrcdir, fname);
  return buf;
}

static char ob_find_file_name_buf[2048];

/* Given the the path to the current source directory
 * and the name of a test file relative to the current source directory,
 * return a path to that file that can be opened,
 * even in out-of-tree builds.
 * For use by tests run via by add_wrapped_test; see top-level CMakeLists.txt.
 * Result is valid until next call in this source file.
 */
static const char *ob_test_source_relative2 (const char *abssrcdir,
                                             const char *fname)
{
  return ob_test_source_relative3 (abssrcdir, fname,
                                   ob_find_file_name_buf,
                                   sizeof (ob_find_file_name_buf));
}

/* Read a file with a path relative to the top of the source tree
 * Returns a buffer that must be freed with free().
 */
static inline char *ob_read_file_srcdir (const char *fname)
{
  return ob_read_file (ob_test_source_relative2 (ABS_TOP_SRCDIR, fname));
}

#endif
