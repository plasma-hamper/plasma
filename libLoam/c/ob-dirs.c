
/* (c)  oblong industries */

#include "libLoam/c/ob-dirs.h"
#include "libLoam/c/ob-util.h"
#include "libLoam/c/ob-sys.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-atomic.h"
#include "libLoam/c/ob-string.h"
#include "libLoam/c/ob-file.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdio.h>

#ifdef _MSC_VER
#include <shlobj.h>
#endif

// http://www.gnu.org/software/hello/manual/autoconf/Defining-Directories.html
#if 1 /* we always have datadir and prefix */
#include "private/datadir.h"
#include "private/prefix.h"
#endif

#include "ob-vers.h" /* for YOBUILD_PREFIX */

static void *cache_tmp;
static OB_UNUSED void *cache_windows_prefix;
static OB_UNUSED void *cache_yobuild_dir;
static void *cache_user_gspeak_dir;
static void *cache_share_path;
static void *cache_etc_path;
static void *cache_var_path;
static void *cache_sys_var_path;
static void *cache_pools_dir;

typedef char *(*generator_func) (va_list ap);

typedef const char *(*const_str_func) (void);

/// This provides a thread-safe way to generate a constant string on first use.
/// We take a bit of inspiration from STM, in that it is lock-free, and
/// each thread makes progress assuming it will win, but in the end, only
/// one thread wins and the others undo what they did.  In our case, all the
/// intermediate work is done in an allocated chunk of memory, so "undo" is
/// as simple as freeing that memory.
/// Takes a pointer to a void* variable that holds the string, and takes
/// a generator function that can generate the string (optionally using
/// additional parameters in a variable argument list).  Will return the
/// cached string if it is already cached; otherwise it will generate the
/// string to be cached, with potentially multiple threads competing as
/// described above.
static const char *cache_path (void **where, generator_func generate, ...)
{
  void *path = ob_atomic_pointer_ref (where);
  if (!path)
    {
      va_list ap;
      va_start (ap, generate);
      void *newpath = generate (ap);
      va_end (ap);
      if (ob_atomic_pointer_compare_and_swap (where, NULL, newpath))
        path = newpath;
      else
        {
          free (newpath);
          path = ob_atomic_pointer_ref (where);
        }
    }
  return (const char *) path;
}

#ifdef _MSC_VER
static char *generate_windows_tmp (void)
{
  char tmpdir[MAX_PATH];
  if (GetTempPath (sizeof (tmpdir), tmpdir))
    {
      char *result = (char *) malloc (MAX_PATH);
      if (result && 0 != GetLongPathName (tmpdir, result, MAX_PATH))
        {
          char *rmbs = strrchr (result, '\\');
          if (rmbs && rmbs - result == strlen (result) - 1)
            *rmbs = 0;  // remove trailing backslash, to match other paths
          return result;
        }
      free (result);
    }
  return ob_resolve_standard_path (ob_var_path, "tmp", "DW,dw,c");
}
#endif

static char *generate_tmp_1 (void)
{
  const char *x = getenv ("OB_TMP_DIR");
  if (!x)
    x = getenv ("TMPDIR");
  if (x)
    return strdup (x);
#ifdef _MSC_VER
  return generate_windows_tmp ();
#else
  return strdup ("/tmp");
#endif
}

/* On OS X, TMPDIR can be set to something like
 * "/var/folders/sY/sYCmBevyGwmSIKYFIs1nwk+++TY/-Tmp-/", which ends
 * with a trailing slash.  We would prefer that it didn't, so that
 * we can concatenate the ob_tmp_dir with a filename using a slash,
 * and not get a path with a double slash.  (Although UNIX in general
 * is happy about treating double slashes as a single slash, it bugs
 * me because it is non-canonical, and currently the pool code is finicky
 * and does not appreciate pool names containing double slashes.) */
static char *generate_tmp (OB_UNUSED va_list unused)
{
  char *tmp = generate_tmp_1 ();
  if (!tmp) /* generally shouldn't happen, unless allocation fails */
    return NULL;
  const size_t len = strlen (tmp);
  if (len == 0)  /* this shouldn't happen, either, but in case it does... */
    return NULL; /* ...better to return NULL than empty string */
  const size_t last = len - 1;
  if (tmp[last] == '/'
#ifdef _MSC_VER
      || tmp[last] == '\\'
#endif
      )
    tmp[last] = 0; /* nuke that trailing slash! */
  return tmp;
}

#ifdef _MSC_VER
static char *generate_user_gspeak_dir (OB_UNUSED va_list unused)
{
  char *buf = (char *) malloc (MAX_PATH);
  if (buf
      && S_OK == SHGetFolderPath (NULL, CSIDL_APPDATA | CSIDL_FLAG_CREATE, NULL,
                                  SHGFP_TYPE_CURRENT, buf))
    {
      ob_safe_append_string (buf, MAX_PATH, "\\oblong");
      return (buf);
    }
  free (buf);
  return NULL;
}

static const char *get_program_files_dir (void)
{
  // We used to make the pools directory be c:\Program Files\oblong\var\pools
  // but c:\opt\oblong\var\pools is much kinder to the user.
  return "c:\\opt";
}

#else
#include <pwd.h>
static char *get_user_info (struct passwd *pwd, char **what)
{
  int bufsize = sysconf (_SC_GETPW_R_SIZE_MAX);
  if (bufsize <= 0)
    bufsize = 4096;

  char *buf = (char *) malloc (bufsize);
  if (buf)
    {
      struct passwd *result;
      if (0 == getpwuid_r (getuid (), pwd, buf, bufsize, &result))
        {
          char *ret = strdup (*what);
          free (buf);
          return ret;
        }
      free (buf);
    }
  return NULL;
}

static char *generate_user_gspeak_dir (OB_UNUSED va_list unused)
{
  struct passwd p;
  char *home = get_user_info (&p, &(p.pw_dir));
  if (!home)
    return NULL;
#ifdef __APPLE__
  const char *suffix = "/Library/Application Support/oblong";
#else
  const char *suffix = "/.oblong";
#endif
  int len = strlen (home) + strlen (suffix) + 1;
  char *ret = (char *) malloc (len);
  if (ret)
    {
      ob_safe_copy_string (ret, len, home);
      ob_safe_append_string (ret, len, suffix);
    }
  free (home);
  return ret;
}
#endif

static const char *get_user_gspeak_dir (void)
{
  return cache_path (&cache_user_gspeak_dir, generate_user_gspeak_dir);
}

static inline void consume (const char *src, size_t len, char *dst, size_t *pos)
{
  if (dst)
    memcpy (dst + *pos, src, len);
  *pos += len;
}

static const char path_sep = OB_PATH_CHAR;

static void measure_or_populate_path (va_list ap, char *dst, size_t *pos)
{
  const char *fmt = NULL;
  bool first = true;
  while (NULL != (fmt = va_arg (ap, const char *)))
    {
      size_t oldpos = *pos;
      if (!first)
        consume (&path_sep, 1, dst, pos);
      const char *p = fmt;
      while (*p)
        {
          char c = *p;
          if (c == '%')
            {
              const char *subst = NULL;
              if (p[1] == 'e')
                {
                  const char *var = va_arg (ap, const char *);
                  subst = getenv (var);
                  p += 2;
                }
              else if (p[1] == 'f')
                {
                  const char *(*func) (void) = va_arg (ap, const_str_func);
                  subst = func ();
                  p += 2;
                }
              else if (p[1] == 's')
                {
                  const char *str = va_arg (ap, const char *);
                  subst = str;
                  p += 2;
                }
              if (subst)
                {
                  consume (subst, strlen (subst), dst, pos);
                }
              else
                {
                  // undo this component...
                  *pos = oldpos;
                  // want to "continue" the outer loop, so use "goto"
                  goto tinue;
                  // XXX: need to consume remaining format arguments!
                  // YYY: well, currently not a problem, since I don't have
                  // any strings with more than one argument.
                }
            }
          else
            {
#ifdef _MSC_VER
              if (c == '/')
                c = '\\';
#endif
              consume (&c, 1, dst, pos);
              p++;
            }
        }
      first = false;
    tinue:
      ob_nop ();  // label has to be followed by a statement; so there!
    }
  // NUL-terminate the string
  consume ("", 1, dst, pos);
}

/* Bad Mignon.  You should have documented this function.
 * So Good Mignon (who is merely a later incarnation of Bad Mignon)
 * will provide some minimal documentation now.
 * Takes a variable argument list of the form:
 *  "format string", arg ...,
 *  "format string", arg ...,
 *  ...,
 *  NULL
 * In that there can be any number of format strings, ultimately
 * terminated by NULL.  And after each format string comes zero or
 * more arguments, depending on the number of % specifiers in the string.
 * (Though for strings with more than one % in them, see the XXX/YYY
 * comments in measure_or_populate_path.)
 *
 * Possible format specifiers are:
 *  %e - argument is a string which names an environment variable
 *  %s - argument is a string which is substituted as-is
 *  %f - argument is a function which returns a string (assumed const, not freed)
 *
 * Oh, yeah: if any of the substitutions turn out to be NULL, then
 * that entire format string is omitted.  (This is where the XXX/YYY
 * comments come in.)  But otherwise, the results of evaluation each
 * format string are concatenated using the path separator character.
 * (normally :, but ; on Windows.)  And on Windows, any / in the format
 * string is changed to \.
 */
static char *generate_path (va_list ap)
{
  size_t len = 0;
  va_list ap2;
  va_copy (ap2, ap);
  measure_or_populate_path (ap, NULL, &len);
  char *result = (char *) malloc (len);
  len = 0;
  if (result)
    measure_or_populate_path (ap2, result, &len);
  va_end (ap2);
  return result;
}

/* The first argument should be a string which names an environment
 * variable.  The second argument should be a function which takes
 * a va_list.  If the environment variable is undefined, the function
 * is called.  If the environment variable is defined, the value
 * of the variable is used.  However, if the variable begins or
 * ends with the path separator character (essentially, has an
 * empty component at the beginning or end), then the result of
 * the function is substituted at that place.
 *
 * When the function is called, it is passed any remaining arguments
 * from the va_list.  A couple of possible candidates for the
 * function would be generate_path(), or another instance of
 * choose_path().  (The latter if you want to "chain" multiple
 * environment variables.)
 */
static char *choose_path (va_list ap)
{
  const char *env = va_arg (ap, const char *);
  generator_func f = va_arg (ap, generator_func);
  const char *s = getenv (env);
  if (!s || !*s)
    return f (ap);
  else if (*s == OB_PATH_CHAR)
    {
      char *dflt = f (ap);
      size_t len = 1 + strlen (dflt) + strlen (s);
      char *ret = (char *) malloc (len);
      ob_safe_copy_string (ret, len, dflt);
      ob_safe_append_string (ret, len, s);
      free (dflt);
      return ret;
    }
  else if (s[strlen (s) - 1] == OB_PATH_CHAR)
    {
      char *dflt = f (ap);
      size_t len = 1 + strlen (dflt) + strlen (s);
      char *ret = (char *) malloc (len);
      ob_safe_copy_string (ret, len, s);
      ob_safe_append_string (ret, len, dflt);
      free (dflt);
      return ret;
    }
  else
    return strdup (s);
}

static const char *get_sys_var_path (void)
{
  return cache_path (&cache_sys_var_path, generate_path,
#ifdef _MSC_VER
                     "%f/oblong/var", get_program_files_dir,
#else
                     "/var/ob",
#endif
                     NULL);
}


static const char *get_var_path (void)
{
  return cache_path (&cache_var_path, choose_path, "OB_VAR_PATH", generate_path,
                     "%f/var", get_user_gspeak_dir, "%f", get_sys_var_path,
                     NULL);
}

// The current mechanism (at time of this change) ALWAYS checks the
// environment variable OB_POOLS_DIR, so cache_pools_dir will, after
// this function is run have the path of the pools directory, have a
// path to a system-wide pools directory in it, but the path returned
// by `ob_get_standard_path (ob_pools_dir)` will be different if
// OB_POOLS_DIR is set.
static const char *get_pools_dir (void)
{
  return cache_path (&cache_pools_dir, generate_path, "%f/pools",
                     get_sys_var_path, NULL);
}

const char *ob_get_standard_path (ob_standard_dir dir)
{
  const char *x;

  switch (dir)
    {
      case ob_share_path:
        return cache_path (&cache_share_path, choose_path, "OB_SHARE_PATH",
                           generate_path, "%f/share", get_user_gspeak_dir,
#ifdef _MSC_VER
                           "%f/oblong/share", get_program_files_dir,
#elif 1 /* we always have datadir and prefix */
                           "%s/oblong", datadir,
#else
                           "/well/since/you/dont/have/a/config/h/i/dont/know/"
                           "what/the/prefix/is/so/now/you/are/just/in/deep/doo/"
                           "doo/arent/you",
#endif
                           NULL);
      case ob_etc_path:
        return cache_path (&cache_etc_path, choose_path, "OB_ETC_PATH",
                           generate_path, "%f/etc", get_user_gspeak_dir,
#ifdef _MSC_VER
                           "%f/oblong/etc", get_program_files_dir,
#else
                           "/etc/oblong",
#endif
                           NULL);
      case ob_var_path:
        return get_var_path ();
      case ob_tmp_dir:
        return cache_path (&cache_tmp, generate_tmp);
      case ob_pools_dir:
        x = getenv ("OB_POOLS_DIR");
        if (x)
          return x;
        return get_pools_dir ();
      case ob_yobuild_dir:
        x = getenv ("YOBUILD");
        if (x)
          return x;
        return YOBUILD_PREFIX;
      case ob_prefix_dir:
#if 1 /* we always have datadir and prefix */
        return prefix;
#elif defined(_MSC_VER)
        return cache_path (&cache_windows_prefix, generate_path, "%f/oblong",
                           get_program_files_dir, NULL);
#else
        return "/no/where";
#endif
      default:
        OB_LOG_BUG_CODE (0x10000000,
                         "Invalid argument to ob_get_standard_path\n");
        return ".";
    }
}

static ob_retort do_callback (const char *pathname, ob_path_callback func,
                              va_list vargies)
{
  va_list cpy;
  va_copy (cpy, vargies);
  ob_retort tort = func (pathname, cpy);
  va_end (cpy);
  return tort;
}

static bool is_writable (const char *pn)
{
  return (0 == access (pn, W_OK));
}

static bool is_local (const char *pn)
{
  return !ob_is_network_path (pn);
}

static bool check_ancestor (const char *pathname,
                            bool (*func) (const char *pn));

static ob_retort do_check (char c, const char *pathname, char *changeme,
                           bool *match)
{
  bool result = false;
  *changeme = (islower (c) ? 0 : OB_DIR_CHAR);

  struct stat sb;
  OB_CLEAR (sb);
  int s = stat (pathname, &sb);
  int typ = (sb.st_mode & S_IFMT);

  switch (tolower (c))
    {
      case 'e':  // pathname exists
        result = (s == 0);
        break;
      case 'f':  // pathname is a file
        result = ((s == 0) && (typ == S_IFREG));
        break;
      case 'd':  // pathname is a directory
        result = ((s == 0) && (typ == S_IFDIR));
        break;
      case 'r':  // pathname is readable
        result = ((s == 0) && 0 == access (pathname, R_OK));
        break;
      case 'w':  // pathname is writable
        result = ((s == 0) && 0 == access (pathname, W_OK));
        break;
      case 'c':  // directory could be created
        result = check_ancestor (pathname, is_writable);
        break;
      case 'l':  // directory is local (or would be if created)
        result = check_ancestor (pathname, is_local);
        break;
      // These are undocumented and are for testing
      case '0':  // always false
        result = false;
        break;
      case '1':  // always true
        result = true;
        break;
      case '^':  // pathname starts with caret
        result = (pathname[0] == '^');
        break;
      default:
        return OB_INVALID_ARGUMENT;
    }

  *match = result;
  return OB_OK;
}

static size_t longest_component (const char *path)
{
  const char *p;
  size_t result = 0;
  for (p = path; *p; p += !!*p)
    {
      size_t len;
      for (len = 0; *p != 0 && *p != OB_PATH_CHAR; p++, len++)
        ;
      if (len > result)
        result = len;
    }
  return result;
}

bool ob_is_absolute (const char *name)
{
  if (!name)
    return false;
  if (*name == '/')
    return true;
#ifdef _MSC_VER
  if (*name == '\\')
    return true;
  if (isalpha (*name) && name[1] == ':')
    return true;
#endif
  return false;
}

static bool is_slashy (char c)
{
#ifdef _MSC_VER
  if (c == '\\')
    return true;
#endif
  return (c == '/');
}

// The notion of "explicit" is defined in bug 917
bool ob_is_explicit (const char *name)
{
  if (!name)
    return false;
  if (name[0] == '.' && is_slashy (name[1]))
    return true;
  if (name[0] == '.' && name[1] == '.' && is_slashy (name[2]))
    return true;
  return ob_is_absolute (name);
}

ob_retort ob_search_path (const char *path, const char *filename,
                          const char *searchspec, ob_path_callback func,
                          va_list vargies)
{
  if (ob_is_explicit (filename))
    return do_callback (filename, func, vargies);

  // avoid going into infinite loop on "while (*specp)" below!
  if (path == NULL || path[0] == 0)
    return OB_OK;

  size_t len = 2 + strlen (filename) + longest_component (path);
  char *scratch = (char *) malloc (len);
  if (!scratch)
    return OB_NO_MEM;

  // XXX: long, confusing, maybe break up into functions?
  const char *specp = searchspec;
  ob_retort tort = OB_OK;
  while (*specp)
    {
      const char *component = path;
      const char *specsave = specp;
      while (*component)
        {
          const char *end = strchr (component, OB_PATH_CHAR);
          if (!end)
            end = component + strlen (component);
          memcpy (scratch, component, end - component);
          *(scratch + (end - component)) = 0;
          ob_safe_append_string (scratch, len, "/");
          ob_safe_append_string (scratch, len, filename);
          char c;
          specp = specsave;
          bool match = true;
          while ((c = *specp) && c != ',' && tort == OB_OK)
            {
              if (c == '|')
                {
                  if (match)
                    {
                      specp = strchr (specp, ',');
                      if (!specp)
                        specp = "";
                      break;
                    }
                  match = true;
                }
              else if (match)
                tort =
                  do_check (c, scratch, scratch + (end - component), &match);
              specp++;
            }
          if (match)
            {
              *(scratch + (end - component)) = OB_DIR_CHAR;
              tort = do_callback (scratch, func, vargies);
            }
          // we want to stop on error, or on the OB_STOP success code
          if (tort != OB_OK)
            goto done;
          component = end;
          if (*component)
            component++;  // skip colon or semicolon
          if (*specp)
            specp++;  // skip comma
        }
    }

done:
  free (scratch);
  return tort;
}

ob_retort ob_search_standard_path (ob_standard_dir dir, const char *filename,
                                   const char *searchspec,
                                   ob_path_callback func, ...)
{
  va_list ap;
  va_start (ap, func);
  ob_retort tort =
    ob_search_path (ob_get_standard_path (dir), filename, searchspec, func, ap);
  va_end (ap);
  return tort;
}

static ob_retort one_shot_func (const char *file, va_list vargies)
{
  char **p = va_arg (vargies, char **);
  *p = strdup (file);
  return OB_STOP;
}

char *ob_resolve_standard_path (ob_standard_dir dir, const char *filename,
                                const char *searchspec)
{
  char *result = NULL;
  ob_search_standard_path (dir, filename, searchspec, one_shot_func, &result);
  return result;
}


char *ob_concat_standard_path (ob_standard_dir dir, const char *subdir)
{
  // If it's a "path", search it
  if (dir == ob_share_path || dir == ob_etc_path || dir == ob_var_path)
    return ob_resolve_standard_path (dir, subdir, "W,R,dw,dr,c");

  // If it's a plain old "dir", not a "path", just concatenate blindly
  const char *d = ob_get_standard_path (dir);
  int len = strlen (d) + strlen (subdir) + 2;  // 1 for '/', plus 1 for NUL
  char *ret = (char *) malloc (len);
  if (!ret)
    return NULL;
  ob_safe_copy_string (ret, len, d);
#ifdef _MSC_VER
  ob_safe_append_string (ret, len, "\\");
#else
  ob_safe_append_string (ret, len, "/");
#endif
  ob_safe_append_string (ret, len, subdir);
  return ret;
}

#ifdef _MSC_VER
static char *last_path_sep (const char *path)
{
  const char *frwd = strrchr (path, '/');
  const char *back = strrchr (path, '\\');
  if (!frwd)
    return (char *) back;
  else if (!back || back < frwd)
    return (char *) frwd;
  else
    return (char *) back;
}
#else
#define last_path_sep(x) strrchr ((x), '/')
#endif

/// Guess whether pathname could be created recursively, by finding the
/// first existing ancestor and seeing if it is a writable directory.
static bool check_ancestor (const char *pathname, bool (*func) (const char *pn))
{
  char *pn = strdup (pathname);

  for (;;)
    {
      struct stat sb;
      OB_CLEAR (sb);
      int s = stat (pn, &sb);
      int typ = (sb.st_mode & S_IFMT);
      if (s == 0)
        {
          bool w = (typ == S_IFDIR && func (pn));
          free (pn);
          return w;
        }
      else if (errno != ENOENT)
        {
          free (pn);
          return false;
        }
      char *p = last_path_sep (pn);
      if (!p)
        {
          free (pn);
          return false;
        }
      // need to handle root specially
      if (p == pn ||                      // UNIX root: use "/", not ""
          (p - pn == 2 && pn[1] == ':'))  // Windows root: use "C:/", not "C:"
        p++;
      *p = 0;  // chop off the last path component
    }
}

ob_retort ob_mkdir_p (const char *dir)
{
  return ob_mkdir_p_with_permissions (dir, -1, -1, -1);
}

ob_retort ob_mkdir_p_with_permissions (const char *dir, int mode, int uid,
                                       int gid)
{
  int ret;
  int perm = (mode >= 0 ? mode : 0777);
  char *parent, *p;
  ob_retort err;

  if (!dir || !*dir)
    return OB_ARGUMENT_WAS_NULL;

#ifdef _MSC_VER
  ret = _mkdir (dir);
#else
  ret = mkdir (dir, perm);
#endif

  if (ret == 0)
    goto ok;  // OK because directory was created
  else if (errno == EEXIST)
    return OB_OK;  // OK because directory already existed
  else if (errno != ENOENT)
    return ob_errno_to_retort (errno);

  // At this point, we couldn't create the directory because the parent
  // doesn't exist, so try to create the parent.
  parent = strdup (dir);
  if (!parent)
    return OB_NO_MEM;

  p = last_path_sep (parent);
  if (!p)
    {
      free (parent);
      return OB_UNKNOWN_ERR;
    }

  *p = 0;  // chop off the last path component
  err = ob_mkdir_p_with_permissions (parent, mode, uid, gid);
  free (parent);
  if (err < OB_OK)
    return err;

// Now the parent must exist, so try, try again...
#ifdef _MSC_VER
  ret = mkdir (dir);
#else
  ret = mkdir (dir, perm);
#endif

  if (ret == 0)
    goto ok;  // OK because directory was created
  else if (errno == EEXIST)
    return OB_OK;  // OK because directory already existed
  else
    return ob_errno_to_retort (errno);

ok:
  return ob_permissionize (dir, mode, uid, gid);
}

ob_retort ob_rmdir_p (const char *dir)
{
  return ob_rmdir_p_ex (dir, NULL);
}

#define ALLOCA_STRDUP(x)                                                       \
  (tmp_size = 1 + strlen ((x)),                                                \
   (char *) memcpy (8 + (char *) ob_make_undefined (alloca (tmp_size + 16),    \
                                                    tmp_size + 16),            \
                    (x), tmp_size))

ob_retort ob_rmdir_p_ex (const char *dir, const char *stop)
{
  int ret;
  size_t tmp_size;

  if (!dir || !*dir)
    return OB_ARGUMENT_WAS_NULL;

  if (stop && 0 == strcmp (dir, stop))
    return OB_OK;  // caller doesn't want us to go futher (bug 2881)

  ret = rmdir (dir);

  if (ret != 0)
    {
      if (errno == EEXIST || errno == ENOTEMPTY || errno == EBUSY
          || errno == EACCES || errno == EPERM || errno == EROFS
          || errno == ENOTDIR  // e. g. a symlink: bug 1144
          || errno == EINVAL   // tried to delete current dir: bug 1340
          )
        // Don't complain about "expected" errors; just stop recursing
        return OB_OK;
      else
        return ob_errno_to_retort (errno);
    }

  // We have deleted the directory; now try to delete the parent
  char *parent = ALLOCA_STRDUP (dir);

  char *p = last_path_sep (parent);
  if (!p)
    {
      // This just means we got to the beginning of a relative path.
      // Unlike in the mkdir case, it is not an error.
      return OB_OK;
    }

  *p = 0;  // chop off the last path component
  return ob_rmdir_p_ex (parent, stop);
}

ob_retort ob_permissionize (const char *path, int mode, int uid, int gid)
{
#ifdef _MSC_VER
  //unix style modes/permissions don't really translate, though it may be
  //worth trying to glean some kind of translation in the future
  return OB_OK;
#else

  if (uid != -1 || gid != -1)
    {
      if (chown (path, uid, gid) < 0)
        return ob_errno_to_retort (errno);
    }

  if (mode != -1 && chmod (path, mode) < 0)
    return ob_errno_to_retort (errno);
  else
    return OB_OK;

#endif
}


const char *ob_basename (const char *longname)
{
  const char *slash = last_path_sep (longname);
  return (slash ? slash + 1 : longname);
}

#ifndef _MSC_VER
static int know_the_unknown (const char *dirp, struct dirent *entry)
{
  const size_t dirlen = strlen (dirp);
  const size_t entlen = strlen (entry->d_name);
  const size_t pathlen = dirlen + entlen + 1; /* 1 for slash */
  char *path = (char *) alloca (pathlen + 1); /* 1 for NUL terminator */
  memcpy (path, dirp, dirlen);
  path[dirlen] = '/';
  memcpy (path + dirlen + 1, entry->d_name, entlen);
  path[pathlen] = 0;
  struct stat st;
  if (stat (path, &st) < 0)
    return -1;
  entry->d_type = IFTODT (st.st_mode);
  return 0;
}

typedef int (*q_func) (const void *, const void *);

int ob_scandir (const char *dirp, struct dirent ***namelist,
                ob_sd_filt_func filter, ob_sd_comp_func compar)
{
  /* First, run the real scandir without any filter or comparison function,
   * in order to just get all the directory entries. */
  int nentries = scandir (dirp, namelist, NULL, NULL);
  if (nentries <= 0)
    return nentries;

  /* If any entries have type DT_UNKNOWN (which happens on the pesky
   * ReiserFS), call stat to figure them out. */

  struct dirent **entries = *namelist;
  int i;
  for (i = 0; i < nentries; i++)
    {
      struct dirent *entry = entries[i];
      if (entry->d_type == DT_UNKNOWN)
        {
          int ret = know_the_unknown (dirp, entry);
          if (ret < 0)
            {
              int erryes = errno;
              ob_scandir_free (entries, nentries);
              *namelist = NULL;
              errno = erryes;
              return -1;
            }
        }
    }

  /* Now apply the user's filter, if any */
  if (filter)
    {
      int e = 0;
      for (i = 0; i < nentries; i++)
        {
          struct dirent *entry = entries[i];
          if (filter (entry))
            {
              if (e != i)
                entries[e] = entry;
              e++;
            }
          else
            free (entry);
        }
      if (e != nentries)
        {
          ob_make_undefined (entries + e,
                             sizeof (struct dirent *) * (nentries - e));
          nentries = e;
        }
    }

  /* Finally, sort it */
  if (compar && nentries > 1)
    qsort (entries, nentries, sizeof (struct dirent *), (q_func) compar);

  return nentries;
}
#endif

void ob_scandir_free (struct dirent **namelist, int nentries)
{
  int entno;
  for (entno = 0; entno < nentries; entno++)
    free (namelist[entno]);
  free (namelist);
}
