
/* (c)  oblong industries */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <assert.h>
#include <ctype.h>

#include "libLoam/c/ob-sys.h"
#include "libLoam/c/ob-dirs.h"
#include "libLoam/c/ob-attrs.h"
#include "libLoam/c/ob-math.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-file.h"
#include "libLoam/c/ob-time.h"
#include "libLoam/c/ob-string.h"
#include "libLoam/c/ob-pthread.h"

#include "libPlasma/c/pool.h"
#include "libPlasma/c/pool_options.h"
#include "libPlasma/c/private/pool_impl.h"
#include "libPlasma/c/private/pool_multi.h"

#include "libPlasma/c/slaw-coerce.h"
#include "libPlasma/c/slaw-io.h"
#include "libPlasma/c/slaw-path.h"
#include "libPlasma/c/slaw-string.h"
#include "libPlasma/c/private/pool-ugly-callback.h"
#include "libPlasma/c/private/plasma-private.h"
#include "libPlasma/c/private/pool_mmap.h"
#include "libPlasma/c/private/plasma-testing.h"

#define LOCAL_POOL_PREFIX "local:"

void (*pool_ugly_callback) (pool_ugly_callback_what what,
                            pool_ugly_callback_when when, const char *file,
                            int line) = NULL;

static pool_hose global_hose_list;

/* Recursively remove specified directory, but don't delete pools dir
 * or anything above it.  (bug 2881)
 * Furthermore, if the specified directory is not inside the pools
 * directory, then don't do any recursion; just remove the specified
 * directory and then stop. */
static ob_retort pool_rmdir_p (const char *dir)
{
  const char *pools_dir = ob_get_standard_path (ob_pools_dir);
  const size_t pd_len = strlen (pools_dir);
  if (0 == strncmp (dir, pools_dir, pd_len) && '/' == dir[pd_len])
    return ob_rmdir_p_ex (dir, pools_dir);
  // If not inside pools_dir, just do a plain old rmdir...
  if (rmdir (dir) < 0)
    {
      const int erryes = errno;
      // Silence the same set of errors that ob_rmdir_p_ex() does.
      // XXX: would prefer not to duplicate this list of error codes.
      if (erryes == EEXIST || erryes == ENOTEMPTY || erryes == EBUSY
          || erryes == EACCES || erryes == EPERM || erryes == EROFS
          || erryes == ENOTDIR || erryes == EINVAL)
        return OB_OK;
      return ob_errno_to_retort (erryes);
    }
  return OB_OK;
}

static void install_atfork_handlers (void);

static pool_hose alloc_ph_structure (void)
{
  pool_hose ph, oldhead;
  pool_hose *ptr = &global_hose_list;
#ifndef _MSC_VER
  /* The atfork handler needs to walk global_hose_list, so be sure it's
   * installed before the first pool_hose is added to
   * global_hose_list. */
  static pthread_once_t atforks_installed = PTHREAD_ONCE_INIT;
  pthread_once (&atforks_installed, &install_atfork_handlers);
#endif
  for (;;)
    {
      ph = ob_atomic_pointer_ref (ptr);
      if (!ph)
        break;
      if (ob_atomic_int32_compare_and_swap (&(ph->mgmt.status), Hose_Unused,
                                            Hose_Used))
        {
          memset (&(ph->name), 0, sizeof (*ph) - sizeof (pool_management_info));
          return ph;
        }
      ptr = &(ph->mgmt.next);
    }
  ph = (pool_hose) calloc (1, sizeof (*ph));
  do
    {
      oldhead = ob_atomic_pointer_ref (&global_hose_list);
      ph->mgmt.next = oldhead;
      ph->mgmt.status = Hose_Used;
    }
  while (!ob_atomic_pointer_compare_and_swap (&global_hose_list, oldhead, ph));
  return ph;
}

static void dealloc_ph_structure (pool_hose ph)
{
  ob_make_undefined (&(ph->name), sizeof (*ph) - sizeof (pool_management_info));
  ob_atomic_int32_set (&(ph->mgmt.status), Hose_Unused);
}

static void hose_close_descriptors (pool_hose ph)
{
#ifdef _MSC_VER
  if (ph->notify_handle != 0)
    {
      if (!CloseHandle (ph->notify_handle))
        ob_log (OBLV_DBUG, 0x20101000,
                "pool_free_hose error closing notify_handle - %d",
                GetLastError ());

      ph->notify_handle = 0;
    }
#else
  /* Ugh.  For network pools, ph->notify_handle is the same as
   * ph->net->connfd, and was already closed by pool_tcp_withdraw(),
   * therefore we can't close it again here. */
  if (ph->notify_handle >= 0 && !ph->net)
    OB_CHECK_POSIX_CODE (0x20101028, close (ph->notify_handle));

  ph->notify_handle = -1;
#endif
  ob_cleanup_wakeup (&ph->w);
}

#ifndef _MSC_VER
static void hose_hiatus (pool_hose ph)
{
  // The child will need its own fifo path, different from parent
  ph->fifo_path[0] = 0;
  if (ph->hiatus)
    ph->hiatus (ph);
  hose_close_descriptors (ph);

  /* Need to close flock() descriptors on fork.  See:
   * https://lists.oblong.com/pipermail/dev/2013-July/000434.html */
  int i;
  for (i = 0; i < POOL_SEM_SET_COUNT; i++)
    if (ph->flock_fds[i] >= 0)
      {
        close (ph->flock_fds[i]);
        ph->flock_fds[i] = -1;
      }
}

static int64 use_the_forks_handler = 1;

/*
 * This atfork handler calls fclose() and free() in the child after
 * forking.  Technically, these functions aren't safe to call in the
 * child after forking a multi-threaded process: see
 * http://pubs.opengroup.org/onlinepubs/9699919799/functions/fork.html.
 * In practice, they are safe to call when using glibc and, seemingly,
 * OS X's standard library.  However, the malloc/free that come along
 * with linking to libcef (via tcmalloc) care not for unportable calls
 * to fclose() and free() in a forked child: the child process can and
 * will deadlock inside these calls if forked from a multi-threaded
 * parent.  Consequently, this handler can be disabled by calling
 * pool_disable_atfork_handlers(); however, if the handler is disabled,
 * the child process must not use hoses inherited from the parent.  See
 * https://bugs.oblong.com/show_bug.cgi?id=14903 for gruesome details.
 */
static void handle_forked_child (void)
{
  int64 do_it = ob_atomic_int64_ref (&use_the_forks_handler);
  if (!do_it)
    return;
  pool_hose *ptr = &global_hose_list;
  for (;;)
    {
      pool_hose ph = ob_atomic_pointer_ref (ptr);
      if (!ph)
        break;
      if (ob_atomic_int32_compare_and_swap (&(ph->mgmt.status), Hose_Used,
                                            Hose_Needs_Reconnection))
        hose_hiatus (ph);
      ptr = &(ph->mgmt.next);
    }
  pool_fork_gangs ();
}

static void install_atfork_handlers (void)
{
  int ret = pthread_atfork (NULL, NULL, handle_forked_child);
  if (ret != 0)
    OB_FATAL_ERROR ("pthread_atfork returned ENOMEM, bailing out.");
}


void pool_disable_atfork_handlers (void)
{
  ob_atomic_int64_set (&use_the_forks_handler, 0);
}

void pool_enable_atfork_handlers (void)
{
  ob_atomic_int64_set (&use_the_forks_handler, 1);
}

#else

/* No-op on Windows. */
void pool_disable_atfork_handlers (void) {}

/* Ditto. */
void pool_enable_atfork_handlers (void) {}

#endif  // ndef _MSC_VER

static ob_retort check_hose_validity (pool_hose ph)
{
  if (!ph)
    return POOL_NULL_HOSE;

  int32 status = ob_atomic_int32_ref (&(ph->mgmt.status));
  switch (status)
    {
      case Hose_Unused:
        OB_FATAL_BUG_CODE (0x2010103d, "Use of freed pool hose\n");
      case Hose_Used:
        return OB_OK;
      case Hose_Needs_Reconnection:
        return POOL_INVALIDATED_BY_FORK;
      default:
        OB_FATAL_BUG_CODE (0x2010103e,
                           "Apparent memory corruption: hose has status %d\n",
                           status);
    }
}

#define CHECK_HOSE_VALIDITY(hoz)                                               \
  {                                                                            \
    ob_retort tortellini = check_hose_validity (hoz);                          \
    if (tortellini < OB_OK)                                                    \
      return tortellini;                                                       \
  }

/// Allocate and initialize a new pool hose.  Must be freed with
/// free_pool_hose()

static pool_hose pool_new_pool_hose (const char *pool_uri)
{
  assert (pool_uri);

  pool_hose ph = alloc_ph_structure ();

  // Everything else is properly initialized by setting to 0, below
  // are the exceptions.
  ph->pool_directory_version = POOL_DIRECTORY_VERSION_CONFIG_IN_MMAP;
  ph->perms.mode = -1;
  ph->perms.uid = -1;
  ph->perms.gid = -1;
  ph->name = strdup (pool_uri);
  if (!ph->name)
    {
      dealloc_ph_structure (ph);
      return NULL;
    }

  ph->notify_handle = OB_NULL_HANDLE;
  ob_initialize_wakeup (&ph->w);

#ifndef _MSC_VER
  ph->sem_id = -1;
  ph->lock_ops = &pool_null_ops;

  int i;
  for (i = 0; i < 2; i++)
    ph->flock_fds[i] = -1;
#endif

  // The hose name can be changed later, but it starts out
  // defaulting to the last component of the pool name.
  ph->hose_name = strdup (ob_basename (pool_uri));
  if (!ph->hose_name)
    {
      free (ph->name);
      dealloc_ph_structure (ph);
      return NULL;
    }

  return ph;
}

// Free resources used by a pool hose.  The pool hose may be in any
// state of initialization as long as the type-specific state has been
// unwound.

static void pool_free_pool_hose (pool_hose ph)
{
  assert (ph);

  if (ph->name)
    free (ph->name);
  if (ph->method)
    free (ph->method);
  if (ph->net)
    pool_net_free (ph->net);

  hose_close_descriptors (ph);
  pool_close_semaphores (ph); /* darn, nowhere to report error */

  /* XXX: apparently this unlink is expected to fail much of the time,
   * thus I'm not wrapping it in CHECK_POSIX_ERROR. */
  if (strlen (ph->fifo_path) > 0)
    unlink (ph->fifo_path);

  if (ph->ext)
    OB_LOG_BUG_CODE (0x20101002, "leaking duffel\n");

  free (ph->hose_name);

  dealloc_ph_structure (ph);
}

static void path_clear (char *path)
{
  assert (path);

  path[0] = 0;
}

static int path_add_string_loc (char *path, const char *x, const char *file,
                                int line)
{
  if (!path)
    ob_log_loc_fatal (file, line, OBLV_BUG, 0, -1, "path was NULL\n");
  if (!x)
    ob_log_loc_fatal (file, line, OBLV_BUG, 0, -1,
                      "x was NULL (but path was '%s')\n", path);

  const size_t freelen = PATH_MAX - strlen (path) - 1;

  if (freelen < strlen (x))
    {
      return 1;
    }
  else
    {
      strncat (path, x, freelen);
      return 0;
    }
}

#define path_add_string(p, x) path_add_string_loc ((p), (x), __FILE__, __LINE__)

static int path_add_element (char *path, const char *x)
{
  assert (path);
  assert (x);

  return path_add_string (path, "/") || path_add_string (path, x);
}

/// Construct the top-level directory pathname containing all of the
/// pool configuration files.

static ob_retort pool_dir_path (char *path)
{
  const char *pools_dir = ob_get_standard_path (ob_pools_dir);

  if (!pools_dir)
    {
      OB_LOG_ERROR_CODE (0x20101027,
                         "Couldn't find or create a suitable directory to put\n"
                         "pools in.  Try setting the OB_POOLS_DIR environment\n"
                         "variable to a writable, non-NFS directory.\n");
      return POOL_NO_POOLS_DIR;
    }

  // The only reason this can fail is because the user either set
  // OB_POOLS_DIR to something ridiculous accidentally, or because they are
  // attempting a buffer overflow attack.

  path_clear (path);

  if (path_add_string (path, pools_dir))
    {
      OB_LOG_ERROR_CODE (0x20101003, "path too long\n");
      return POOL_POOLNAME_BADTH;
    }

  return OB_OK;
}

ob_retort pool_add_path_element (char *path, const char *new_element)
{
  if (path_add_element (path, new_element))
    return POOL_POOLNAME_BADTH;
  return OB_OK;
}

// If pool_uri is not a URI for a local pool, return 0.
// If it is, return the number of characters to skip.
static size_t pool_is_local_uri (const char *pool_uri)
{
  const size_t local_pool_prefix_len = strlen (LOCAL_POOL_PREFIX);
  if (0 == strncmp (pool_uri, LOCAL_POOL_PREFIX, local_pool_prefix_len))
    return local_pool_prefix_len;
  return 0;
}

ob_retort pool_build_pool_dir_path (char *dir_path, const char *pool_uri)
{
  assert (pool_uri);
  const size_t prefix_len = pool_is_local_uri (pool_uri);
  if (prefix_len)
    {
      pool_uri += prefix_len;
      ob_safe_copy_string (dir_path, PATH_MAX, pool_uri);
      return OB_OK;
    }
  else
    {
      const ob_retort pret = pool_dir_path (dir_path);
      if (pret != OB_OK)
        return pret;
      return pool_add_path_element (dir_path, pool_uri);
    }
}

/// Create the directory that contains all the configuration files for
/// this particular pool.  Called during creation of a new pool.

static ob_retort pool_create_pool_dir (pool_hose ph, int mode, int uid, int gid)
{
  assert (ph);

  char dir_path[PATH_MAX];
  ob_retort pret = pool_build_pool_dir_path (dir_path, ph->name);
  if (pret != OB_OK)
    return pret;

  if (ph->pool_directory_version == POOL_DIRECTORY_VERSION_SINGLE_FILE)
    {
      /* If this is a single file pool, chop off the last component,
       * because the last component will be a file (created later),
       * not a directory.  But we still need to create any
       * intermediate directories. */
      const char *all_your_base = ob_basename (dir_path);
      const size_t slashpos = all_your_base - dir_path;
      if (slashpos > 0)
        dir_path[slashpos - 1] = 0; /* nuke the slash */
    }

  POOL_UGLY_CALLBACK (POOL_UGLY_CALLBACK_CONFIG_LOCK,
                      POOL_UGLY_CALLBACK_ASSERT_OWNED);

  return ob_mkdir_p_with_permissions (dir_path, mode, uid, gid);
}

/// Remove the directory that contains all the configuration files for
/// this particular pool.  Called during destruction of a pool.  The
/// directory must already be empty of all files.

static ob_retort pool_remove_pool_dir (pool_hose ph)
{
  assert (ph);

  char dir_path[PATH_MAX];
  ob_retort pret = pool_build_pool_dir_path (dir_path, ph->name);
  if (pret != OB_OK)
    return pret;

  if (ph->pool_directory_version == POOL_DIRECTORY_VERSION_SINGLE_FILE)
    {
      /* If this is a single file pool, chop off the last component,
       * because the last component will be a file (already deleted),
       * not a directory.  But we still need to delete any
       * intermediate directories. */
      const char *all_your_base = ob_basename (dir_path);
      const size_t slashpos = all_your_base - dir_path;
      if (slashpos > 0)
        dir_path[slashpos - 1] = 0; /* nuke the slash */
    }

  POOL_UGLY_CALLBACK (POOL_UGLY_CALLBACK_CONFIG_LOCK,
                      POOL_UGLY_CALLBACK_ASSERT_OWNED);

  // If rmdir() fails, the directory is not empty because someone
  // forgot to delete a file.  We could automatically try to delete
  // all files in the directory, but it is dangerous to recursively
  // delete a directory, especially when part of the path name is
  // specified by an environment variable which may be unset.  You can
  // easily end up removing large portions of your file system by
  // accident.  I learned about this with bad RPM spec files which
  // didn't check for SOME_DIR_PATH being non-null before rm -rf
  // /${SOME_DIR_PATH} - very bad. -VAL
  // That's not the only reason rmdir() can fail.  It can also
  // fail because the directory doesn't exist, probably because the
  // pool doesn't exist.  This can happen if someone calls pool_dispose()
  // on a non-existant pool, or can also happen if pool creation fails
  // and we try to clean up from it.  But since these are "normal"
  // failures, we don't want to perror() in these cases and clutter
  // up the output.  --Mignon
  pret = pool_rmdir_p (dir_path);
  if (ob_retort_to_errno (pret) == ENOENT)
    return OB_OK; /* It's okay if the directory doesn't exist */
  else
    return pret;
}

static inline bool is_1_thru_9 (char c)
{
  return (c >= '1' && c <= '9');
}

static ob_retort validate_name_component (const char *s)
{
  // can't be empty or start with '.'
  // (The prohibition on '.' makes sure we don't have "." or "..",
  // and it also avoids creating hidden files on UNIX)
  if (s[0] == 0 || s[0] == '.')
    return POOL_POOLNAME_BADTH;

  const size_t len = strlen (s);
  // Let's prohibit ' ', '.', and '$' as the last character.
  // This document discourages Windows filenames from ending in dot or space:
  // http://msdn.microsoft.com/en-us/library/aa365247%28v=vs.85%29.aspx
  // And avoiding '$' as the last character means we prohibit "CONIN$"
  // and "CONOUT$":
  // http://support.microsoft.com/kb/90088
  char c = s[len - 1];
  if (c == ' ' || c == '.' || c == '$')
    return POOL_POOLNAME_BADTH;

  // These names are forbidden on Windows: CON, PRN, AUX, NUL, COM1, COM2,
  // COM3, COM4, COM5, COM6, COM7, COM8, COM9, LPT1, LPT2, LPT3, LPT4, LPT5,
  // LPT6, LPT7, LPT8, and LPT9, according to this document:
  // http://msdn.microsoft.com/en-us/library/aa365247%28v=vs.85%29.aspx
  // We'll do the comparison case-insensitively, and following the advice of
  // that document, we'll avoid names that exactly match these, or that
  // start with these if the next character is '.'
  // "lost+found" has special meaning on UNIX; see bug 7340
  c = tolower (s[0]);
  if (c == 'a' || c == 'c' || c == 'l' || c == 'n' || c == 'p')
    {
      bool bad = false;
      if (0 == strncasecmp (s, "con", 3))
        bad = (s[3] == 0 || s[3] == '.');
      else if (0 == strncasecmp (s, "prn", 3))
        bad = (s[3] == 0 || s[3] == '.');
      else if (0 == strncasecmp (s, "aux", 3))
        bad = (s[3] == 0 || s[3] == '.');
      else if (0 == strncasecmp (s, "nul", 3))
        bad = (s[3] == 0 || s[3] == '.');
      else if (0 == strncasecmp (s, "com", 3))
        bad = is_1_thru_9 (s[3]) && (s[4] == 0 || s[4] == '.');
      else if (0 == strncasecmp (s, "lpt", 3))
        bad = is_1_thru_9 (s[3]) && (s[4] == 0 || s[4] == '.');
      else if (0 == strncasecmp (s, "lost+found", 10))
        bad = (s[10] == 0 || s[10] == '.');

      if (bad)
        return POOL_POOLNAME_BADTH;
    }

  return OB_OK;
}

#define MAX_POOL_NAME 100

#define LEGAL_POOL_CHARS                                                       \
  " !#$%&'()+,-./0123456789;=@ABCDEFGHIJKLMNOPQ"                               \
  "RSTUVWXYZ[]^_`abcdefghijklmnopqrstuvwxyz{}~"

// used by pool_list_internal()
static ob_retort validate_name_component1 (const char *s)
{
  const size_t len = strspn (s, LEGAL_POOL_CHARS);
  // length should be between 1 and 100
  if (len < 1 || len > MAX_POOL_NAME)
    return POOL_POOLNAME_BADTH;
  // if strspn didn't reach the end of the string, we had a forbidden char
  if (s[len])
    return POOL_POOLNAME_BADTH;

  return validate_name_component (s);
}

ob_retort pool_validate_name (const char *pool_uri)
{
  if (!pool_uri)
    return POOL_POOLNAME_BADTH;

  const size_t local_prefix = pool_is_local_uri (pool_uri);
  if (local_prefix)
    {
      pool_uri += local_prefix;
#ifdef _MSC_VER
      if (isalpha (pool_uri[0]) && ':' == pool_uri[1])
        pool_uri += 2;
#endif
      if (*pool_uri != '/') /* require an absolute path */
        return POOL_POOLNAME_BADTH;
      pool_uri++;
    }

  const size_t len = strspn (pool_uri, LEGAL_POOL_CHARS);
  // length should be between 1 and 100
  if (len < 1 || len > MAX_POOL_NAME)
    return POOL_POOLNAME_BADTH;
  // if strspn didn't reach the end of the string, we had a forbidden char
  if (pool_uri[len])
    return POOL_POOLNAME_BADTH;

  // have to make a copy of pool_uri because strsep modifies it
  char buf[1 + MAX_POOL_NAME];
  memcpy (buf, pool_uri, len);
  buf[len] = 0;

  char *p = buf;
  char *s;

  while (NULL != (s = strsep (&p, "/")))
    {
      ob_retort pret = validate_name_component (s);
      if (pret < OB_OK)
        return pret;
    }

  return OB_OK;
}

// Exported solely so that sem_ops_win32.c can make a mutex name from
// the path name.  On UNIX, not called outside this file.

ob_retort pool_config_file_path (char *path, const char *config_name,
                                 const char *pool_uri)
{
  ob_retort pret = pool_validate_name (pool_uri);
  if (pret < OB_OK)
    return pret;
  // Default for pool framework config file name
  if (!config_name)
    config_name = "pool";
  pret = pool_build_pool_dir_path (path, pool_uri);
  if (pret != OB_OK)
    return pret;
  if (path_add_element (path, config_name) || path_add_string (path, ".conf"))
    return POOL_POOLNAME_BADTH;
  return OB_OK;
}

/// Write a configuration protein to a config file.

ob_retort pool_write_config_file (const char *config_name, const char *pool_uri,
                                  bprotein conf, int mode, int uid, int gid)
{
  ob_retort pret;
  char path[PATH_MAX];

  if ((pret = pool_config_file_path (path, config_name, pool_uri)) != OB_OK)
    return pret;

  POOL_UGLY_CALLBACK (POOL_UGLY_CALLBACK_CONFIG_LOCK,
                      POOL_UGLY_CALLBACK_ASSERT_OWNED);

  pret = slaw_write_to_binary_file (path, conf);
  int e;
  if ((e = ob_retort_to_errno (pret)) > 0)
    {
      if (e == ENOENT)
        return POOL_NO_SUCH_POOL;
      else
        return POOL_CONF_WRITE_BADTH;
    }

  if (pret >= OB_OK)
    pret = ob_permissionize (path, pool_combine_modes (mode, 0666), uid, gid);

  return pret;
}

#ifdef _MSC_VER
/* Windows makes no distinction between ENOENT and ENOTDIR, which,
 * sadly, we were counting on.  So let's hack together a little
 * test to tell the difference. */
static bool simulate_enotdir (const char *path)
{
  char *pathology = strdup (path);
  if (!pathology)
    return false;
  DWORD atts;
  while (INVALID_FILE_ATTRIBUTES == (atts = GetFileAttributes (pathology)))
    {
      size_t dirlen = ob_basename (pathology) - pathology;
      if (dirlen == 0)
        {
          free (pathology);
          return false;
        }
      pathology[dirlen - 1] = 0;
    }
  free (pathology);
  return (0 == (atts & FILE_ATTRIBUTE_DIRECTORY));
}
#endif

ob_retort pool_read_config_file (const char *config_name, const char *pool_uri,
                                 protein *conf_p)
{
  char path[PATH_MAX];
  ob_retort pret;
  if ((pret = pool_config_file_path (path, config_name, pool_uri)) != OB_OK)
    return pret;

  POOL_UGLY_CALLBACK (POOL_UGLY_CALLBACK_CONFIG_LOCK,
                      POOL_UGLY_CALLBACK_ASSERT_OWNED);

  pret = slaw_read_from_binary_file (path, conf_p);
  int e;
  if ((e = ob_retort_to_errno (pret)) > 0 && e != ENOTDIR)
    {
      if (e == ENOENT)
        {
#ifdef _MSC_VER
          if (simulate_enotdir (path))
            return ob_errno_to_retort (ENOTDIR);
#endif
          return POOL_NO_SUCH_POOL;
        }
      else if (e == ENAMETOOLONG)
        return POOL_POOLNAME_BADTH;
      else if (e == EINVAL)
        // On Windows, we can get EINVAL if we use characters like
        // '*', '?', or '\n' in a filename.
        return POOL_POOLNAME_BADTH;
      else
        return POOL_CONF_READ_BADTH;
    }
  else
    return pret;
}

ob_retort pool_remove_config_file (const char *config_name,
                                   const char *pool_uri)
{
  char path[PATH_MAX];
  ob_retort pret;
  if ((pret = pool_config_file_path (path, config_name, pool_uri)) != OB_OK)
    return pret;

  POOL_UGLY_CALLBACK (POOL_UGLY_CALLBACK_CONFIG_LOCK,
                      POOL_UGLY_CALLBACK_ASSERT_OWNED);

  if (unlink (path) != 0)
    {
      if (errno != ENOENT) /* ENOENT just means the pool didn't exist */
        OB_PERROR_CODE (0x2010102b, "unlink");
      return POOL_CONFIG_BADTH;
    }
  return OB_OK;
}

static ob_retort pool_save_default_config_with_permissions (pool_hose ph,
                                                            int mode, int uid,
                                                            int gid)
{
  CHECK_HOSE_VALIDITY (ph);

  const char *reliable_type = ph->method;
  if (!reliable_type)
    OB_FATAL_BUG_CODE (0x20101004,
                       "thought this shouldn't be able to happen but it did!");
  v3int32 perms_v;
  perms_v.x = ph->perms.mode;
  perms_v.y = ph->perms.uid;
  perms_v.z = ph->perms.gid;
  const bool cf =
    (POOL_DIRECTORY_VERSION_CONFIG_IN_FILE == ph->pool_directory_version);
  if (cf)
    OB_LOG_DEBUG_CODE (0x20101005, "writing sem-key %u\n", ph->sem_key);
  slaw m1 =
    slaw_map_inline_cf ("type", slaw_string (reliable_type), "pool-version",
                        slaw_int32 (ph->pool_directory_version), NULL);
  slaw m2 = slaw_map_inline_cf ("sem-key", slaw_int32 (ph->sem_key), "perms",
                                slaw_v3int32 (perms_v), NULL);
  slaw m = slaw_maps_merge (m1, (cf ? m2 : NULL), NULL);
  if (!m1 || !m2 || !m)
    return slaw_free (m1), slaw_free (m2), slaw_free (m), OB_NO_MEM;
  Free_Slaw (m1);
  Free_Slaw (m2);
  protein conf = protein_from_ff (NULL, m);
  if (!conf)
    return OB_NO_MEM;
  ob_retort pret =
    pool_write_config_file (NULL, ph->name, conf, mode, uid, gid);
  protein_free (conf);
  return pret;
}

// only exported for rewrite of config file in pool_sem.c
ob_retort pool_save_default_config (pool_hose ph)
{
  return pool_save_default_config_with_permissions (
    ph, ph->perms.mode, ph->perms.uid, ph->perms.gid);
}

static ob_retort pool_load_default_config (pool_hose ph)
{
  assert (ph);

  protein conf;
  ob_retort pret = pool_read_config_file (NULL, ph->name, &conf);
  if (ob_retort_to_errno (pret) == ENOTDIR)
    {
      ph->pool_directory_version = POOL_DIRECTORY_VERSION_SINGLE_FILE;
      ph->method = strdup ("mmap");
      return OB_OK;
    }
  if (pret != OB_OK)
    return pret;

  int64 version = slaw_path_get_int64 (conf, "pool-version",
                                       POOL_DIRECTORY_VERSION_CONFIG_IN_FILE);
  ph->pool_directory_version = (unt8) version;
  /* Note: we don't expect to ever find POOL_DIRECTORY_VERSION_SINGLE_FILE
   * in a config file, because such pools, by definition, don't have a
   * config file. */
  if (version != POOL_DIRECTORY_VERSION_CONFIG_IN_FILE
      && version != POOL_DIRECTORY_VERSION_CONFIG_IN_MMAP)
    {
      protein_free (conf);
      OB_LOG_ERROR_CODE (
        0x20101006,
        "'%s' had pool-version %" OB_FMT_64 "d, but expected either %d or %d\n",
        ph->name, version, POOL_DIRECTORY_VERSION_CONFIG_IN_FILE,
        POOL_DIRECTORY_VERSION_CONFIG_IN_MMAP);
      return POOL_WRONG_VERSION;
    }

  bslaw type_s = slaw_map_find_c (protein_ingests (conf), "type");
  if (!slaw_is_string (type_s))
    {
      OB_LOG_ERROR_CODE (0x20101007, "Pool '%s' config file missing 'type'\n",
                         ph->name);
      protein_free (conf);
      return POOL_CONFIG_BADTH;
    }
  ph->method = strdup (slaw_string_emit (type_s));
  if (ph->method == NULL)
    {
      protein_free (conf);
      return OB_NO_MEM;
    }

  // In later versions, the sem-key and perms are stored in the backing file.
  if (POOL_DIRECTORY_VERSION_CONFIG_IN_FILE != ph->pool_directory_version)
    return protein_free (conf), OB_OK;

  bslaw sem_key_s = slaw_map_find_c (protein_ingests (conf), "sem-key");
  if (!slaw_is_int32 (sem_key_s))
    {
      OB_LOG_ERROR_CODE (0x20101008,
                         "Pool '%s' config file missing 'sem-key'\n", ph->name);
      protein_free (conf);
      return POOL_CONFIG_BADTH;
    }
  ph->sem_key = *slaw_int32_emit (sem_key_s);
  ob_log (OBLV_DBUG, 0x20101009, "read sem-key %u\n", ph->sem_key);

  bslaw perms_s = slaw_map_find_c (protein_ingests (conf), "perms");
  const v3int32 *perms_v = slaw_v3int32_emit (perms_s);
  if (perms_v)
    {
      ph->perms.mode = perms_v->x;
      ph->perms.uid = perms_v->y;
      ph->perms.gid = perms_v->z;
    }

  protein_free (conf);
  return OB_OK;
}

/// Delete the default configuration file.  Once this file is gone,
/// the pool cannot be re-opened or located.

static ob_retort pool_remove_default_config (pool_hose ph)
{
  assert (ph);

  if (ph->pool_directory_version == POOL_DIRECTORY_VERSION_SINGLE_FILE)
    return OB_OK; /* no config file to remove! */
  return pool_remove_config_file (NULL, ph->name);
}

bool pool_is_local (pool_hose ph)
{
  return (ph && !ph->remote);
}

bool pool_is_remote (pool_hose ph)
{
  return (ph && ph->remote);
}

ob_retort pool_deposit (pool_hose ph, bprotein p, int64 *idx)
{
  return pool_deposit_ex (ph, p, idx, NULL);
}

ob_retort pool_deposit_ex (pool_hose ph, bprotein p, int64 *idx,
                           pool_timestamp *ret_ts)
{
  CHECK_HOSE_VALIDITY (ph);

  if (!slaw_is_protein (p))
    return POOL_NOT_A_PROTEIN;
  return ph->deposit (ph, p, idx, ret_ts);
}

ob_retort pool_advance_oldest (pool_hose ph, int64 idx_in)
{
  CHECK_HOSE_VALIDITY (ph);

  if (!ph->advance_oldest)
    return POOL_UNSUPPORTED_OPERATION;

  return ph->advance_oldest (ph, idx_in);
}

ob_retort pool_change_options (pool_hose ph, bslaw options)
{
  CHECK_HOSE_VALIDITY (ph);

  if (!ph->change_options)
    return POOL_UNSUPPORTED_OPERATION;

  if (slaw_is_protein (options))
    return ph->change_options (ph, options);
  else
    {
      protein p = protein_from (NULL, options);
      ob_retort tort = ph->change_options (ph, p);
      protein_free (p);
      return tort;
    }
}

ob_retort pool_newest_index (pool_hose ph, int64 *idx_out)
{
  CHECK_HOSE_VALIDITY (ph);
  return ph->newest_index (ph, idx_out);
}

ob_retort pool_oldest_index (pool_hose ph, int64 *idx_out)
{
  CHECK_HOSE_VALIDITY (ph);
  return ph->oldest_index (ph, idx_out);
}

// XXX Do we need to return an error at all for the below two?
// YYY Well, since we are obsessed with returning errors for NULL,
// I guess we do.

ob_retort pool_index (pool_hose ph, int64 *idx_out)
{
  CHECK_HOSE_VALIDITY (ph);

  if (!idx_out)
    return OB_ARGUMENT_WAS_NULL;
  *idx_out = ph->index;
  return OB_OK;
}

ob_retort pool_seekto (pool_hose ph, int64 idx)
{
  CHECK_HOSE_VALIDITY (ph);

  ph->index = idx;
  return OB_OK;
}

static ob_retort seek_to_time (pool_hose ph, float64 ts, time_comparison bound,
                               bool rel)
{
  CHECK_HOSE_VALIDITY (ph);

  if (!ph->index_lookup)
    return POOL_UNSUPPORTED_OPERATION;
  int64 idx = -1;
  ob_retort ret = ph->index_lookup (ph, &idx, ts, bound, rel);
  if (OB_OK == ret)
    ph->index = idx;
  return ret;
}

ob_retort pool_seekto_time (pool_hose ph, float64 timestamp,
                            time_comparison bound)
{
  return seek_to_time (ph, timestamp, bound, false);
}

ob_retort pool_seekby_time (pool_hose ph, float64 lapse, time_comparison bound)
{
  return seek_to_time (ph, lapse, bound, true);
}

const char *pool_name (pool_hose ph)
{
  return ph ? ph->name : NULL;
}

const char *pool_get_hose_name (pool_hose ph)
{
  return ph ? ph->hose_name : NULL;
}

ob_retort pool_set_hose_name (pool_hose ph, const char *name)
{
  CHECK_HOSE_VALIDITY (ph);

  if (!name)
    return OB_ARGUMENT_WAS_NULL;
  if (strcmp (name, ph->hose_name) != 0)
    {
      char *name_copy = strdup (name);
      if (!name_copy)
        return OB_NO_MEM;
      free (ph->hose_name);
      ph->hose_name = name_copy;
      // propagate name to tcp_pool_server
      if (ph->set_hose_name)
        return ph->set_hose_name (ph, name);
    }
  return OB_OK;
}

ob_retort pool_get_info (pool_hose ph, int64 hops, protein *return_prot)
{
  CHECK_HOSE_VALIDITY (ph);

  if (!return_prot)
    return OB_ARGUMENT_WAS_NULL;

  if (ph->info)
    return ph->info (ph, hops, return_prot);

  // else, fake it
  slaw ingests = slaw_map_inline_cf ("type", slaw_string (ph->method),
                                     "terminal", slaw_boolean (true), NULL);
  if (!ingests)
    return OB_NO_MEM;
  *return_prot = protein_from_ff (NULL, ingests);
  if (!*return_prot)
    return OB_NO_MEM;
  return OB_OK;
}

ob_retort pool_rewind (pool_hose ph)
{
  CHECK_HOSE_VALIDITY (ph);

  int64 idx;
  ob_retort pret = ph->oldest_index (ph, &idx);
  // Already at 0 index if it's empty
  if (pret == POOL_NO_SUCH_PROTEIN)
    return OB_OK;
  if (pret != OB_OK)
    return pret;
  return pool_seekto (ph, idx);
}

ob_retort pool_tolast (pool_hose ph)
{
  CHECK_HOSE_VALIDITY (ph);

  int64 idx;
  ob_retort pret = ph->newest_index (ph, &idx);
  if (pret == POOL_NO_SUCH_PROTEIN)
    // already at last
    return OB_OK;
  if (pret != OB_OK)
    return pret;
  return pool_seekto (ph, idx);
}

ob_retort pool_runout (pool_hose ph)
{
  CHECK_HOSE_VALIDITY (ph);

  int64 idx;
  ob_retort pret;
  pret = ph->newest_index (ph, &idx);
  // Runout on an empty pool is fine
  if ((pret != OB_OK) && (pret != POOL_NO_SUCH_PROTEIN))
    return pret;
  return pool_seekto (ph, idx + 1);
}

ob_retort pool_frwdby (pool_hose ph, int64 indoff)
{
  return ph ? pool_seekto (ph, ph->index + indoff) : POOL_NULL_HOSE;
}

ob_retort pool_backby (pool_hose ph, int64 indoff)
{
  return ph ? pool_seekto (ph, ph->index - indoff) : POOL_NULL_HOSE;
}

ob_retort pool_nth_protein (pool_hose ph, int64 idx, protein *p,
                            pool_timestamp *ts)
{
  CHECK_HOSE_VALIDITY (ph);

  if (!p)
    return OB_ARGUMENT_WAS_NULL;
  ob_retort pret = ph->nth_protein (ph, idx, p, ts);
  return pret;
}

ob_retort pool_curr (pool_hose ph, protein *p, pool_timestamp *ts)
{
  CHECK_HOSE_VALIDITY (ph);

  if (!p)
    return OB_ARGUMENT_WAS_NULL;
  ob_retort pret = ph->nth_protein (ph, ph->index, p, ts);
  return pret;
}

ob_retort pool_next (pool_hose ph, protein *ret_prot, pool_timestamp *ret_ts,
                     int64 *ret_index)
{
  CHECK_HOSE_VALIDITY (ph);

  ob_retort pret;
  int64 index_to_get = ph->index;

  if (ph->next)
    // Assume that if a next function pointer exists, it can handle
    // the case where ret_prot is NULL.
    return ph->next (ph, ret_prot, ret_ts, ret_index);

  protein p = NULL;
  if (!ret_prot)
    // make pool_next null-safe even if nth_protein isn't
    ret_prot = &p;

  // Perform "bounds-squishing" to 0 as described in bug 313 and bug 1286,
  // since nth_protein treats negative indices very differently.
  if (index_to_get < 0)
    index_to_get = 0;

  // The protein might get thrown away between the time we verify the
  // index and we ask for the protein, so loop until we get something
  // back.
  while (1)
    {
      // Try to get the next protein
      pret = ph->nth_protein (ph, index_to_get, ret_prot, ret_ts);
      // If the protein at our current index was discarded, try again
      // with the next available protein (the oldest protein in the
      // pool).  Otherwise, return the error.
      if (pret != POOL_NO_SUCH_PROTEIN)
        break;
      int64 idx = -1;
      pret = ph->oldest_index (ph, &idx);
      if (OB_OK == pret && idx > index_to_get)
        index_to_get = idx;
      else
        {
          // Currently it's impossible to have an empty pool if you had a
          // discarded protein, but may be possible in the future.  Go
          // ahead and support it now.
          if (OB_OK == pret)
            pret = POOL_NO_SUCH_PROTEIN;
          break;
        }
    }

  // Advance the index if we actually read something
  if (pret == OB_OK)
    {
      ob_err_accum (&pret, pool_seekto (ph, index_to_get + 1));
      if (ret_index)
        *ret_index = index_to_get;
    }

  protein_free (p);  // this is usually null; see above
  return pret;
}

ob_retort pool_await_next (pool_hose ph, pool_timestamp timeout,
                           protein *ret_prot, pool_timestamp *ret_ts,
                           int64 *ret_index)
{
  CHECK_HOSE_VALIDITY (ph);

  if (!ret_prot)
    return OB_ARGUMENT_WAS_NULL;

  // Hopefully common case: the very next protein we want is available
  ob_retort pret = pool_next (ph, ret_prot, ret_ts, ret_index);
  if (pret == OB_OK || pret == POOL_AWAIT_WOKEN || timeout == POOL_NO_WAIT)
    {
      // Even if timeout was zero, we should always say the request timed
      // out, rather than the protein didn't exist.  (bug 720)
      if (pret == POOL_NO_SUCH_PROTEIN)
        pret = POOL_AWAIT_TIMEDOUT;
      return pret;
    }

  // Drat.  Now we have to do work.  Set ourselves up in a while loop
  // to handle the case that the protein we were requesting was
  // discarded.
  while (true)
    {
      // At this point, the requested protein is either in the future
      // or the past.  If it's in the past, fast-forward to the next
      // available protein.
      if (pret == POOL_NO_SUCH_PROTEIN)
        {
          int64 idx = -1;
          pret = ph->oldest_index (ph, &idx);
          if (pret == OB_OK && idx > ph->index)
            {
              pret = pool_seekto (ph, idx);
              if (pret < OB_OK)
                return pret;
              pret = pool_next (ph, ret_prot, ret_ts, ret_index);
              if (pret != POOL_NO_SUCH_PROTEIN)
                return pret;
            }
          // Currently, pool can't be empty if we have a discarded
          // protein, but may be possible in the future.  Handle it
          // now.
          else if (pret != OB_OK && pret != POOL_NO_SUCH_PROTEIN)
            {
              // Real error, return
              return pret;
            }
        }

      // Okay, our protein is officially not in existence yet.  Await
      // a deposit.
      pret = ph->await_next_single (ph, timeout, ret_prot, ret_ts, ret_index);

      if (pret != POOL_NO_SUCH_PROTEIN)
        // We have found a protein, or the await timed out,
        // or there was an error.  In all cases, we should return now.
        return pret;
    }
}

ob_retort pool_hose_enable_wakeup (pool_hose ph)
{
  CHECK_HOSE_VALIDITY (ph);
  return ob_enable_wakeup (&ph->w);
}

ob_retort pool_hose_wake_up (pool_hose ph)
{
  CHECK_HOSE_VALIDITY (ph);
  return ob_wake_up (&(ph->w));
}

ob_retort pool_probe_frwd (pool_hose ph, bslaw search, protein *ret_prot,
                           pool_timestamp *ret_ts, int64 *ret_index)
{
  CHECK_HOSE_VALIDITY (ph);

  if (!ret_prot)
    return OB_ARGUMENT_WAS_NULL;

  ob_retort pret;
  protein prot = NULL;
  pool_timestamp ts;
  int64 idx = 0;

  if (ph->probe_frwd)
    return ph->probe_frwd (ph, search, ret_prot, ret_ts, ret_index);

  int64 saved = ph->index;

  while ((pret = pool_next (ph, &prot, &ts, &idx)) == OB_OK)
    {
      if (protein_search (prot, search) >= 0)
        {
          *ret_prot = prot;
          if (ret_ts)
            *ret_ts = ts;
          if (ret_index)
            *ret_index = idx;
          return OB_OK;
        }
      Free_Protein (prot);
    }

  // bug 547: restore index to old value if probe is unsuccessful
  ph->index = saved;
  assert (pret != OB_OK);

  return pret;
}

// Used to implement an overall, rather than per-protein, timeout.
// See bug 547 comment 3

pool_timestamp private_incremental_timeout (pool_timestamp timeout,
                                            pool_timestamp *target)
{
  if (timeout <= 0)
    return timeout;  // POOL_NO_WAIT and POOL_WAIT_FOREVER unchanged

  bool unused;
  if (OB_FP_NAN == ob_fpclassify (*target, &unused))
    {
      *target = timeout + ob_current_time ();
      return timeout;
    }
  else
    {
      pool_timestamp delta = *target - ob_current_time ();
      if (delta <= 0)
        delta = POOL_NO_WAIT;
      return delta;
    }
}

ob_retort pool_await_probe_frwd (pool_hose ph, bslaw search,
                                 pool_timestamp timeout, protein *ret_prot,
                                 pool_timestamp *ret_ts, int64 *ret_index)
{
  CHECK_HOSE_VALIDITY (ph);

  if (!ret_prot)
    return OB_ARGUMENT_WAS_NULL;

  if (ph->await_probe_frwd)
    {
      ob_retort pret =
        ph->await_probe_frwd (ph, search, timeout, ret_prot, ret_ts, ret_index);
      if (pret != POOL_UNSUPPORTED_OPERATION)
        return pret;
      // If the pool type generally supports await_probe_frwd, but this
      // specific hose does not (e. g. TCP pools now support await_probe_frwd,
      // but if we connect to an older server, it won't be supported)
      // then we get POOL_UNSUPPORTED_OPERATION, and fall through to
      // the non-optimized path below.
    }

  pool_timestamp target = OB_NAN;
  ob_retort pret;
  protein prot = NULL;
  pool_timestamp ts;
  int64 idx;
  int64 saved = ph->index;

  while (
    (pret = pool_await_next (ph, private_incremental_timeout (timeout, &target),
                             &prot, &ts, &idx))
    == OB_OK)
    {
      if (protein_search (prot, search) >= 0)
        {
          *ret_prot = prot;
          if (ret_ts)
            *ret_ts = ts;
          if (ret_index)
            *ret_index = idx;
          return OB_OK;
        }
      Free_Protein (prot);
    }

  // bug 547: restore index to old value if probe is unsuccessful
  ph->index = saved;
  assert (pret != OB_OK);

  // bug 720:
  // Looks like POOL_NO_WAIT returns POOL_NO_SUCH_PROTEIN, but a nonzero
  // wait returns POOL_AWAIT_TIMEDOUT.  Since we may have modified
  // the user's timeout behind the scenes, we may also need to modify
  // the retort to match.  Ugh.
  if (pret == POOL_NO_SUCH_PROTEIN && timeout > 0)
    pret = POOL_AWAIT_TIMEDOUT;

  return pret;
}

ob_retort pool_prev (pool_hose ph, protein *ret_prot, pool_timestamp *ret_ts,
                     int64 *ret_index)
{
  CHECK_HOSE_VALIDITY (ph);

  if (!ret_prot)
    return OB_ARGUMENT_WAS_NULL;
  // starting_index is the index of the protein last returned by a
  // pool_prev or pool_curr op
  int64 starting_index = ph->index;
  int64 last_index;

  // Check for zero index to prevent infinite loops
  if (ph->index <= 0)
    return POOL_NO_SUCH_PROTEIN;

  if (ph->prev)
    {
      ob_retort pret = ph->prev (ph, ret_prot, ret_ts, ret_index);
      if (pret != POOL_UNSUPPORTED_OPERATION)
        return pret;
      // If the pool type generally supports prev, but this
      // specific hose does not (e. g. TCP pools now support prev,
      // but if we connect to an older server, it won't be supported)
      // then we get POOL_UNSUPPORTED_OPERATION, and fall through to
      // the non-optimized path below.
    }

  // Are we past the last protein?  Note that we will retrieve the
  // protein *before* the starting index.
  ob_retort pret = ph->newest_index (ph, &last_index);
  if (pret != OB_OK)
    return pret;

  if (starting_index > last_index + 1)
    starting_index = last_index + 1;

  // Ratchet the index back to get the previous protein
  pret = pool_seekto (ph, starting_index - 1);

  if (pret == OB_OK)
    pret = ph->nth_protein (ph, ph->index, ret_prot, ret_ts);

  if (pret == OB_OK && ret_index)
    *ret_index = ph->index;

  return pret;
}

ob_retort pool_probe_back (pool_hose ph, bslaw search, protein *ret_prot,
                           pool_timestamp *ret_ts, int64 *ret_index)
{
  CHECK_HOSE_VALIDITY (ph);

  if (!ret_prot)
    return OB_ARGUMENT_WAS_NULL;

  ob_retort pret;
  protein prot = NULL;
  pool_timestamp ts;
  int64 idx = -1;

  // No proteins below 0, so if we're already there, we know it can't
  // succeed.
  if (ph->index <= 0)
    return POOL_NO_SUCH_PROTEIN;

  if (ph->probe_back)
    {
      pret = ph->probe_back (ph, search, ret_prot, ret_ts, ret_index);
      if (pret != POOL_UNSUPPORTED_OPERATION)
        return pret;
      // If the pool type generally supports probe_back, but this
      // specific hose does not (e. g. TCP pools now support probe_back,
      // but if we connect to an older server, it won't be supported)
      // then we get POOL_UNSUPPORTED_OPERATION, and fall through to
      // the non-optimized path below.
    }

  int64 saved = ph->index;

  while ((pret = pool_prev (ph, &prot, &ts, &idx)) == OB_OK)
    {
      if (protein_search (prot, search) >= 0)
        {
          *ret_prot = prot;
          if (ret_ts)
            *ret_ts = ts;
          if (ret_index)
            *ret_index = idx;
          return OB_OK;
        }
      Free_Protein (prot);
    }

  // bug 547: restore index to old value if probe is unsuccessful
  ph->index = saved;
  assert (pret != OB_OK);

  return pret;
}


void private_maybe_fill_index (pool_hose ph,
                               ob_retort (*fn) (pool_hose ph, int64 *index_out),
                               int64 *out)
{
  if (out)
    {
      ob_retort tort = fn (ph, out);
      if (tort < OB_OK)
        *out = tort;
    }
}


static void fill_protein_metadata (bslaw s, int64 *len, int64 *num)
{
  if (s)
    {
      *len = slaw_len (s);
      *num = slaw_list_count (s);
    }
  else
    {
      *len = 0;
      *num = -1;
    }
}


void pool_fetch (pool_hose ph, pool_fetch_op *ops, int64 nops,
                 int64 *oldest_idx_out, int64 *newest_idx_out)
{
  pool_fetch_ex (ph, ops, nops, oldest_idx_out, newest_idx_out, false);
}


void pool_fetch_ex (pool_hose ph, pool_fetch_op *ops, int64 nops,
                    int64 *oldest_idx_out, int64 *newest_idx_out, bool clamp)
{
  int64 i;
  ob_retort tort;

  tort = check_hose_validity (ph);
  if (tort < OB_OK)
    {
splatter_retort:
      if (oldest_idx_out)
        *oldest_idx_out = tort;
      if (newest_idx_out)
        *newest_idx_out = tort;
      if (ops)
        for (i = 0; i < nops; i++)
          {
            ops[i].tort = tort;
            ops[i].p = NULL;
          }
      return;
    }

  if (ph->fetch)
    {
      tort = ph->fetch (ph, ops, nops, oldest_idx_out, newest_idx_out, clamp);
      if (tort >= OB_OK)
        return;
      if (tort != POOL_UNSUPPORTED_OPERATION)
        goto splatter_retort;
    }

  for (i = 0; ops && i < nops; i++)
    {
      protein p = NULL;
      ops[i].tort = pool_nth_protein (ph, ops[i].idx, &p, &ops[i].ts);
      if (ops[i].tort >= OB_OK)
        {
          ops[i].total_bytes = protein_len (p);
          fill_protein_metadata (protein_descrips (p), &ops[i].descrip_bytes,
                                 &ops[i].num_descrips);
          fill_protein_metadata (protein_ingests (p), &ops[i].ingest_bytes,
                                 &ops[i].num_ingests);
          const unt8 *rude =
            (const unt8 *) protein_rude (p, &ops[i].rude_bytes);
          int64 rlen = 0;
          if (ops[i].rude_offset >= 0)
            {
              rlen = ops[i].rude_bytes - ops[i].rude_offset;
              if (ops[i].rude_length >= 0 && ops[i].rude_length < rlen)
                rlen = ops[i].rude_length;
              if (rlen < 0 || !rude)
                rlen = 0;
              if (rlen > 0)
                rude += ops[i].rude_offset;
            }
          if (ops[i].want_descrips && ops[i].want_ingests
              && rlen == ops[i].rude_bytes)
            ops[i].p = p; /* use whole protein */
          else
            {
              // construct a new protein with the parts we want
              protein np = protein_from_llr (
                ops[i].want_descrips ? protein_descrips (p) : NULL,
                ops[i].want_ingests ? protein_ingests (p) : NULL, rude, rlen);
              protein_free (p);
              ops[i].p = np;
            }
        }
      else if (ops[i].tort == POOL_NO_SUCH_PROTEIN && clamp)
        {
          int64 idx;
          if (OB_OK == pool_oldest_index (ph, &idx))
            {
              if (idx > ops[i].idx
                  || (OB_OK == pool_newest_index (ph, &idx)
                      && idx < ops[i].idx))
                {
                  ops[i].idx = idx; /* clamp the index to oldest or newest */
                  i--;              /* and go round again */
                }
            }
        }
    }

  private_maybe_fill_index (ph, pool_oldest_index, oldest_idx_out);
  private_maybe_fill_index (ph, pool_newest_index, newest_idx_out);
}


static ob_retort pool_list_internal (slabu *sb, const char *dir_path,
                                     int prefix);

static ob_retort pool_list_local (const char *uri, slaw *ret_slaw)
{
  char dir_path[PATH_MAX];
  //
  // Pool configuration file pathnames look like:
  //
  // ${OB_POOLS_DIR}/<pool_name>/pool.conf
  //
  // We only care about the directory itself since we're only trying
  // to get the pool name back.  Future generations may want to open
  // the configuration file and get other information out.
  //

  ob_retort pret;
  if (!uri || !*uri)
    pret = pool_dir_path (dir_path);
  else
    pret = pool_build_pool_dir_path (dir_path, uri);

  if (pret < OB_OK)
    return pret;

  slabu *sb = slabu_new ();
  if (sb == NULL)
    return OB_NO_MEM;

  pret = pool_list_internal (sb, dir_path, strlen (dir_path));
  if (pret < OB_OK)
    {
      slabu_free (sb);
      return pret;
    }

  slabu_sort (sb);

  *ret_slaw = slaw_list_f (sb);
  return pret;
}

#ifdef _MSC_VER

// reslash because the slashed pool name convention expects
// forward slashes only
static int64 reslash_and_add (slabu *sb, const char *namep)
{
  const size_t len = strlen (namep);
  char *reslashed = (char *) alloca (1 + len);
  for (size_t i = 0; i < len; i++)
    {
      char c = namep[i];
      if (c == OB_DIR_CHAR)
        c = '/';
      reslashed[i] = c;
    }
  reslashed[len] = 0;
  return slabu_list_add_c (sb, reslashed);
}

// Windows version uses entry and ignores name
static unt64 size_of_file (const struct dirent *entry, const char *ignored_name)
{
  unt64 sz = entry->d_win32.nFileSizeHigh;
  sz <<= 32;
  sz |= entry->d_win32.nFileSizeLow;
  return sz;
}

#else /* _MSC_VER */

#define reslash_and_add(x, y) slabu_list_add_c (x, y)

// UNIX version uses name and ignores entry
static unt64 size_of_file (const struct dirent *ignore, const char *name)
{
  struct stat st;
  if (stat (name, &st) < 0)
    return 0; /* on error, pretend size is 0 */
  return st.st_size;
}

#endif /* _MSC_VER */

#define POOL_CONF_NAME "pool.conf"

/* The important point is that if there is an entry named "pool.conf",
 * it must sort first.  Other than that, we don't really care what the
 * order is, so lexicographic is fine. */
static int hackishly_sort_my_way (const struct dirent **a,
                                  const struct dirent **b)
{
  const char *s1 = (*a)->d_name;
  const char *s2 = (*b)->d_name;

  bool pc1 = (0 == strcmp (s1, POOL_CONF_NAME));
  bool pc2 = (0 == strcmp (s2, POOL_CONF_NAME));

  if (pc1 && pc2)
    return 0;
  else if (pc1 && !pc2)
    return -1;
  else if (!pc1 && pc2)
    return 1;
  else
    return (strcmp (s1, s2));
}

static int filter_out_dot_and_doubledot (const struct dirent *entry)
{
  if ((strcmp (entry->d_name, ".") == 0) || (strcmp (entry->d_name, "..") == 0))
    return 0;
  else
    return 1;
}

static ob_retort pool_list_internal (slabu *sb, const char *dir_path,
                                     int prefix)
{
  struct dirent **namelist = NULL;
  const int nentries = ob_scandir (
    dir_path, &namelist, filter_out_dot_and_doubledot, hackishly_sort_my_way);

  if (nentries < 0 && errno == ENOENT)
    {
      // bug 388: Don't consider missing directory an error; it just means
      // there are no pools.
      return OB_OK;
    }
  else if (nentries < 0)
    {
      const int erryes = errno;
      OB_LOG_WARNING_CODE (0x2010103f, "Got '%s' when examining '%s'\n",
                           strerror (erryes), dir_path);
      return OB_OK;
    }

  int entno;
  for (entno = 0; entno < nentries; entno++)
    {
      struct dirent *entry = namelist[entno];

#ifndef _MSC_VER /* handle symlinks on UNIX only */
      struct dirent fake_entry;

      if (entry->d_type == DT_LNK)
        {
          struct stat st;
          OB_INVALIDATE (st);
          OB_INVALIDATE (fake_entry);

          slaw linkname = slaw_string_format ("%s/%s", dir_path, entry->d_name);
          if (!linkname)
            {
              // out of memory
              ob_scandir_free (namelist, nentries);
              return OB_NO_MEM;
            }

          const int stret = stat (slaw_string_emit (linkname), &st);
          const int erryes = errno;
          if (stret < 0)
            {
              OB_LOG_WARNING_CODE (0x2010103c, "Got '%s' for link '%s'\n",
                                   strerror (erryes),
                                   slaw_string_emit (linkname));
              Free_Slaw (linkname);
              continue;  // probably a broken link, so skip it
            }
          Free_Slaw (linkname);
          ob_safe_copy_string (fake_entry.d_name, sizeof (fake_entry.d_name),
                               entry->d_name);
          if (S_ISREG (st.st_mode))
            fake_entry.d_type = DT_REG;
          else if (S_ISDIR (st.st_mode))
            fake_entry.d_type = DT_DIR;
          else  // something weird; skip it
            continue;
          entry = &fake_entry;
        }
#endif /* symlink handling */

      /* Don't even bother looking at files/directories whose names
       * are not legal as a pool name component. */
      if (OB_OK != validate_name_component1 (entry->d_name))
        continue;

      slaw subdir =
        slaw_string_format ("%s%c%s", dir_path, OB_DIR_CHAR, entry->d_name);

      if (!subdir)
        {
          // out of memory
          ob_scandir_free (namelist, nentries);
          return OB_NO_MEM;
        }

      unt64 filsiz;
      if (entry->d_type == DT_REG
          && (filsiz = size_of_file (entry, slaw_string_emit (subdir)))
          && 0 == (filsiz % POOL_SIZE_GRANULARITY))
        {
          // We use this heuristic: files that are a multiple of the page
          // size are single-file pools.
          const char *namep = slaw_string_emit (subdir) + prefix;
          if (*namep == '/' || *namep == OB_DIR_CHAR)
            {
              namep++;
              if (reslash_and_add (sb, namep) < 0)
                {
                  // out of memory
                  ob_scandir_free (namelist, nentries);
                  slaw_free (subdir);
                  return OB_NO_MEM;
                }
            }
        }
      else if (entry->d_type == DT_REG
               && strcmp (entry->d_name, POOL_CONF_NAME) == 0)
        {
          const char *namep = dir_path + prefix;
          if (*namep == '/' || *namep == OB_DIR_CHAR)
            {
              namep++;
              if (reslash_and_add (sb, namep) < 0)
                {
                  // out of memory
                  ob_scandir_free (namelist, nentries);
                  slaw_free (subdir);
                  return OB_NO_MEM;
                }
              /* So, here's the thing.  If we found "pool.conf" here,
               * then this whole directory is a pool, and therefore
               * no other file in this directory can be a single-file
               * pool (even though the backing file will look like one).
               * That's why we went to all the trouble to sort the
               * directory so "pool.conf" comes first. */
              slaw_free (subdir);
              break;
            }
        }
      else if (entry->d_type == DT_DIR)
        {
          ob_retort tort =
            pool_list_internal (sb, slaw_string_emit (subdir), prefix);
          if (tort < OB_OK)
            {
              ob_scandir_free (namelist, nentries);
              slaw_free (subdir);
              return tort;
            }
        }

      slaw_free (subdir);
    }

  ob_scandir_free (namelist, nentries);
  return OB_OK;
}


typedef struct
{
  ob_retort status;
#ifdef _MSC_VER
  HANDLE fd;
#else
  int fd;
#endif
} pclock_t;

/// Is the pool with this name hosted on another system?

static bool pool_name_is_remote (const char *pool_uri)
{
  assert (pool_uri);
  return (!!strstr (pool_uri, "://"));
}

/// Figure out what type this pool is.  The pool in question may
/// already exist, or the caller may be about to create it.

// When the ugly was getting pushed around, a lot of it ended up in
// this poor function, where it is at least isolated from the pretty.
// Right now, there are three ways to figure out what kind of pool a
// pool is: (1) the caller tells us the type because it is creating
// the pool for the first time, (2) the pool is hosted on the local
// machine and we can read the type out of the configuration file, (3)
// the pool lives on some other machine somewhere off in the network,
// and so we extract the type from a URI-ish pool name.
//
// In the future, the pool almanac will return and do all this nasty
// stuff off in some other place, and we may also get more information
// to work with than just the pool name.  Until then, we are limited
// by the information available.

static ob_retort pool_characterize (pool_hose ph, const char *type)
{
  assert (ph);
  // Is this a local or remote pool?
  if (pool_name_is_remote (ph->name))
    {
      ph->remote = true;
      // Pull out exactly what kind of remote pool and use it for our method
      char *protocol = strdup (ph->name);
      if (!protocol)
        return OB_NO_MEM;
      char *end = strstr (protocol, "://");
      // end should not be NULL, because pool_name_is_remote() was true
      assert (end);
      *end = '\0';
      ph->method = strdup (protocol);
      free (protocol);
      if (!ph->method)
        return OB_NO_MEM;
      // Allocate network data struct, to be filled in by type methods
      ph->net = (pool_net_data *) malloc (sizeof (*(ph->net)));
      if (!ph->net)
        return OB_NO_MEM;
      memset (ph->net, 0, sizeof (*ph->net));
    }
  else  // Local
    {
      if (type)
        {
          ph->method = strdup (type);
          if (!ph->method)
            return OB_NO_MEM;
        }
      else
        {
          // Get it from the config file - load config sets method
          ob_retort pret = pool_load_default_config (ph);
          if (pret != OB_OK)
            return pret;
        }
    }
  return OB_OK;
}

typedef ob_retort (*load_func_t) (pool_hose);

/// Load the methods necessary to access this pool and fill in the
/// pool's type-specific operation vector with them.  pool_hose must
/// have the ph->method set correctly.
///
/// Returns POOL_TYPE_BADTH if we can't find an implementation of the
/// requested pool type.
///
/// Note: this used to be implemented with dlopen and dlsym, but that
/// turned out to be more trouble than it was worth (bug 1005), so
/// now we just do string comparison and return the appropriate
/// pointer based on the two supported pool types we know about.

static ob_retort pool_load_methods (pool_hose ph)
{
  assert (ph);
  load_func_t load_func = NULL;

  if (ph->method)
    {
      if (0 == strcmp (ph->method, "mmap"))
        load_func = pool_mmap_load_methods;
      else if (0 == strcmp (ph->method, "tcp")
               || 0 == strcmp (ph->method, "tcpo")
               || 0 == strcmp (ph->method, "tcps"))
        load_func = pool_tcp_load_methods;
    }

  if (!load_func)
    {
      OB_LOG_ERROR_CODE (0x20101010, "Pool type %s not implemented\n",
                         ph->method);
      return POOL_TYPE_BADTH;
    }

  ob_retort pret = load_func (ph);

  ob_log (OBLV_DBUG, 0x20101011, "loadedits fun %s methods\n", ph->method);

  return pret;
}

/// Get an exclusive lock on the configuration for all pools sharing
/// this OB_POOLS_DIR.  While this lock is held, no other process can read
/// or write the configuration for any pool.  This lock must be held
/// while creating, disposing, or participating in a pool.  Returns -1
/// if the lock failed, otherwise an integer that must be passed to
/// pool_config_unlock().

#ifdef _MSC_VER

// Windows implementation notes:
//
// Mutexes are the best choice for configuration locking because
// creating/opening an existing named mutex is done with the same call
// (CreateMutex)
//
// If the mutex already exists, the "initial owner" parameter is ignored.
// we don't use that parameter anyway, and always do a wait after
// acquiring the mutex handle

static pclock_t pool_config_lock (bool need_not_exist, bprotein permissions)
{
  ob_log (OBLV_DBUG, 0x20101014, "grabbing config lock\n");

  pclock_t result;
  OB_CLEAR (result);

  const char *mutex_name = "com.oblong.plasma.pools.lock";

  HANDLE mutex_handle = CreateMutex (0, 0, mutex_name);
  if (mutex_handle == NULL)
    {
      result.status = ob_win32err_to_retort (GetLastError ());
      return result;
    }

  DWORD winResult = WaitForSingleObject (mutex_handle, INFINITE);

  switch (winResult)
    {
      case WAIT_OBJECT_0:
        //we have the lock
        {
          POOL_UGLY_CALLBACK (POOL_UGLY_CALLBACK_CONFIG_LOCK,
                              POOL_UGLY_CALLBACK_POST_ACQUIRE);

          result.status = OB_OK;
          result.fd = mutex_handle;
          return result;
        }

      case WAIT_ABANDONED_0:
        //we have the lock, but we have it because some other
        //process/thread exited without releasing it (likely because
        //of a crash). the state of our pool folder is dubious now.
        //The good news is we are not deadlocked, and we can at
        //least make a decision to continue on or exit with an error
        //message
        POOL_UGLY_CALLBACK (POOL_UGLY_CALLBACK_CONFIG_LOCK,
                            POOL_UGLY_CALLBACK_POST_ACQUIRE);

        OB_LOG_ERROR_CODE (0x20101017, "acquired an abandoned config lock, "
                                       "state of pool folder is dubious\n");
        result.status = OB_OK;
        result.fd = mutex_handle;
        return result;
    }

  OB_LOG_ERROR_CODE (
    0x20101018, "unknown result waiting for config lock - %d\n", winResult);

  result.status = OB_UNKNOWN_ERR;
  return result;
}

static ob_retort pool_config_unlock (pclock_t lck)
{
  OB_LOG_DEBUG_CODE (0x20101019, "dropping config lock\n");
  if (lck.status < OB_OK)
    return lck.status;

  if (lck.status == POOL_LOCK_NO_DIR)
    return OB_OK;

  POOL_UGLY_CALLBACK (POOL_UGLY_CALLBACK_CONFIG_LOCK,
                      POOL_UGLY_CALLBACK_PRE_RELEASE);

  BOOL release_result = ReleaseMutex (lck.fd);
  BOOL close_result = CloseHandle (lck.fd);

  if (release_result == false || close_result == false)
    {
      OB_LOG_DEBUG_CODE (0x2010101a,
                         "ReleaseMutex returned %d, CloseHandle returned %d\n",
                         release_result, close_result);

      return OB_UNKNOWN_ERR;
    }

  POOL_UGLY_CALLBACK (POOL_UGLY_CALLBACK_CONFIG_LOCK,
                      POOL_UGLY_CALLBACK_POST_RELEASE);

  return OB_OK;
}

#else

// Unix/Linux Implementation notes:
//
// Semaphores are a bad choice for configuration locking because you
// you have to try to get the semaphore, create it if it doesn't
// exist, and set the value, every time you do a configuration
// command.  This is bad because it exposes the worst hole in SysV
// semaphores - the race between semaphore creation and setting of the
// initial value - whenever pools are first created.  It's quite
// reasonable to expect this race to be hit in reality, since a common
// paradigm is for several processes to simultaneous
// participate_creatingly() on multiple pools at program start-up
// time.
//
// File locks are portable, automatically dropped on process death,
// and naturally map to the top-level configuration dir.  Use 'em.

/// Acquire the exclusive lock on the configuration for all pools sharing
/// this OB_POOLS_DIR.  If OB_POOLS_DIR does not exist, and \a need_not_exist
/// is true, then the internal success code POOL_LOCK_NO_DIR is returned.
/// If \a need_not_exist is false, then OB_POOLS_DIR will be created if it
/// does not exist, and \a permissions will be consulted to set the
/// permissions on the new directory.

static pclock_t pool_config_lock (bool need_not_exist, bprotein permissions)
{
  ob_log (OBLV_DBUG, 0x2010101b, "grabbing config lock\n");
  pclock_t result;
  OB_CLEAR (result);

  /* This is probably not particularly wise, but we need some global file
   * or directory that will always exist and is accessible by all users,
   * in order to obtain a global lock.  (We used to use the pools directory,
   * but that can vary from one process to another, and we don't have to have
   * a pools directory at all if you're only using "local:" URIs with
   * absolute filenames.)  And, /tmp will always exist and be accessible.
   * The downside is that if some other program is also locking /tmp,
   * we'll be sharing the lock with them.  Shouldn't be terrible, as long
   * as they don't hold it for long periods. */

  char dir_path[PATH_MAX] = "/tmp";

  /* If you're building a snap package, and pool creation fails because
   * locking /tmp fails,
   * it's because snapd has access locked down six ways from sunday.
   * Workaround for now is as follows: in snapcraft.yaml, do
   * apps:
   *   shiny-app-name:
   *     environment:
   *       # Modify libPlasma's behavior to be friendlier to snap environment
   *       OB_LOCK_POOL_DIR: 'true'
   * Then plasma will lock the pool dir instead, as in days of yore.
   */
  if (getenv ("OB_LOCK_POOL_DIR"))
    {
      // Open the pool dir for locking purposes
      ob_retort pret = pool_dir_path (dir_path);
      if (pret != OB_OK)
        {
          OB_LOG_ERROR_CODE (0x2010101c, "Couldn't get pool directory path\n");
          result.status = pret;
          return result;
        }
    }

  POOL_UGLY_CALLBACK (POOL_UGLY_CALLBACK_CONFIG_LOCK,
                      POOL_UGLY_CALLBACK_PRE_ACQUIRE);
  int fd = ob_open_cloexec (dir_path, O_RDONLY, 0);
  if (fd < 0)
    {
      OB_PERRORF_CODE (0x20101032, "Couldn't open directory '%s' for locking",
                       dir_path);
      result.status = ob_errno_to_retort (errno);
      return result;
    }
  if (flock (fd, LOCK_EX) < 0)
    {
      int erryes = errno;
      OB_CHECK_POSIX_CODE (0x20101033, close (fd));
      result.status = ob_errno_to_retort (erryes);
      OB_LOG_WARNING_CODE (0x20101035, "Failed to lock %s\n", dir_path);
      return result;
    }
  POOL_UGLY_CALLBACK (POOL_UGLY_CALLBACK_CONFIG_LOCK,
                      POOL_UGLY_CALLBACK_POST_ACQUIRE);
  result.status = OB_OK;
  result.fd = fd;
  return result;
}


/// Drop the exclusive lock on the configuration for all pools sharing
/// this OB_POOLS_DIR.  Pass it the pclock_t returned by pool_config_lock().

static ob_retort pool_config_unlock (pclock_t lck)
{
  ob_log (OBLV_DBUG, 0x2010101f, "dropping config lock\n");
  if (lck.status < OB_OK)
    return lck.status;
  if (lck.status == POOL_LOCK_NO_DIR)
    return OB_OK;
  int fd = lck.fd;
  POOL_UGLY_CALLBACK (POOL_UGLY_CALLBACK_CONFIG_LOCK,
                      POOL_UGLY_CALLBACK_PRE_RELEASE);
  if (flock (fd, LOCK_UN) < 0 || close (fd) < 0)
    return ob_errno_to_retort (errno);
  POOL_UGLY_CALLBACK (POOL_UGLY_CALLBACK_CONFIG_LOCK,
                      POOL_UGLY_CALLBACK_POST_RELEASE);
  return OB_OK;
}

#endif

// We have an unpleasant design problem to do with locking and code
// reuse.  Pool create, dispose, participate, and
// participate_creatingly all change pool configuration state, and
// must take the config lock.  However, many of these functions wish
// to call other functions in this class and must therefore avoid
// attempting to take the config lock a second time (which results in
// deadlock).  We must therefore either (1) duplicate lots of code, or
// (2) structure the code such that callers holding the lock can avoid
// taking the lock a second time.
//
// Of the ugly solutions available, the least ugly and most standard
// method I'm aware of is to create wrapper functions that take and
// drop the lock, and call the actual meat of the
// create/dispose/etc. function.  The locking wrapper is named, e.g.,
// pool_create() and is the entry point from outside the library.  The
// wrapped function is named, e.g., __pool_create(), and is called by
// other configuration functions, e.g.,
// __pool_participate_creatingly() calls __pool_create().
//
// We run into exactly the same problem with remote pools.  When
// creating, disposing, etc. remote pools, all the locking is done on
// the remote end, so we don't want to hold the lock while operating on
// them.

static ob_retort __pool_participate (const char *pool_uri, pool_hose *ret_ph);

static void accum (ob_retort *dest, ob_retort err, const char *what,
                   pool_hose ph, bool whether)
{
  assert (dest);
  if (!what)
    what = "";

  if (whether)
    {
      if (err != OB_OK)
        {
          const char *name = NULL;
          if (ph != NULL)
            name = pool_name (ph);
          OB_LOG_ERROR_CODE (0x20101020, "'%s' failed with %s on '%s'\n", what,
                             ob_error_string (err), (name ? name : "unknown"));
        }
      if (*dest == OB_OK)
        *dest = err;
    }
}

#define ACCUM(x) accum (&pret, x, #x, ph, doComplain)

/// Free, close, shutdown, destroy, etc. any state associated with this
/// pool, including the configuration files.  When this function
/// returns, the pool no longer exists.  This function may be called on
/// a pool_hose at any level of initialization (as long as the
/// type-specific implementation has wrapped up its own state
/// properly).  ph is a pool_hose in any state of initialization.
/// pret is a ob_retort-style error code to return once the pool
/// has been destroyed.
///
/// This slightly non-standard arrangement of return value in which a
/// return value is passed in and returned unaltered, allows recovery
/// and exit in one line rather than two, thus:
///
/// if (pret != OB_OK)
///   return pool_create_cleanup (ph, pret);

static ob_retort pool_create_cleanup (pool_hose ph, ob_retort pret,
                                      bool doComplain)
{
  // If the name was invalid, don't try to clean it up by
  // deleting the invalid name!  (For example, could be a security
  // risk if the pool was named "../../some/secret/directory", which
  // is illegal since it contains components starting with ".",
  // but then we would still try to clean up by deleting it.)
  if (pool_is_local (ph) && pret != POOL_POOLNAME_BADTH)
    {
      ACCUM (pool_remove_default_config (ph));
      // Below must come last, after everything else has removed its
      // configuration files.
      ACCUM (pool_remove_pool_dir (ph));
    }
  pool_free_pool_hose (ph);
  return pret;
}

static ob_retort __pool_dispose (const char *pool_uri);

static ob_retort __pool_create (const char *pool_uri, const char *type,
                                bprotein options)
{
  ob_retort pret;
  pool_hose ph;
  // Sanity checks
  if (!pool_uri || strlen (pool_uri) == 0)
    return POOL_POOLNAME_BADTH;
  if (!type)
    return POOL_TYPE_BADTH;

  // Does it already exist?
  pret = __pool_participate (pool_uri, &ph);

  // Could have other errors, but they would mean pool existed
  // XXX: That statement is clearly false.  For example, if the
  // name is illegal, then obviously it *can't* already exist!
  if (pret != POOL_NO_SUCH_POOL)
    {
      if (pret == OB_OK)
        {
          // Can't call pool_withdraw here, for two related reasons:
          // 1) pool_withdraw could call pool_dispose, which will result
          //    in a deadlock, since it tries to reacquire the config
          //    lock, which we already hold, and which is not recursive.
          //    (this was bug 2490)
          // 2) even if it wasn't for the deadlock problem, we need to
          //    specially handle the case where pool_withdraw calls
          //    pool_dispose, since that would result in the pool not
          //    existing, and our whole goal is to make sure the pool
          //    exists.  So if withdraw destroys the pool, we need to
          //    re-create it.
          // For these reasons, we call ph->withdraw directly, and handle
          // the POOL_DELETE_ME case ourselves.
          pret = ph->withdraw (ph);
          pool_free_pool_hose (ph);
          OB_INVALIDATE (ph);
          if (pret == POOL_DELETE_ME)
            {
              if (pool_name_is_remote (pool_uri))
                {
                  OB_LOG_BUG_CODE (
                    0x2010103a, "Didn't expect that for pool '%s'\n", pool_uri);
                  return OB_UNKNOWN_ERR;
                }
              pret = __pool_dispose (pool_uri);
              if (pret < OB_OK)
                return pret;
              goto do_create;
            }
          else if (pret < OB_OK)
            return pret;
        }
      else if (pret == POOL_POOLNAME_BADTH)
        return pret;
      return POOL_EXISTS;
    }

do_create:
  ph = pool_new_pool_hose (pool_uri);
  if (!ph)
    return OB_NO_MEM;

  const bool flock_by_default = (0 != (POOL_FLAG_FLOCK & POOL_DEFAULT_FLAGS));

  ph->pool_directory_version =
    (slaw_path_get_bool (options, "resizable", OB_RESIZABLE_BY_DEFAULT)
       ? POOL_DIRECTORY_VERSION_CONFIG_IN_MMAP
       : POOL_DIRECTORY_VERSION_CONFIG_IN_FILE);

#ifndef _MSC_VER
  if (slaw_path_get_bool (options, "flock", flock_by_default))
    {
      ph->lock_ops = &pool_flock_ops;
      /* the older pool style doesn't have a place to store the flock flag */
      ph->pool_directory_version = POOL_DIRECTORY_VERSION_CONFIG_IN_MMAP;
    }
  else
    ph->lock_ops = &pool_sem_ops;
#endif

  if (slaw_path_get_bool (options, "single-file", false))
    ph->pool_directory_version = POOL_DIRECTORY_VERSION_SINGLE_FILE;

  pret = pool_characterize (ph, type);
  if (pret != OB_OK)
    return pool_create_cleanup (ph, pret, false);

  pret = pool_load_methods (ph);
  if (pret != OB_OK)
    return pool_create_cleanup (ph, pret, false);

  if (pool_is_local (ph))
    {
      int mode, uid, gid;
      pret = pool_parse_permissions (options, &mode, &uid, &gid);
      if (pret != OB_OK)
        return pool_create_cleanup (ph, pret, false);
      ph->perms.mode = mode;
      ph->perms.uid = uid;
      ph->perms.gid = gid;
      pret = pool_create_pool_dir (ph, mode, uid, gid);
      if (pret != OB_OK)
        return pool_create_cleanup (ph, pret, false);
#ifndef _MSC_VER
      if (ph->pool_directory_version != POOL_DIRECTORY_VERSION_SINGLE_FILE
          || ph->lock_ops != &pool_flock_ops)
#endif
        {
          /* Just shoot me now.  Keeping straight all the different ordering
           * requirements for different sets of options is driving me nuts.
           * But, the idea here is that if we're using real semaphores,
           * we want to go ahead and create them now.  Or, if we're using
           * a directory-per-pool structure (in which case the directory
           * has already been created, and the lock files will go under
           * that directory), then also create the lock files now.
           * But, if we're using lock files and have a single-file pool,
           * then we have to defer lockfile creation, because the logic
           * which determines the location of the lockfiles calls
           * realpath() on the pool backing file (i. e. the single file),
           * and that will fail since the backing file hasn't been created
           * yet; it is created below in the call to ph->create().
           * But this is really brittle and horrendous.  Why must we have
           * so many possibilities? */
          pret = pool_create_semaphores (ph);
          if (pret != OB_OK)
            return pool_create_cleanup (ph, pret, false);
        }
      if (ph->pool_directory_version != POOL_DIRECTORY_VERSION_SINGLE_FILE)
        {
          // We have to write the config file *after* we choose the sem_key.
          pret = pool_save_default_config_with_permissions (ph, mode, uid, gid);
          if (pret != OB_OK)
            {
              pool_destroy_semaphores (ph, false);
              return pool_create_cleanup (ph, pret, false);
            }
          pret = pool_init_multi_await (ph, mode, uid, gid);
          if (pret != OB_OK)
            {
              pool_destroy_semaphores (ph, false);
              return pool_create_cleanup (ph, pret, false);
            }
        }
    }

  // Must be after all opportunities to fail unless we want to define
  // a type-specific failed create cleanup function too (ugh!)
  // Could possibly use the type pool_destroy () function if necessary
  pret = ph->create (ph, type, options);
  if (pret != OB_OK)
    {
      if (ph->pool_directory_version != POOL_DIRECTORY_VERSION_SINGLE_FILE)
        {
          pool_destroy_multi_await (ph);
          pool_destroy_semaphores (ph, false);
        }
      return pool_create_cleanup (ph, pret, false);
    }

  pool_free_pool_hose (ph);
  return OB_OK;
}

/// Allocate a pool_hose for a pool (existing or not) and fill it out
/// just enough that we know how to run pool configuration operations
/// on it (create, dispose, etc.).

// XXX Use in pool_create() too?

static ob_retort pool_hose_with_methods (const char *pool_uri,
                                         pool_hose *ret_ph, pool_context ctx)
{
  assert (pool_uri);
  assert (ret_ph);
  pool_validate_context (ctx, "pool_hose_with_methods");
  pool_hose ph = pool_new_pool_hose (pool_uri);
  if (!ph)
    return OB_NO_MEM;
  // Must be a remote pool or below will fail
  ob_retort pret = pool_characterize (ph, NULL);
  if (pret != OB_OK)
    {
      pool_free_pool_hose (ph);
      return pret;
    }
  pret = pool_load_methods (ph);
  if (pret != OB_OK)
    {
      pool_free_pool_hose (ph);
      return pret;
    }
  ph->ctx = ctx;
  *ret_ph = ph;
  return OB_OK;
}

// XXX Can probably refactor so that lock is taken only for local pools,
// then __pool_create() called, saving 6-7 lines of code and eliminating
// an extra code path.  Same for dispose, etc.

ob_retort pool_create (const char *pool_uri, const char *type,
                       bslaw options_map_or_prot)
{
  return pool_create_ctx (pool_uri, type, options_map_or_prot, NULL);
}

ob_retort pool_create_ctx (const char *pool_uri, const char *type,
                           bslaw options_map_or_prot, pool_context ctx)
{
  if (!pool_uri)
    return POOL_POOLNAME_BADTH;
  if (!type)
    return POOL_TYPE_BADTH;

  slaw freeme = NULL;
  bslaw options;
  if (slaw_is_map (options_map_or_prot))
    {
      freeme = protein_from (NULL, options_map_or_prot);
      if (!freeme)
        return OB_NO_MEM;
      options = freeme;
    }
  else
    {
      options = options_map_or_prot;
      if (options != NULL && !slaw_is_protein (options))
        {
          OB_LOG_ERROR_CODE (0x20101021,
                             "For pool '%s', options is not a map "
                             "or a protein\n",
                             pool_uri);
          return POOL_NOT_A_PROTEIN_OR_MAP;
        }
    }

  // Skip all local cruft for remote pools
  if (pool_name_is_remote (pool_uri))
    {
      pool_hose ph;
      ob_retort pret = pool_hose_with_methods (pool_uri, &ph, ctx);
      if (pret != OB_OK)
        return slaw_free (freeme), pret;
      pret = ph->create (ph, type, options);
      pool_free_pool_hose (ph);
      return slaw_free (freeme), pret;
    }
  // Local pools
  pclock_t lock = pool_config_lock (false, options);
  if (lock.status < OB_OK)
    return slaw_free (freeme), lock.status;
  ob_retort pret = __pool_create (pool_uri, type, options);
  ob_err_accum (&pret, pool_config_unlock (lock));
  return slaw_free (freeme), pret;
}

/// Unwind any state associated with a connection with a pool, leaving
/// the pool itself intact.
///
/// See pool_create_cleanup() for comments on return code style.

static ob_retort pool_participate_cleanup (pool_hose ph, ob_retort pret)
{
  assert (ph);
  pool_free_pool_hose (ph);
  return pret;
}

static ob_retort __pool_participate (const char *pool_uri, pool_hose *ret_ph)
{
  assert (pool_uri);
  assert (ret_ph);

  ob_retort pret;
  pool_hose ph;

  ph = pool_new_pool_hose (pool_uri);
  if (!ph)
    return OB_NO_MEM;

  pret = pool_characterize (ph, NULL);
  if (pret != OB_OK)
    return pool_participate_cleanup (ph, pret);

  pret = pool_load_methods (ph);
  if (pret != OB_OK)
    return pool_participate_cleanup (ph, pret);

  // Keep this call after all opportunities to fail or you'll have to
  // do type-specific cleanup.
  if ((pret = ph->participate (ph)) != OB_OK)
    return pool_participate_cleanup (ph, pret);

  *ret_ph = ph;
  return OB_OK;
}

// XXX REALLY need to refactor this so the remote pool stuff is merged
// with the local.

// YYY Also would be nice to share the "freshest protein" stuff
// between pool_participate and pool_participate_creatingly.

ob_retort pool_participate (const char *pool_uri, pool_hose *ret_ph,
                            bslaw options_map_or_prot)
{
  return pool_participate_ctx (pool_uri, ret_ph, NULL);
}

ob_retort pool_participate_ctx (const char *pool_uri, pool_hose *ret_ph,
                                pool_context ctx)
{
  if (!pool_uri)
    return POOL_POOLNAME_BADTH;
  if (!ret_ph)
    return OB_ARGUMENT_WAS_NULL;

  pool_hose ph = 0;
  ob_retort pret;
  // Skip all local cruft for remote pools
  if (pool_name_is_remote (pool_uri))
    {
      pret = pool_hose_with_methods (pool_uri, &ph, ctx);
      if (pret != OB_OK)
        return pret;
      pret = ph->participate (ph);
      if (pret != OB_OK)
        pool_free_pool_hose (ph);
    }
  else
    {
      // Local pools
      pclock_t lock = pool_config_lock (true, NULL);
      if (lock.status < OB_OK)
        return lock.status;
      if (lock.status == POOL_LOCK_NO_DIR)
        // no pool directory means no pools can exist
        return POOL_NO_SUCH_POOL;
      pret = __pool_participate (pool_uri, &ph);
      ob_err_accum (&pret, pool_config_unlock (lock));
    }

  if (pret == OB_OK)
    {
      // Move the index to the end of the pool, so callers get the
      // freshest proteins by default.
      int64 newest_index;
      ob_retort tort;
      if ((tort = ph->newest_index (ph, &newest_index)) >= OB_OK)
        ob_err_accum (&tort, pool_seekto (ph, newest_index + 1));
      else if (tort == POOL_NO_SUCH_PROTEIN)
        tort = OB_OK;
      ob_err_accum (&pret, tort);
      *ret_ph = ph;
    }
  return pret;
}

static ob_retort __pool_participate_creatingly (const char *pool_uri,
                                                const char *type,
                                                pool_hose *ret_ph,
                                                bprotein options)
{
  assert (pool_uri);
  assert (ret_ph);

  ob_retort pret;
  pret = __pool_participate (pool_uri, ret_ph);
  if (pret != POOL_NO_SUCH_POOL)
    return pret;
  // Otherwise, create the pool
  pret = __pool_create (pool_uri, type, options);
  // If someone else created it before we did, no problemo
  if ((pret != OB_OK) && (pret != POOL_EXISTS))
    return pret;
  bool created = (OB_OK == pret);
  // Okay, now it exists for sure
  pret = __pool_participate (pool_uri, ret_ph);
  return (OB_OK == pret && created) ? POOL_CREATED : pret;
}

ob_retort pool_participate_creatingly (const char *pool_uri, const char *type,
                                       pool_hose *ret_ph,
                                       bslaw options_map_or_prot)
{
  return pool_participate_creatingly_ctx (pool_uri, type, ret_ph,
                                          options_map_or_prot, NULL);
}

ob_retort pool_participate_creatingly_ctx (const char *pool_uri,
                                           const char *type, pool_hose *ret_ph,
                                           bslaw options_map_or_prot,
                                           pool_context ctx)
{
  if (!pool_uri)
    return POOL_POOLNAME_BADTH;
  if (!type)
    return POOL_TYPE_BADTH;
  if (!ret_ph)
    return OB_ARGUMENT_WAS_NULL;

  pool_hose ph = NULL;
  ob_retort pret;

  slaw freeme = NULL;
  bslaw options;
  if (slaw_is_map (options_map_or_prot))
    {
      freeme = protein_from (NULL, options_map_or_prot);
      if (!freeme)
        return OB_NO_MEM;
      options = freeme;
    }
  else
    {
      options = options_map_or_prot;
      if (options != NULL && !slaw_is_protein (options))
        {
          OB_LOG_ERROR_CODE (0x20101039,
                             "For pool '%s', options is not a map "
                             "or a protein\n",
                             pool_uri);
          return POOL_NOT_A_PROTEIN_OR_MAP;
        }
    }

  // Skip all local cruft for remote pools
  if (pool_name_is_remote (pool_uri))
    {
      pret = pool_hose_with_methods (pool_uri, &ph, ctx);
      if (pret == OB_OK)
        pret = ph->participate_creatingly (ph, type, options);
      if (pret < 0)
        pool_free_pool_hose (ph);
    }
  else
    {
      // Local pools
      pclock_t lock = pool_config_lock (false, options);
      if (lock.status < OB_OK)
        return slaw_free (freeme), lock.status;
      pret = __pool_participate_creatingly (pool_uri, type, &ph, options);
      ob_err_accum (&pret, pool_config_unlock (lock));
    }

  if (pret >= 0)
    {
      // Move the index to the end of the pool, so callers get the
      // freshest proteins by default.
      int64 newest_index;
      ob_retort tort;
      if ((tort = ph->newest_index (ph, &newest_index)) >= OB_OK)
        ob_err_accum (&tort, pool_seekto (ph, newest_index + 1));
      else if (tort == POOL_NO_SUCH_PROTEIN)
        tort = OB_OK;
      ob_err_accum (&pret, tort);
      *ret_ph = ph;
    }

  return slaw_free (freeme), pret;
}

// A hack for bug 2566.  If there was no config file, see if there's
// a "mmap-pool" file, and if so, just delete it, since that's what
// was getting left around in bug 2566.  A total hack.
static ob_retort maybe_dipose_of_remnants (const char *poo)
{
  ob_retort tort = POOL_NO_SUCH_POOL;
  char *path = (char *) calloc (PATH_MAX, 1);
  if (path)
    {
      tort = pool_build_pool_dir_path (path, poo);
      if (tort >= OB_OK)
        {
          const size_t dir_last = strlen (path);
          tort = pool_add_path_element (path, "mmap-pool");
          if (tort >= OB_OK)
            {
              if (unlink (path) < 0)
                tort = POOL_NO_SUCH_POOL;
              else
                {
                  tort = OB_OK;
                  path[dir_last] = 0;  // back to just directory name
                  pool_rmdir_p (path);
                }
            }
        }
    }
  free (path);
  return tort;
}

// Long ago, in c786c711fcc8aed8650e450962bfb960d44bdbac, Val said:
//
// A common problem with delete/destroy is that it fails when there is
// some error or corruption going on, which is also when you most want
// to destroy something.  Our approach is to remove as much state as
// possible while ignoring failures.
//
// But she is wrong!  Dead wrong!  Deleting random parts of the pool
// but not others leads to bugs like bug 845 and bug 2566.  This actually
// creates corrupt pools which can't be further deleted, the exact
// opposite of what she claims the goal is.  It is much better to leave
// the pool as-is if you're not able to cleanly, fully delete it.
//
// [Added later, taking a less extremist position...]
//
// Well... the counterexample is bug 7309.  Obviously this is a very
// fine line to tread.

static ob_retort __pool_dispose (const char *pool_uri)
{
  assert (pool_uri);

  pool_hose ph;
  ob_retort pret;

  ph = pool_new_pool_hose (pool_uri);
  if (!ph)
    return OB_NO_MEM;

  /* Tricky thing to be aware of:
   * The pool_create_cleanup() at the end of this function calls
   * pool_destroy_semaphores(), which expects ph->sem_key to be set, so
   * it can destroy the semaphore.  How this happens depends on the
   * pool version.  For POOL_DIRECTORY_VERSION_CONFIG_IN_FILE pools,
   * it happens in pool_characterize().  But for
   * POOL_DIRECTORY_VERSION_CONFIG_IN_MMAP pools, it happens in
   * ph->dispose(), since only the mmap-specific code knows how to
   * get the sem key out of the backing file.
   */

  // Learn enough about the pool to destroy it
  pret = pool_characterize (ph, NULL);
  if (pret == OB_OK)
    {
      pret = pool_load_methods (ph);
      if (pret == OB_OK)
        pret = ph->dispose (ph);
    }
  else if (pret == POOL_NO_SUCH_POOL)
    {
      // This just means there was no config file, so it's not a
      // well-formed pool.  But there could be random remnants floating
      // around due to bugs and whatnot.  (e. g. bug 2566)
      // So, see if we can get rid of the remnants.
      pool_free_pool_hose (ph);
      return maybe_dipose_of_remnants (pool_uri);
    }

  // If anything went wrong, DON'T continue deleting random parts
  // of the pool.  (e. g. bug 2566 and bug 845)
  if (pret < OB_OK)
    {
      pool_free_pool_hose (ph);
      return pret;
    }

  // Clean up from a partially completed create has to unwind all the
  // same state, so use it.
  return pool_create_cleanup (ph, pret, true);
}

ob_retort pool_dispose (const char *pool_uri)
{
  return pool_dispose_ctx (pool_uri, NULL);
}

ob_retort pool_dispose_ctx (const char *pool_uri, pool_context ctx)
{
  if (!pool_uri)
    return OB_ARGUMENT_WAS_NULL;

  // Skip all local cruft for remote pools
  if (pool_name_is_remote (pool_uri))
    {
      pool_hose ph;
      ob_retort pret = pool_hose_with_methods (pool_uri, &ph, ctx);
      if (pret != OB_OK)
        return pret;
      pret = ph->dispose (ph);
      pool_free_pool_hose (ph);
      return pret;
    }
  // Local pools
  pclock_t lock = pool_config_lock (true, NULL);
  if (lock.status < OB_OK)
    return lock.status;
  else if (lock.status == POOL_LOCK_NO_DIR)
    return POOL_NO_SUCH_POOL;  // no pool directory means no pools can exist
  ob_retort pret = __pool_dispose (pool_uri);
  ob_err_accum (&pret, pool_config_unlock (lock));
  return pret;
}

static ob_retort __pool_rename1 (const char *old_old_mmap, va_list vargs);

static ob_retort __pool_rename2 (const char *new_new_mmap, va_list vargs);

static ob_retort __pool_rename (const char *old_name, const char *new_name)
{
  // Step 0: error checking and setup
  ob_retort pret = pool_validate_name (old_name);
  ob_err_accum (&pret, pool_validate_name (new_name));
  if (pret < OB_OK)
    return pret;

  pool_hose ph = pool_new_pool_hose (old_name);
  if (!ph)
    return OB_NO_MEM;

  // Perhaps we should really call pool_characterize(), but in
  // our particular case it just boils down to pool_load_default_config()
  pret = pool_load_default_config (ph);
  const unt8 pool_directory_version = ph->pool_directory_version;
  if (pret < OB_OK)
    {
      pool_free_pool_hose (ph);
      return pret;
    }

  pret = pool_mmap_call_with_backing_file (old_name, pool_directory_version,
                                           true, __pool_rename1, new_name, ph);
  pool_free_pool_hose (ph);
  return pret;
}

static ob_retort __pool_rename1 (const char *old_old_mmap, va_list vargs)
{
  const char *new_name = va_arg (vargs, const char *);
  pool_hose ph = va_arg (vargs, pool_hose);
  const unt8 pool_directory_version = ph->pool_directory_version;

  if (pool_directory_version == POOL_DIRECTORY_VERSION_SINGLE_FILE)
    {
      // For single file pools, delete the old notification directory,
      // since it won't move with the pool.  (And since we only allow
      // renames when the pool is not open, there should be nothing
      // in the notification directory we need to keep.)
      ob_retort tort = pool_destroy_multi_await (ph);
      if (tort < OB_OK)
        return tort;
    }

  return pool_mmap_call_with_backing_file (new_name, pool_directory_version,
                                           false, __pool_rename2, old_old_mmap,
                                           (int) pool_directory_version);
}

static ob_retort __pool_rename2 (const char *new_new_mmap, va_list vargs)
{
  const char *old_old_mmap = va_arg (vargs, const char *);
  unt8 pool_directory_version = (unt8) va_arg (vargs, int);

  const bool snglfil =
    (pool_directory_version == POOL_DIRECTORY_VERSION_SINGLE_FILE);

  const char *old_path;
  const char *new_path;
  const char *old_parent;
  const char *new_parent;

  {
    const char *srcs[4];
    const char *dsts[4];

    OB_INVALIDATE (srcs);
    OB_INVALIDATE (dsts);

    srcs[0] = old_old_mmap;
    srcs[1] = new_new_mmap;

    const int goal = (snglfil ? 2 : 4);
    int i;
    // for each i, set dsts[i] to the basename of srcs[i]
    for (i = 0; i < goal; i++)
      {
        const size_t baselen = strlen (ob_basename (srcs[i]));
        const size_t len = strlen (srcs[i]);
        const size_t newlen = len - baselen;
        char *str = (char *) alloca (1 + newlen);
        memcpy (str, srcs[i], newlen);
        str[newlen] = 0;
        if (newlen > 0)
          str[newlen - 1] = 0;  // delete trailing slash
        dsts[i] = str;
        srcs[2] = dsts[0];
        srcs[3] = dsts[1];
      }

    if (snglfil)
      {
        old_path = old_old_mmap;
        new_path = new_new_mmap;
        old_parent = dsts[0];
        new_parent = dsts[1];
      }
    else
      {
        old_path = dsts[0];
        new_path = dsts[1];
        old_parent = dsts[2];
        new_parent = dsts[3];
      }
  }

  // Step 1: create destination parent directory, if necessary
  {
    // XXX: TODO - use permissions from pool config file?
    // YYY: Well, given the obsession everyone has with everything being
    // world-writable, let's just make the new directory world-writable,
    // and screw the permissions, because no one uses them anyway.
    ob_retort pret = ob_mkdir_p_with_permissions (new_parent, 0777, -1, -1);
    if (pret < OB_OK)
      return pret;
  }

  int (*my_rename) (const char *, const char *) = rename;
#ifndef _MSC_VER
  /* We want an error if the destination file exists.  For
   * directories, rename() does what we want.  But for single-file
   * pools, step 2 (below) is actually renaming the file, not the
   * directory.  On Windows, rename() returns an error if the
   * destination file exists, which is what we want.  But on
   * UNIX, rename() on a file (rather than a directory) will
   * overwrite the destination if it exists, which is not what we
   * want.  So in that one case, we need an alternate function. */
  if (snglfil)
    my_rename = ob_non_overwriting_rename;
#endif

  // It's important to rename the pool directory (2) before renaming the
  // mmap file (3), because if the directory rename fails (e. g. because
  // destination exists), we don't want the mmap file to have been already
  // renamed.

  // Step 2: rename pool directory
  if (my_rename (old_path, new_path) < 0)
    {
      const int erryes = errno;
#ifdef _MSC_VER
      if (EACCES == erryes)
        return POOL_IN_USE;
      else if (ENOENT == erryes)
        return POOL_NO_SUCH_POOL;
#endif
      if (EEXIST == erryes || ENOTEMPTY == erryes)
        return POOL_EXISTS;
      else
        return ob_errno_to_retort (erryes);
    }

  // Step 3: rename mmap file
  if (!snglfil)
    {
      slaw new_old_mmap =
        slaw_string_format ("%s/%s", new_path, ob_basename (old_old_mmap));
      const int rnr = rename (slaw_string_emit (new_old_mmap), new_new_mmap);
      const int erryes = errno;
      Free_Slaw (new_old_mmap);
      if (rnr < 0)
        {
#ifdef _MSC_VER
          if (EACCES == erryes)
            return POOL_IN_USE;
#endif
          return ob_errno_to_retort (erryes);
        }
    }

  // Step 4: clean up old parent directory, if necessary
  return pool_rmdir_p (old_parent);
}

ob_retort pool_rename (const char *old_name, const char *new_name)
{
  return pool_rename_ctx (old_name, new_name, NULL);
}

ob_retort pool_rename_ctx (const char *old_name, const char *new_name,
                           pool_context ctx)
{
  if (!old_name || !new_name)
    return OB_ARGUMENT_WAS_NULL;

  bool remote;
  if ((remote = pool_name_is_remote (old_name))
      != pool_name_is_remote (new_name))
    return POOL_IMPOSSIBLE_RENAME;

  // Skip all local cruft for remote pools
  if (remote)
    {
      pool_hose ph;
      ob_retort pret = pool_hose_with_methods (old_name, &ph, ctx);
      if (pret != OB_OK)
        return pret;
      if (ph->rename)
        pret = ph->rename (ph, old_name, new_name);
      else
        pret = POOL_UNSUPPORTED_OPERATION;
      pool_free_pool_hose (ph);
      return pret;
    }
  // Local pools
  pclock_t lock = pool_config_lock (true, NULL);
  if (lock.status < OB_OK)
    return lock.status;
  else if (lock.status == POOL_LOCK_NO_DIR)
    return POOL_NO_SUCH_POOL;  // no pool directory means no pools can exist
  ob_retort pret = __pool_rename (old_name, new_name);
  ob_err_accum (&pret, pool_config_unlock (lock));
  return pret;
}

static ob_retort __pool_sleep1 (const char *path, va_list vargs);

static ob_retort __pool_sleep (const char *pool_uri)
{
  // error checking and setup
  ob_retort pret = pool_validate_name (pool_uri);
  if (pret < OB_OK)
    return pret;

  pool_hose ph = pool_new_pool_hose (pool_uri);
  if (!ph)
    return OB_NO_MEM;

  // Perhaps we should really call pool_characterize(), but in
  // our particular case it just boils down to pool_load_default_config()
  pret = pool_load_default_config (ph);
  if (pret >= OB_OK)
    {
      const unt8 pool_directory_version = ph->pool_directory_version;
      pret = pool_mmap_call_with_backing_file (pool_uri, pool_directory_version,
                                               true, __pool_sleep1, ph);
    }
  pool_free_pool_hose (ph);
  return pret;
}

static ob_retort __pool_sleep1 (const char *path, va_list vargs)
{
  pool_hose ph = va_arg (vargs, pool_hose);
  ob_retort pret = OB_OK;

#ifndef _MSC_VER
  pret = pool_load_methods (ph);
  if (pret >= OB_OK && ph->get_semkey)
    pret = ph->get_semkey (ph);
  if (pret >= OB_OK)
    pret = pool_destroy_semaphores (ph, true);
  if (pret >= OB_OK)
    {
      /* Create new conf file.*/
      ph->sem_key = -1;
      if (ph->pool_directory_version == POOL_DIRECTORY_VERSION_CONFIG_IN_FILE)
        pret = pool_save_default_config (ph);
    }
#endif
  return pret;
}

ob_retort pool_sleep (const char *pool_uri)
{
  return pool_sleep_ctx (pool_uri, NULL);
}

ob_retort pool_sleep_ctx (const char *pool_uri, pool_context ctx)
{
  if (!pool_uri)
    return OB_ARGUMENT_WAS_NULL;

  bool remote = pool_name_is_remote (pool_uri);

  // Skip all local cruft for remote pools
  if (remote)
    {
      pool_hose ph;
      OB_INVALIDATE (ph);
      ob_retort pret = pool_hose_with_methods (pool_uri, &ph, ctx);
      if (pret != OB_OK)
        return pret;
      if (ph->psleep)
        pret = ph->psleep (ph);
      else
        pret = POOL_UNSUPPORTED_OPERATION;
      pool_free_pool_hose (ph);
      return pret;
    }
  // Local pools
  pclock_t lock = pool_config_lock (true, NULL);
  if (lock.status < OB_OK)
    return lock.status;
  else if (lock.status == POOL_LOCK_NO_DIR)
    return POOL_NO_SUCH_POOL;  // no pool directory means no pools can exist
  ob_retort pret = __pool_sleep (pool_uri);
  ob_err_accum (&pret, pool_config_unlock (lock));
  return pret;
}

ob_retort pool_check_in_use (const char *pool_uri)
{
  return pool_check_in_use_ctx (pool_uri, NULL);
}

ob_retort pool_check_in_use_ctx (const char *pool_uri, pool_context ctx)
{
  // For now, this can be implemented just with a self-rename, since
  // this does the necessary check without having any side-effect.
  // We could implement it differently in the future if desired,
  // but currently I see no reason not to do it this easy way.
  return pool_rename_ctx (pool_uri, pool_uri, ctx);
}

ob_retort pool_exists (const char *pool_uri)
{
  return pool_exists_ctx (pool_uri, NULL);
}

ob_retort pool_exists_ctx (const char *pool_uri, pool_context ctx)
{
  pool_hose ph = NULL;
  ob_retort tort = pool_participate_ctx (pool_uri, &ph, ctx);
  if (tort >= OB_OK)
    {
      tort = OB_YES;
      ob_err_accum (&tort, pool_withdraw (ph));
      return tort;
    }
  else if (tort == POOL_NO_SUCH_POOL)
    return OB_NO;
  return tort;
}

ob_retort pool_hose_clone (pool_hose orig_ph, pool_hose *new_ph)
{
  CHECK_HOSE_VALIDITY (orig_ph);

  if (!new_ph)
    return OB_ARGUMENT_WAS_NULL;

  pool_validate_context (orig_ph->ctx, "pool_hose_clone");

  pool_hose ph = NULL;
  ob_retort pret = pool_participate_ctx (orig_ph->name, &ph, orig_ph->ctx);
  if (pret != OB_OK)
    return pret;

  if (orig_ph->hose_name)
    pret = pool_set_hose_name (ph, orig_ph->hose_name);

  if (OB_OK == pret)
    {
      int64 idx;
      // pool_index can't fail right now, but pretend it does
      pret = pool_index (orig_ph, &idx);
      if (pret == OB_OK)
        ob_err_accum (&pret, pool_seekto (ph, idx));
    }

  if (OB_OK == pret)
    *new_ph = ph;
  else
    pool_withdraw (ph);

  return pret;
}

ob_retort pool_withdraw (pool_hose ph)
{
  if (!ph)
    return POOL_NULL_HOSE;

  int32 status = ob_atomic_int32_ref (&(ph->mgmt.status));
  switch (status)
    {
      case Hose_Unused:
        OB_FATAL_BUG_CODE (0x20101040, "Use of freed pool hose\n");
      case Hose_Used:
        break;
      case Hose_Needs_Reconnection:
        pool_free_pool_hose (ph);
        return OB_OK;
      default:
        OB_FATAL_BUG_CODE (0x20101041,
                           "Apparent memory corruption: hose has status %d\n",
                           status);
    }

  OB_LOG_DEBUG_CODE (0x20101034, "withdrawing from '%s'\n", ph->name);

  pool_validate_context (ph->ctx, "pool_withdraw");

  ob_retort pret;

  pret = ph->withdraw (ph);
  const size_t sz = 1 + strlen (ph->name);
  char *name_copy = (char *) alloca (sz);
  memcpy (name_copy, ph->name, sz);

  pool_free_pool_hose (ph);
  if (pret == POOL_DELETE_ME)
    {
      // intentionally ignore return value of pool_dispose()
      // (as they say on Mythbusters, "failure is always an option")
      pool_dispose (name_copy);
      pret = OB_OK;
    }
  return pret;
}

ob_retort pool_list_ex (const char *server, slaw *ret_slaw)
{
  return pool_list_ctx (server, ret_slaw, NULL);
}

ob_retort pool_list_ctx (const char *server, slaw *ret_slaw, pool_context ctx)
{
  if (ret_slaw == NULL)
    return OB_ARGUMENT_WAS_NULL;

  if (server == NULL || !pool_name_is_remote (server))
    return pool_list_local (server, ret_slaw);

  pool_hose ph;
  ob_retort pret = pool_hose_with_methods (server, &ph, ctx);
  if (pret != OB_OK)
    return pret;
  if (ph->list_pools)
    pret = ph->list_pools (ph, ret_slaw);
  else
    pret = POOL_UNSUPPORTED_OPERATION;
  pool_free_pool_hose (ph);
  return pret;
}

ob_retort pool_parse_permissions (bslaw options, int *mode, int *uid, int *gid)
{
  if (slaw_is_protein (options))
    options = protein_ingests (options);

  bslaw s_mode = slaw_map_find_c (options, "mode");
  bslaw s_owner = slaw_map_find_c (options, "owner");
  bslaw s_group = slaw_map_find_c (options, "group");

  if (slaw_is_string (s_mode))
    *mode = strtol (slaw_string_emit (s_mode), NULL, 8);
  else if (s_mode)
    {
      int64 mo;
      ob_retort tort = slaw_to_int64 (s_owner, &mo);
      if (tort < OB_OK)
        return tort;
      *mode = mo;
    }
  else
    /* bug 2964 - default to world readable/writable,
     * regardless of the user's umask. */
    *mode = 0777;

  if (slaw_is_string (s_owner))
    {
      ob_retort tort = ob_uid_from_name (slaw_string_emit (s_owner), uid);
      if (tort < OB_OK)
        return tort;
    }
  else if (s_owner)
    {
      int64 id;
      ob_retort tort = slaw_to_int64 (s_owner, &id);
      if (tort < OB_OK)
        return tort;
      *uid = id;
    }
  else
    *uid = -1;

  if (slaw_is_string (s_group))
    {
      ob_retort tort = ob_gid_from_name (slaw_string_emit (s_group), gid);
      if (tort < OB_OK)
        return tort;
    }
  else if (s_group)
    {
      int64 id;
      ob_retort tort = slaw_to_int64 (s_group, &id);
      if (tort < OB_OK)
        return tort;
      *gid = id;
    }
  else
    *gid = -1;

  return OB_OK;
}

#define YIELD_FD(f)                                                            \
  if (count < nfd)                                                             \
    {                                                                          \
      fd[count].fd = (f);                                                      \
      fd[count++].description = (#f);                                          \
    }

// For testing purposes only, fill fd[] with various file descriptors
// used by the open hose.  Extra ones up to nfd are filled with -1.
void private_get_file_descriptors (pool_hose ph, fd_and_description *fd,
                                   int nfd)
{
  int count = 0;
#ifndef _MSC_VER
  YIELD_FD (ph->w.wakeup_write_fd);
  YIELD_FD (ph->w.wakeup_read_fd);
  YIELD_FD (ph->notify_handle);
  if (ph->net)
    {
      YIELD_FD (ph->net->connfd);
    }
  else
    {
      pool_mmap_data *d = (pool_mmap_data *) ph->ext;
      FILE *f = d->file;
      int mmap_fd = fileno (f);
      YIELD_FD (mmap_fd);
    }
#endif
  while (count < nfd)
    YIELD_FD (-1);
}
