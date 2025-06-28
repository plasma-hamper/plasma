
/* (c)  oblong industries */

// functions for swapping byte order
// (using compiler/platform-specific extensions for efficiency if possible)

#ifndef OB_ENDIAN_SUBCONTINENT
#define OB_ENDIAN_SUBCONTINENT

#include "libLoam/c/ob-types.h"

#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 2))
/**
 * \cond INTERNAL
 * gcc 4.3 and higher have __builtin_bswap32 and __builtin_bswap64
 */
#define OB_HAVE_GCC_BSWAP_BUILTINS
/** \endcond */
#endif

/**
 * Byte-swap a 16-bit integer
 */
static inline unt16 ob_swap16 (unt16 x)
{
  return (x >> 8) | (x << 8);
}

/**
 * Byte-swap a 32-bit integer
 */
static inline unt32 ob_swap32 (unt32 x)
{
#ifdef OB_HAVE_GCC_BSWAP_BUILTINS
  return __builtin_bswap32 (x);
#else
  return (( x << 24)             |
          ((x <<  8) & 0xff0000) |
          ((x >>  8) & 0xff00)   |
          ( x >> 24));
#endif /* OB_HAVE_GCC_BSWAP_BUILTINS */
}

/**
 * Byte-swap a 64-bit integer
 */
static inline unt64 ob_swap64 (unt64 x)
{
#ifdef OB_HAVE_GCC_BSWAP_BUILTINS
  return __builtin_bswap64 (x);
#else
  // Inspired by:
  // https://web.archive.org/web/20090805070309/http://blogs.sun.com/DanX/entry/optimizing_byte_swapping_for_fun
  // (see "I rewrote it to this a less elegant, but faster implementation:")
  return (( x << 56)                                    |
          ((x << 40) & OB_CONST_U64 (0xff000000000000)) |
          ((x << 24) & OB_CONST_U64 (0xff0000000000))   |
          ((x <<  8) & OB_CONST_U64 (0xff00000000))     |
          ((x >>  8) & OB_CONST_U64 (0xff000000))       |
          ((x >> 24) & OB_CONST_U64 (0xff0000))         |
          ((x >> 40) & OB_CONST_U64 (0xff00))           |
          ( x >> 56));
#endif /* OB_HAVE_GCC_BSWAP_BUILTINS */
}

#endif /* OB_ENDIAN_SUBCONTINENT */
