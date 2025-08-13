
/* (c)  oblong industries */

#ifndef OB_VERS_CTRL
#define OB_VERS_CTRL

#include "libLoam/c/ob-api.h"
#include "libLoam/c/ob-attrs.h"
#include "libLoam/c/ob-types.h"
#include "libLoam/c/ob-retorts.h"
#include <stdio.h>

#include "ob-vers-gen.h" /* a generated file containing version numbers */

#ifdef __cplusplus
extern "C" {
#endif

// XXX: OB_VERSION_OF_CPU and OB_VERSION_OF_MACHINE are really more of a
// "name" or "model" rather than "version".

/**
 * Argument to ob_get_version()
 */
typedef enum ob_version_of_what {
  OB_VERSION_OF_GSPEAK,
  OB_VERSION_OF_COMPILER,
  OB_VERSION_OF_OS,
  OB_VERSION_OF_KERNEL,
  OB_VERSION_OF_LIBC,
  OB_VERSION_OF_CPU,
  OB_VERSION_OF_YOBUILD,
  OB_VERSION_OF_MACHINE,
  OB_VERSION_OF_ABI,
  OB_BUILD_CONFIGURATION,
} ob_version_of_what;

/**
 * Returns the version number of the specified component as an
 * allocated string that the caller must free by calling the
 * standard ANSI C function free():
 * http://www.opengroup.org/onlinepubs/009695399/functions/free.html
 */
OB_LOAM_API char *ob_get_version (ob_version_of_what what);

/**
 * Prints a standard banner for all of our tools that includes the
 * version number.
 */
OB_LOAM_API void ob_banner (FILE *where);

/**
 * Argument to ob_get_system_info()
 */
typedef enum ob_system_info {
  OB_SYSINFO_NUM_CORES,
  OB_SYSINFO_CPU_MHZ,
  OB_SYSINFO_PHYSICAL_MEGABYTES,
  // XXX: maybe should call this swap instead of virtual
  OB_SYSINFO_VIRTUAL_MEGABYTES
} ob_system_info;

/**
 * Returns the requested information about the system.  (Similar to
 * ob_get_version(), but returns integers instead of strings.)
 * If value is unknown, returns -1.
 */
OB_LOAM_API OB_CONST int64 ob_get_system_info (ob_system_info what);

/**
 * On x86 processors, returns the values of ECX (in upper 32 bits) and
 * EDX (in lower 32 bits) when CPUID is called with EAX=1.  On non-x86
 * processors, returns 0.  See http://www.sandpile.org/x86/cpuid.htm
 */
OB_LOAM_API OB_CONST unt64 ob_x86_features (void);

/**
 * Defines some helpful bits that can be used to test the output of
 * ob_x86_features().
 */
//@{
#define OB_X86_FEATURE_SSE       (OB_CONST_U64 (1) << 25)
#define OB_X86_FEATURE_SSE2      (OB_CONST_U64 (1) << 26)
#define OB_X86_FEATURE_SSE3      (OB_CONST_U64 (1) << 32)
#define OB_X86_FEATURE_PCLMULQDQ (OB_CONST_U64 (1) << 33)
#define OB_X86_FEATURE_SSSE3     (OB_CONST_U64 (1) << 41)
#define OB_X86_FEATURE_FMA3      (OB_CONST_U64 (1) << 44)
#define OB_X86_FEATURE_CX16      (OB_CONST_U64 (1) << 45)
#define OB_X86_FEATURE_SSE41     (OB_CONST_U64 (1) << 51)
#define OB_X86_FEATURE_SSE42     (OB_CONST_U64 (1) << 52)
#define OB_X86_FEATURE_MOVBE     (OB_CONST_U64 (1) << 54)
#define OB_X86_FEATURE_POPCNT    (OB_CONST_U64 (1) << 55)
#define OB_X86_FEATURE_AES       (OB_CONST_U64 (1) << 57)
#define OB_X86_FEATURE_AVX       (OB_CONST_U64 (1) << 60)
#define OB_X86_FEATURE_CVT16     (OB_CONST_U64 (1) << 61)
#define OB_X86_FEATURE_RDRND     (OB_CONST_U64 (1) << 62)
//@}

/**
 * This is only for internal use.  So instead of using it, how about
 * using the OB_CHECK_ABI() macro instead?  It's much better, because
 * you don't have to give it any arguments!
 */
OB_LOAM_API ob_retort ob_check_abi (const char *file,
                                    const char *abi_string,
                                    const char *gs_version,
                                    int64 compiler_abi_version);

#ifdef __GXX_ABI_VERSION
#define OB_COMPILER_ABI_VERSION __GXX_ABI_VERSION
#else
#define OB_COMPILER_ABI_VERSION -1
#endif

/**
 * Check the g-speak ABI version of this header file against the
 * ABI version of the g-speak libraries being used.  Print a warning
 * if there is a mismatch.
 *
 * Returns a retort, which you can ignore if all you want is for the
 * warning to be printed, or you can use the retort to take further
 * action in a version mismatch.
 */
#define OB_CHECK_ABI()                                                         \
  ob_check_abi (__FILE__, G_SPEAK_ABI_VERSION, G_SPEAK_VERSION,                \
                OB_COMPILER_ABI_VERSION)

#ifdef __cplusplus
}
#endif

#endif /* OB_VERS_CTRL */
