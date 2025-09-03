
/* (c)  oblong industries */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE  // necessary to enable non-portable shenanigans on Linux
#endif

#include "libLoam/c/ob-util.h"
#include "libLoam/c/ob-sys.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-rand.h"
#include "libLoam/c/ob-dirs.h"
#include "libLoam/c/ob-file.h"
#include "libLoam/c/ob-string.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ob_retort ob_generate_uuid (char uu[37])
{
  /*
   * To avoid unnecessary dependencies, and since we already have a
   * high-quality source of random numbers, let's just generate
   * version 4 random UUIDs ourselves:
   *   http://en.wikipedia.org/wiki/Uuid#Version_4_.28random.29
   */
  size_t i;
  unt8 bites[18];
  ob_retort tort = ob_truly_random (bites, sizeof (bites));
  if (tort < OB_OK)
    return tort;
  bites[9] |= 0x80;
  bites[9] &= 0xbf;
  for (i = 0; i < 2 * sizeof (bites); i++)
    {
      unt8 n = (bites[i >> 1] >> ((i & 1) * 4));
      n &= 0xf;
      if (n < 10)
        n += '0';
      else
        n += 'a' - 10;
      uu[i] = n;
    }
  uu[i] = 0;
  uu[8] = uu[13] = uu[18] = uu[23] = '-';
  uu[14] = '4';
  char check = uu[19];
  if (check != '8' && check != '9' && check != 'a' && check != 'b')
    return OB_UNKNOWN_ERR; /* an assertion; should never happen */
  else
    return OB_OK;
}

static int saved_argc = 0;
static char **saved_argv = NULL;

void ob_set_program_arguments (int argc, const char *const *argv)
{
  int i;

  if (saved_argv)
    {
      OB_LOG_WARNING_CODE (0x10020008,
                           "existing program arguments being overwritten\n");
      for (i = 0; i < saved_argc; i++)
        free (saved_argv[i]);

      free (saved_argv);
    }

  saved_argc = argc;
  // 1 + argc so that we have an additional NULL pointer after the end.
  // (Initialized to NULL thanks to calloc.)
  // This is so that the retrieved argv obeys the same convention as the
  // argv passed to main:
  // http://publications.gbdirect.co.uk/c_book/chapter10/arguments_to_main.html
  // http://lkml.indiana.edu/hypermail/linux/kernel/0409.2/0287.html
  saved_argv = (char **) calloc (1 + argc, sizeof (char *));
  for (i = 0; i < argc; i++)
    if (argv[i])
      saved_argv[i] = strdup (argv[i]);
}

void ob_get_program_arguments (int *argc, const char *const **argv)
{
  *argc = saved_argc;
  // This cast would not be needed in C++
  // http://c-faq.com/ansi/constmismatch.html
  *argv = (const char *const *) saved_argv;
}

#ifndef _MSC_VER
#include <pwd.h>
#include <grp.h>
#endif

/* Ram discovered that _SC_GETPW_R_SIZE_MAX and _SC_GETGR_R_SIZE_MAX are
 * not so much "max", as just "completely arbitrary guess."  In the case
 * of a group with 82 members, they are too small, and the call fails.
 * Ram suggested adding 1024.  But, what about a company with thousands
 * of employees?  What then?  Of course, no number can ever be enough,
 * but I at least picked a big one.
 */

#define FUDGE 1000000

ob_retort ob_uid_from_name (const char *name, int *uid)
{
#ifdef _MSC_VER
  OB_LOG_BUG_CODE (0x10020009,
                   "Not implemented on windows: ob_uid_from_name\n");
  return OB_UNKNOWN_ERR;
#else

  /* Use "long" because it is the return type of sysconf().
   * Note that it must be a signed type (and therefore, specifically
   * must not be "size_t") in order to handle the return value -1. */
  long bufsize = sysconf (_SC_GETPW_R_SIZE_MAX);
  if (bufsize <= 0)
    {
      ob_log (OBLV_INFO, 0x10020000,
              "sysconf (_SC_GETPW_R_SIZE_MAX) wasn't meaningful\n");
      bufsize = 4096;
    }
  bufsize += FUDGE;

  char *buf = (char *) malloc (bufsize);
  if (!buf)
    return OB_NO_MEM;

  struct passwd presult;
  struct passwd *result;
  int erryes = getpwnam_r (name, &presult, buf, bufsize, &result);
  if (0 != erryes)
    {
      OB_LOG_ERROR_CODE (0x10020001, "getpwnam_r (%s) returned %s\n", name,
                         strerror (erryes));
      free (buf);
      return ob_errno_to_retort (erryes);
    }

  if (!result)
    {
      free (buf);
      return OB_NOT_FOUND;
    }

  *uid = result->pw_uid;
  free (buf);
  return OB_OK;

#endif
}

ob_retort ob_gid_from_name (const char *name, int *gid)
{
#ifdef _MSC_VER
  OB_LOG_BUG_CODE (0x1002000a,
                   "Not implemented on windows: ob_gid_from_name\n");
  return OB_UNKNOWN_ERR;
#else

  /* Use "long" because it is the return type of sysconf().
   * Note that it must be a signed type (and therefore, specifically
   * must not be "size_t") in order to handle the return value -1. */
  long bufsize = sysconf (_SC_GETGR_R_SIZE_MAX);
  if (bufsize <= 0)
    {
      ob_log (OBLV_INFO, 0x10020002,
              "sysconf (_SC_GETGR_R_SIZE_MAX) wasn't meaningful\n");
      bufsize = 4096;
    }
  bufsize += FUDGE;

  char *buf = (char *) malloc (bufsize);
  if (!buf)
    return OB_NO_MEM;

  struct group presult;
  struct group *result;
  int erryes = getgrnam_r (name, &presult, buf, bufsize, &result);
  if (0 != erryes)
    {
      OB_LOG_ERROR_CODE (0x10020003, "getgrnam_r (%s) returned %s\n", name,
                         strerror (erryes));
      free (buf);
      return ob_errno_to_retort (erryes);
    }

  if (!result)
    {
      free (buf);
      return OB_NOT_FOUND;
    }

  *gid = result->gr_gid;
  free (buf);
  return OB_OK;

#endif
}

void ob_nop (void)
{
}

void ob_ignore (OB_UNUSED int64 foo, ...)
{
}

#if defined(__gnu_linux__)
#include <sys/syscall.h>
#elif defined(__APPLE__)
#include <pthread.h>
#elif defined(_MSC_VER)
static /* const */ unt32 main_thread_id;
OB_PRE_POST (main_thread_id = GetCurrentThreadId (), ob_nop ());
#endif

void ob_get_small_integer_thread_id (unt32 *tid, bool *is_main)
{
#if defined(__gnu_linux__)
  *tid = syscall (SYS_gettid);
  *is_main = ((pid_t) *tid == getpid ());
#elif defined(__APPLE__)
  *tid = pthread_mach_thread_np (pthread_self ());
  *is_main = pthread_main_np ();
#elif defined(_MSC_VER)
  *tid = GetCurrentThreadId ();
  *is_main = (*tid == main_thread_id);
#else
  *tid = 0;
  *is_main = true;
#endif
}

const char *ob_get_user_name (void)
{
  static char userName[1024];

  /* userName is all zeroes initially, so if it's not, we've already set it */
  if (userName[0] != 0)
    return userName;

  int userNameLen = sizeof (userName);

#ifdef _MSC_VER
  if (GetUserName (userName, &userNameLen))
    return userName;
#else
  struct passwd presult;
  struct passwd *result;
  int erryes = getpwuid_r (getuid (), &presult, userName, userNameLen, &result);
  if (0 == erryes && result)
    {
      ob_safe_copy_string (userName, sizeof (userName), presult.pw_name);
      return userName;
    }
#endif

  return "unknown";
}

#ifdef _MSC_VER
static const char *w32_get_prog_name (void)
{
  static char buf[1024];
  static const char *w32_prog_name = NULL;
  if (w32_prog_name)
    {
      /* Return previously computed pointer into buf */
      /* not quite threadsafe, but previous technique required C++ */
      return w32_prog_name;
    }
  const char *cmdline = GetCommandLine ();
  char term = ' ';         // argv[0] ends with a space, or ...
  if (cmdline[0] == '\"')  // ... is enclosed in quotes
    {
      term = '\"';
      cmdline++;
    }
  ob_safe_copy_string (buf, sizeof (buf), cmdline);
  char *p = strchr (buf, term);
  if (p)
    *p = 0;
  return ob_basename (buf);
}

#endif /* _MSC_VER */

// Limiting the length shouldn't be a problem, since the name is
// supposed to be something short you can prefix error messages with.
static char overridden_name[80];

const char *ob_get_prog_name (void)
{
  const char *name = NULL;
  if (overridden_name[0])
    return overridden_name;
#if defined(__gnu_linux__)
  name = program_invocation_short_name;
#elif defined(__APPLE__)
  name = getprogname ();
#elif defined(_MSC_VER)
  name = w32_get_prog_name ();
#endif
  if (!name || !*name)
    {
      int argc;
      const char *const *argv;
      ob_get_program_arguments (&argc, &argv);
      if (argc > 0 && argv && argv[0] && *(argv[0]))
        name = ob_basename (argv[0]);
    }

  if (!name || !*name)
    name = "<program name unknown>";

  return name;
}

void ob_set_prog_name (const char *newname)
{
  if (newname)
    ob_safe_copy_string (overridden_name, sizeof (overridden_name), newname);
  else
    overridden_name[0] = 0;
}

ob_retort ob_setenv (const char *name, const char *value)
{
#ifdef _MSC_VER
  const errno_t erryes = _putenv_s (name, value);
  return (erryes ? ob_errno_to_retort (erryes) : OB_OK);
#else
  if (setenv (name, value, true) < 0)
    return ob_errno_to_retort (errno);
  else
    return OB_OK;
#endif
}

ob_retort ob_unsetenv (const char *name)
{
#ifdef _MSC_VER
  const errno_t erryes = _putenv_s (name, "");
  return (erryes ? ob_errno_to_retort (erryes) : OB_OK);
#else
  if (unsetenv (name) < 0)
    return ob_errno_to_retort (errno);
  else
    return OB_OK;
#endif
}

ob_retort ob_append_env_list (const char *name, const char *value)
{
  ob_retort err;
  const char *oldval = getenv (name);
  if (!oldval)
    return ob_setenv (name, value);

  char *buf = malloc (strlen (value) + 1 + strlen (oldval) + 1);
  if (!buf)
    return OB_NO_MEM;
  sprintf (buf, "%s%c%s", oldval, OB_PATH_CHAR, value);

  err = ob_setenv (name, buf);
  free (buf);
  return err;
}

ob_retort ob_prepend_env_list (const char *name, const char *value)
{
  ob_retort err;
  const char *oldval = getenv (name);
  if (!oldval)
    return ob_setenv (name, value);

  char *buf = malloc (strlen (value) + 1 + strlen (oldval) + 1);
  if (!buf)
    return OB_NO_MEM;
  sprintf (buf, "%s%c%s", value, OB_PATH_CHAR, oldval);

  err = ob_setenv (name, buf);
  free (buf);
  return err;
}

#ifdef __GNUC__
// valgrind.h uses gcc extensions, so we must not use it on other compilers
#include "libLoam/c/valgrind/valgrind.h"
#include "libLoam/c/valgrind/memcheck.h"
#endif

bool ob_running_under_valgrind (void)
{
#ifdef __GNUC__
  return !!(RUNNING_ON_VALGRIND);
#else
  return false;
#endif
}

void *ob_make_undefined (void *addr, size_t len)
{
  memset (addr, '?', len);
#ifdef __GNUC__
  (void) VALGRIND_MAKE_MEM_UNDEFINED (addr, len);
#endif
  return addr;
}

static const char hexdigs[16] = "0123456789abcdef";

void ob_fmt_hex_line (char        dst[OB_HEX_LINE_LEN],
                      const unt8 *src,
                      size_t      srcLen)
{
  size_t hexIdx = 0;
  size_t ascIdx = 50;
  size_t i;

  memset (dst, ' ', OB_HEX_LINE_LEN);
  dst[ascIdx++] = '|';

  for (i = 0; i < 16 && i < srcLen; i++)
    {
      const unt8 b  = src[i];
      const unt8 lo = b & 0xf;
      const unt8 hi = b >> 4;
      char       c  = '.';

      if (i == 8)
        hexIdx++;

      dst[hexIdx    ] = hexdigs[hi];
      dst[hexIdx + 1] = hexdigs[lo];
      hexIdx += 3;

      if (b >= ' ' && b <= '~')
        c = (char) b;

      dst[ascIdx++] = c;
    }

  dst[ascIdx++] = '|';
  dst[ascIdx  ] = 0;
}
