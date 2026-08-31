
/* (c)  oblong industries */

/* Test slaw_spew_overview_to_string_ex(). */

#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-sys.h"   /* for unistd.h, for isatty() */
#include "libLoam/c/ob-vers.h"
#include "libPlasma/c/slaw.h"
#include "libPlasma/c/protein.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define STDOUT_IS_A_TTY true
#else
#define STDOUT_IS_A_TTY isatty(STDOUT_FILENO)
#endif

/* Test that _ex(s, false, NULL) returns the same string as
 * slaw_spew_overview_to_string(s). */
static void test_false_null_matches_non_ex (void)
{
  slaw s = slaw_string ("phosphorescent");
  slaw r1 = slaw_spew_overview_to_string (s);
  slaw r2 = slaw_spew_overview_to_string_ex (s, 0, NULL);
  if (!slawx_equal (r1, r2))
    OB_FATAL_ERROR_CODE (0x2031b000,
                         "expected:\n%s\nbut got:\n%s\n",
                         slaw_string_emit (r1),
                         slaw_string_emit (r2));
  slaw_free (r1);
  slaw_free (r2);
  slaw_free (s);
}

/* Test that relative-offset output differs from absolute-pointer
 * output.  Since the top-level slaw's relative offset is always 0,
 * but its absolute address is non-zero, the two strings must differ.
 */
static void test_relative_differs_from_absolute (void)
{
  slaw s = slaw_string ("phosphorescent");
  slaw rel =
      slaw_spew_overview_to_string_ex (s, SLAW_SPEW_FLAG_REL_OFF, NULL);
  slaw abs =
      slaw_spew_overview_to_string_ex (s, 0, NULL);

  if (slawx_equal (rel, abs))
    OB_FATAL_ERROR_CODE (
      0x2031b001,
      "relative and absolute outputs should not be equal\n");
  slaw_free (rel);
  slaw_free (abs);
  slaw_free (s);
}

/* Test that the top-level slaw's relative offset is annotated as 0.
 * Use a small string so that num_digits is 1 and no padding is added,
 * making the marker unambiguous. */
static void test_relative_offset_is_zero_for_top_level (void)
{
  slaw s = slaw_string ("abc");
  slaw r = slaw_spew_overview_to_string_ex (s, SLAW_SPEW_FLAG_REL_OFF, NULL);
  const char *str = slaw_string_emit (r);
  if (!str)
    OB_FATAL_ERROR_CODE (0x2031b002, "got NULL string\n");
  /* The top-level slaw is at relative offset 0, so the annotation
   * must contain "o.0x0]:" with no padding. */
  if (!strstr (str, "o.0x0]: "))
    OB_FATAL_ERROR_CODE (
      0x2031b003,
      "expected 'o.0x0]: ' in relative-offset output: %s\n", str);
  slaw_free (r);
  slaw_free (s);
}

/* Test that the prolo is prepended to the top-level annotation. */
static void test_prolo_on_single_slaw (void)
{
  slaw s = slaw_string ("phosphorescent");
  slaw r = slaw_spew_overview_to_string_ex (s, 0, ">>");
  const char *str = slaw_string_emit (r);
  if (!str)
    OB_FATAL_ERROR_CODE (0x2031b004, "got NULL string\n");
  if (strncmp (str, ">>slaw[", 7) != 0)
    OB_FATAL_ERROR_CODE (
      0x2031b005,
      "expected '>>slaw[' at start, got: %s\n", str);
  slaw_free (r);
  slaw_free (s);
}

/* Test that the prolo is prepended to each sub-element line in a
 * multi-element list. */
static void test_prolo_on_list (void)
{
  slaw s =
    slaw_list_inline_f (slaw_string ("alpha"), slaw_string ("beta"),
                        NULL);
  slaw r = slaw_spew_overview_to_string_ex (s, 0, "::");
  const char *str = slaw_string_emit (r);
  if (!str)
    OB_FATAL_ERROR_CODE (0x2031b006, "got NULL string\n");
  /* Top-level line starts with "::" */
  if (strncmp (str, "::slaw[", 7) != 0)
    OB_FATAL_ERROR_CODE (
      0x2031b007,
      "expected '::slaw[' at start, got: %s\n", str);
  /* Each sub-element line is prefixed with "::" followed by
   * " N: ".  The newline before the prefix is emitted separately,
   * so we look for "\n:: ". */
  if (!strstr (str, "\n:: "))
    OB_FATAL_ERROR_CODE (
      0x2031b008,
      "expected '\\n:: ' for sub-element lines, got: %s\n", str);
  slaw_free (r);
  slaw_free (s);
}

/* Test that a NULL slaw produces "[no slaw -- NULL]". */
static void test_null_slaw (void)
{
  slaw r = slaw_spew_overview_to_string_ex (NULL, 0, NULL);
  const char *str = slaw_string_emit (r);
  if (!str)
    OB_FATAL_ERROR_CODE (0x2031b009, "got NULL string\n");
  const char *expected = "[no slaw -- NULL]";
  if (strcmp (str, expected) != 0)
    OB_FATAL_ERROR_CODE (0x2031b00a,
                         "expected '%s' but got '%s'\n",
                         expected, str);
  slaw_free (r);
}

/* Test that a NULL slaw with a prolo returns the prolo prepended to
 * "[no slaw -- NULL]". */
static void test_null_slaw_with_prolo (void)
{
  slaw r = slaw_spew_overview_to_string_ex (NULL, 0, "XX");
  const char *str = slaw_string_emit (r);
  if (!str)
    OB_FATAL_ERROR_CODE (0x2031b00b, "got NULL string\n");
  const char *expected = "XX[no slaw -- NULL]";
  if (strcmp (str, expected) != 0)
    OB_FATAL_ERROR_CODE (0x2031b00c,
                         "expected '%s' but got '%s'\n",
                         expected, str);
  slaw_free (r);
}

static const char *const relative_expected_lines[] =
  {
    "slaw[13o.0x00]: MAP (4 elems): {",
    " 1: slaw[3o.0x08]: CONS:",
    " 1:  L: slaw[1o.0x10]: STR(3): \"nil\"",
    " 1:  R: slaw[1o.0x18]: NIL.",
    " 2: slaw[3o.0x20]: CONS:",
    " 2:  L: slaw[1o.0x28]: STR(5): \"false\"",
    " 2:  R: slaw[1o.0x30]: BOOLEAN: false",
    " 3: slaw[3o.0x38]: CONS:",
    " 3:  L: slaw[1o.0x40]: STR(5): \"empty\"",
    " 3:  R: slaw[1o.0x48]: MAP (0 elems): {",
    " 3:  R:  }",
    " 4: slaw[3o.0x50]: CONS:",
    " 4:  L: slaw[1o.0x58]: STR(5): \"int32\"",
    " 4:  R: slaw[1o.0x60]: INT32 = 12",
    " }",
    NULL
  };

static void diff_lines (const char *actual, const char *const *expected)
{
  char   *str = strdup (actual);
  char   *s1  = str;
  int     i;
  char   *line;

  for (i = 0; expected[i]; i++)
    {
      line = strtok (s1, "\n");
      if (strcmp (line, expected[i]) != 0)
        {
          OB_FATAL_ERROR_CODE (0x2031b00d,
                               "On line %d,\n"
                               "  expected '%s'\n"
                               "  but got  '%s'\n",
                               i + 1,
                               expected[i],
                               (line ? line : "(null)"));
        }

      s1 = NULL;
    }

  line = strtok (s1, "\n");
  if (line != NULL)
    {
      OB_FATAL_ERROR_CODE (0x2031b00e,
                           "Did not expect extra line\n"
                           "  '%s'\n", line);
    }

  free (str);
}

static void test_relative_offset (void)
{
  slaw s = slaw_map_inline_cf ("nil",   slaw_nil(),
                               "false", slaw_boolean (false),
                               "empty", slaw_map_empty(),
                               "int32", slaw_int32 (12),
                               NULL);
  slaw r = slaw_spew_overview_to_string_ex (s, SLAW_SPEW_FLAG_REL_OFF, NULL);
  const char *str = slaw_string_emit (r);

  diff_lines (str, relative_expected_lines);

  slaw_free (r);
  slaw_free (s);
}

typedef struct accum_str
{
  char  *buf;
  size_t len;
  size_t capacity;
} accum_str;

static ob_retort accum_str_func (void       *cookie,
                                 const char *str,
                                 size_t      len)
{
  accum_str *acc   = (accum_str *) cookie;
  size_t available = acc->capacity - acc->len;
  size_t len1      = len + 1;   /* account for terminating NUL */

  if (len1 >= available)
    {
      size_t newcap1 = acc->capacity * 2;
      size_t newcap2 = acc->len + len1 + 128;
      size_t newcap  = (newcap1 > newcap2) ? newcap1 : newcap2;
      char  *newbuf  = realloc (acc->buf, newcap);

      if (newbuf == NULL)
        return OB_NO_MEM;

      acc->buf      = newbuf;
      acc->capacity = newcap;
    }

  memcpy (acc->buf + acc->len, str, len1);
  acc->len += len;

  return OB_OK;
}

static char *accum_spew_str (bslaw s,
                             unt32 flags,
                             const char *prolo)
{
  accum_str acc;
  acc.len      = 0;
  acc.capacity = 128;
  acc.buf      = calloc (acc.capacity, sizeof (char));

  if (acc.buf == NULL)
    {
      OB_FATAL_ERROR_CODE (0x2031b00f, "Out of memory");
    }

  ob_retort tort = slaw_spew_overview_to_func (s,
                                               accum_str_func,
                                               &acc,
                                               80,
                                               flags,
                                               prolo);

  OB_DIE_ON_ERR_CODE (0x2031b010, tort);

  return acc.buf;
}

static slaw make_rude_protein (void)
{
  unt8 rude[256];
  size_t i;

  for (i = 0; i < sizeof (rude); i++)
    rude[i] = (unt8) i;

  return protein_from_ffr (slaw_nil(),
                           slaw_nil(),
                           rude, sizeof (rude));
}

static const char *const rude_ascii_expected_lines[] =
  {
    "> slaw[36o.0x000]: PROT: ((",
    "> descrips:",
    "> slaw[1o.0x010]: NIL.",
    "> ingests:",
    "> slaw[1o.0x018]: NIL.",
    "> rude data: 256 bytes",
    ">  00 01 02 03 04 05 06 07  08 09 0a 0b 0c 0d 0e 0f  |................|",
    ">  10 11 12 13 14 15 16 17  18 19 1a 1b 1c 1d 1e 1f  |................|",
    ">  20 21 22 23 24 25 26 27  28 29 2a 2b 2c 2d 2e 2f  | !\"#$%&'()*+,-./|",
    ">  30 31 32 33 34 35 36 37  38 39 3a 3b 3c 3d 3e 3f  |0123456789:;<=>?|",
    ">  40 41 42 43 44 45 46 47  48 49 4a 4b 4c 4d 4e 4f  |@ABCDEFGHIJKLMNO|",
    ">  50 51 52 53 54 55 56 57  58 59 5a 5b 5c 5d 5e 5f  |PQRSTUVWXYZ[\\]^_|",
    ">  60 61 62 63 64 65 66 67  68 69 6a 6b 6c 6d 6e 6f  |`abcdefghijklmno|",
    ">  70 71 72 73 74 75 76 77  78 79 7a 7b 7c 7d 7e 7f  |pqrstuvwxyz{|}~.|",
    ">  80 81 82 83 84 85 86 87  88 89 8a 8b 8c 8d 8e 8f  |................|",
    ">  90 91 92 93 94 95 96 97  98 99 9a 9b 9c 9d 9e 9f  |................|",
    ">  a0 a1 a2 a3 a4 a5 a6 a7  a8 a9 aa ab ac ad ae af  |................|",
    ">  b0 b1 b2 b3 b4 b5 b6 b7  b8 b9 ba bb bc bd be bf  |................|",
    ">  c0 c1 c2 c3 c4 c5 c6 c7  c8 c9 ca cb cc cd ce cf  |................|",
    ">  d0 d1 d2 d3 d4 d5 d6 d7  d8 d9 da db dc dd de df  |................|",
    ">  e0 e1 e2 e3 e4 e5 e6 e7  e8 e9 ea eb ec ed ee ef  |................|",
    ">  f0 f1 f2 f3 f4 f5 f6 f7  f8 f9 fa fb fc fd fe ff  |................|",
    ">  ))",
    NULL
  };

static void test_rude_ascii (void)
{
  slaw  s   = make_rude_protein();
  unt32 flg = SLAW_SPEW_FLAG_REL_OFF | SLAW_SPEW_FLAG_RUDE_ASCII;
  char *str = accum_spew_str (s, flg, "> ");

  diff_lines (str, rude_ascii_expected_lines);

  free (str);
  slaw_free (s);
}

static const char *const rude_noascii_expected_lines[] =
  {
    "% slaw[36o.0x000]: PROT: ((",
    "% descrips:",
    "% slaw[1o.0x010]: NIL.",
    "% ingests:",
    "% slaw[1o.0x018]: NIL.",
    "% rude data: 256 bytes",
    "%  00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f",
    "%  10 11 12 13 14 15 16 17 18 19 1a 1b 1c 1d 1e 1f",
    "%  20 21 22 23 24 25 26 27 28 29 2a 2b 2c 2d 2e 2f",
    "%  30 31 32 33 34 35 36 37 38 39 3a 3b 3c 3d 3e 3f",
    "%  40 41 42 43 44 45 46 47 48 49 4a 4b 4c 4d 4e 4f",
    "%  50 51 52 53 54 55 56 57 58 59 5a 5b 5c 5d 5e 5f",
    "%  60 61 62 63 64 65 66 67 68 69 6a 6b 6c 6d 6e 6f",
    "%  70 71 72 73 74 75 76 77 78 79 7a 7b 7c 7d 7e 7f",
    "%  80 81 82 83 84 85 86 87 88 89 8a 8b 8c 8d 8e 8f",
    "%  90 91 92 93 94 95 96 97 98 99 9a 9b 9c 9d 9e 9f",
    "%  a0 a1 a2 a3 a4 a5 a6 a7 a8 a9 aa ab ac ad ae af",
    "%  b0 b1 b2 b3 b4 b5 b6 b7 b8 b9 ba bb bc bd be bf",
    "%  c0 c1 c2 c3 c4 c5 c6 c7 c8 c9 ca cb cc cd ce cf",
    "%  d0 d1 d2 d3 d4 d5 d6 d7 d8 d9 da db dc dd de df",
    "%  e0 e1 e2 e3 e4 e5 e6 e7 e8 e9 ea eb ec ed ee ef",
    "%  f0 f1 f2 f3 f4 f5 f6 f7 f8 f9 fa fb fc fd fe ff",
    "%  ))",
    NULL
  };

static void test_rude_noascii (void)
{
  slaw  s   = make_rude_protein();
  unt32 flg = SLAW_SPEW_FLAG_REL_OFF;
  char *str = accum_spew_str (s, flg, "% ");

  diff_lines (str, rude_noascii_expected_lines);

  free (str);
  slaw_free (s);
}

static slaw make_string_example (void)
{
  const char *str = "If you like pi\303\261a coladas\nCome with me and \033";
  char        buf[256];
  size_t      i;

  for (i = 0; i < sizeof (buf); i++)
    buf[i] = (char) (i + 1);

  return slaw_list_inline_c (str, buf, NULL);
}

static const char *const escape_string_expected_lines[] =
  {
    "slaw[41o.0x000]: LIST (2 elems): {",
    " 1: slaw[7o.0x008]: STR(44): \"If you like pi\303\261a coladas\\nCome with me and \\e\"",
    " 2: slaw[33o.0x040]: STR(255): \"\\x01\\x02\\x03\\x04\\x05\\x06\\a\\b\\t\\n\\v\\f\\r\\x0E\\x0F\\x10\\x11\\x12\\x13\\x14\\x15\\x16\\x17\\x18\\x19\\x1A\\e\\x1C\\x1D\\x1E\\x1F !\\\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\\x7F\\x80\\x81\\x82\\x83\\x84\\x85\\x86\\x87\\x88\\x89\\x8A\\x8B\\x8C\\x8D\\x8E\\x8F\\x90\\x91\\x92\\x93\\x94\\x95\\x96\\x97\\x98\\x99\\x9A\\x9B\\x9C\\x9D\\x9E\\x9F\\xA0\\xA1\\xA2\\xA3\\xA4\\xA5\\xA6\\xA7\\xA8\\xA9\\xAA\\xAB\\xAC\\xAD\\xAE\\xAF\\xB0\\xB1\\xB2\\xB3\\xB4\\xB5\\xB6\\xB7\\xB8\\xB9\\xBA\\xBB\\xBC\\xBD\\xBE\\xBF\\xC0\\xC1\\xC2\\xC3\\xC4\\xC5\\xC6\\xC7\\xC8\\xC9\\xCA\\xCB\\xCC\\xCD\\xCE\\xCF\\xD0\\xD1\\xD2\\xD3\\xD4\\xD5\\xD6\\xD7\\xD8\\xD9\\xDA\\xDB\\xDC\\xDD\\xDE\\xDF\\xE0\\xE1\\xE2\\xE3\\xE4\\xE5\\xE6\\xE7\\xE8\\xE9\\xEA\\xEB\\xEC\\xED\\xEE\\xEF\\xF0\\xF1\\xF2\\xF3\\xF4\\xF5\\xF6\\xF7\\xF8\\xF9\\xFA\\xFB\\xFC\\xFD\\xFE\\xFF\"",
    " }",
    NULL
  };

static void test_escape_string (void)
{
  slaw  s   = make_string_example();
  unt32 flg = SLAW_SPEW_FLAG_REL_OFF | SLAW_SPEW_FLAG_ESCAPE_STRINGS;
  char *str = accum_spew_str (s, flg, NULL);

  diff_lines (str, escape_string_expected_lines);

  free (str);
  slaw_free (s);
}

static const char *const noescape_string_expected_lines[] =
  {
    "slaw[41o.0x000]: LIST (2 elems): {",
    " 1: slaw[7o.0x008]: STR(44): \"If you like pi\303\261a coladas",
    "Come with me and \033\"",
    " 2: slaw[33o.0x040]: STR(255): \"\001\002\003\004\005\006\007\010\011",
    "\013\014\015\016\017\020\021\022\023\024\025\026\027\030\031\032\033\034\035\036\037 !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\177\200\201\202\203\204\205\206\207\210\211\212\213\214\215\216\217\220\221\222\223\224\225\226\227\230\231\232\233\234\235\236\237\240\241\242\243\244\245\246\247\250\251\252\253\254\255\256\257\260\261\262\263\264\265\266\267\270\271\272\273\274\275\276\277\300\301\302\303\304\305\306\307\310\311\312\313\314\315\316\317\320\321\322\323\324\325\326\327\330\331\332\333\334\335\336\337\340\341\342\343\344\345\346\347\350\351\352\353\354\355\356\357\360\361\362\363\364\365\366\367\370\371\372\373\374\375\376\377\"",
    " }",
    NULL
  };

static void test_noescape_string (void)
{
  slaw  s   = make_string_example();
  unt32 flg = SLAW_SPEW_FLAG_REL_OFF;
  char *str = accum_spew_str (s, flg, NULL);

  diff_lines (str, noescape_string_expected_lines);

  free (str);
  slaw_free (s);
}

static int         n_passed    = 0;
static const char *start_green = "";
static const char *stop_green  = "";

static void print_msg (const char *txt1, const char *txt2)
{
  if (txt2)
    printf ("%s[%s]%s %s\n", start_green, txt1, stop_green, txt2);
  else
    printf ("%s[%s]%s\n", start_green, txt1, stop_green);
}

static void run_test (const char *name, void (*func)(void))
{
  print_msg (" RUN      ", name);
  func();
  /* tests will exit on failure, so if we reach this point, it passed */
  n_passed++;
  print_msg ("       OK ", name);
}

static void setup (void)
{
  const char *nocolor = getenv ("NO_COLOR");

  if ((nocolor == NULL || nocolor[0] == 0) && STDOUT_IS_A_TTY)
    {
      start_green = "\033[1m\033[38;5;46m";
      stop_green  = "\033[0m";
    }

  print_msg ("==========", NULL);
}

static void summary (void)
{
  char buf[80];
  snprintf (buf, sizeof(buf), "%d tests.", n_passed);
  print_msg ("==========", NULL);
  print_msg ("  PASSED  ", buf);
}

#define T(x) run_test(#x, x)

int main (int argc, char **argv)
{
  OB_DIE_ON_ERROR (OB_CHECK_ABI ());

  setup();

  T(test_false_null_matches_non_ex);
  T(test_relative_differs_from_absolute);
  T(test_relative_offset_is_zero_for_top_level);
  T(test_prolo_on_single_slaw);
  T(test_prolo_on_list);
  T(test_null_slaw);
  T(test_null_slaw_with_prolo);
  T(test_relative_offset);
  T(test_rude_ascii);
  T(test_rude_noascii);
  T(test_escape_string);
  T(test_noescape_string);

  summary();

  return EXIT_SUCCESS;
}
