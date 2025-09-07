
/* (c)  oblong industries */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "libLoam/c/ob-dirs.h"
#include "libLoam/c/ob-string.h"
#include "libLoam/c/ob-vers.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Necessary to get strtok_r emulation on Windows
#include "libLoam/c/ob-sys.h"

typedef void (*print_string_func) (const char *key, const char *value);
typedef void (*print_integer_func) (const char *key, int64 value);
typedef void (*print_list_func) (const char *key, const char *value,
                                 const char *delim);

static void print_human_string (const char *key, const char *value)
{
  printf ("%16s : %s\n", key, value);
}

static void print_human_integer (const char *key, int64 value)
{
  printf ("%16s : %" OB_FMT_64 "d\n", key, value);
}

static void print_human_list (const char *key, const char *value,
                              const char *delim)
{
  const char *name = key;
  char *s = strdup (value);
  char *first = s;
  char *lasts = NULL;
  char *tokn;
  while ((tokn = strtok_r (first, delim, &lasts)))
    {
      printf ("%16s : %s\n", name, tokn);
      first = NULL;
      name = "";
    }
  free (s);
}

static void commacide (void)
{
  static const char *comma = "{";
  fputs (comma, stdout);
  comma = ", ";
}

static void quote (const char *s)
{
  const char *p = s;
  char c;
  putchar ('\"');

  while ((c = *(p++)))
    {
      if (c == '\"' || c == '\\')
        printf ("\\%c", c);
      else if (c < ' ' || c == 0x7f)
        // The four-digit little-u escape is the one numerical escape
        // that's *supposed* to be valid in both JSON and YAML.
        // Sadly, Ruby's YAML parser (Syck) is not compliant.
        // But there shouldn't really be any of these control characters
        // in ob-version's output anyway.
        printf ("\\u%04X", c);
      else
        // Let through ASCII characters that don't need quoting, as well
        // as all non-ASCII UTF-8 code points, as themselves.
        putchar (c);
    }

  putchar ('\"');
}

static void print_key (const char *s)
{
  // Convert key into all lowercase and dashes, as suggested in
  // "slaw naming" section of CODING-STYLE document

  char c, *k, *p;
  k = p = strdup (s);

  while ((c = *p))
    {
      if (isalpha (c))
        *p = tolower (c);
      else
        *p = '-';
      p++;
    }

  quote (k);
  free (k);
}

static void print_yaml_string (const char *key, const char *value)
{
  commacide ();
  print_key (key);
  fputs (": ", stdout);
  quote (value);
}

static void print_yaml_integer (const char *key, int64 value)
{
  commacide ();
  print_key (key);
  printf (": %" OB_FMT_64 "d", value);
}

static void print_yaml_list (const char *key, const char *value,
                             const char *delim)
{
  char *s = strdup (value);
  char *first = s;
  char *lasts = NULL;
  char *tokn;
  commacide ();
  print_key (key);
  fputs (": [", stdout);

  while ((tokn = strtok_r (first, delim, &lasts)))
    {
      if (first)
        first = NULL;
      else
        fputs (", ", stdout);
      quote (tokn);
    }

  fputs ("]", stdout);
  free (s);
}

static print_string_func print_string = print_human_string;
static print_integer_func print_integer = print_human_integer;
static print_list_func print_list = print_human_list;

static void print_version (ob_version_of_what what, const char *name)
{
  char buf[128];
  char *s = ob_get_version (what);
  snprintf (buf, sizeof (buf), "%s version", name);
  print_string (buf, s);
  free (s);
}

static const char sep[] = {OB_PATH_CHAR, 0};

static void print_dir (ob_standard_dir what, const char *name)
{
  const char *s = ob_get_standard_path (what);
  print_list (name, s, sep);
}

static void print_info (ob_system_info what, const char *name)
{
  int64 n = ob_get_system_info (what);
  if (n >= 0)
    print_integer (name, n);
}

static void print_env (const char *delim, const char *e)
{
  const char *val = getenv (e);
  if (val)
    print_list (e, val, delim);
}

static OB_NORETURN void usage (void)
{
  ob_banner (stderr);
  fprintf (stderr, "Usage: ob-version [-y]\n"
                   "    -y format output as yaml/json\n");
  exit (EXIT_FAILURE);
}

int main (int argc, char **argv)
{
  int c;
  bool yaml = false;
  char buf[128];
  const char *features;

  while ((c = getopt (argc, argv, "y")) != -1)
    {
      switch (c)
        {
          case 'y':
            yaml = true;
            break;
          default:
            usage ();
        }
    }

  if (yaml)
    {
      print_string = print_yaml_string;
      print_integer = print_yaml_integer;
      print_list = print_yaml_list;
    }
  else
    {
      ob_banner (stdout);
      OB_CHECK_ABI ();
      putchar ('\n');
      printf ("Compile-time things\n");
      printf ("===================\n");
    }

  print_version (OB_VERSION_OF_GSPEAK, "plasma");
  print_version (OB_VERSION_OF_ABI, "abi");
  print_version (OB_VERSION_OF_COMPILER, "compiler");
  print_integer ("bit width", 8 * sizeof (void *));
  print_version (OB_BUILD_CONFIGURATION, "build");
  print_dir (ob_prefix_dir, "ob_prefix_dir");

  if (!yaml)
    {
      putchar ('\n');
      printf ("Run-time things\n");
      printf ("===============\n");
    }

  print_version (OB_VERSION_OF_OS, "os");
  print_version (OB_VERSION_OF_KERNEL, "kernel");
  // XXX: On Windows, it's the C++ library version, and is determined at
  // compile-time.
  print_version (OB_VERSION_OF_LIBC, "libc");
  print_version (OB_VERSION_OF_YOBUILD, "yobuild");
  print_version (OB_VERSION_OF_MACHINE, "machine");
  print_version (OB_VERSION_OF_CPU, "cpu");

  buf[0] = 0;
  if (ob_x86_features () & OB_X86_FEATURE_SSE)
    ob_safe_append_string (buf, sizeof (buf), " SSE");
  if (ob_x86_features () & OB_X86_FEATURE_SSE2)
    ob_safe_append_string (buf, sizeof (buf), " SSE2");
  if (ob_x86_features () & OB_X86_FEATURE_SSE3)
    ob_safe_append_string (buf, sizeof (buf), " SSE3");
  if (ob_x86_features () & OB_X86_FEATURE_PCLMULQDQ)
    ob_safe_append_string (buf, sizeof (buf), " PCLMULQDQ");
  if (ob_x86_features () & OB_X86_FEATURE_SSSE3)
    ob_safe_append_string (buf, sizeof (buf), " SSSE3");
  if (ob_x86_features () & OB_X86_FEATURE_FMA3)
    ob_safe_append_string (buf, sizeof (buf), " FMA3");
  if (ob_x86_features () & OB_X86_FEATURE_CX16)
    ob_safe_append_string (buf, sizeof (buf), " CX16");
  if (ob_x86_features () & OB_X86_FEATURE_SSE41)
    ob_safe_append_string (buf, sizeof (buf), " SSE4.1");
  if (ob_x86_features () & OB_X86_FEATURE_SSE42)
    ob_safe_append_string (buf, sizeof (buf), " SSE4.2");
  if (ob_x86_features () & OB_X86_FEATURE_MOVBE)
    ob_safe_append_string (buf, sizeof (buf), " MOVBE");
  if (ob_x86_features () & OB_X86_FEATURE_POPCNT)
    ob_safe_append_string (buf, sizeof (buf), " POPCNT");
  if (ob_x86_features () & OB_X86_FEATURE_AES)
    ob_safe_append_string (buf, sizeof (buf), " AES");
  if (ob_x86_features () & OB_X86_FEATURE_AVX)
    ob_safe_append_string (buf, sizeof (buf), " AVX");
  if (ob_x86_features () & OB_X86_FEATURE_CVT16)
    ob_safe_append_string (buf, sizeof (buf), " CVT16");
  if (ob_x86_features () & OB_X86_FEATURE_RDRND)
    ob_safe_append_string (buf, sizeof (buf), " RDRND");

  features = (buf[0] ? buf + 1 : buf);
  print_list ("cpu features", features, " ");

  print_info (OB_SYSINFO_CPU_MHZ, "CPU MHz");
  print_info (OB_SYSINFO_NUM_CORES, "number of cores");
  print_info (OB_SYSINFO_PHYSICAL_MEGABYTES, "RAM megabytes");
  print_info (OB_SYSINFO_VIRTUAL_MEGABYTES, "swap megabytes");

  print_dir (ob_share_path, "ob_share_path");
  print_dir (ob_etc_path, "ob_etc_path");
  print_dir (ob_var_path, "ob_var_path");
  print_dir (ob_tmp_dir, "ob_tmp_dir");
  print_dir (ob_pools_dir, "ob_pools_dir");
  print_dir (ob_yobuild_dir, "ob_yobuild_dir");

  if (yaml)
    printf ("}\n");
  else
    {
      putchar ('\n');
      printf ("Environment variables\n");
      printf ("=====================\n");
      print_env (sep, "LD_LIBRARY_PATH");
      print_env (sep, "LD_PRELOAD");
      print_env (sep, "LD_TRACE_LOADED_OBJECTS");
      print_env (sep, "LD_BIND_NOW");
      print_env (sep, "LD_BIND_NOT");
      print_env (sep, "LD_AOUT_LIBRARY_PATH");
      print_env (sep, "LD_AOUT_PRELOAD");
      print_env (sep, "LD_NOWARN");
      print_env (sep, "LD_WARN");
      print_env (sep, "LD_KEEPDIR");
      print_env (sep, "LD_DEBUG");
      print_env (sep, "LD_DEBUG_OUTPUT");
      print_env (sep, "LD_VERBOSE");
      print_env (sep, "LD_PROFILE");
      print_env (sep, "LD_PROFILE_OUTPUT");
      print_env (sep, "LD_ASSUME_KERNEL");
      print_env (sep, "DYLD_FRAMEWORK_PATH");
      print_env (sep, "DYLD_FALLBACK_FRAMEWORK_PATH");
      print_env (sep, "DYLD_LIBRARY_PATH");
      print_env (sep, "DYLD_FALLBACK_LIBRARY_PATH");
      print_env (sep, "DYLD_ROOT_PATH");
      print_env (sep, "DYLD_SHARED_REGION");
      print_env (sep, "DYLD_INSERT_LIBRARIES");
      print_env (sep, "DYLD_FORCE_FLAT_NAMESPACE");
      print_env (sep, "DYLD_IMAGE_SUFFIX");
      print_env (sep, "DYLD_PRINT_OPTS");
      print_env (sep, "DYLD_PRINT_ENV");
      print_env (sep, "DYLD_PRINT_LIBRARIES");
      print_env (sep, "DYLD_PRINT_LIBRARIES_POST_LAUNCH");
      print_env (sep, "DYLD_BIND_AT_LAUNCH");
      print_env (sep, "DYLD_NO_FIX_PREBINDING");
      print_env (sep, "DYLD_DISABLE_DOFS");
      print_env (sep, "DYLD_PRINT_APIS");
      print_env (sep, "DYLD_PRINT_BINDINGS");
      print_env (sep, "DYLD_PRINT_INITIALIZERS");
      print_env (sep, "DYLD_PRINT_REBASINGS");
      print_env (sep, "DYLD_PRINT_SEGMENTS");
      print_env (sep, "DYLD_PRINT_STATISTICS");
      print_env (sep, "DYLD_PRINT_DOFS");
      print_env (sep, "RUBYLIB");
      print_env (sep, "RUBYOPT");
      print_env (sep, "RUBYPATH");
      print_env (sep, "RUBYSHELL");
      print_env (sep, "PATH");
      print_env (sep, "RUBYLIB_PREFIX");
      print_env (sep, "MallocLogFile");
      print_env (sep, "MallocGuardEdges");
      print_env (sep, "MallocDoNotProtectPrelude");
      print_env (sep, "MallocDoNotProtectPostlude");
      print_env (sep, "MallocStackLogging");
      print_env (sep, "MallocStackLoggingNoCompact");
      print_env (sep, "MallocScribble");
      print_env (sep, "MallocCheckHeapStart");
      print_env (sep, "MallocCheckHeapEach");
      print_env (sep, "MallocCheckHeapSleep");
      print_env (sep, "MallocCheckHeapAbort");
      print_env (sep, "MallocErrorAbort");
      print_env (sep, "MallocHelp");
      print_env (sep, "OBLONG_MACHINE_SETUP_UNRECOGNIZED_PLATFORM");
      print_env (sep, "OBLONG_MACHINE_SETUP_UNRECOGNIZED_PLATFORM_COVERITY");
      print_env (sep, "OBLONG_MACHINE_SETUP_COVERITY_NOT_SUPPORTED_ON_MAC_106");
      print_env (sep, "OBLONG_MACHINE_SETUP_MAC_COVERITY_SUPPORT_UNKNOWN");
      print_env (sep, "OBLONG_MACHINE_SETUP_COULD_NOT_DETERMINE_NUM_SEMS");
      print_env (sep, "OBLONG_MACHINE_SETUP_TOO_FEW_SEMS");
      print_env (sep, "OBLONG_MACHINE_SETUP_MISSING_PACKAGES");
      print_env (sep, "OBLONG_MACHINE_SETUP_UNEXPECTED_KERNEL");
      print_env (sep, "OBLONG_MACHINE_SETUP_BAD_YOBUILD");
      print_env (sep, "LANG");
      print_env (sep, "LC_ALL");
      print_env (sep, "LC_COLLATE");
      print_env (sep, "LC_CTYPE");
      print_env (sep, "LC_MESSAGES");
      print_env (sep, "LC_MONETARY");
      print_env (sep, "LC_NUMERIC");
      print_env (sep, "LC_TIME");
      print_env ("", "DISPLAY");
      print_env (sep, "GST_PLUGIN_PATH");
      print_env (sep, "ISE_COORD_POOL");
      print_env (sep, "KIPPLE");
      print_env (sep, "LIBVID_NO_GLSL_COLORSPACE");
      print_env (sep, "NET_FETCH_POOL");
      print_env (sep, "OB_FOR_TESTING_PURPOSES_ASSUME_POOLS_ARENT_SLEEPING");
      print_env (" ", "OB_LOG");
      print_env (sep, "OB_POOLS_DIR");
      print_env (sep, "OB_TMP_DIR");
      print_env (sep, "POGO_COMMAND");
      print_env (sep, "TMPDIR");
      print_env (sep, "YOBUILD");
      print_env (sep, "YOVERSION");
      print_env (sep, "YOWORDSZ");
      print_env (sep, "G_SPEAK_HOME");
      print_env (sep, "CC");
      print_env (sep, "CPP");
      print_env (" ", "CPPFLAGS");
      print_env (" ", "CFLAGS");
      print_env (sep, "CXX");
      print_env (sep, "CXXCPP");
      print_env (" ", "CXXFLAGS");
      print_env (sep, "OBJC");
      print_env (sep, "OBJCPP");
      print_env (" ", "OBJCFLAGS");
      print_env (sep, "OBJCXX");
      print_env (sep, "OBJCXXCPP");
      print_env (" ", "OBJCXXFLAGS");
      print_env (sep, "CCAS");
      print_env (" ", "CCASFLAGS");
      print_env (sep, "__GL_FSAA_MODE");
      print_env (sep, "__GL_LOG_MAX_ANISO");
      print_env (sep, "__GL_SYNC_TO_VBLANK");
      print_env (sep, "__GL_SYNC_DISPLAY_DEVICE");
      print_env (sep, "__GL_SORT_FBCONFIGS");
      print_env (sep, "__GL_YIELD");
      print_env (sep, "XLIB_SKIP_ARGB_VISUALS");
      print_env (sep, "LIBGL_ALWAYS_INDIRECT");
      print_env (sep, "LIBGL_ALWAYS_SOFTWARE");
      print_env ("", "POSIXLY_CORRECT");
      print_env (":", "OB_CIPHER_SUITES");
      print_env (":", "OB_CIPHER_SUITES_ANON");
    }

  return EXIT_SUCCESS;
}
