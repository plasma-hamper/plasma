
/* (c)  oblong industries */

// A very basic test of the non-complex, non-vector, non-array numeric types.

#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-vers.h"
#include "libPlasma/c/slaw.h"

#include <stdlib.h>
#include <stdio.h>

#define TEST(num, type)                                                        \
  s = slaw_##type (num);                                                       \
  if (!slaw_is_numeric (s))                                                    \
    OB_FATAL_ERROR_CODE (0x2030a000, "slaw_is_numeric failed\n");              \
  if (!slaw_is_##type (s))                                                     \
    OB_FATAL_ERROR_CODE (0x2030a001, "slaw_is_%s failed\n", #type);            \
  if (slaw_is_##type##_array (s))                                              \
    OB_FATAL_ERROR_CODE (0x2030a002, "unexpectedly an array\n");               \
  {                                                                            \
    type x = *slaw_##type##_emit (s);                                          \
    if (x != (num))                                                            \
      OB_FATAL_ERROR_CODE (                                                    \
        0x2030a003, "expected %" OB_FMT_64 "d but got %" OB_FMT_64 "d\n",      \
        (int64) (num), (int64) x);                                             \
  }                                                                            \
  slaw_free (s);

int main (int argc, char **argv)
{
  OB_DIE_ON_ERROR (OB_CHECK_ABI ());

  slaw s;

  TEST (123, int8);
  TEST (456, int16);
  TEST (7890, int32);
  TEST (OB_CONST_I64 (0x1234567890abcdef), int64);

  TEST (-123, int8);
  TEST (-456, int16);
  TEST (-7890, int32);
  TEST (OB_CONST_I64 (-0x1234567890abcdef), int64);

  TEST (123, unt8);
  TEST (456, unt16);
  TEST (7890, unt32);
  TEST (OB_CONST_I64 (0x1234567890abcdef), unt64);

  TEST (123, float32);
  TEST (456, float64);

  return EXIT_SUCCESS;
}
