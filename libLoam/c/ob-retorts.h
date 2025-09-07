
/* (c)  oblong industries */

#ifndef OB_RETORTS_CONDENSER
#define OB_RETORTS_CONDENSER

/**
 * \file
 *
 * Success and failure codes in yovo are indicated with an ob_retort,
 * which is a signed 64-bit integer.  Failure codes are indicated with
 * negative ob_retorts, and success is indicated with nonnegative
 * ob_retorts.  The canonical success value is OB_OK, with the
 * numerical value 0, but it is possible to define other, positive,
 * success codes, which convey additional information.  But failure
 * codes are where most of the action is.
 *
 * In order to make it easy to print user-friendly error messages,
 * it's possible to convert an ob_retort into a string, using the
 * function ob_error_string().
 *
 * ob_retorts also have the ability to encapsulate POSIX error codes
 * (i. e. errno).  To convert an errno to a retort, call
 * ob_errno_to_retort().  Whoever called you can print this out just
 * like any other retort; ob_error_string() will produce the same
 * string for the retort that strerror() would produce for the errno.
 * Or, if the caller really wants to know what the original errno was,
 * they can call ob_retort_to_errno(), which will return the original
 * errno for a retort that encapsulates an errno, or -1 for a retort
 * that does not encapsulate an errno.
 *
 * ob_retorts are extensible.  Although a few very common ob_retorts
 * are defined in this file, most libraries define their own retorts.
 *
 * For more about ob_retort extensibility, see \ref ExtendingObRetort.
 */

#include "libLoam/c/ob-types.h"
#include "libLoam/c/ob-api.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * The type for error and success codes.  Negative codes are errors,
 * and nonnegative codes are successes.
 */
typedef int64 ob_retort;

/**
 * \cond INTERNAL
 * Still used in some crufty old code, but really we'd prefer that you
 * print out retorts as strings using ob_error_string().
 */
#define OB_FMT_RETORT OB_FMT_64
/** \endcond */

#define OB_CONST_RETORT(val) OB_CONST_I64 (val)

/**
 * Negative ob_retorts indicate an error.
 * Positive ob_retorts indicate success (but convey some
 * additional information beyond the canonical success
 * code, OB_OK).
 * Ranges of ob_retorts are allocated for various
 * purposes.  Each range includes both positive and negative
 * ob_retorts. */

/** defined in this file; generally useful */
//@{
#define OB_RETORTS_COMMON_FIRST   OB_CONST_RETORT (0)
#define OB_RETORTS_COMMON_LAST    OB_CONST_RETORT (99999)
//@}

/**
 * reserved for use by the application
 */
//@{
#define OB_RETORTS_APP_FIRST      OB_CONST_RETORT (100000)
#define OB_RETORTS_APP_LAST       OB_CONST_RETORT (199999)
//@}

/**
 * libPlasma/c
 */
//@{
#define OB_RETORTS_PLASMA_FIRST   OB_CONST_RETORT (200000)
#define OB_RETORTS_PLASMA_LAST    OB_CONST_RETORT (299999)
//@}

/**
 * libMedia (formerly libVid)
 */
//@{
#define OB_RETORTS_VIDEO_FIRST    OB_CONST_RETORT (300000)
#define OB_RETORTS_VIDEO_LAST     OB_CONST_RETORT (399999)
//@}

/**
 * libBasement
 */
//@{
#define OB_RETORTS_BASEMENT_FIRST OB_CONST_RETORT (400000)
#define OB_RETORTS_BASEMENT_LAST  OB_CONST_RETORT (499999)
//@}

/**
 * libImpetus
 */
//@{
#define OB_RETORTS_IMPETUS_FIRST  OB_CONST_RETORT (500000)
#define OB_RETORTS_IMPETUS_LAST   OB_CONST_RETORT (599999)
//@}

/**
 * libNoodoo
 */
//@{
#define OB_RETORTS_NOODOO_FIRST   OB_CONST_RETORT (600000)
#define OB_RETORTS_NOODOO_LAST    OB_CONST_RETORT (699999)
//@}

/**
 * libAfferent
 */
//@{
#define OB_RETORTS_AFFERENT_FIRST OB_CONST_RETORT (700000)
#define OB_RETORTS_AFFERENT_LAST  OB_CONST_RETORT (799999)
//@}

/**
 * system/libProtist
 */
//@{
#define OB_RETORTS_PROTIST_FIRST  OB_CONST_RETORT (800000)
#define OB_RETORTS_PROTIST_LAST   OB_CONST_RETORT (899999)
//@}

/**
 * libLoam/c++
 */
//@{
#define OB_RETORTS_LOAMXX_FIRST   OB_CONST_RETORT (900000)
#define OB_RETORTS_LOAMXX_LAST    OB_CONST_RETORT (999999)
//@}

/**
 * libGanglia
 */
//@{
#define OB_RETORTS_GANGLIA_FIRST  OB_CONST_RETORT (1000000)
#define OB_RETORTS_GANGLIA_LAST   OB_CONST_RETORT (1099999)
//@}

/**
 * libPlasma/c++
 */
//@{
#define OB_RETORTS_PLASMAXX_FIRST OB_CONST_RETORT (1100000)
#define OB_RETORTS_PLASMAXX_LAST  OB_CONST_RETORT (1199999)
//@}

/**
 * libResource
 */
//@{
#define OB_RETORTS_RESOURCE_FIRST OB_CONST_RETORT (1200000)
#define OB_RETORTS_RESOURCE_LAST  OB_CONST_RETORT (1299999)
//@}

/**
 * libSplotch
 */
//@{
#define OB_RETORTS_SPLOTCH_FIRST  OB_CONST_RETORT (1300000)
#define OB_RETORTS_SPLOTCH_LAST   OB_CONST_RETORT (1399999)
//@}

/**
 * libNoodoo2
 */
//@{
#define OB_RETORTS_NOODOO2_FIRST  OB_CONST_RETORT (1400000)
#define OB_RETORTS_NOODOO2_LAST   OB_CONST_RETORT (1499999)
//@}

/**
 * libTwillig2
 */
//@{
#define OB_RETORTS_TWILLIG2_FIRST OB_CONST_RETORT (1500000)
#define OB_RETORTS_TWILLIG2_LAST  OB_CONST_RETORT (1599999)
//@}

/**
 * hs-plasma
 */
//@{
#define OB_RETORTS_HASKELL_FIRST  OB_CONST_RETORT (1600000)
#define OB_RETORTS_HASKELL_LAST   OB_CONST_RETORT (1699999)
//@}

// so add additional ranges for other libraries here, starting at 1700000

/**
 * \cond INTERNAL
 * Windows errors take 32 bits, so we allocate the highest-numbered
 * 4294967296 retorts to encapsulate Windows errors.
 */
#define OB_RETORTS_WIN32_FIRST    OB_CONST_I64 (0x7fffffff00000000)
#define OB_RETORTS_WIN32_LAST     OB_CONST_I64 (0x7fffffffffffffff)
/** \endcond */

/* common codes */

/** The canonical success code, which conveys no further information. */
#define OB_OK                OB_CONST_RETORT (0)

// 1-199 reserved for local use
// (that means they should not be defined here or in any other core header file)

// error codes

/**
 * malloc failed, or similar
 */
#define OB_NO_MEM            OB_CONST_RETORT (-201)

/**
 * out-of-bounds access
 */
#define OB_BAD_INDEX         OB_CONST_RETORT (-202)

/**
 * function was not expecting a NULL argument, but it was nice enough
 * to tell you instead of segfaulting.
 */
#define OB_ARGUMENT_WAS_NULL OB_CONST_RETORT (-203)

/**
 * not the droids you're looking for
 */
#define OB_NOT_FOUND         OB_CONST_RETORT (-204)

/**
 * argument badness other than NULL or out-of-bounds
 */
#define OB_INVALID_ARGUMENT  OB_CONST_RETORT (-205)

/**
 * There was no way to determine what the error was, or the error is
 * so esoteric that nobody has bothered allocating a code for it yet.
 */
#define OB_UNKNOWN_ERR       OB_CONST_RETORT (-220)

/**
 * wrong parentage
 */
#define OB_INADEQUATE_CLASS  OB_CONST_RETORT (-221)

/**
 * You tried to add something that was already there.
 */
#define OB_ALREADY_PRESENT   OB_CONST_RETORT (-222)

/**
 * There was nothing there.  (e. g. popping from an empty stack)
 */
#define OB_EMPTY             OB_CONST_RETORT (-223)

/**
 * You tried to do something that was not allowed.
 */
#define OB_INVALID_OPERATION OB_CONST_RETORT (-224)

/**
 * The link to whatever-you-were-talking-to has been severed
 */
#define OB_DISCONNECTED      OB_CONST_RETORT (-260)

/**
 * Illegal mixing of different versions of Plasma headers and
 * shared libs. */
#define OB_VERSION_MISMATCH  OB_CONST_RETORT (-261)

/**
 * Unable to parse the given string.
 */
#define OB_PARSE_ERROR       OB_CONST_RETORT (-270)

// >>> add new error codes here <<<

// success codes

/**
 * not an error, but don't continue
 */
#define OB_STOP              OB_CONST_RETORT (300)

/**
 * Things are already in the state you requested them to be in.
 */
#define OB_NOTHING_TO_DO     OB_CONST_RETORT (301)

/** Success, and the answer was "yes" */
#define OB_YES               OB_CONST_RETORT (302)

/** Success, and the answer was "no" */
#define OB_NO                OB_CONST_RETORT (303)

/** Situation is fine, but you've proceeded far enough. */
#define OB_BOUNCE            OB_CONST_RETORT (304)

// >>> add new success codes here <<<

/**
 * This is how we encapsulate errno in an ob_retort.
 * According to the Single UNIX Specification:
 * http://www.opengroup.org/onlinepubs/9699919799/basedefs/errno.h.html
 * errnos are only required to be positive values of type "int".
 * But in practice, they never seem to go above the low 100's.
 * So, allocating 1000 should be plenty.  In general, each OS has its own
 * allocation of errno values.  So, to make ob_retorts globally unique
 * and allow them to be sent across a wire, we allocate a separate
 * space of 1000 for each major OS flavor (currently for us that means
 * Linux, Mac OS X, and Windows).  However, there are a non-trivial
 * number of errnos (most, but not all, of the values below 35) which
 * are shared across all 3 OSes.  To allow those values to be communicated
 * from one OS to another, we have a separate range for containing these
 * "non-OS-specific" errno values.  So this makes it a little complicated,
 * but it works.
 */

/**
 * \cond INTERNAL
 */
#define OB_MIN_ERRNO 1
#define OB_MAX_ERRNO 999

/* 1,2,3,4,5,6,7,8,9,10,12,13,14,16,17,18,19,
 * 20,21,22,23,24,25,27,28,29,30,31,32,33,34 */
#define OB_SHARED_ERRNOS OB_CONST_U64 (0x7fbff77fe)

// These retort ranges are inside the "common" range.
#define OB_RETORTS_ERRNO_SHARED  OB_CONST_RETORT (90000)  // thru 90999
#define OB_RETORTS_ERRNO_LINUX   OB_CONST_RETORT (91000)  // thru 91999
#define OB_RETORTS_ERRNO_MACOSX  OB_CONST_RETORT (92000)  // thru 92999
#define OB_RETORTS_ERRNO_WINDOWS OB_CONST_RETORT (93000)  // thru 93999
/** \endcond */

/**
 * Encapsulates a POSIX error, such as would be found in "errno",
 * in an ob_retort.
 */
static inline ob_retort ob_errno_to_retort (int e)
{
  if (e < OB_MIN_ERRNO || e > OB_MAX_ERRNO)
    return OB_UNKNOWN_ERR;
  if (e < 64 && 0 != (OB_SHARED_ERRNOS & (OB_CONST_U64 (1) << e)))
    return -(OB_RETORTS_ERRNO_SHARED + e);
#if defined(__gnu_linux__)
  return -(OB_RETORTS_ERRNO_LINUX + e);
#elif defined(__APPLE__)
  return -(OB_RETORTS_ERRNO_MACOSX + e);
#elif defined(_MSC_VER)
  return -(OB_RETORTS_ERRNO_WINDOWS + e);
#else
  return OB_UNKNOWN_ERR;
#endif
}

/**
 * Returns the POSIX error that was encapsulated in the given retort,
 * or -1 if the retort does not encapsulate a POSIX error.
 */
static inline int ob_retort_to_errno (ob_retort t)
{
  if (t == OB_OK)
    return 0;
  t = -t;  // positive numbers are easier to work with
  if (t >= OB_RETORTS_ERRNO_SHARED
      && t <= OB_RETORTS_ERRNO_SHARED + OB_MAX_ERRNO)
    return (int) (t - OB_RETORTS_ERRNO_SHARED);
  ob_retort base;
#if defined(__gnu_linux__)
  base = OB_RETORTS_ERRNO_LINUX;
#elif defined(__APPLE__)
  base = OB_RETORTS_ERRNO_MACOSX;
#elif defined(_MSC_VER)
  base = OB_RETORTS_ERRNO_WINDOWS;
#else
  base = OB_RETORTS_ERRNO_SHARED;
#endif
  if (t >= base && t <= base + OB_MAX_ERRNO)
    return (int) (t - base);
  return -1;
}

/**
 * Given an ob_retort, return a string which describes that error code.
 * Strings for ob_retorts defined in libLoam and libPlasma are
 * built-in, and strings for other modules can be added by calling
 * ob_add_error_names().
 */

OB_LOAM_API const char *ob_error_string (ob_retort err);

/**
 * This is like ob_error_string(), but it returns NULL
 * if the retort is unknown.
 */

OB_LOAM_API const char *ob_error_string_literal (ob_retort err);

/**
 * The function pointer type expected by ob_add_error_names().
 */

typedef const char *(*ob_translation_func) (ob_retort err);

/**
 * Register a function which will attempt to convert an ob_retort
 * to a string.  func should return NULL if it does not have a
 * translation for the given ob_retort.
 *
 * In C++, such functions can be registered automatically by
 * doing something like this:
 * static ob_retort dummy = ob_add_error_names (my_func);
 */

OB_LOAM_API ob_retort ob_add_error_names (ob_translation_func func);

/**
 * Check whether a given retort code has been registered.
 */

OB_LOAM_API bool ob_retort_exists (ob_retort ret);


/**
 * Handy function to combine error codes from a bunch of operations
 */
static inline void ob_err_accum (ob_retort *a, ob_retort e)
{
  if (e < OB_OK && *a >= OB_OK)
    *a = e;
}

/**
 * Takes a retort and does nothing with it.  This can be used to indicate
 * that a retort is being intentionally ignored, rather than accidentally.
 * Especially useful in combination with functions that return an
 * ob_retort and use the OB_WARN_UNUSED_RESULT attribute.
 */
OB_LOAM_API void ob_ignore_retort (ob_retort ret);

#ifdef __cplusplus
}
#endif


#endif /* OB_RETORTS_CONDENSER */
