
/* (c)  oblong industries */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* for O_CLOEXEC and friends */
#endif
#include "libLoam/c/ob-file.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-sys.h"
#include "libLoam/c/ob-util.h"
#include "libLoam/c/ob-vers.h"
#include "libLoam/c/ob-dirs.h"
#include "libLoam/c/ob-rand.h"

#include <stdio.h>

char *ob_read_file (const char *filename)
{
  long ft_ret;
  size_t fileSize;
  char *s;

  FILE *f = ob_fopen_cloexec (filename, "rb");
  if (f == NULL)
    return NULL;

  if (0 != fseek (f, 0, SEEK_END))
    goto tragic;
  // ftell() returns a long, which can hold -1, but size_t is unsigned
  ft_ret = ftell (f);
  if (ft_ret < 0)
    goto tragic;
  fileSize = (size_t) ft_ret;
  if (0 != fseek (f, 0, SEEK_SET))
    goto tragic;

  // Files in the /proc filesystem claim to have size 0, even though they
  // contain data that can be read.  And files in the /sys filesystem are
  // even weirder, because they claim to have size 4096, even though they
  // probably contain less data.
  // ext4 directories have max offset LONG_MAX, which would confuse malloc;
  // see EXT4_HTREE_EOF_64BIT in the Linux kernel source.  (We noticed
  // because one of our unit tests tickles this, causing a memory leak.)
  // FIXME: add upper limit on file size here.
  // But since they don't fail seeking, there
  // doesn't seem to be a good way to detect these files.  (Could try to
  // check the filename for /sys or /proc, although this could fail in some
  // cases, e. g. symlinks.)  So just special-case these two sizes.  Of
  // course, an ordinary file could also be 0 or 4096 bytes, but that's
  // fine; this will still work for them, just a tiny bit slower.
  if (fileSize == 0 || fileSize == 4096 || fileSize == LONG_MAX)
    goto funny_file;

  s = (char *) malloc (1 + fileSize);
  if (!s)
    {
      fclose (f);
      return NULL;
    }
  if (fread (s, 1, fileSize, f) != fileSize)
    {
      fclose (f);
      free (s);
      return NULL;
    }
  OB_CHECK_POSIX_CODE (0x1002000c, fclose (f));
  s[fileSize] = 0;  // NUL terminate
  return s;

tragic:
  if (errno != ESPIPE)
    {
      OB_LOG_ERROR_CODE (0x10020004, "fseek or ftell on %s returned %d: %s\n",
                         filename, errno, strerror (errno));
      fclose (f);
      return NULL;
    }

  // If we try reading from a pipe, (like if stdin is a pipe and filename
  // is "/dev/fd/0", then we will get ESPIPE and fall thru to here.)
  // But, need to clear error condition before we can try again...
  clearerr (f);
funny_file:
  // At this point, we can depend on f being open and s not being allocated.
  fileSize = 0;
  size_t capacity = 128;
  s = (char *) malloc (capacity);

  for (;;)
    {
      if (!s)  // malloc or realloc failed
        {
          fclose (f);
          return NULL;
        }
      size_t nread = fread (s + fileSize, 1, capacity - fileSize, f);
      if (nread == 0)
        break;  // error or end of file
      fileSize += nread;
      if (fileSize < capacity)
        continue;
      capacity *= 2;
      char *prev = s;
      s = (char *) realloc (s, capacity);
      if (!s)         // realloc doesn't free old memory on failure...
        free (prev);  // ...so we do, and then error is handled at top of loop
    }

  int erryes = errno;
  bool isEof = feof (f);
  bool isError = ferror (f);
  OB_CHECK_POSIX_CODE (0x1002000d, fclose (f));

  if (isEof)
    {
      char *prev = s;
      // potentially grow (for NUL terminator) or shrink the buffer
      s = (char *) realloc (s, fileSize + 1);
      if (!s)
        {
          free (prev);
          return NULL;
        }
      // NUL terminate the string
      s[fileSize] = 0;
      return s;
    }

  if (isError)
    OB_LOG_ERROR_CODE (0x10020005, "%s while reading %s\n", strerror (erryes),
                       filename);
  else
    OB_LOG_ERROR_CODE (0x10020006,
                       "shouldn't have happened: neither feof nor "
                       "ferror on %s\n",
                       filename);

  free (s);
  return NULL;
}

ob_retort ob_read_binary_file (const char *path, unt8 **buf, unt64 *len)
{
  int fd = ob_open_cloexec (path, O_RDONLY | OB_O_BINARY, 0);
  if (fd <= -1)
    return ob_errno_to_retort (errno);
  off_t olen = lseek (fd, 0, SEEK_END);
  if (olen < 0)
    {
      const int erryes = errno;
      close (fd);
      return ob_errno_to_retort (erryes);
    }
  /* Funny story: On Raspberry Pi, lseek seems to return 2147483647
   * on error, instead of -1.  Darned if I know why, but let's go
   * with it.  Unfortunately, though, it doesn't seem to set errno,
   * so we don't actually know what the error is.  But I think maybe
   * this only happens on directories, so let's try EISDIR.
   * I think this may be relevant:
   * http://marc.info/?l=reiserfs-devel&m=112515355929626&w=2 */
  if (olen == LONG_MAX)
    {
      close (fd);
      return ob_errno_to_retort (EISDIR);
    }
  *len = olen;
  if (lseek (fd, 0, SEEK_SET) < 0)
    {
      const int erryes = errno;
      close (fd);
      return ob_errno_to_retort (erryes);
    }

  *buf = (unt8 *) malloc (*len);
  if (!*buf)
    {
      close (fd);
      return OB_NO_MEM;
    }
  unt8 *ptr = *buf;
  size_t remaining = *len;
  while (remaining)
    {
      ssize_t n = read (fd, ptr, remaining);
      if (n < 0)
        {
          if (errno == EINTR)
            continue;
          else
            {
              int s = errno;
              free (*buf);
              *buf = NULL;
              close (fd);
              return ob_errno_to_retort (s);
            }
        }
      else if (n == 0)
        {
          ob_log (OBLV_WRNU, 0x1002000b,
                  "warning: expected to read %" OB_FMT_64 "u more from %s\n",
                  (unt64) remaining, path);
          *len -= remaining;
          break;
        }
      else
        {
          ptr += n;
          remaining -= n;
          break;
        }
    }
  if (close (fd) < 0)
    return ob_errno_to_retort (errno);
  else
    return OB_OK;
}

ob_retort ob_pipe (int fildes[2])
{
#ifdef _MSC_VER
  if (_pipe (fildes, 16384, _O_BINARY) < 0)
    return ob_errno_to_retort (errno);
  else
    return OB_OK;
#elif 0  // an alternate, lower-level way on Windows
  HANDLE h[2];
  if (!CreatePipe (&(h[0]), &(h[1]), NULL, 16384))
    return ob_win32err_to_retort (GetLastError ());
  for (int i = 0; i < 2; i++)
    fildes[i] = _open_osfhandle ((intptr_t) h[i], (i ? 0 : _O_RDONLY));
  return (fildes[0] >= 0 && fildes[1] >= 0) ? OB_OK : OB_UNKNOWN_ERR;
#else
  if (pipe (fildes) < 0)
    return ob_errno_to_retort (errno);
  else
    return OB_OK;
#endif
}

/* So why all this complexity?  We can create close-on-exec file
 * descriptors portably on all UNIXish systems by first creating the
 * descriptor and then using fcntl() to mark it as close-on-exec.
 * However, this is not atomic, and so presents a potential problem
 * in multithreaded programs.  (And although we claim to abhor threads
 * at Oblong, a number of our programs actually are multithreaded.)
 * Linux has introduced a solution to this problem:
 *
 * http://lwn.net/Articles/249006/
 *
 * So, we make use of this solution if it is available.  However,
 * if it is not available (such as on Mac OS X, or on a version of
 * Linux which is too old, like RHEL 5.x) then we fall back to
 * the portable-but-not-atomic fcntl() solution.
 *
 * To be thorough, we have to test for the presence of these features
 * both at compile time (to make sure the necessary functions and
 * constants are available) and at run time, because we could theoretically
 * be run on a kernel which is older than the one we were built on:
 *
 * https://issues.apache.org/bugzilla/show_bug.cgi?id=47662
 *
 * (Not to mention that running under valgrind is a bit like running
 * under an older kernel, since valgrind might not support all the system
 * calls the kernel supports.)
 *
 * So that's why there are all these compile-time and runtime checks.
 */
#if defined(__GLIBC__) && defined(__GLIBC_MINOR__)
#if __GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 9)
#define CAN_USE_DUP3_PIPE2 1
#endif /* __GLIBC__ > 2  ||  (__GLIBC__ == 2  &&  __GLIBC_MINOR__ >= 9) */
#endif /* defined (__GLIBC__)  &&  defined (__GLIBC_MINOR__) */

#if defined(O_CLOEXEC) || defined(SOCK_CLOEXEC)
// Cache the results of whether certain features are supported.
// Although it's possible that two threads could simultaneously
// see PP_MAYBE and both go off and compute the result, that's
// okay, because they'll both eventually set it to the same thing.
// (So the second thread to finish is harmlessly redundant.)
typedef enum
{
  PP_NO,
  PP_YES,
  PP_MAYBE
} possibly;

// Parse a dotted version string into up to three components.
// Returns the number of components parsed.
// Destructively modifies the string passed to it.
static int parse_version (char *vs, int v[3])
{
  v[0] = v[1] = v[2] = 0;

  char *s;
  int n = 0;
  while (NULL != (s = strsep (&vs, ".")))
    {
      char *e;
      v[n] = strtol (s, &e, 10);
      n++;
      if (n == 3 || *e)
        break;
    }

  return n;
}

// For 2.6.x kernels, returns x.
// For older kernels, returns 0.
// For newer kernels, returns 100.
// Also returns 0 if running under valgrind, because valgrind might not
// support all the system calls the underlying kernel supports, and there
// doesn't seem to be any good way to query valgrind to find out what
// version it is or what system calls it supports.
static int kernel_version (void)
{
  if (ob_running_under_valgrind ())
    return 0;

  char *vs = ob_get_version (OB_VERSION_OF_KERNEL);
  if (!vs)
    return 0;

  int v[3];
  const int n = parse_version (vs, v);
  free (vs);

  if (n < 3 || v[0] < 2)
    return 0;
  if (v[0] > 2)
    return 100;

  if (v[1] < 6)
    return 0;
  if (v[1] > 6)
    return 100;

  return v[2];
}

// For glibc 2.x, returns x.
// For old glibcs, returns 0.
// For newer glibcs, returns 100.
static int glibc_version (void)
{
  char *vs = ob_get_version (OB_VERSION_OF_LIBC);
  if (!vs || 0 != strncmp (vs, "glibc ", 6))
    return 0;

  int v[3];
  const int n = parse_version (vs + 6, v);
  free (vs);

  if (n < 2 || v[0] < 2)
    return 0;
  if (v[0] > 2)
    return 100;

  return v[1];
}

static void inform (possibly p, const char *what)
{
  OB_LOG_DEBUG_CODE (0x1002000e, "%s %s\n",
                     (p == PP_YES ? "supports" : "does not support"), what);
}
#endif /* defined (O_CLOEXEC)  ||  defined (SOCK_CLOEXEC) */

#ifdef O_CLOEXEC
static bool supports_o_cloexec (void)
{
  static volatile possibly pp = PP_MAYBE;
  possibly p = pp;

  if (p == PP_MAYBE)
    {
      p = PP_NO;
      if (kernel_version () >= 23) /* since kernel 2.6.23 */
        p = PP_YES;
      pp = p;
      inform (p, "O_CLOEXEC");
    }

  return (bool) p;
}

static bool supports_e_in_fopen (void)
{
  static volatile possibly pp = PP_MAYBE;
  possibly p = pp;

  if (p == PP_MAYBE)
    {
      p = PP_NO;
      if (supports_o_cloexec ())
        if (glibc_version () >= 9) /* since glibc 2.7 */
          p = PP_YES;
      pp = p;
      inform (p, "'e' in fopen()");
    }

  return (bool) p;
}
#define FOPEN_CLOEXEC_CHAR 'e'
#elif defined(_MSC_VER)
#define supports_o_cloexec() true
#define supports_e_in_fopen() true
#define O_CLOEXEC _O_NOINHERIT
#define FOPEN_CLOEXEC_CHAR 'N'
#endif /* O_CLOEXEC */

#ifdef F_DUPFD_CLOEXEC
static bool supports_dupfd_cloexec (void)
{
  static volatile possibly pp = PP_MAYBE;
  possibly p = pp;

  if (p == PP_MAYBE)
    {
      p = PP_NO;
      if (kernel_version () >= 24) /* since kernel 2.6.24 */
        p = PP_YES;
      pp = p;
      inform (p, "F_DUPFD_CLOEXEC");
    }

  return (bool) p;
}
#endif /* F_DUPFD_CLOEXEC */

#ifdef CAN_USE_DUP3_PIPE2
static bool supports_dup3_pipe2 (void)
{
  static volatile possibly pp = PP_MAYBE;
  possibly p = pp;

  if (p == PP_MAYBE)
    {
      p = PP_NO;
      if (kernel_version () >= 27) /* since kernel 2.6.27 */
        if (glibc_version () >= 9) /* since glibc 2.9 */
          p = PP_YES;
      pp = p;
      inform (p, "dup3() and pipe2()");
    }

  return (bool) p;
}
#endif /* CAN_USE_DUP3_PIPE2 */

#ifdef SOCK_CLOEXEC
static bool supports_sock_cloexec (void)
{
  static volatile possibly pp = PP_MAYBE;
  possibly p = pp;

  if (p == PP_MAYBE)
    {
      p = PP_NO;
      if (kernel_version () >= 27) /* since kernel 2.6.27 */
        p = PP_YES;
      pp = p;
      inform (p, "SOCK_CLOEXEC");
    }

  return (bool) p;
}
#endif /* SOCK_CLOEXEC */

int ob_open_cloexec (const char *path, int oflag, int mode)
{
#ifdef O_CLOEXEC
  if (supports_o_cloexec ())
    return open (path, oflag | O_CLOEXEC, mode);
#endif
  int fd = open (path, oflag, mode);
  if (fd < 0)
    return fd;
#ifdef FD_CLOEXEC
  int r = fcntl (fd, F_SETFD, FD_CLOEXEC);
  if (r == -1)
    {
      const int erryes = errno;
      close (fd);
      errno = erryes;
      return -1;
    }
#endif
  return fd;
}

FILE *ob_fopen_cloexec (const char *filename, const char *mode)
{
#ifdef O_CLOEXEC
  if (supports_e_in_fopen ())
    {
      const size_t len = strlen (mode);
      char *newmode = (char *) alloca (2 + len);
      memcpy (newmode, mode, len);
      newmode[len] = FOPEN_CLOEXEC_CHAR;
      newmode[len + 1] = 0;
      return fopen (filename, newmode);
    }
#endif
  FILE *f = fopen (filename, mode);
  if (!f)
    return f;
#ifdef FD_CLOEXEC
  int r = fcntl (fileno (f), F_SETFD, FD_CLOEXEC);
  if (r == -1)
    {
      const int erryes = errno;
      fclose (f);
      errno = erryes;
      return NULL;
    }
#endif
  return f;
}

int ob_dup_cloexec (int oldfd)
{
#ifdef F_DUPFD_CLOEXEC
  long arg = 20;
  if (supports_dupfd_cloexec ())
    return fcntl (oldfd, F_DUPFD_CLOEXEC, arg);
#endif
  int fd = dup (oldfd);
  if (fd < 0)
    return fd;
#ifdef FD_CLOEXEC
  int r = fcntl (fd, F_SETFD, FD_CLOEXEC);
  if (r == -1)
    {
      const int erryes = errno;
      close (fd);
      errno = erryes;
      return -1;
    }
#endif
  return fd;
}

int ob_dup2_cloexec (int oldfd, int newfd)
{
#ifdef CAN_USE_DUP3_PIPE2
  if (supports_dup3_pipe2 ())
    return dup3 (oldfd, newfd, O_CLOEXEC);
#endif
  int fd = dup2 (oldfd, newfd);
  if (fd < 0)
    return fd;
#ifdef FD_CLOEXEC
  int r = fcntl (fd, F_SETFD, FD_CLOEXEC);
  if (r == -1)
    {
      const int erryes = errno;
      close (fd);
      errno = erryes;
      return -1;
    }
#endif
  return fd;
}

int ob_socket_cloexec (int domain, int type, int protocol)
{
#ifdef SOCK_CLOEXEC
  if (supports_sock_cloexec ())
    return socket (domain, type | SOCK_CLOEXEC, protocol);
#endif
  int fd = socket (domain, type, protocol);
  if (fd < 0)
    return fd;
#ifdef FD_CLOEXEC
  int r = fcntl (fd, F_SETFD, FD_CLOEXEC);
  if (r == -1)
    {
      const int erryes = errno;
      close (fd);
      errno = erryes;
      return -1;
    }
#endif
  return fd;
}

#ifdef _MSC_VER
static ob_retort ob_evutil_ersatz_socketpair_ (int family, int type,
                                               int protocol, int fd[2]);
#endif

ob_retort ob_socketpair_cloexec (int domain, int type, int protocol,
                                 int socket_vector[2])
{
#ifdef _MSC_VER
  return ob_evutil_ersatz_socketpair_ (domain, type, protocol, socket_vector);
#else
#ifdef SOCK_CLOEXEC
  if (supports_sock_cloexec ())
    {
      int ret =
        socketpair (domain, type | SOCK_CLOEXEC, protocol, socket_vector);
      if (ret < 0)
        return ob_errno_to_retort (errno);
      else
        return OB_OK;
    }
#endif /* SOCK_CLOEXEC */
  int ret = socketpair (domain, type, protocol, socket_vector);
  if (ret < 0)
    return ob_errno_to_retort (errno);
  int i;
  for (i = 0; i < 2; i++)
    {
      int r = fcntl (socket_vector[i], F_SETFD, FD_CLOEXEC);
      if (r == -1)
        {
          const int erryes = errno;
          int j;
          for (j = 0; j < 2; j++)
            close (socket_vector[j]);
          return ob_errno_to_retort (erryes);
        }
    }
  return OB_OK;
#endif /* _MSC_VER */
}

ob_retort ob_pipe_cloexec (int fildes[2])
{
#ifdef _MSC_VER
  if (_pipe (fildes, 16384, _O_BINARY | _O_NOINHERIT) < 0)
    return ob_errno_to_retort (errno);
  else
    return OB_OK;
#else
#ifdef CAN_USE_DUP3_PIPE2
  if (supports_dup3_pipe2 ())
    return pipe2 (fildes, O_CLOEXEC);
#endif /* CAN_USE_DUP3_PIPE2 */
  ob_retort tort = ob_pipe (fildes);
  if (tort < OB_OK)
    return tort;
#ifdef FD_CLOEXEC
  int i;
  for (i = 0; i < 2; i++)
    {
      int r = fcntl (fildes[i], F_SETFD, FD_CLOEXEC);
      if (r == -1)
        {
          const int erryes = errno;
          int j;
          for (j = 0; j < 2; j++)
            close (fildes[j]);
          return ob_errno_to_retort (erryes);
        }
    }
#endif /* FD_CLOEXEC */
  return OB_OK;
#endif /* _MSC_VER */
}

ob_retort ob_realpath (const char *path, char **real_ret)
{
#ifdef __gnu_linux__
  char *real = realpath (path, NULL);
  if (real)
    {
      *real_ret = real;
      return OB_OK;
    }
  else
    return ob_errno_to_retort (errno);
#else
  char buf[PATH_MAX + 1024];
#ifdef _MSC_VER
  char fub[PATH_MAX + 1024];
  DWORD ret = GetFullPathName (path, sizeof (fub), fub, NULL);
  if (ret == 0 || ret >= sizeof (fub))
    return ob_win32err_to_retort (GetLastError ());
  ret = GetLongPathName (fub, buf, sizeof (buf));
  if (ret == 0 || ret >= sizeof (buf))
    return ob_win32err_to_retort (GetLastError ());
#else
  if (!realpath (path, buf))
    return ob_errno_to_retort (errno);
#endif
  char *real = strdup (buf);
  if (real)
    {
      *real_ret = real;
      return OB_OK;
    }
  else
    return OB_NO_MEM;
#endif
}

ob_retort ob_mkstemp (const char *prefix, char **name_ret, int *fd_ret,
                      OB_UNUSED bool binary)
{
  const char *tmpdir = ob_get_standard_path (ob_tmp_dir);
  if (!tmpdir)
    tmpdir = ".";

  if (!prefix)
    prefix = "gs-";

  const size_t td_len = strlen (tmpdir);
  const size_t pf_len = strlen (prefix);
  const size_t before_x_len = td_len + pf_len + 1;
  const size_t x_len = 6;
  char *name = (char *) malloc (1 + before_x_len + x_len);
  if (!name)
    return OB_NO_MEM;

  memcpy (name, tmpdir, td_len);
  name[td_len] = OB_DIR_CHAR;
  memcpy (name + td_len + 1, prefix, pf_len);
  memset (name + before_x_len, 'X', x_len);
  name[before_x_len + x_len] = 0;

#ifdef _MSC_VER
  for (;;)
    {
      // 32 bits are sufficient for 6 digits of base 36
      unt32 r = 0;
      /* Another alternative, especially since we only need
       * 32 bits, is to use rand_s().  But let's stay Oblongy. */
      ob_retort tort = ob_truly_random (&r, sizeof (r));
      if (tort < OB_OK)
        {
          free (name);
          return tort;
        }
      char *p = name + before_x_len;
      for (size_t i = 0; i < x_len; i++)
        {
          int n = r % 36;
          r /= 36;
          n += (n < 10) ? '0' : ('A' - 10);
          *p++ = n;
        }
      HANDLE h = CreateFile (name, (GENERIC_READ | GENERIC_WRITE), 0, NULL,
                             CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY, NULL);
      if (h == INVALID_HANDLE_VALUE)
        {
          const DWORD lasterr = GetLastError ();
          if (lasterr == ERROR_FILE_EXISTS)
            continue;  // not unique; let's try again!
          // some other error; too bad
          free (name);
          return ob_win32err_to_retort (lasterr);
        }
      int fd = _open_osfhandle ((intptr_t) h,
                                _O_NOINHERIT | (binary ? _O_BINARY : _O_TEXT));
      if (fd < 0)
        {
          const int erryes = errno;
          free (name);
          return ob_errno_to_retort (erryes);
        }
      *name_ret = name;
      *fd_ret = fd;
      return OB_OK;
    }
#else
  int fd = mkstemp (name);
  if (fd < 0)
    {
      const int erryes = errno;
      free (name);
      return ob_errno_to_retort (erryes);
    }
  *name_ret = name;
  *fd_ret = fd;
  return OB_OK;
#endif
}

/* XXX: has too much in common with ob_mkstemp.  Figure out how to
 * factor the common parts. */
ob_retort ob_mkdtemp (const char *prefix, char **name_ret)
{
  const char *tmpdir = ob_get_standard_path (ob_tmp_dir);
  if (!tmpdir)
    tmpdir = ".";

  if (!prefix)
    prefix = "gs-";

  const size_t td_len = strlen (tmpdir);
  const size_t pf_len = strlen (prefix);
  const size_t before_x_len = td_len + pf_len + 1;
  const size_t x_len = 6;
  char *name = (char *) malloc (1 + before_x_len + x_len);
  if (!name)
    return OB_NO_MEM;

  memcpy (name, tmpdir, td_len);
  name[td_len] = OB_DIR_CHAR;
  memcpy (name + td_len + 1, prefix, pf_len);
  memset (name + before_x_len, 'X', x_len);
  name[before_x_len + x_len] = 0;

#ifdef _MSC_VER
  for (;;)
    {
      // 32 bits are sufficient for 6 digits of base 36
      unt32 r = 0;
      /* Another alternative, especially since we only need
       * 32 bits, is to use rand_s().  But let's stay Oblongy. */
      ob_retort tort = ob_truly_random (&r, sizeof (r));
      if (tort < OB_OK)
        {
          free (name);
          return tort;
        }
      char *p = name + before_x_len;
      for (size_t i = 0; i < x_len; i++)
        {
          int n = r % 36;
          r /= 36;
          n += (n < 10) ? '0' : ('A' - 10);
          *p++ = n;
        }
      if (_mkdir (name) < 0)
        {
          const int erryes = errno;
          if (EEXIST == erryes)
            continue;  // not unique; let's try again!
          // some other error; too bad
          free (name);
          return ob_errno_to_retort (erryes);
        }
      *name_ret = name;
      return OB_OK;
    }
#else
  if (!mkdtemp (name))
    {
      const int erryes = errno;
      free (name);
      return ob_errno_to_retort (erryes);
    }
  *name_ret = name;
  return OB_OK;
#endif
}

/* Although the rename() function overwrites its destination,
 * simulating rename() with link() and unlink() does what we
 * want: an error if the destination exists.
 * (Without the TOCTOU problems we would have if we checked
 * for the destination and then called rename.) */
int ob_non_overwriting_rename (const char *src, const char *dst)
{
#ifdef _MSC_VER
  return rename (src, dst); /* Windows already does what we want */
#else
  /* Unfortunately, the simple link/unlink doesn't handle the case
   * where src and dst are the same file (we want that to be
   * a no-op and succeed).  So, must go to extra lengths to figure
   * out if we're in that case. */
  char *real1 = NULL;
  char *real2 = NULL;
  bool same_file = false;
  if (ob_realpath (src, &real1) >= OB_OK && ob_realpath (dst, &real2) >= OB_OK
      && 0 == strcmp (real1, real2))
    same_file = true;

  free (real1);
  free (real2);

  if (same_file)
    return 0; /* succeed, when otherwise we would fail if src equals dst */

  if (link (src, dst) < 0)
    return -1;
  return unlink (src);
#endif
}


ob_retort ob_close_socket (ob_sock_t sock)
{
#ifdef _MSC_VER
  if (shutdown (sock, SD_RECEIVE | SD_SEND) != 0)
    return ob_win32err_to_retort (WSAGetLastError ());
  if (closesocket (sock) != 0)
    return ob_win32err_to_retort (WSAGetLastError ());
#else
  if (close (sock) < 0)
    return ob_errno_to_retort (errno);
#endif
  return OB_OK;
}


#ifdef __gnu_linux__
#include <sys/statfs.h>
#elif defined(__APPLE__)
#include <sys/param.h>
#include <sys/mount.h>
#endif

#ifndef NFS_SUPER_MAGIC
#define NFS_SUPER_MAGIC 0x6969
#endif

bool ob_is_network_path (const char *path)
{
#if defined(__gnu_linux__) || defined(__APPLE__)
  struct statfs f;
  if (statfs (path, &f) < 0)
    {
      OB_LOG_WARNING_CODE (0x10020025, "statfs '%s': %s\n", path,
                           strerror (errno));
      return false;
    }
#endif

#ifdef __gnu_linux__
  return (f.f_type == NFS_SUPER_MAGIC);
#elif defined(__APPLE__)
  return (strcmp (f.f_fstypename, "nfs") == 0);
#else
  return false;
#endif
}

// --------------- code stolen from evutil.c ---------------

// At 2ea15ed0f69badb27b5ea58ffd873b1c2008c5d9 of git://github.com/libevent/libevent.git

/*
 * Copyright (c) 2007-2012 Niels Provos and Nick Mathewson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

typedef int evutil_socket_t;

#ifdef _MSC_VER
static ob_retort ob_evutil_ersatz_socketpair_ (int family, int type,
                                               int protocol,
                                               evutil_socket_t fd[2])
{
  /* This code is originally from Tor.  Used with permission. */

  /* This socketpair does not work when localhost is down. So
         * it's really not the same thing at all. But it's close enough
         * for now, and really, when localhost is down sometimes, we
         * have other problems too.
         */

#define ERR(e) WSA##e

  evutil_socket_t listener = -1;
  evutil_socket_t connector = -1;
  evutil_socket_t acceptor = -1;
  struct sockaddr_in listen_addr;
  struct sockaddr_in connect_addr;
  int size;
  int saved_errno = -1;

  if (protocol || (family != AF_INET))
    {
      return ob_win32err_to_retort (ERR (EAFNOSUPPORT));
    }
  if (!fd)
    {
      return ob_win32err_to_retort (ERR (EINVAL));
    }

  listener = socket (AF_INET, type, 0);
  if (listener < 0)
    return ob_win32err_to_retort (WSAGetLastError ());
  memset (&listen_addr, 0, sizeof (listen_addr));
  listen_addr.sin_family = AF_INET;
  listen_addr.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
  listen_addr.sin_port = 0; /* kernel chooses port.  */
  if (bind (listener, (struct sockaddr *) &listen_addr, sizeof (listen_addr))
      == -1)
    goto tidy_up_and_fail;
  if (listen (listener, 1) == -1)
    goto tidy_up_and_fail;

  connector = socket (AF_INET, type, 0);
  if (connector < 0)
    goto tidy_up_and_fail;
  /* We want to find out the port number to connect to.  */
  size = sizeof (connect_addr);
  if (getsockname (listener, (struct sockaddr *) &connect_addr, &size) == -1)
    goto tidy_up_and_fail;
  if (size != sizeof (connect_addr))
    goto abort_tidy_up_and_fail;
  if (connect (connector, (struct sockaddr *) &connect_addr,
               sizeof (connect_addr))
      == -1)
    goto tidy_up_and_fail;

  size = sizeof (listen_addr);
  acceptor = accept (listener, (struct sockaddr *) &listen_addr, &size);
  if (acceptor < 0)
    goto tidy_up_and_fail;
  if (size != sizeof (listen_addr))
    goto abort_tidy_up_and_fail;
  /* Now check we are talking to ourself by matching port and host on the
           two sockets.  */
  if (getsockname (connector, (struct sockaddr *) &connect_addr, &size) == -1)
    goto tidy_up_and_fail;
  if (size != sizeof (connect_addr)
      || listen_addr.sin_family != connect_addr.sin_family
      || listen_addr.sin_addr.s_addr != connect_addr.sin_addr.s_addr
      || listen_addr.sin_port != connect_addr.sin_port)
    goto abort_tidy_up_and_fail;
  closesocket (listener);
  fd[0] = connector;
  fd[1] = acceptor;

  return OB_OK;

abort_tidy_up_and_fail:
  saved_errno = ERR (ECONNABORTED);
tidy_up_and_fail:
  if (saved_errno < 0)
    saved_errno = WSAGetLastError ();
  if (listener != -1)
    closesocket (listener);
  if (connector != -1)
    closesocket (connector);
  if (acceptor != -1)
    closesocket (acceptor);

  return ob_win32err_to_retort (saved_errno);
#undef ERR
}
#endif /* _MSC_VER */

ob_retort ob_make_socket_nonblocking (evutil_socket_t fd)
{
#ifdef _WIN32
  {
    u_long nonblocking = 1;
    if (ioctlsocket (fd, FIONBIO, &nonblocking) == SOCKET_ERROR)
      {
        const DWORD erryes = WSAGetLastError ();
        OB_LOG_WARNING_CODE (0x10020026, "ioctlsocket(%d, FIONBIO)", (int) fd);
        return ob_win32err_to_retort (erryes);
      }
  }
#else
  {
    int flags;
    if ((flags = fcntl (fd, F_GETFL, NULL)) < 0)
      {
        const int erryes = errno;
        OB_LOG_WARNING_CODE (0x10020027, "fcntl(%d, F_GETFL)", fd);
        return ob_errno_to_retort (erryes);
      }
    if (!(flags & O_NONBLOCK))
      {
        if (fcntl (fd, F_SETFL, flags | O_NONBLOCK) == -1)
          {
            const int erryes = errno;
            OB_LOG_WARNING_CODE (0x10020028, "fcntl(%d, F_SETFL)", fd);
            return ob_errno_to_retort (erryes);
          }
      }
  }
#endif
  return OB_OK;
}

ob_retort ob_make_socket_blocking (evutil_socket_t fd)
{
#ifdef _WIN32
  {
    u_long nonblocking = 0;
    if (ioctlsocket (fd, FIONBIO, &nonblocking) == SOCKET_ERROR)
      {
        const DWORD erryes = WSAGetLastError ();
        OB_LOG_WARNING_CODE (0x10020029, "ioctlsocket(%d, FIONBIO)", (int) fd);
        return ob_win32err_to_retort (erryes);
      }
  }
#else
  {
    int flags;
    if ((flags = fcntl (fd, F_GETFL, NULL)) < 0)
      {
        const int erryes = errno;
        OB_LOG_WARNING_CODE (0x1002002a, "fcntl(%d, F_GETFL)", fd);
        return ob_errno_to_retort (erryes);
      }
    if ((flags & O_NONBLOCK))
      {
        if (fcntl (fd, F_SETFL, flags & ~O_NONBLOCK) == -1)
          {
            const int erryes = errno;
            OB_LOG_WARNING_CODE (0x1002002b, "fcntl(%d, F_SETFL)", fd);
            return ob_errno_to_retort (erryes);
          }
      }
  }
#endif
  return OB_OK;
}
