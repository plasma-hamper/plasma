
/* (c)  oblong industries */

#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-vers.h"
#include "libPlasma/c/slaw.h"
#include "libPlasma/c/slaw-string.h"
#include "libPlasma/c/pool.h"
#include <stdlib.h>

int main (int argc, char **argv)
{
  OB_DIE_ON_ERROR (OB_CHECK_ABI ());

  int64 got;
  int64 expected;

  slaw lst =
    slaw_list_inline_c ("a", "man", "a", "plan", "a", "canal", "panama", NULL);
  slaw s1 = slaw_string ("banana");
  slaw s2 = slaw_list_inline_c ("a", "plan", "a", NULL);
  slaw s3 = slaw_list_inline_c ("canal", "man", NULL);
  slaw s4 = slaw_list_inline (s1, NULL);
  slaw s5 = slaw_lists_concat (s2, s4, NULL);
  slaw s6 = slaw_strings_join (s5, " ");

  if (!slawx_equal_lc (s6, "a plan a banana"))
    OB_FATAL_ERROR_CODE (0x20305000, "banana error\n");

  got = slaw_list_find (lst, s1);
  expected = -1;
  if (got != expected)
    OB_FATAL_ERROR_CODE (0x20305001,
                         "got %" OB_FMT_64 "d but expected %" OB_FMT_64 "d\n",
                         got, expected);

  got = slaw_list_find_f (lst, slaw_string ("panama"));
  expected = 6;
  if (got != expected)
    OB_FATAL_ERROR_CODE (0x20305002,
                         "got %" OB_FMT_64 "d but expected %" OB_FMT_64 "d\n",
                         got, expected);

  got = slaw_list_find_c (lst, "a");
  expected = 0;
  if (got != expected)
    OB_FATAL_ERROR_CODE (0x20305003,
                         "got %" OB_FMT_64 "d but expected %" OB_FMT_64 "d\n",
                         got, expected);

  got = slaw_list_contigsearch_inline (lst, slaw_list_emit_nth (s2, 1),
                                       slaw_list_emit_nth (s2, 0),
                                       slaw_list_emit_nth (s3, 0), NULL);
  expected = 3;
  if (got != expected)
    OB_FATAL_ERROR_CODE (0x20305004,
                         "got %" OB_FMT_64 "d but expected %" OB_FMT_64 "d\n",
                         got, expected);

  got = slaw_list_gapsearch_inline (lst, slaw_list_emit_nth (s3, 1),
                                    slaw_list_emit_nth (lst, 6), NULL);
  expected = 1;
  if (got != expected)
    OB_FATAL_ERROR_CODE (0x20305005,
                         "got %" OB_FMT_64 "d but expected %" OB_FMT_64 "d\n",
                         got, expected);

  got = slaw_list_contigsearch (lst, s2);
  expected = 2;
  if (got != expected)
    OB_FATAL_ERROR_CODE (0x20305006,
                         "got %" OB_FMT_64 "d but expected %" OB_FMT_64 "d\n",
                         got, expected);

  got = slaw_list_gapsearch_f (lst, s2);
  expected = 0;
  if (got != expected)
    OB_FATAL_ERROR_CODE (0x20305007,
                         "got %" OB_FMT_64 "d but expected %" OB_FMT_64 "d\n",
                         got, expected);

  got = slaw_list_gapsearch (lst, s3);
  expected = -1;
  if (got != expected)
    OB_FATAL_ERROR_CODE (0x20305008,
                         "got %" OB_FMT_64 "d but expected %" OB_FMT_64 "d\n",
                         got, expected);

  got = slaw_list_contigsearch_f (lst, s3);
  expected = -1;
  if (got != expected)
    OB_FATAL_ERROR_CODE (0x20305009,
                         "got %" OB_FMT_64 "d but expected %" OB_FMT_64 "d\n",
                         got, expected);

  got = slaw_list_contigsearch_inline_f (
    lst, slaw_string ("man"), slaw_string ("a"), slaw_string ("plan"), NULL);
  expected = 1;
  if (got != expected)
    OB_FATAL_ERROR_CODE (0x2030500a,
                         "got %" OB_FMT_64 "d but expected %" OB_FMT_64 "d\n",
                         got, expected);

  got = slaw_list_gapsearch_inline_f (
    lst, slaw_string ("plan"), slaw_string ("a"), slaw_string ("panama"), NULL);
  expected = 3;
  if (got != expected)
    OB_FATAL_ERROR_CODE (0x2030500b,
                         "got %" OB_FMT_64 "d but expected %" OB_FMT_64 "d\n",
                         got, expected);

  got = slaw_list_contigsearch_inline_c (lst, "canal", NULL);
  expected = 5;
  if (got != expected)
    OB_FATAL_ERROR_CODE (0x2030500c,
                         "got %" OB_FMT_64 "d but expected %" OB_FMT_64 "d\n",
                         got, expected);

  got = slaw_list_gapsearch_inline_c (lst, "a", "a", "a", NULL);
  expected = 0;
  if (got != expected)
    OB_FATAL_ERROR_CODE (0x2030500d,
                         "got %" OB_FMT_64 "d but expected %" OB_FMT_64 "d\n",
                         got, expected);

  got = slaw_list_contigsearch_f (s5, s4);
  expected = 3;
  if (got != expected)
    OB_FATAL_ERROR_CODE (0x2030500e,
                         "got %" OB_FMT_64 "d but expected %" OB_FMT_64 "d\n",
                         got, expected);

  got = slaw_list_gapsearch_f (lst, s5);
  expected = -1;
  if (got != expected)
    OB_FATAL_ERROR_CODE (0x2030500f,
                         "got %" OB_FMT_64 "d but expected %" OB_FMT_64 "d\n",
                         got, expected);

  slaw_free (lst);
  slaw_free (s1);
  slaw_free (s6);

  return EXIT_SUCCESS;
}
