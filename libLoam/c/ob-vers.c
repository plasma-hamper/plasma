
/* (c)  oblong industries */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "libLoam/c/ob-sys.h"
#include "libLoam/c/ob-util.h"
#include "libLoam/c/ob-vers.h"
#include "libLoam/c/ob-dirs.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-file.h"
#include "libLoam/c/ob-string.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#if defined(_MSC_VER)
#include <psapi.h>
#include <crtversion.h>
#elif defined(__APPLE__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#define PRODNAME "plasma"

static const char *parse_proc_file (const char *s, const char *label);


#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))

#ifdef __x86_64__
// Adapted from gstreamer,
// https://gitlab.freedesktop.org/gstreamer/orc/blob/bef97d21fd5328a30cda3f5270f004a6ff768ebf/orc/orccpu-x86.c#L182
static void
ob_cpuid (unt32 op, unt32 *reg)
{
  __asm__ (
      "  cpuid\n"
      : "=a" (reg[0]), "=b" (reg[1]), "=c" (reg[2]), "=d" (reg[3])
      : "a"(op));
}

#else
// adapted from some public domain code at
// http://wiki.osdev.org/CPUID#Using_CPUID_from_GCC
// with additional information from:
// http://sam.zoy.org/blog/2007-04-13-shlib-with-non-pic-code-have-inline-assembly-and-pic-mix-well
static inline void ob_cpuid (unt32 op, unt32 *reg)
{
  __asm__ ("movl %%ebx, %%esi  \n\t" /* save %ebx */
           "cpuid              \n\t"
           "xchgl %%ebx, %%esi \n\t" /* swap %ebx with saved ebx */
           : "=a"(reg[0]), "=S"(reg[1]), "=c"(reg[2]), "=d"(reg[3])
           : "a"(op));
}
#endif

#define OB_HAVE_CPUID
#elif defined(_MSC_VER)

// http://msdn.microsoft.com/en-us/library/hskdteyh(VS.80).aspx
#include <intrin.h>  // for __cpuid
static inline void ob_cpuid (unt32 op, unt32 *reg)
{
  int ireg[4];
  __cpuid (ireg, (int) op);
  for (int i = 0; i < 4; i++)
    reg[i] = ireg[i];
}

#define OB_HAVE_CPUID
#endif /* _MSC_VER */

#ifdef OB_HAVE_CPUID

enum cpuid_requests
{
  CPUID_GETVENDORSTRING,
  CPUID_GETFEATURES,
  CPUID_GETTLB,
  CPUID_GETSERIAL,

  CPUID_INTELEXTENDED = 0x80000000,
  CPUID_INTELFEATURES,
  CPUID_INTELBRANDSTRING,
  CPUID_INTELBRANDSTRINGMORE,
  CPUID_INTELBRANDSTRINGEND,
};

unt64 ob_x86_features (void)
{
  unt32 reg[4];
  ob_cpuid (CPUID_GETFEATURES, reg);
  return reg[3] | (((unt64) reg[2]) << 32);
}

typedef union
{
  unt32 u[13];
  char c[52];
} brand_string_t;

static char *cpu_version (void)
{
  brand_string_t brand;

  OB_CLEAR (brand);
  ob_cpuid (CPUID_INTELBRANDSTRING, brand.u + 0);
  ob_cpuid (CPUID_INTELBRANDSTRINGMORE, brand.u + 4);
  ob_cpuid (CPUID_INTELBRANDSTRINGEND, brand.u + 8);

  // trim end
  int i;
  for (i = 48; i > 0; i--)
    {
      char c = brand.c[i];
      if (c == 0 || c == ' ')
        brand.c[i] = 0;
      else
        break;
    }

  // trim beginning
  const char *bstr = brand.c;
  while (*bstr == ' ')
    bstr++;

  return strdup (bstr);
}

#else /* OB_HAVE_CPUID */

unt64 ob_x86_features (void)
{
  return 0;
}

static char *cpu_version (void)
{
  char *ret = NULL;
#if defined(__gnu_linux__)
  char *s = ob_read_file ("/proc/cpuinfo");
  const char *label = "model name";
  const char *p = parse_proc_file (s, label);
  if (p)
    {
      char *nl;
      ret = strdup (p);
      if (ret && (nl = strchr (ret, '\n')))
        *nl = 0;
    }
  free (s);
#endif
  if (!ret)
    ret = strdup ("unknown");
  return ret;
}

#endif /* OB_HAVE_CPUID */

static char *os_version (void)
{
  char *s = NULL;
  const char *v = "unknown";

#ifdef __APPLE__
  s = ob_read_file ("/System/Library/CoreServices/SystemVersion.plist");
  if (!s)
    return strdup ("unknown");
  char *p = strstr (s, "<key>ProductVersion</key>");
  if (p)
    {
      const char *tag = "<string>";
      p = strstr (p, tag);
      if (p)
        {
          char *p2 = strstr (p, "</string>");
          *p2 = 0;
          v = p + strlen (tag);
        }
    }
#elif defined(_MSC_VER)
  OSVERSIONINFOEX ovx;
  OB_CLEAR (ovx);
  ovx.dwOSVersionInfoSize = sizeof (ovx);
  if (GetVersionEx ((OSVERSIONINFO *) &ovx))
    {
      const char *name = "Unknown Species of Windows";
      unt32 vn = ovx.dwMinorVersion + (ovx.dwMajorVersion << 4);
      bool ws = (ovx.wProductType == VER_NT_WORKSTATION);
      switch (vn)
        {
          case 0x62:
            name = "Windows 8";
            break;
          case 0x61:
            name = (ws ? "Windows 7" : "Windows Server 2008 R2");
            break;
          case 0x60:
            name = (ws ? "Windows Vista" : "Windows Server 2008");
            break;
          case 0x52:
            if (ws)
              name = "Windows XP Professional x64 Edition";
            else if (GetSystemMetrics (SM_SERVERR2))
              name = "Windows Server 2003 R2";
            else
              name = "Windows Server 2003";
            break;
          case 0x51:
            name = "Windows XP";
            break;
          case 0x50:
            name = "Windows 2000";
            break;
        }
      const int enough = 256;
      s = (char *) malloc (enough);
      if (s)
        {
          snprintf (s, enough, "%s build %u %s", name, ovx.dwBuildNumber,
                    ovx.szCSDVersion);
          v = s;
        }
    }
#else /* assume Linux */
  s = ob_read_file ("/etc/redhat-release");
  if (s)
    {
      char *p = strchr (s, '\n');
      if (p)
        *p = 0;
      return s;
    }
  const char *dd = "DISTRIB_DESCRIPTION=";
  s = ob_read_file ("/etc/lsb-release");
  if (!s)
    {
      s = ob_read_file ("/etc/os-release");
      if (!s)
        return strdup ("unknown");
      dd = "PRETTY_NAME=";
    }
  char *p = strstr (s, dd);
  if (p)
    {
      p += strlen (dd);
      if (*p == '\"')
        p++;
      char *q = p;
      while (*q != 0 && *q != '\"' && *q != '\n')
        q++;
      *q = 0;
      v = p;
    }
#endif

  char *r = strdup (v);
  free (s);
  return r;
}

static char *yobuild_version (void)
{
  const char *v = "unknown";
  char *yo =
    ob_concat_standard_path (ob_yobuild_dir, "yobuild-var-assignments.txt");
  char *s = NULL;
  if (yo && (s = ob_read_file (yo)))
    {
      const char *key = "YOBUILD_VERSION=";
      char *p = strstr (s, key);
      if (p)
        {
          p += strlen (key);
          char *p2 = strchr (p, '\n');
          if (p2)
            *p2 = 0;
          v = p;
        }
    }
  char *r = strdup (v);
  free (s);
  free (yo);
  return r;
}

static OB_UNUSED void trim_end (char *s)
{
  if (s)
    {
      size_t l = strlen (s);
      while (l-- > 0 && isspace (s[l]))
        s[l] = 0;
    }
}

static char *machine_version (void)
{
#if defined(__gnu_linux__)
  /* The outer loop is because usually the system vendor and product name
   * are the most informative:
   *   mignon@mignon64:/sys/class/dmi/id$ cat sys_vendor
   *   Acer
   *   mignon@mignon64:/sys/class/dmi/id$ cat product_name
   *   Aspire X1300
   *   mignon@mignon64:/sys/class/dmi/id$ cat board_vendor
   *   Acer
   *   mignon@mignon64:/sys/class/dmi/id$ cat board_name
   *   WMCP78M
   * But on coconut, they are distinctly uninformative, so we want to use
   * the board vendor and name instead:
   *   mignon@coconut:/sys/class/dmi/id$ cat sys_vendor
   *   System manufacturer
   *   mignon@coconut:/sys/class/dmi/id$ cat product_name
   *   System Product Name
   *   mignon@coconut:/sys/class/dmi/id$ cat board_vendor
   *   ASUSTeK Computer INC.
   *   mignon@coconut:/sys/class/dmi/id$ cat board_name
   *   P5N32-E SLI
   * So we try to make a heuristic decision about which to use.
   */
  const char *const dmifiles[2][2] = {{"/sys/class/dmi/id/sys_vendor",
                                       "/sys/class/dmi/id/product_name"},
                                      {"/sys/class/dmi/id/board_vendor",
                                       "/sys/class/dmi/id/board_name"}};
  int i, j;
  for (i = 0; i < 2; i++)
    {
      char *pair[2];
      for (j = 0; j < 2; j++)
        {
          pair[j] = ob_read_file (dmifiles[i][j]);
          trim_end (pair[j]);
        }
      if (pair[0] && pair[1]
          && ((*(pair[0]) && 0 != strcasecmp (pair[0], "System manufacturer"))
              || (*(pair[1])
                  && 0 != strcasecmp (pair[1], "System Product Name"))))
        {
          size_t capacity = 2 + strlen (pair[0]) + strlen (pair[1]);
          char *machine = (char *) malloc (capacity);
          if (!machine)
            {
              free (pair[1]);
              return pair[0];
            }
          ob_safe_copy_string (machine, capacity, pair[0]);
          ob_safe_append_string (machine, capacity, " ");
          ob_safe_append_string (machine, capacity, pair[1]);
          for (j = 0; j < 2; j++)
            free (pair[j]);
          return machine;
        }
      for (j = 0; j < 2; j++)
        free (pair[j]);
    }
  /* This fallback is for ARM (Raspberry Pi) e. g. "BCM2708" */
  char *s = ob_read_file ("/proc/cpuinfo");
  const char *label = "\nHardware";
  const char *p = parse_proc_file (s, label);
  if (p)
    {
      char *nl;
      char *ret = strdup (p);
      if (ret)
        {
          if ((nl = strchr (ret, '\n')))
            *nl = 0;
          free (s);
          return ret;
        }
    }
  free (s);
#elif defined(__APPLE__)
  char buf[80];
  size_t siz = sizeof (buf);
  if (0 == sysctlbyname ("hw.model", buf, &siz, NULL, 0))
    return strdup (buf);
#endif
  // It's possible to get this information on Windows, but it's a big pain,
  // because it requires using WMI, which in turn requires using COM.  So,
  // let's skip it for now.
  // http://msdn.microsoft.com/en-us/library/aa389762(v=VS.85).aspx
  return strdup ("unknown");
}

char *ob_get_version (ob_version_of_what what)
{
#ifdef _MSC_VER
  OSVERSIONINFOEX ovx;
  char buf[64];
#else
  struct utsname u;
#endif

  const char *c = "unknown";
  switch (what)
    {
      case OB_VERSION_OF_GSPEAK:
#ifdef PACKAGE_VERSION_PATCH
        c = PRODNAME " " PACKAGE_VERSION_PATCH;
#elif defined(G_SPEAK_VERSION)
        c = PRODNAME " " G_SPEAK_VERSION;
#endif
        return strdup (c);

      case OB_VERSION_OF_ABI:
        c = G_SPEAK_ABI_VERSION;
        return strdup (c);

      case OB_VERSION_OF_COMPILER:
#if defined(__clang_version__)
        c = "Clang " __clang_version__;
#elif defined(__GNUC__) && defined(__VERSION__)
        c = "gcc " __VERSION__;
#elif defined(_MSC_FULL_VER) && defined(_MSC_BUILD)
        snprintf (buf, sizeof (buf), "Microsoft C %u.%02u.%05u.%02u",
                  _MSC_FULL_VER / 10000000, (_MSC_FULL_VER / 100000) % 100,
                  _MSC_FULL_VER % 100000, _MSC_BUILD);
        c = buf;
#elif defined(_MSC_VER)
        snprintf (buf, sizeof (buf), "Microsoft C %u.%02u", _MSC_VER / 100,
                  _MSC_VER % 100);
        c = buf;
#endif
        return strdup (c);

      case OB_VERSION_OF_OS:
        return os_version ();

      case OB_VERSION_OF_KERNEL:
#ifdef _MSC_VER
        OB_CLEAR (ovx);
        ovx.dwOSVersionInfoSize = sizeof (ovx);
        if (GetVersionEx ((OSVERSIONINFO *) &ovx))
          {
            const int enough = 32;
            char *s = (char *) malloc (enough);
            snprintf (s, enough, "%u.%u", ovx.dwMajorVersion,
                      ovx.dwMinorVersion);
            return s;
          }
#else /* some kind of UNIX */
        if (uname (&u) == 0)
          c = u.release;
#endif /* _MSC_VER */
        return strdup (c);

      case OB_VERSION_OF_LIBC:
#ifdef _CS_GNU_LIBC_VERSION
        if (confstr (_CS_GNU_LIBC_VERSION, u.release, sizeof (u.release)) > 0)
          c = u.release;
#elif defined(_VC_CRT_MAJOR_VERSION)
        snprintf (buf, sizeof (buf), "CRT %u.%02u.%05u.%02u",
                  _VC_CRT_MAJOR_VERSION, _VC_CRT_MINOR_VERSION,
                  _VC_CRT_BUILD_VERSION, _VC_CRT_RBUILD_VERSION);
        c = buf;
#endif
        return strdup (c);

      case OB_VERSION_OF_CPU:
        return cpu_version ();

      case OB_VERSION_OF_YOBUILD:
        return yobuild_version ();

      case OB_VERSION_OF_MACHINE:
        return machine_version ();

      case OB_BUILD_CONFIGURATION:
#ifdef BUILD_CONFIG
        c = BUILD_CONFIG;
#elif defined(_MSC_VER)
        c =
#ifdef _DEBUG
          "Debug"
#elif defined(NDEBUG)
          "Release"
#else
          "Unknown"
#endif
#ifdef OB_WINDOWS_STATIC
          "Static"
#endif
          ;
#endif
        return strdup (c);

      default:
        return strdup (c);
    }
}

void ob_banner (FILE *where)
{
  char *yovo = ob_get_version (OB_VERSION_OF_GSPEAK);
  // Omit year in copyright because CODING-STYLE says to,
  // plus it just gets out of date anyway.
  fprintf (where,
           PRODNAME " (c) Oblong Industries and ANIMIST contributors - %s"
#ifdef GREENHOUSE
                  "+gh"
#endif
                  "\n",
           yovo);
  free (yovo);
#if defined(__GNUC__) && !defined(__OPTIMIZE__)
  fprintf (where, "!!! WARNING: Compiled without optimization, "
                  "which will reduce performance !!!\n");
#endif
}

static const char *parse_proc_file (const char *s, const char *label)
{
  if (!s)
    return NULL;
  const char *p = strstr (s, label);
  if (p)
    {
      p += strlen (label);
      while (*p == ' ' || *p == ':' || *p == '\t')
        p++;
      return p;
    }
  return NULL;
}

static OB_UNUSED int64 determine_memory (const char *what)
{
  int64 mem = -1;
  char *s = ob_read_file ("/proc/meminfo");
  const char *p = parse_proc_file (s, what);
  if (p)
    {
      int64 kb = strtol (p, NULL, 10);
      mem = kb / 1024;
    }
  free (s);
  return mem;
}

static int64 determine_mhz (void)
{
  int64 cpu_mhz = -1;
#if defined(__gnu_linux__)
  char *s = ob_read_file ("/proc/cpuinfo");
  const char *label = "\ncpu MHz";
  const char *p = parse_proc_file (s, label);
  if (p)
    {
      float64 f = strtod (p, NULL);
      cpu_mhz = (int64) (0.5 + f);
    }
  free (s);
#elif defined(__APPLE__) || defined(_MSC_VER)
  // Perhaps there's a better way, but all I can see to do is to
  // parse it out of the CPUID string (which luckily contains the GHz
  // on recent Intel processors, such as those used in Apple hardware).
  char *cpu = cpu_version ();
  const char *p = strrchr (cpu, 'G');
  if (p && p[1] == 'H' && p[2] == 'z')
    {
      char c;
      while (p > cpu && ((c = p[-1]) == '.' || (c >= '0' && c <= '9')))
        p--;
      float64 cpu_ghz = strtod (p, NULL);
      cpu_mhz = (int64) (0.5 + cpu_ghz * 1000);
    }
  free (cpu);
#endif
  return cpu_mhz;
}

#ifdef _MSC_VER
static ob_w32_func_wish get_performance_info_wish = {"psapi.dll",
                                                     "GetPerformanceInfo"};

typedef BOOL (WINAPI *get_performance_info_func) (PPERFORMANCE_INFORMATION,
                                                  DWORD);
static get_performance_info_func get_get_performance_info (void)
{
  return (get_performance_info_func) ob_w32_wish_for_func (
    &get_performance_info_wish);
}
#endif

int64 ob_get_system_info (ob_system_info what)
{
  switch (what)
    {
      case OB_SYSINFO_NUM_CORES:
#ifdef _MSC_VER
        // XXX: we may want GetLogicalProcessorInformation() instead, although
        // that's not available on Windows XP.
        {
          SYSTEM_INFO si;
          GetSystemInfo (&si);
          return si.dwNumberOfProcessors;
        }
#else
        return sysconf (_SC_NPROCESSORS_ONLN);
#endif
      case OB_SYSINFO_CPU_MHZ:
        return determine_mhz ();
      case OB_SYSINFO_PHYSICAL_MEGABYTES:
#if defined(__gnu_linux__)
        return determine_memory ("MemTotal");
#elif defined(__APPLE__)
        {
          int64 n;
          size_t nsiz = sizeof (n);
          if (0 == sysctlbyname ("hw.memsize", &n, &nsiz, NULL, 0)
              && nsiz == sizeof (n))
            return n / (1024 * 1024);
        }
#elif defined(_MSC_VER)
        {
          PERFORMANCE_INFORMATION pi;
          get_performance_info_func get_performance_info =
            get_get_performance_info ();
          if (get_performance_info && get_performance_info (&pi, sizeof (pi)))
            return (((int64) pi.PhysicalTotal) * (int64) pi.PageSize)
                   / (1024 * 1024);
        }
#endif
        break;
      case OB_SYSINFO_VIRTUAL_MEGABYTES:
#if defined(__gnu_linux__)
        return determine_memory ("\nSwapTotal");
#elif defined(__APPLE__)
        {
          struct xsw_usage x;
          size_t xsiz = sizeof (x);
          if (0 == sysctlbyname ("vm.swapusage", &x, &xsiz, NULL, 0)
              && xsiz == sizeof (x))
            return x.xsu_total / (1024 * 1024);
        }
#elif defined(_MSC_VER)
        {
          PERFORMANCE_INFORMATION pi;
          get_performance_info_func get_performance_info =
            get_get_performance_info ();
          if (get_performance_info && get_performance_info (&pi, sizeof (pi)))
            return (((int64) pi.CommitLimit) * (int64) pi.PageSize)
                   / (1024 * 1024);
        }
#endif
        break;
    }
  return -1;
}

ob_retort ob_check_abi (const char *file, const char *abi_string,
                        OB_UNUSED const char *gs_version,
                        int64 compiler_abi_version)
{
  const char *bn = ob_basename (file);
  ob_retort tort = OB_OK;

#if 0
  /* Now that the ABI version is auto-generated, I don't think we need to
   * also compare the plasma version, too. */
  if (gs_version)
    { char *gs_lib_vers = ob_get_version (OB_VERSION_OF_GSPEAK);
      if (0 != strcmp (gs_lib_vers, gs_version))
        { OB_LOG_WARNING_CODE (0x10080000,
                               "%s was compiled against '%s' headers,\n"
                               "but " PRODNAME " shared libraries are '%s'\n",
                               bn,
                               gs_version,
                               gs_lib_vers);
          tort = OB_VERSION_MISMATCH;
        }
      free (gs_lib_vers);
    }
#endif

  if (abi_string && 0 != strcmp (G_SPEAK_ABI_VERSION, abi_string))
    {
      OB_LOG_WARNING_CODE (0x10080001,
                           "%s was compiled against '" G_SPEAK_ABI_VERSION
                           "' headers,\n"
                           "but " PRODNAME " shared libraries are '%s'\n",
                           bn, abi_string);
      tort = OB_VERSION_MISMATCH;
    }

  const int64 our_gxx_abi = OB_COMPILER_ABI_VERSION;
  if (our_gxx_abi != compiler_abi_version)
    {
      OB_LOG_WARNING_CODE (0x10080002,
                           "%s was compiled against g++ ABI %" OB_FMT_64
                           "d headers,\n"
                           "but " PRODNAME " shared libraries are g++ ABI "
                           "%" OB_FMT_64 "d\n",
                           bn, compiler_abi_version, our_gxx_abi);
      tort = OB_VERSION_MISMATCH;
    }

  return tort;
}
