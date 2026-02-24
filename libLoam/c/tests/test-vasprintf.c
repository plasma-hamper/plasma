
/* (c)  oblong industries */

/* On UNIX, this is kind of silly, because it is just a test of the standard
 * library functions, not Oblong code.  However, on Windows, we have to
 * provide our own implementations of snprintf, vsnprintf, asprintf, and
 * vasprintf, since on Windows these functions are either missing or have
 * incorrect semantics.  (One can forgive the absence of the asprintf-family
 * functions, since they are not standard, but snprintf is standardized by
 * C99, and 1999 was 11 years ago.)
 *
 * Anyway, this test verifies that these functions have the same behavior
 * on all platforms, including the versions from ob-sys-win32.c on Windows.
 */

// Oh, vasprintf, come out and play on Linux!
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "libLoam/c/ob-sys.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-util.h"

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

static int test_vsnprintf (char *buf, size_t len, const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
  int ret = vsnprintf (buf, len, fmt, ap);
  va_end (ap);
  return ret;
}

static void do1_snprintf_test (const char *str, char *buf, int len,
                               int (*snp) (char *buf, size_t len,
                                           const char *fmt, ...))
{
  const int slen = strlen (str);

  int ret = snp (buf, len, "%s", str);
  if (ret != slen)
    error_exit ("For '%s' with %d buffer, got %d but expected %d\n", str, len,
                ret, slen);

  if (len > slen)
    {
      if (0 != strcmp (str, buf))
        error_exit ("expected '%s' but got '%s'\n", str, buf);
    }
  else if (len > 0)
    {
      if (buf[len - 1] != 0)
        error_exit ("expected buffer to be NUL-terminated, but it wasn't\n");
      if (0 != strncmp (str, buf, len - 1))
        error_exit ("expected first %d characters of '%s' but got '%s'\n",
                    len - 1, str, buf);
    }

  memset (buf, 'X', len);
  ret = snp (buf, len, str);
  if (ret != slen)
    error_exit ("For '%s' with %d buffer, got %d but expected %d\n", str, len,
                ret, slen);

  if (len > slen)
    {
      if (0 != strcmp (str, buf))
        error_exit ("expected '%s' but got '%s'\n", str, buf);
    }
  else if (len > 0)
    {
      if (buf[len - 1] != 0)
        error_exit ("expected buffer to be NUL-terminated, but it wasn't\n");
      if (0 != strncmp (str, buf, len - 1))
        error_exit ("expected first %d characters of '%s' but got '%s'\n",
                    len - 1, str, buf);
    }
}

#define TEST(s) do1_snprintf_test (s, buf, len, snp)

static void do_snprintf_test (int len, int (*snp) (char *buf, size_t len,
                                                   const char *fmt, ...))
{
  char *buf = (char *) (len ? malloc (len) : NULL);

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

  free (buf);
}

#undef TEST

static void do1_asprintf_test (const char *str)
{
  const int slen = strlen (str);
  char *a, *b, *c;

  int ret1 = asprintf (&a, "%-*s", slen, str);
  int ret2 = asprintf (&b, "%s", str);
  int ret3 = 0;
  if (slen > 0)
    ret3 = asprintf (&c, "%c%s", str[0], str + 1);
  else
    c = strdup ("");

  if (ret1 != slen || ret2 != slen || ret3 != slen)
    error_exit ("Expected %d, but got %d, %d, and %d\n", slen, ret1, ret2,
                ret3);

  if (0 != strcmp (str, a))
    error_exit ("Expected '%s' but got '%s'\n", str, a);

  if (0 != strcmp (str, b))
    error_exit ("Expected '%s' but got '%s'\n", str, b);

  if (0 != strcmp (str, c))
    error_exit ("Expected '%s' but got '%s'\n", str, c);

  free (a);
  free (b);
  free (c);
}

#define TEST(s) do1_asprintf_test (s)

static void do_asprintf_test (void)
{
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
}

void do_test (int n, const char *expected, const char *fmt, ...)
{
  va_list ap;
  char *a;
  char big[400];
  int little_len = 1 + n * 2;
  char *little = (char *) malloc (little_len);

  va_start (ap, fmt);
  int ret = vasprintf (&a, fmt, ap);
  va_end (ap);
  if (ret != strlen (expected))
    error_exit ("got %d but expected %" OB_FMT_SIZE "d\n", ret,
                strlen (expected));

  if (0 != strcmp (expected, a))
    error_exit ("got '%s' but expected '%s'\n", a, expected);

  free (a);

  va_start (ap, fmt);
  ret = vsnprintf (little, little_len, fmt, ap);
  va_end (ap);
  if (ret != strlen (expected))
    error_exit ("got %d but expected %" OB_FMT_SIZE "d\n", ret,
                strlen (expected));

  if (little[little_len - 1] != 0)
    error_exit ("For length %d, not terminated\n", little_len);

  if (0 != strncmp (little, expected, little_len - 1))
    error_exit ("Expected first %d of '%s', but got '%s'\n", little_len - 1,
                expected, little);

  va_start (ap, fmt);
  ret = vsnprintf (big, sizeof (big), fmt, ap);
  va_end (ap);
  if (ret != strlen (expected))
    error_exit ("got %d but expected %" OB_FMT_SIZE "d\n", ret,
                strlen (expected));

  if (0 != strcmp (expected, big))
    error_exit ("got '%s' but expected '%s'\n", big, expected);

  free (little);
}

void do_strftime_test (void)
{
  struct tm stm;
  char buf[80];

  OB_CLEAR (stm);
  stm.tm_sec = 45;    /* seconds */
  stm.tm_min = 23;    /* minutes */
  stm.tm_hour = 1;    /* hours */
  stm.tm_mday = 4;    /* day of the month */
  stm.tm_mon = 6;     /* month (Jaunary=0) */
  stm.tm_year = -124; /* year */

  strftime (buf, sizeof (buf),
            "%b " OB_STRFTIME_DAY_OF_MONTH_NO_LEADING_ZERO ", %Y %H:%M:%S",
            &stm);

  // We may get a varying number of spaces, but we shouldn't get "Jul 04".
  const char *expected1 = "Jul  4, 1776 01:23:45"; /* UNIX - %e */
  const char *expected2 = "Jul 4, 1776 01:23:45";  /* Windows - %#d */
  if (0 != strcmp (expected1, buf) && 0 != strcmp (expected2, buf))
    error_exit ("expected '%s' or '%s' but got '%s'\n", expected1, expected2,
                buf);
}

int main (int argc, char **argv)
{
  int i, j;

  for (j = 0; j < 4; j++)
    {
      for (i = 0; i < 64; i++)
        {
          do_snprintf_test (i, snprintf);
          do_snprintf_test (i, test_vsnprintf);
        }

      do_test (j, "string theory, hex DEADBEEF, negative -1000000000000000000",
               "string %s, hex %X, negative %" OB_FMT_64 "d", "theory",
               0xdeadbeef, OB_CONST_I64 (-1000000000000000000));

      do_test (j,
               "This is a truly evil string, because it has a gr8 number of "
               "arguments 2 format, so if there R problems with not having "
               "va_copy on Windows, we should hopefully C them!",
               "%s is %x truly %c%c%c%c string, because it has %x%c"
               "gr%d number of arguments %d %s, so if there %c "
               "problems with not having %s on %s, we should %s "
               "%" OB_FMT_64 "X them%c",
               "This", 10, 'e', 'v', 'i', 'l', 10, ' ', 8, 2, "format", 'R',
               "va_copy", "Windows", "hopefully", OB_CONST_I64 (12), '!');

      do_test (j,
               "Parser error: while parsing a block mapping at "
               "line 9259457, column 5\n"
               "did not find expected key at line 9260367, column 5\n",
               "Parser error: %s at line %" OB_FMT_SIZE
               "d, column %" OB_FMT_SIZE "d\n"
               "%s at line %" OB_FMT_SIZE "d, column %" OB_FMT_SIZE "d\n",
               "while parsing a block mapping", (size_t) 9259457, (size_t) 5,
               "did not find expected key", (size_t) 9260367, (size_t) 5);

      const char *expected =
        "This is a truly evil string, because it has a gr8 number of "
        "arguments 2 format, so if there R problems with not having "
        "va_copy on Windows, we should hopefully C them!";

      char *a;
      int ret =
        asprintf (&a,
                  "%s is %x truly %c%c%c%c string, because it has %x%c"
                  "gr%d number of arguments %d %s, so if there %c "
                  "problems with not having %s on %s, we should %s "
                  "%" OB_FMT_64 "X them%c",
                  "This", 10, 'e', 'v', 'i', 'l', 10, ' ', 8, 2, "format", 'R',
                  "va_copy", "Windows", "hopefully", OB_CONST_I64 (12), '!');
      if (ret != strlen (expected))
        error_exit ("got %d but expected %" OB_FMT_SIZE "d\n", ret,
                    strlen (expected));

      do_asprintf_test ();

      if (0 != strcmp (expected, a))
        error_exit ("got '%s' but expected '%s'\n", a, expected);

      free (a);

      // Well, yeah strftime is a different function entirely, but
      // it is a formatting function, and it didn't seem worth
      // putting in its own file.  So let's test it here.
      do_strftime_test ();
    }

  return EXIT_SUCCESS;
}
