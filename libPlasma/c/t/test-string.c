
/* (c)  oblong industries */

/* A test of slaw strings; both basic string creation of various lengths,
 * and a test of slabu_of_strings_from_split and slaw_string_format. */

#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-vers.h"
#include "libLoam/c/ob-sys.h"
#include "libPlasma/c/slaw.h"
#include "libPlasma/c/slaw-string.h"

#include <stdlib.h>

#define EXPECT(str)                                                            \
  if (s == NULL)                                                               \
    OB_FATAL_ERROR_CODE (0x20315000, "unexpectedly NULL!\n");                  \
  if (!slaw_is_string (s))                                                     \
    OB_FATAL_ERROR_CODE (0x20315001, "not a string\n");                        \
  if (!slawx_equal_lc (s, str))                                                \
    OB_FATAL_ERROR_CODE (0x20315002, "'%s' is not '%s'\n",                     \
                         slaw_string_emit (s), str);                           \
  slaw_free (s);

#define TEST(str)                                                              \
  x = slaw_string (str);                                                       \
  if (!slaw_is_string (x))                                                     \
    OB_FATAL_ERROR_CODE (0x20315003, "not a string\n");                        \
  expected = strlen (str);                                                     \
  really = slaw_string_emit_length (x);                                        \
  if (really != expected)                                                      \
    OB_FATAL_ERROR_CODE (0x20315004, "length was %" OB_FMT_64                  \
                                     "d, but expected %" OB_FMT_64 "d\n",      \
                         really, expected);                                    \
  actual = slaw_string_emit (x);                                               \
  if (!actual)                                                                 \
    OB_FATAL_ERROR_CODE (0x20315005, "got NULL\n");                            \
  if (strcmp (str, actual) != 0)                                               \
    OB_FATAL_ERROR_CODE (0x20315006, "expected '%s' but got '%s'\n", str,      \
                         actual);                                              \
  slaw_free (x);


int main (int argc, char **argv)
{
  OB_DIE_ON_ERROR (OB_CHECK_ABI ());

  slaw x;
  int64 expected, really;
  const char *actual;

  TEST ("");
  TEST ("a");
  TEST ("at");
  TEST ("ace");
  TEST ("acid");
  TEST ("aback");
  TEST ("abacus");
  TEST ("abalone");
  TEST ("aardvark");
  TEST ("abdominal");
  TEST ("abbreviate");
  TEST ("abandonment");
  TEST ("abolitionist");
  TEST ("accessibility");
  TEST ("administration");
  TEST ("agriculturalist");
  TEST ("anesthesiologist");
  TEST ("commercialization");
  TEST ("chlorofluorocarbon");
  TEST ("counterintelligence");
  TEST ("uncharacteristically");
  TEST ("electroencephalograph");
  TEST ("counterrevolutionaries");
  TEST ("Raxacoricofallapatorius");
  TEST ("honorificabilitudinitatibus");
  TEST ("antidisestablishmentarianism");
  TEST ("floccinaucinihilipilification");
  TEST ("pseudopseudohypoparathyroidism");
  TEST ("supercalifragilisticexpialidocious");
  TEST ("pneumonoultramicroscopicsilicovolcanoconiosis");

  // if the string ends in the delimeter, it should be the same
  // as if the delimeter wasn't there (but it covers a different
  // code path)

  slabu *sb = slabu_of_strings_from_split ("ends,in,comma,", ",");
  slaw s = slaw_list_f (sb);

  if (slaw_list_count (s) != 3)
    OB_FATAL_ERROR_CODE (0x20315007, "length not 3\n");

  if (strcmp ((actual = slaw_string_emit (slaw_list_emit_nth (s, 2))), "comma")
      != 0)
    OB_FATAL_ERROR_CODE (0x20315008, "last string was '%s', not 'comma'\n",
                         actual);

  slaw_free (s);

  // splitting a zero-length string should result in an empty list

  sb = slabu_of_strings_from_split ("", ",");
  s = slaw_list_f (sb);

  if (!slaw_is_list (s))
    OB_FATAL_ERROR_CODE (0x20315009, "not a list\n");

  if (slaw_list_count (s) != 0)
    OB_FATAL_ERROR_CODE (0x2031500a, "length not 0\n");

  slaw_free (s);

  // now a short test of slaw_string_format

  s = slaw_string_format ("foo");
  EXPECT ("foo");

  // now a longer test (more than 32 characters)

  s = slaw_string_format ("I am a very long string, of more than 32 chars!");
  EXPECT ("I am a very long string, of more than 32 chars!");

  // now try some actual formatting

  s =
    slaw_string_format ("string %s, hex %X, negative %" OB_FMT_64 "d", "theory",
                        0xdeadbeef, OB_CONST_I64 (-1000000000000000000));
  EXPECT ("string theory, hex DEADBEEF, negative -1000000000000000000");

  // and now pure evil

  s = slaw_string_format ("%s is %x truly %c%c%c%c string, because it has %x%c"
                          "gr%d number of arguments %d %s, so if there %c "
                          "problems with not having %s on %s, we should %s "
                          "%" OB_FMT_64 "X them%c",
                          "This", 10, 'e', 'v', 'i', 'l', 10, ' ', 8, 2,
                          "format", 'R', "va_copy", "Windows", "hopefully",
                          OB_CONST_I64 (12), '!');
  EXPECT ("This is a truly evil string, because it has a gr8 number of "
          "arguments 2 format, so if there R problems with not having "
          "va_copy on Windows, we should hopefully C them!");

  // finally, try some strings right around the magic 32-char point...
  // 31 chars

  s = slaw_string_format ("%s", "abcdefghijklmnopqrstuvwxyz78901");
  EXPECT ("abcdefghijklmnopqrstuvwxyz78901");

  // 32 chars

  s = slaw_string_format ("%s", "abcdefghijklmnopqrstuvwxyz789012");
  EXPECT ("abcdefghijklmnopqrstuvwxyz789012");

  // 33 chars

  s = slaw_string_format ("%s", "abcdefghijklmnopqrstuvwxyz7890123");
  EXPECT ("abcdefghijklmnopqrstuvwxyz7890123");

  // is_valid_utf8 tests
  s = slaw_string_format("%s", "it was the best of times it was the blurst of times");
  if (!slaw_string_is_valid_utf8((bslaw)s))
    OB_FATAL_ERROR_CODE(0x20315010, "'%s' is incorrectly marked as an invalid utf8 string\n",
                        slaw_string_emit(s));
  slaw_free(s);

  s = slaw_string_format("%s", "\xc3\x28 was the best of times it was the blurst of times");

  if (slaw_string_is_valid_utf8((bslaw)s))
    OB_FATAL_ERROR_CODE(0x20315011, "'%s' is incorrectly marked as a valid utf8 string\n",
                        slaw_string_emit(s));
  slaw_free(s);

  return EXIT_SUCCESS;
}
