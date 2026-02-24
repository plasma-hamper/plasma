
/* (c)  oblong industries */

#include "libPlasma/c/slaw-coerce.h"
#include <stdio.h>
#include <stdlib.h>

// This is just to make valgrind happy!
static slaw to_be_freed[500];
static int free_counter = 0;

static slaw f (slaw s)
{
  to_be_freed[free_counter++] = s;
  return s;
}

static void report_unt64 (char c, unt64 x, ob_retort err)
{
  if (err == OB_OK)
    printf ("%c) %" OB_FMT_64 "u"
            "\n",
            c, x);
  else if (err == SLAW_RANGE_ERR)
    printf ("%c) SLAW_RANGE_ERR\n", c);
  else if (err == SLAW_NOT_NUMERIC)
    printf ("%c) SLAW_NOT_NUMERIC\n", c);
  else
    printf ("%c) some other error!\n", c);
}

static void report_int64 (char c, int64 y, ob_retort err)
{
  if (err == OB_OK)
    printf ("%c) %" OB_FMT_64 "d"
            "\n",
            c, y);
  else if (err == SLAW_RANGE_ERR)
    printf ("%c) SLAW_RANGE_ERR\n", c);
  else if (err == SLAW_NOT_NUMERIC)
    printf ("%c) SLAW_NOT_NUMERIC\n", c);
  else
    printf ("%c) some other error!\n", c);
}

typedef union
{
  float64 f;
  unt64 u;
} pun64;

static void report_float64 (char c, float64 z, ob_retort err)
{
  if (err == OB_OK)
    {
      // Unfortunately, printf gives different results on different plaforms:
      // printf ("%c) %f\n", c, z);
      //
      // So to avoid that, print the bitwise representation of the
      // floating point number:
      pun64 bizarre;
      bizarre.f = z;
      printf ("%c) hex bits of float64 = %016" OB_FMT_64 "X\n", c, bizarre.u);
    }
  else if (err == SLAW_RANGE_ERR)
    printf ("%c) SLAW_RANGE_ERR\n", c);
  else if (err == SLAW_NOT_NUMERIC)
    printf ("%c) SLAW_NOT_NUMERIC\n", c);
  else
    printf ("%c) some other error!\n", c);
}

int main (int argc, char **argv)
{
  unt64 x;
  int64 y;
  float64 z;
  ob_retort err;
  int i;

  err =
    slaw_to_unt64 (f (slaw_unt64 (OB_CONST_U64 (12345678901234567890))), &x);
  report_unt64 ('a', x, err);

  err = slaw_to_unt64 (f (slaw_int64 (OB_CONST_I64 (1234567890123456789))), &x);
  report_unt64 ('b', x, err);

  err =
    slaw_to_unt64 (f (slaw_int64 (OB_CONST_I64 (-1234567890123456789))), &x);
  report_unt64 ('c', x, err);

  err = slaw_to_unt64 (f (slaw_int32 (1234567890)), &x);
  report_unt64 ('d', x, err);

  err = slaw_to_unt64 (f (slaw_int32 (-1234567890)), &x);
  report_unt64 ('e', x, err);

  err = slaw_to_unt64 (f (slaw_unt32 (1234567890)), &x);
  report_unt64 ('f', x, err);

  err = slaw_to_unt64 (f (slaw_unt16 (12345)), &x);
  report_unt64 ('g', x, err);

  err = slaw_to_unt64 (f (slaw_unt8 (123)), &x);
  report_unt64 ('h', x, err);

  err = slaw_to_unt64 (f (slaw_float64 (1e19)), &x);
  report_unt64 ('i', x, err);

  err = slaw_to_unt64 (f (slaw_float32 (1.2297829e+19f)), &x);
  report_unt64 ('j', x, err);

  err = slaw_to_unt64 (f (slaw_string ("12345678901234567890")), &x);
  report_unt64 ('k', x, err);

  err = slaw_to_unt64 (f (slaw_string ("blech")), &x);
  report_unt64 ('l', x, err);

  err = slaw_to_unt64 (f (slaw_nil ()), &x);
  report_unt64 ('m', x, err);

  err = slaw_to_unt64 (f (slaw_float64 (-1.0)), &x);
  report_unt64 ('n', x, err);

  err = slaw_to_unt64 (f (slaw_float64 (1.5)), &x);
  report_unt64 ('o', x, err);

  err = slaw_to_unt64 (f (slaw_int16_array_filled (1, 157)), &x);
  report_unt64 ('p', x, err);

  err = slaw_to_unt64 (f (slaw_int8_array_filled (1, 7)), &x);
  report_unt64 ('q', x, err);

  err = slaw_to_unt64 (f (slaw_list_inline_c ("0x0015ed", NULL)), &x);
  report_unt64 ('r', x, err);

  err = slaw_to_unt64 (
    f (slaw_int64_array_filled (1, OB_CONST_I64 (-4503599627370495))), &x);
  report_unt64 ('s', x, err);

  err = slaw_to_unt64 (
    f (slaw_unt64_array_filled (1, OB_CONST_U64 (4503599627370495))), &x);
  report_unt64 ('t', x, err);

  err =
    slaw_to_unt64 (f (slaw_float64_array_filled (1, 4503599627370495.0)), &x);
  report_unt64 ('u', x, err);

  err = slaw_to_unt64 (f (slaw_float32_array_filled (1, -8388607.0)), &x);
  report_unt64 ('v', x, err);

  err = slaw_to_unt64 (f (slaw_int16 (-157)), &x);
  report_unt64 ('w', x, err);

  err = slaw_to_unt64 (f (slaw_int8 (-7)), &x);
  report_unt64 ('x', x, err);

  err = slaw_to_unt64 (f (slaw_int32_array_filled (1, -2147483647 - 1)), &x);
  report_unt64 ('y', x, err);

  err = slaw_to_unt64 (f (slaw_unt8_array_filled (1, 255)), &x);
  report_unt64 ('z', x, err);

  err = slaw_to_unt64 (f (slaw_unt16_array_filled (1, 65535)), &x);
  report_unt64 ('0', x, err);

  err = slaw_to_unt64 (f (slaw_unt32_array_filled (1, 4294967295U)), &x);
  report_unt64 ('1', x, err);

  err = slaw_to_unt64 (f (slaw_unt32_array_filled (0, 4294967295U)), &x);
  report_unt64 ('2', x, err);

  err = slaw_to_unt64 (f (slaw_unt32_array_filled (2, 4294967295U)), &x);
  report_unt64 ('3', x, err);

  printf ("\n");

  err =
    slaw_to_int64 (f (slaw_unt64 (OB_CONST_U64 (12345678901234567890))), &y);
  report_int64 ('a', y, err);

  err = slaw_to_int64 (f (slaw_int64 (OB_CONST_I64 (1234567890123456789))), &y);
  report_int64 ('b', y, err);

  err =
    slaw_to_int64 (f (slaw_int64 (OB_CONST_I64 (-1234567890123456789))), &y);
  report_int64 ('c', y, err);

  err = slaw_to_int64 (f (slaw_int32 (1234567890)), &y);
  report_int64 ('d', y, err);

  err = slaw_to_int64 (f (slaw_int32 (-1234567890)), &y);
  report_int64 ('e', y, err);

  err = slaw_to_int64 (f (slaw_unt32 (1234567890)), &y);
  report_int64 ('f', y, err);

  err = slaw_to_int64 (f (slaw_unt16 (12345)), &y);
  report_int64 ('g', y, err);

  err = slaw_to_int64 (f (slaw_unt8 (123)), &y);
  report_int64 ('h', y, err);

  err = slaw_to_int64 (f (slaw_float64 (1e19)), &y);
  report_int64 ('i', y, err);

  err = slaw_to_int64 (f (slaw_float32 (1.2297829e+19f)), &y);
  report_int64 ('j', y, err);

  err = slaw_to_int64 (f (slaw_string ("12345678901234567890")), &y);
  report_int64 ('k', y, err);

  err = slaw_to_int64 (f (slaw_string ("blech")), &y);
  report_int64 ('l', y, err);

  err = slaw_to_int64 (f (slaw_nil ()), &y);
  report_int64 ('m', y, err);

  err = slaw_to_int64 (f (slaw_float64 (-1.0)), &y);
  report_int64 ('n', y, err);

  err = slaw_to_int64 (f (slaw_float64 (1.5)), &y);
  report_int64 ('o', y, err);

  err = slaw_to_int64 (f (slaw_int16_array_filled (1, 157)), &y);
  report_int64 ('p', y, err);

  err = slaw_to_int64 (f (slaw_int8_array_filled (1, 7)), &y);
  report_int64 ('q', y, err);

  err = slaw_to_int64 (f (slaw_list_inline_c ("0x0015ed", NULL)), &y);
  report_int64 ('r', y, err);

  err = slaw_to_int64 (
    f (slaw_int64_array_filled (1, OB_CONST_I64 (-4503599627370495))), &y);
  report_int64 ('s', y, err);

  err = slaw_to_int64 (
    f (slaw_unt64_array_filled (1, OB_CONST_U64 (4503599627370495))), &y);
  report_int64 ('t', y, err);

  err =
    slaw_to_int64 (f (slaw_float64_array_filled (1, 4503599627370495.0)), &y);
  report_int64 ('u', y, err);

  err = slaw_to_int64 (f (slaw_float32_array_filled (1, -8388607.0)), &y);
  report_int64 ('v', y, err);

  err = slaw_to_int64 (f (slaw_int16 (-157)), &y);
  report_int64 ('w', y, err);

  err = slaw_to_int64 (f (slaw_int8 (-7)), &y);
  report_int64 ('x', y, err);

  err = slaw_to_int64 (f (slaw_int32_array_filled (1, -2147483647 - 1)), &y);
  report_int64 ('y', y, err);

  err = slaw_to_int64 (f (slaw_unt8_array_filled (1, 255)), &y);
  report_int64 ('z', y, err);

  err = slaw_to_int64 (f (slaw_unt16_array_filled (1, 65535)), &y);
  report_int64 ('0', y, err);

  err = slaw_to_int64 (f (slaw_unt32_array_filled (1, 4294967295U)), &y);
  report_int64 ('1', y, err);

  err = slaw_to_int64 (f (slaw_unt32_array_filled (0, 4294967295U)), &y);
  report_int64 ('2', y, err);

  err = slaw_to_int64 (f (slaw_unt32_array_filled (2, 4294967295U)), &y);
  report_int64 ('3', y, err);

  printf ("\n");

  err =
    slaw_to_float64 (f (slaw_unt64 (OB_CONST_U64 (12345678901234567890))), &z);
  report_float64 ('a', z, err);

  err =
    slaw_to_float64 (f (slaw_int64 (OB_CONST_I64 (1234567890123456789))), &z);
  report_float64 ('b', z, err);

  err =
    slaw_to_float64 (f (slaw_int64 (OB_CONST_I64 (-1234567890123456789))), &z);
  report_float64 ('c', z, err);

  err = slaw_to_float64 (f (slaw_int32 (1234567890)), &z);
  report_float64 ('d', z, err);

  err = slaw_to_float64 (f (slaw_int32 (-1234567890)), &z);
  report_float64 ('e', z, err);

  err = slaw_to_float64 (f (slaw_unt32 (1234567890)), &z);
  report_float64 ('f', z, err);

  err = slaw_to_float64 (f (slaw_unt16 (12345)), &z);
  report_float64 ('g', z, err);

  err = slaw_to_float64 (f (slaw_unt8 (123)), &z);
  report_float64 ('h', z, err);

  err = slaw_to_float64 (f (slaw_float64 (1e19)), &z);
  report_float64 ('i', z, err);

  err = slaw_to_float64 (f (slaw_float32 (1.2297829e+19f)), &z);
  report_float64 ('j', z, err);

  err = slaw_to_float64 (f (slaw_string ("12345678901234567890")), &z);
  report_float64 ('k', z, err);

  err = slaw_to_float64 (f (slaw_string ("blech")), &z);
  report_float64 ('l', z, err);

  err = slaw_to_float64 (f (slaw_nil ()), &z);
  report_float64 ('m', z, err);

  err = slaw_to_float64 (f (slaw_float64 (-1.0)), &z);
  report_float64 ('n', z, err);

  err = slaw_to_float64 (f (slaw_float64 (1.5)), &z);
  report_float64 ('o', z, err);

  err = slaw_to_float64 (f (slaw_int16_array_filled (1, 157)), &z);
  report_float64 ('p', z, err);

  err = slaw_to_float64 (f (slaw_int8_array_filled (1, 7)), &z);
  report_float64 ('q', z, err);

  err = slaw_to_float64 (f (slaw_list_inline_c ("0x0015ed", NULL)), &z);
  report_float64 ('r', z, err);

  err = slaw_to_float64 (
    f (slaw_int64_array_filled (1, OB_CONST_I64 (-4503599627370495))), &z);
  report_float64 ('s', z, err);

  err = slaw_to_float64 (
    f (slaw_unt64_array_filled (1, OB_CONST_U64 (4503599627370495))), &z);
  report_float64 ('t', z, err);

  err =
    slaw_to_float64 (f (slaw_float64_array_filled (1, 4503599627370495.0)), &z);
  report_float64 ('u', z, err);

  err = slaw_to_float64 (f (slaw_float32_array_filled (1, -8388607.0)), &z);
  report_float64 ('v', z, err);

  err = slaw_to_float64 (f (slaw_int16 (-157)), &z);
  report_float64 ('w', z, err);

  err = slaw_to_float64 (f (slaw_int8 (-7)), &z);
  report_float64 ('x', z, err);

  err = slaw_to_float64 (f (slaw_int32_array_filled (1, -2147483647 - 1)), &z);
  report_float64 ('y', z, err);

  err = slaw_to_float64 (f (slaw_unt8_array_filled (1, 255)), &z);
  report_float64 ('z', z, err);

  err = slaw_to_float64 (f (slaw_unt16_array_filled (1, 65535)), &z);
  report_float64 ('0', z, err);

  err = slaw_to_float64 (f (slaw_unt32_array_filled (1, 4294967295U)), &z);
  report_float64 ('1', z, err);

  err = slaw_to_float64 (f (slaw_unt32_array_filled (0, 4294967295U)), &z);
  report_float64 ('2', z, err);

  err = slaw_to_float64 (f (slaw_unt32_array_filled (2, 4294967295U)), &z);
  report_float64 ('3', z, err);

  // free stuff to please valgrind
  for (i = 0; i < free_counter; i++)
    slaw_free (to_be_freed[i]);

  return EXIT_SUCCESS;
}
