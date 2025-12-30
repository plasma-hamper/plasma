
/* (c)  oblong industries */

// A Google test for all things libLoam/c.
// (Creating new files is a pain, especially with Windows, so let's
// just stick all the new tests in here-- Google Test provides enough
// separation.)

#include <gtest/gtest.h>
#include "libLoam/c/ob-math.h"
#include "libLoam/c/ob-util.h"
#include "libLoam/c/ob-string.h"
#include "libLoam/c/ob-hash.h"
#include "libLoam/c/ob-file.h"
#include "libLoam/c/ob-dirs.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-thread.h"
#include "libLoam/c/ob-time.h"
#include "libLoam/c/ob-rand.h"
#include "libLoam/c/ob-sys.h"

TEST (TestString, Glob)
{
  EXPECT_TRUE (ob_match_glob ("slaw.c", "*.[Cch]"));
  EXPECT_TRUE (ob_match_glob ("slaw.h", "*.[Cch]"));
  EXPECT_FALSE (ob_match_glob ("slaw.o", "*.[Cch]"));
  EXPECT_TRUE (ob_match_glob ("Death", "Death*"));
  EXPECT_TRUE (ob_match_glob ("Death Valley", "Death*"));
  EXPECT_TRUE (ob_match_glob ("Napoleonic Wars", "*Wars"));
  EXPECT_FALSE (ob_match_glob ("culture wars", "*Wars"));
  EXPECT_TRUE (ob_match_glob ("abc", "a?c"));
  EXPECT_FALSE (ob_match_glob ("ac", "a?c"));
  EXPECT_TRUE (ob_match_glob ("ac", "a*c"));
  EXPECT_FALSE (ob_match_glob ("ac/dc", "a?c"));
  EXPECT_TRUE (ob_match_glob ("ac/dc", "a*c"));
  EXPECT_TRUE (ob_match_glob ("", "*"));
  EXPECT_FALSE (ob_match_glob ("", "?"));
  EXPECT_TRUE (ob_match_glob ("?", "?"));
  EXPECT_TRUE (ob_match_glob ("?", "\\?"));
  EXPECT_FALSE (ob_match_glob ("X", "\\?"));
  EXPECT_TRUE (ob_match_glob ("\\", "\\\\"));
  EXPECT_TRUE (ob_match_glob ("abc", "a[^X]c"));
  EXPECT_FALSE (ob_match_glob ("aXc", "a[^X]c"));
  EXPECT_FALSE (ob_match_glob ("aXc", "a[0-9]c"));
  EXPECT_TRUE (ob_match_glob ("a1c", "a[0-9]c"));
  EXPECT_TRUE (ob_match_glob ("*", "\\*"));
  EXPECT_FALSE (ob_match_glob ("\\", "\\*"));
  EXPECT_TRUE (ob_match_glob ("[", "\\["));
  EXPECT_TRUE (ob_match_glob ("n", "\\n"));
  EXPECT_FALSE (ob_match_glob ("\\X", "\\X"));
}

TEST (TestString, GlobIllegals)
{
  // I would consider these patterns to be illegal, but since
  // ob_match_glob() doesn't return an error code, all we really
  // expect is that it not crash.  These cases document the
  // current behavior-- a pattern that ends in the middle of a
  // character set is always a non-match, and a pattern that ends
  // in the middle of a backslash escape is treated as ending in
  // a literal backslash.
  EXPECT_FALSE (ob_match_glob ("", "["));
  EXPECT_FALSE (ob_match_glob ("[", "["));
  EXPECT_FALSE (ob_match_glob ("foo", "["));
  EXPECT_FALSE (ob_match_glob ("", "[^"));
  EXPECT_FALSE (ob_match_glob ("[", "[^"));
  EXPECT_FALSE (ob_match_glob ("foo", "[^"));
  EXPECT_FALSE (ob_match_glob ("", "[a-z"));
  EXPECT_FALSE (ob_match_glob ("[", "[a-z"));
  EXPECT_FALSE (ob_match_glob ("foo", "[a-z"));
  EXPECT_FALSE (ob_match_glob ("", "[a-"));
  EXPECT_FALSE (ob_match_glob ("[", "[a-"));
  EXPECT_FALSE (ob_match_glob ("foo", "[a-"));
  EXPECT_FALSE (ob_match_glob ("", "\\"));
  EXPECT_FALSE (ob_match_glob ("X", "\\"));
  EXPECT_TRUE (ob_match_glob ("\\", "\\"));
  EXPECT_TRUE (ob_match_glob ("z\\", "z\\"));
  EXPECT_FALSE (ob_match_glob ("\\X", "\\"));

  /* An out-of-bounds access was detected with Address Sanitizer:
   * https://lists.oblong.com/pipermail/buildtools/2013-April/001123.html
   * Since Address Sanitizer bounds-checks static data like strings, but
   * valgrind doesn't, I wanted to find a way to reproduce the problem
   * with valgrind.  Since valgrind does bounds-check malloced memory,
   * I make a malloced copy of "[^" in order to demonstrate the out-of-bounds
   * access with valgrind. */
  char *mal = strdup ("[^");
  EXPECT_FALSE (ob_match_glob ("", mal));
  EXPECT_FALSE (ob_match_glob ("[", mal));
  EXPECT_FALSE (ob_match_glob ("foo", mal));
  free (mal);
}

TEST (TestHash, Driver5)
{
  unt32 b, c;

  b = 0, c = 0, ob_jenkins_hash2 ("", 0, &c, &b);
  EXPECT_EQ (0xdeadbeefU, c);
  EXPECT_EQ (0xdeadbeefU, b);

  b = 0xdeadbeef, c = 0, ob_jenkins_hash2 ("", 0, &c, &b);
  EXPECT_EQ (0xbd5b7ddeU, c);
  EXPECT_EQ (0xdeadbeefU, b);

  b = 0xdeadbeef, c = 0xdeadbeef, ob_jenkins_hash2 ("", 0, &c, &b);
  EXPECT_EQ (0x9c093ccdU, c);
  EXPECT_EQ (0xbd5b7ddeU, b);

  b = 0, c = 0, ob_jenkins_hash2 ("Four score and seven years ago", 30, &c, &b);
  EXPECT_EQ (0x17770551U, c);
  EXPECT_EQ (0xce7226e6U, b);

  b = 1, c = 0, ob_jenkins_hash2 ("Four score and seven years ago", 30, &c, &b);
  EXPECT_EQ (0xe3607caeU, c);
  EXPECT_EQ (0xbd371de4U, b);

  b = 0, c = 1, ob_jenkins_hash2 ("Four score and seven years ago", 30, &c, &b);
  EXPECT_EQ (0xcd628161U, c);
  EXPECT_EQ (0x6cbea4b3U, b);

  c = ob_jenkins_hash ("Four score and seven years ago", 30, 0);
  EXPECT_EQ (0x17770551U, c);

  c = ob_jenkins_hash ("Four score and seven years ago", 30, 1);
  EXPECT_EQ (0xcd628161U, c);
}

TEST (TestHash, Driver5City)
{
  unt32 b, c;
  unt64 d;

  b = 0, c = 0, d = ob_city_hash64_with_seeds ("", 0, c, b);
  EXPECT_EQ (OB_CONST_U64 (6665653827947065942), d);

  b = 0xdeadbeef, c = 0, d = ob_city_hash64_with_seeds ("", 0, c, b);
  EXPECT_EQ (OB_CONST_U64 (1672851438643332704), d);

  b = 0xdeadbeef, c = 0xdeadbeef, d = ob_city_hash64_with_seeds ("", 0, c, b);
  EXPECT_EQ (OB_CONST_U64 (3378629863966295516), d);

  b = 0, c = 0,
  d = ob_city_hash64_with_seeds ("Four score and seven years ago", 30, c, b);
  EXPECT_EQ (OB_CONST_U64 (7082357486021397415), d);

  b = 1, c = 0,
  d = ob_city_hash64_with_seeds ("Four score and seven years ago", 30, c, b);
  EXPECT_EQ (OB_CONST_U64 (13986154154733474857), d);

  b = 0, c = 1,
  d = ob_city_hash64_with_seeds ("Four score and seven years ago", 30, c, b);
  EXPECT_EQ (OB_CONST_U64 (11770465625188382926), d);

  d = ob_city_hash64 ("Four score and seven years ago", 30);
  EXPECT_EQ (OB_CONST_U64 (15085882761502754826), d);
}

#define MAXLEN 70
TEST (TestHash, Driver3)
{
  unt8 buf[MAXLEN + 20], *b;
  unt32 len;
  unt8 q[] =
    "This is the time for all good men to come to the aid of their country...";
  unt32 h;
  unt8 qq[] =
    "xThis is the time for all good men to come to the aid of their country...";
  unt32 i;
  unt8 qqq[] = "xxThis is the time for all good men to come to the aid of "
               "their country...";
  unt32 j;
  unt8 qqqq[] = "xxxThis is the time for all good men to come to the aid of "
                "their country...";
  unt32 ref, x, y;
  unt8 *p;

  p = q;
  const unt32 n1 = ob_jenkins_hash (p, sizeof (q) - 1, 13);
  const unt32 n2 = ob_jenkins_hash (p, sizeof (q) - 2, 13);
  const unt32 n3 = ob_jenkins_hash (p, sizeof (q) - 3, 13);
  const unt32 n4 = ob_jenkins_hash (p, sizeof (q) - 4, 13);
  const unt32 n5 = ob_jenkins_hash (p, sizeof (q) - 5, 13);
  const unt32 n6 = ob_jenkins_hash (p, sizeof (q) - 6, 13);
  const unt32 n7 = ob_jenkins_hash (p, sizeof (q) - 7, 13);
  const unt32 n8 = ob_jenkins_hash (p, sizeof (q) - 8, 13);
  const unt32 n9 = ob_jenkins_hash (p, sizeof (q) - 9, 13);
  const unt32 n10 = ob_jenkins_hash (p, sizeof (q) - 10, 13);
  const unt32 n11 = ob_jenkins_hash (p, sizeof (q) - 11, 13);
  const unt32 n12 = ob_jenkins_hash (p, sizeof (q) - 12, 13);
  p = &qq[1];
  EXPECT_EQ (n1, ob_jenkins_hash (p, sizeof (q) - 1, 13));
  EXPECT_EQ (n2, ob_jenkins_hash (p, sizeof (q) - 2, 13));
  EXPECT_EQ (n3, ob_jenkins_hash (p, sizeof (q) - 3, 13));
  EXPECT_EQ (n4, ob_jenkins_hash (p, sizeof (q) - 4, 13));
  EXPECT_EQ (n5, ob_jenkins_hash (p, sizeof (q) - 5, 13));
  EXPECT_EQ (n6, ob_jenkins_hash (p, sizeof (q) - 6, 13));
  EXPECT_EQ (n7, ob_jenkins_hash (p, sizeof (q) - 7, 13));
  EXPECT_EQ (n8, ob_jenkins_hash (p, sizeof (q) - 8, 13));
  EXPECT_EQ (n9, ob_jenkins_hash (p, sizeof (q) - 9, 13));
  EXPECT_EQ (n10, ob_jenkins_hash (p, sizeof (q) - 10, 13));
  EXPECT_EQ (n11, ob_jenkins_hash (p, sizeof (q) - 11, 13));
  EXPECT_EQ (n12, ob_jenkins_hash (p, sizeof (q) - 12, 13));
  p = &qqq[2];
  EXPECT_EQ (n1, ob_jenkins_hash (p, sizeof (q) - 1, 13));
  EXPECT_EQ (n2, ob_jenkins_hash (p, sizeof (q) - 2, 13));
  EXPECT_EQ (n3, ob_jenkins_hash (p, sizeof (q) - 3, 13));
  EXPECT_EQ (n4, ob_jenkins_hash (p, sizeof (q) - 4, 13));
  EXPECT_EQ (n5, ob_jenkins_hash (p, sizeof (q) - 5, 13));
  EXPECT_EQ (n6, ob_jenkins_hash (p, sizeof (q) - 6, 13));
  EXPECT_EQ (n7, ob_jenkins_hash (p, sizeof (q) - 7, 13));
  EXPECT_EQ (n8, ob_jenkins_hash (p, sizeof (q) - 8, 13));
  EXPECT_EQ (n9, ob_jenkins_hash (p, sizeof (q) - 9, 13));
  EXPECT_EQ (n10, ob_jenkins_hash (p, sizeof (q) - 10, 13));
  EXPECT_EQ (n11, ob_jenkins_hash (p, sizeof (q) - 11, 13));
  EXPECT_EQ (n12, ob_jenkins_hash (p, sizeof (q) - 12, 13));
  p = &qqqq[3];
  EXPECT_EQ (n1, ob_jenkins_hash (p, sizeof (q) - 1, 13));
  EXPECT_EQ (n2, ob_jenkins_hash (p, sizeof (q) - 2, 13));
  EXPECT_EQ (n3, ob_jenkins_hash (p, sizeof (q) - 3, 13));
  EXPECT_EQ (n4, ob_jenkins_hash (p, sizeof (q) - 4, 13));
  EXPECT_EQ (n5, ob_jenkins_hash (p, sizeof (q) - 5, 13));
  EXPECT_EQ (n6, ob_jenkins_hash (p, sizeof (q) - 6, 13));
  EXPECT_EQ (n7, ob_jenkins_hash (p, sizeof (q) - 7, 13));
  EXPECT_EQ (n8, ob_jenkins_hash (p, sizeof (q) - 8, 13));
  EXPECT_EQ (n9, ob_jenkins_hash (p, sizeof (q) - 9, 13));
  EXPECT_EQ (n10, ob_jenkins_hash (p, sizeof (q) - 10, 13));
  EXPECT_EQ (n11, ob_jenkins_hash (p, sizeof (q) - 11, 13));
  EXPECT_EQ (n12, ob_jenkins_hash (p, sizeof (q) - 12, 13));

  /* check that hashlittle2 and hashlittle produce the same results */
  i = 47;
  j = 0;
  ob_jenkins_hash2 (q, sizeof (q), &i, &j);
  EXPECT_EQ (i, ob_jenkins_hash (q, sizeof (q), 47));

  /* check hashlittle doesn't read before or after the ends of the string */
  for (h = 0, b = buf + 1; h < 8; ++h, ++b)
    {
      for (i = 0; i < MAXLEN; ++i)
        {
          len = i;
          for (j = 0; j < i; ++j)
            *(b + j) = 0;

          /* these should all be equal */
          ref = ob_jenkins_hash (b, len, (unt32) 1);
          *(b + i) = (unt8) ~0;
          *(b - 1) = (unt8) ~0;
          x = ob_jenkins_hash (b, len, (unt32) 1);
          y = ob_jenkins_hash (b, len, (unt32) 1);
          EXPECT_EQ (ref, x);
          EXPECT_EQ (ref, y);
        }
    }
}

TEST (TestHash, Driver3City)
{
  unt8 buf[MAXLEN + 20], *b, *p;
  size_t len, h, i, j;
  unt64 ref, x, y;
  unt8 q[] =
    "This is the time for all good men to come to the aid of their country...";
  unt8 qq[] =
    "xThis is the time for all good men to come to the aid of their country...";
  unt8 qqq[] = "xxThis is the time for all good men to come to the aid of "
               "their country...";
  unt8 qqqq[] = "xxxThis is the time for all good men to come to the aid of "
                "their country...";

  p = q;
  const unt64 n1 = ob_city_hash64 (p, sizeof (q) - 1);
  const unt64 n2 = ob_city_hash64 (p, sizeof (q) - 2);
  const unt64 n3 = ob_city_hash64 (p, sizeof (q) - 3);
  const unt64 n4 = ob_city_hash64 (p, sizeof (q) - 4);
  const unt64 n5 = ob_city_hash64 (p, sizeof (q) - 5);
  const unt64 n6 = ob_city_hash64 (p, sizeof (q) - 6);
  const unt64 n7 = ob_city_hash64 (p, sizeof (q) - 7);
  const unt64 n8 = ob_city_hash64 (p, sizeof (q) - 8);
  const unt64 n9 = ob_city_hash64 (p, sizeof (q) - 9);
  const unt64 n10 = ob_city_hash64 (p, sizeof (q) - 10);
  const unt64 n11 = ob_city_hash64 (p, sizeof (q) - 11);
  const unt64 n12 = ob_city_hash64 (p, sizeof (q) - 12);
  p = &qq[1];
  EXPECT_EQ (n1, ob_city_hash64 (p, sizeof (q) - 1));
  EXPECT_EQ (n2, ob_city_hash64 (p, sizeof (q) - 2));
  EXPECT_EQ (n3, ob_city_hash64 (p, sizeof (q) - 3));
  EXPECT_EQ (n4, ob_city_hash64 (p, sizeof (q) - 4));
  EXPECT_EQ (n5, ob_city_hash64 (p, sizeof (q) - 5));
  EXPECT_EQ (n6, ob_city_hash64 (p, sizeof (q) - 6));
  EXPECT_EQ (n7, ob_city_hash64 (p, sizeof (q) - 7));
  EXPECT_EQ (n8, ob_city_hash64 (p, sizeof (q) - 8));
  EXPECT_EQ (n9, ob_city_hash64 (p, sizeof (q) - 9));
  EXPECT_EQ (n10, ob_city_hash64 (p, sizeof (q) - 10));
  EXPECT_EQ (n11, ob_city_hash64 (p, sizeof (q) - 11));
  EXPECT_EQ (n12, ob_city_hash64 (p, sizeof (q) - 12));
  p = &qqq[2];
  EXPECT_EQ (n1, ob_city_hash64 (p, sizeof (q) - 1));
  EXPECT_EQ (n2, ob_city_hash64 (p, sizeof (q) - 2));
  EXPECT_EQ (n3, ob_city_hash64 (p, sizeof (q) - 3));
  EXPECT_EQ (n4, ob_city_hash64 (p, sizeof (q) - 4));
  EXPECT_EQ (n5, ob_city_hash64 (p, sizeof (q) - 5));
  EXPECT_EQ (n6, ob_city_hash64 (p, sizeof (q) - 6));
  EXPECT_EQ (n7, ob_city_hash64 (p, sizeof (q) - 7));
  EXPECT_EQ (n8, ob_city_hash64 (p, sizeof (q) - 8));
  EXPECT_EQ (n9, ob_city_hash64 (p, sizeof (q) - 9));
  EXPECT_EQ (n10, ob_city_hash64 (p, sizeof (q) - 10));
  EXPECT_EQ (n11, ob_city_hash64 (p, sizeof (q) - 11));
  EXPECT_EQ (n12, ob_city_hash64 (p, sizeof (q) - 12));
  p = &qqqq[3];
  EXPECT_EQ (n1, ob_city_hash64 (p, sizeof (q) - 1));
  EXPECT_EQ (n2, ob_city_hash64 (p, sizeof (q) - 2));
  EXPECT_EQ (n3, ob_city_hash64 (p, sizeof (q) - 3));
  EXPECT_EQ (n4, ob_city_hash64 (p, sizeof (q) - 4));
  EXPECT_EQ (n5, ob_city_hash64 (p, sizeof (q) - 5));
  EXPECT_EQ (n6, ob_city_hash64 (p, sizeof (q) - 6));
  EXPECT_EQ (n7, ob_city_hash64 (p, sizeof (q) - 7));
  EXPECT_EQ (n8, ob_city_hash64 (p, sizeof (q) - 8));
  EXPECT_EQ (n9, ob_city_hash64 (p, sizeof (q) - 9));
  EXPECT_EQ (n10, ob_city_hash64 (p, sizeof (q) - 10));
  EXPECT_EQ (n11, ob_city_hash64 (p, sizeof (q) - 11));
  EXPECT_EQ (n12, ob_city_hash64 (p, sizeof (q) - 12));

  /* check hash function doesn't read before or after the ends of the string */
  for (h = 0, b = buf + 1; h < 8; ++h, ++b)
    {
      for (i = 0; i < MAXLEN; ++i)
        {
          len = i;
          for (j = 0; j < i; ++j)
            *(b + j) = 0;

          /* these should all be equal */
          ref = ob_city_hash64 (b, len);
          *(b + i) = (unt8) ~0;
          *(b - 1) = (unt8) ~0;
          x = ob_city_hash64 (b, len);
          y = ob_city_hash64 (b, len);
          EXPECT_EQ (ref, x);
          EXPECT_EQ (ref, y);
        }
    }
}

TEST (TestHash, Malloc)
{
  unt32 x[10];
  for (int i = 0; i < 10; i++)
    {
      void *foo = calloc (1, i);
      x[i] = ob_jenkins_hash (foo, i, 0xbadf00d);
      free (foo);
    }
  EXPECT_EQ (0xea5baefcU, x[0]);
  EXPECT_EQ (0x0eeea4a8U, x[1]);
  EXPECT_EQ (0x59d5b205U, x[2]);
  EXPECT_EQ (0xe218495aU, x[3]);
  EXPECT_EQ (0xa60d89eaU, x[4]);
  EXPECT_EQ (0xf58c3500U, x[5]);
  EXPECT_EQ (0x5c5c6f4eU, x[6]);
  EXPECT_EQ (0x96d96c18U, x[7]);
  EXPECT_EQ (0x587702afU, x[8]);
  EXPECT_EQ (0x529a7285U, x[9]);
}

TEST (TestHash, AllBits)
{
  ob_rand_t *r = ob_rand_allocate_state (0xbadf00d);
  for (size_t len = 3; len <= 3000; len *= 10)
    {
      const unt64 zero = 0;
      const unt64 ones = ~zero;
      unt64 should_be_zero = ones;
      unt64 should_be_ones = zero;
      byte *data = (byte *) malloc (len);
      for (int i = 0; i < 100; i++)
        {
          ob_random_bytes_state (r, data, len);
          const unt64 hash = ob_city_hash64 (data, len);
          should_be_ones |= hash;
          should_be_zero &= hash;
        }
      free (data);
      EXPECT_EQ (zero, should_be_zero);
      EXPECT_EQ (ones, should_be_ones);
    }
  ob_rand_free_state (r);
}

TEST (TestHash, HashUnt64)
{
  EXPECT_EQ (OB_CONST_U64 (4791496246630595898),
             ob_hash_unt64 (OB_CONST_U64 (0xdefacedbadfacade)));
  EXPECT_EQ (OB_CONST_U64 (913836480423252886),
             ob_hash_unt64 (OB_CONST_U64 (0x1234567890abcdef)));
}

TEST (TestHash, HashUnt32)
{
  EXPECT_EQ (3451475126U, ob_hash_unt32 (250141383U));
  EXPECT_EQ (644598342U, ob_hash_unt32 (613992554U));
  EXPECT_EQ (3816608188U, ob_hash_unt32 (0x12345678U));
  EXPECT_EQ (233162409U, ob_hash_unt32 (0xdeadbeefU));
}

TEST (TestHash, HashSizeT)
{
  size_t e1, e2, e3, s1;
  if (sizeof (size_t) == 8)
    {
      e1 = (size_t) OB_CONST_U64 (5223249755948292319);
      e2 = (size_t) OB_CONST_U64 (8132467604113860101);
      e3 = (size_t) OB_CONST_U64 (856682781148973835);
      s1 = (size_t) OB_CONST_U64 (63042831503876386);
    }
  else
    {
      e1 = 1309046835U;
      e2 = 3264488965U;
      e3 = 2069967920U;
      s1 = 831905382U;
    }
  EXPECT_EQ (e1, ob_hash_size_t (s1));
  EXPECT_EQ (e2, ob_hash_unt64_to_size_t (OB_CONST_U64 (16564032121141038273)));
  EXPECT_EQ (e3, ob_hash_unt32_to_size_t (3251189968U));
}

TEST (TestHash, Combine64)
{
  EXPECT_EQ (OB_CONST_U64 (13272961317045887248),
             ob_hash_2xunt64_to_unt64 (OB_CONST_U64 (0xdefacedbadfacade),
                                       OB_CONST_U64 (0x1234567890abcdef)));
  EXPECT_EQ (OB_CONST_U64 (14967027099503488440),
             ob_hash_2xunt64_to_unt64 (OB_CONST_U64 (0xaaaabccddddeeeff),
                                       OB_CONST_U64 (0x2540407606304269)));
  EXPECT_EQ (OB_CONST_U64 (3542680201337472268),
             ob_hash_2xunt64_to_unt64 (OB_CONST_U64 (0x1132560032180434),
                                       OB_CONST_U64 (0xa78bfde098ccc56d)));
}

TEST (TestHash, CombineSizeT)
{
  size_t s1, s2, s3;

  s1 = (size_t) OB_CONST_U64 (5223249755948292319);
  s2 = (size_t) OB_CONST_U64 (8132467604113860101);
  s3 = (size_t) OB_CONST_U64 (856682781148973835);

  EXPECT_NE (ob_hash_2xsize_t_to_size_t (s1, s2),
             ob_hash_2xsize_t_to_size_t (s2, s1));
  EXPECT_NE (ob_hash_2xsize_t_to_size_t (s1, s2),
             ob_hash_2xsize_t_to_size_t (s1, s3));
  EXPECT_NE (ob_hash_2xsize_t_to_size_t (s1, s2),
             ob_hash_2xsize_t_to_size_t (s3, s2));
}

#ifndef _MSC_VER  // CLOEXEC not needed or supported on Windows
TEST (TestCloexec, Open)
{
  int fd = ob_open_cloexec ("/etc/passwd", O_RDONLY, 0);
  EXPECT_GE (fd, 0);
  EXPECT_EQ (FD_CLOEXEC, fcntl (fd, F_GETFD));
  EXPECT_EQ (0, close (fd));
}

TEST (TestCloexec, Fopen)
{
  FILE *f = ob_fopen_cloexec ("/etc/passwd", "r");
  EXPECT_TRUE (f != NULL);
  EXPECT_EQ (FD_CLOEXEC, fcntl (fileno (f), F_GETFD));
  EXPECT_EQ (0, fclose (f));
}

TEST (TestCloexec, Dup)
{
  int fd = ob_dup_cloexec (STDOUT_FILENO);
  EXPECT_GE (fd, 0);
  EXPECT_EQ (FD_CLOEXEC, fcntl (fd, F_GETFD));
  EXPECT_EQ (0, close (fd));
}

TEST (TestCloexec, Dup2)
{
  const int nfd = 123;  // hopefully not in use
  int fd = ob_dup2_cloexec (STDOUT_FILENO, nfd);
  EXPECT_EQ (nfd, fd) << "error was " << errno << " (" << strerror (errno)
                      << ")";
  if (fd >= 0)
    {
      EXPECT_EQ (FD_CLOEXEC, fcntl (fd, F_GETFD));
      EXPECT_EQ (0, close (fd));
    }
}

TEST (TestCloexec, Socketpair)
{
  int fd[2];
  EXPECT_EQ (OB_OK, ob_socketpair_cloexec (AF_UNIX, SOCK_STREAM, 0, fd));
  for (int i = 0; i < 2; i++)
    {
      EXPECT_EQ (FD_CLOEXEC, fcntl (fd[i], F_GETFD));
      EXPECT_EQ (0, close (fd[i]));
    }
}

TEST (TestCloexec, Pipe)
{
  int fd[2];
  EXPECT_EQ (OB_OK, ob_pipe_cloexec (fd));
  for (int i = 0; i < 2; i++)
    {
      EXPECT_EQ (FD_CLOEXEC, fcntl (fd[i], F_GETFD));
      EXPECT_EQ (0, close (fd[i]));
    }
}
#endif

TEST (TestTime, Monotonic)
{
  unt64 t1 = ob_monotonic_time ();
  unt64 t2 = ob_monotonic_time ();
  EXPECT_LE (t1, t2);
  // 50 milliseconds should be enough for time to advance
  ob_micro_sleep (50000);
  unt64 t3 = ob_monotonic_time ();
  EXPECT_GT (t3, t2);
  // let's say sleeping 50 milliseconds should take at least 10 milliseconds
  EXPECT_GT (t3 - t2, OB_CONST_U64 (10000000));
  // but let's assume sleeping 50 milliseconds didn't take more than 30 seconds
  EXPECT_LT (t3 - t2, OB_CONST_U64 (30000000000));
}

TEST (TestIgnore, TestIgnore)
{
  char uu[37];
  ob_ignore (ob_generate_uuid (uu));
  ob_ignore (write (2, "", 0), ob_current_time (), ob_monotonic_time ());
}

TEST (TestString, AppendFloat)
{
  char buf[100];
  buf[0] = 0;
  ob_safe_append_float64 (buf, sizeof (buf), 0, 3);
  EXPECT_STREQ ("0.000", buf);
  buf[0] = 0;
  ob_safe_append_float64 (buf, sizeof (buf), -1, 2);
  EXPECT_STREQ ("-1.00", buf);
  buf[0] = 0;
  ob_safe_append_float64 (buf, sizeof (buf), 1.5, 1);
  EXPECT_STREQ ("1.5", buf);
  buf[0] = 0;
  ob_safe_append_float64 (buf, sizeof (buf), 1.618033988749895, 5);
  EXPECT_STREQ ("1.61803", buf);
  buf[0] = 0;
  ob_safe_append_float64 (buf, sizeof (buf), 99.9, 1);
  EXPECT_STREQ ("99.9", buf);
}

TEST (TestDirs, MkdirRmdir)
{
  const char *long_path = "scratch/bomb.com without a comma/bomb.com, with a "
                          "comma/you don't exist, go away/configure.ac";
  const char *medium_path =
    "scratch/bomb.com without a comma/bomb.com, with a comma";
  const char *short_path = "scratch/bomb.com without a comma";
  EXPECT_EQ (OB_OK, ob_mkdir_p (long_path));
  EXPECT_EQ (0, access (long_path, R_OK));
  EXPECT_EQ (0, access (medium_path, R_OK));
  EXPECT_EQ (0, access (short_path, R_OK));
  EXPECT_EQ (OB_OK, ob_rmdir_p_ex (long_path, short_path));
  EXPECT_EQ (-1, access (long_path, R_OK));
  EXPECT_EQ (-1, access (medium_path, R_OK));
  EXPECT_EQ (0, access (short_path, R_OK));
  EXPECT_EQ (OB_OK, ob_rmdir_p (short_path));
  EXPECT_EQ (-1, access (long_path, R_OK));
  EXPECT_EQ (-1, access (medium_path, R_OK));
  EXPECT_EQ (-1, access (short_path, R_OK));
}

TEST (TestEnv, TestEnv)
{
  const char *name = "PSYCHOPATH";
  const char *value = "/home/nbates:/etc/shower.d";

  EXPECT_EQ (OB_OK, ob_setenv (name, value));
  EXPECT_STREQ (value, getenv (name));
  EXPECT_EQ (OB_OK, ob_unsetenv (name));
  EXPECT_TRUE (NULL == getenv (name));
}

TEST (Math, Round)
{
  EXPECT_DOUBLE_EQ (1.0, ob_round (0.5));
  EXPECT_DOUBLE_EQ (-1.0, ob_round (-0.5));
  EXPECT_DOUBLE_EQ (3.0, ob_round (3.14));
  EXPECT_DOUBLE_EQ (-3.0, ob_round (-3.14));
  EXPECT_DOUBLE_EQ (3.0, ob_round (2.71));
  EXPECT_DOUBLE_EQ (-3.0, ob_round (-2.71));
  EXPECT_DOUBLE_EQ (0.0, ob_round (0.1));
  EXPECT_DOUBLE_EQ (0.0, ob_round (-0.1));
  EXPECT_DOUBLE_EQ (0.0, ob_round (0.0));
}

TEST (Math, Trunc)
{
  EXPECT_DOUBLE_EQ (0.0, ob_trunc (0.5));
  EXPECT_DOUBLE_EQ (0.0, ob_trunc (-0.5));
  EXPECT_DOUBLE_EQ (3.0, ob_trunc (3.14));
  EXPECT_DOUBLE_EQ (-3.0, ob_trunc (-3.14));
  EXPECT_DOUBLE_EQ (2.0, ob_trunc (2.71));
  EXPECT_DOUBLE_EQ (-2.0, ob_trunc (-2.71));
  EXPECT_DOUBLE_EQ (0.0, ob_trunc (0.1));
  EXPECT_DOUBLE_EQ (0.0, ob_trunc (-0.1));
  EXPECT_DOUBLE_EQ (0.0, ob_trunc (0.0));
}

TEST (Math, Roundf)
{
  EXPECT_FLOAT_EQ (1.0, ob_roundf (0.5));
  EXPECT_FLOAT_EQ (-1.0, ob_roundf (-0.5));
  EXPECT_FLOAT_EQ (3.0, ob_roundf (3.14));
  EXPECT_FLOAT_EQ (-3.0, ob_roundf (-3.14));
  EXPECT_FLOAT_EQ (3.0, ob_roundf (2.71));
  EXPECT_FLOAT_EQ (-3.0, ob_roundf (-2.71));
  EXPECT_FLOAT_EQ (0.0, ob_roundf (0.1));
  EXPECT_FLOAT_EQ (0.0, ob_roundf (-0.1));
  EXPECT_FLOAT_EQ (0.0, ob_roundf (0.0));
}

TEST (Math, Truncf)
{
  EXPECT_FLOAT_EQ (0.0, ob_truncf (0.5));
  EXPECT_FLOAT_EQ (0.0, ob_truncf (-0.5));
  EXPECT_FLOAT_EQ (3.0, ob_truncf (3.14));
  EXPECT_FLOAT_EQ (-3.0, ob_truncf (-3.14));
  EXPECT_FLOAT_EQ (2.0, ob_truncf (2.71));
  EXPECT_FLOAT_EQ (-2.0, ob_truncf (-2.71));
  EXPECT_FLOAT_EQ (0.0, ob_truncf (0.1));
  EXPECT_FLOAT_EQ (0.0, ob_truncf (-0.1));
  EXPECT_FLOAT_EQ (0.0, ob_truncf (0.0));
}

static ob_static_mutex my_mutex = OB_STATIC_MUTEX_INITIALIZER;

static int numbers[] = {1975, 1993, 1997, 1999, 2001, 2010, 2013, 1, 3, -1};

typedef int (*cmp_func) (const void *a, const void *b);

static int comp1 (const void *a, const void *b)
{
  int x = *(const int *) a;
  int y = *(const int *) b;
  return x - y;
}

static int comp2 (const void *a, const void *b)
{
  int x = *(const int *) a;
  int y = *(const int *) b;
  return y - x;
}

static void *thread_main (void *arg)
{
  cmp_func cmp = reinterpret_cast<cmp_func> (arg);
  for (int i = 0; i != 250; i++)
    {
      OB_DIE_ON_ERROR (ob_lock_static_mutex (&my_mutex));
      qsort (numbers, sizeof (numbers) / sizeof (numbers[0]),
             sizeof (numbers[0]), cmp);
      OB_DIE_ON_ERROR (ob_unlock_static_mutex (&my_mutex));
    }
  return NULL;
}

/* We have two threads: one is sorting the numbers[] array in ascending
 * order, and the other is sorting the numbers[] array in descending order.
 * Presumably, if the mutual exclusion isn't working correctly, the array
 * will get corrupted.  We check that the array hasn't been corrupted at
 * the end by summing it.  (Which is a cheap way to check that it probably
 * contains the correct elements, without caring about order.)
 */
TEST (StaticMutex, StaticMutex)
{
  pthread_t t1, t2;
  cmp_func c1 = comp1;
  cmp_func c2 = comp2;
  EXPECT_EQ (
    0, pthread_create (&t1, NULL, thread_main, reinterpret_cast<void *> (c1)));
  EXPECT_EQ (
    0, pthread_create (&t2, NULL, thread_main, reinterpret_cast<void *> (c2)));
  EXPECT_EQ (0, pthread_join (t1, NULL));
  EXPECT_EQ (0, pthread_join (t2, NULL));
  int sum = 0;
  for (size_t i = 0; i != (sizeof (numbers) / sizeof (numbers[0])); i++)
    sum += numbers[i];
  EXPECT_EQ (13991, sum);
}
