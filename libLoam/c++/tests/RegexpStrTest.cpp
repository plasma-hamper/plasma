
/* (c)  oblong industries */

#include <gtest/gtest.h>
#include <libLoam/c++/Str.h>

#include "libLoam/c/ob-rand.h"
#include "libLoam/c/ob-util.h"

using namespace oblong::loam;

class RegexpStrTest1 : public ::testing::Test
{
 public:
  static void SetUpTestCase ()
  {
    // suppress message "U_REGEX_MISMATCHED_PAREN"
    OB_DIE_ON_ERROR (ob_suppress_message (OBLV_WARN, 0x11000005));
  }
};

TEST (RegexpStrTest, StatelessMatch)
{
  const Str s = "It's dead, Jim";

  EXPECT_TRUE (s.Matches ("dead"));
  EXPECT_TRUE (s.Matches ("Jim"));
  EXPECT_TRUE (s.Matches ("dea"));
  EXPECT_TRUE (s.Matches ("'"));
  EXPECT_TRUE (s.Matches (", "));
  EXPECT_TRUE (s.Matches (" [ade]+,"));
  EXPECT_TRUE (s.Matches ("x*"));
  EXPECT_FALSE (s.Matches ("' "));
  EXPECT_FALSE (s.Matches ("jim"));
  EXPECT_FALSE (s.Matches ("it"));
  EXPECT_FALSE (s.Matches ("dead Jim"));
  EXPECT_FALSE (s.Matches (" [ade]+ "));
  EXPECT_FALSE (s.Matches ("x+"));

  const Str t =
    "A-si-wi-jo and A-swi-jo for Aswios "
    "(\341\274\214\317\203\316\271\316\277\317\202). Similarly, "
    "\360\220\201\210 (rya), \360\220\201\212 (ryo) and "
    "\360\220\201\213 (tya) begin with palatalized consonants rather than "
    "two consonants: -ti-ri-ja or -ti-rya for -trya "
    "(-\317\204\317\201\316\271\316\261).";

  EXPECT_TRUE (t.Matches ("A.*\\."));
  EXPECT_TRUE (t.Matches ("\\([-\316\261\316\271\317\201\317\204]+\\)"));
  EXPECT_TRUE (
    t.Matches ("[\360\220\201\210\360\220\201\212\360\220\201\213]+"));
  EXPECT_TRUE (t.Matches ("\360\220\201\210[ ,ary()]+\360\220\201\212"));
  EXPECT_TRUE (t.Matches (Str (0x10048)));
  EXPECT_TRUE (t.Matches (Str (0x1004a)));
  EXPECT_TRUE (t.Matches (Str (0x1004b)));
  EXPECT_TRUE (t.Matches ("\341\274\214\317\203\316\271\316\277\317\202"));
  EXPECT_FALSE (t.Matches ("\360\220\201\210[ ary()]+\360\220\201\212"));
  EXPECT_FALSE (t.Matches (Str (0x10049)));
  EXPECT_FALSE (t.Matches ("dead"));
  EXPECT_FALSE (t.Matches ("Jim"));
  EXPECT_FALSE (t.Matches ("aswios"));
  EXPECT_FALSE (t.Matches ("  "));
  EXPECT_FALSE (t.Matches (s));
  EXPECT_FALSE (s.Matches (t));
}

TEST (RegexpStrTest, SimpleMatch)
{
  Str str = "foo bar bash";
  EXPECT_TRUE (str.Match ("bar"));

  Str substr = str.MatchSlice ();
  EXPECT_STREQ ("bar", substr);

  EXPECT_STREQ ("foo ", str.MatchPreSlice ());
  EXPECT_STREQ (" bash", str.MatchPostSlice ());
}

TEST (RegexpStrTest, CaptureGroups)
{
  Str str = "foo bar bash";
  EXPECT_TRUE (str.Match ("(\\w+) (\\w+) (\\w+)"));
  EXPECT_EQ (3, str.MatchNumCaptures ());
  EXPECT_STREQ ("foo", str.MatchNthCaptureSlice (1));
  EXPECT_STREQ ("bar", str.MatchNthCaptureSlice (2));
  EXPECT_STREQ ("bash", str.MatchNthCaptureSlice (3));
}

TEST (RegexpStrTest, SuccessiveMatches)
{
  Str str = "foo bar bash";
  EXPECT_TRUE (str.Match ("(\\w+)"));
  EXPECT_STREQ ("foo", str.MatchSlice ());

  EXPECT_TRUE (str.MatchAgain ());
  EXPECT_STREQ ("bar", str.MatchSlice ());

  EXPECT_TRUE (str.MatchAgain ());
  EXPECT_STREQ ("bash", str.MatchSlice ());

  EXPECT_FALSE (str.MatchAgain ());

  Str cpy = str;
  EXPECT_FALSE (cpy.MatchAgain ());
}

TEST_F (RegexpStrTest1, IllegalPattern)
{
  Str str = "foo bar bash";
  EXPECT_FALSE (str.Match ("foo)"));
  EXPECT_STREQ ("U_REGEX_MISMATCHED_PAREN", str.MatchErrorStr ());
}

TEST (RegexpStrTest, EmpyMatchData)
{
  Str str = "something";
  EXPECT_FALSE (str.Match ());
  EXPECT_FALSE (str.MatchHasMatched ());
  EXPECT_EQ (Str (""), str.MatchSlice ());
  EXPECT_EQ (Str (""), str.MatchPreSlice ());
  EXPECT_EQ (Str (""), str.MatchPostSlice ());
}

TEST (RegexpStrTest, MatchReplacement)
{
  Str str = "one two three";
  str.ReplaceAll ("two", "golly");
  EXPECT_FALSE (str.MatchHasErred ()) << str.MatchErrorStr ();
  EXPECT_STREQ ("one golly three", str);
  EXPECT_FALSE (str.MatchHasMatched ());

  str = "one two three";
  str.ReplaceAll ("(\\w+) (\\w+) (\\w+)", "$1 -- $2 -- $3");
  EXPECT_FALSE (str.MatchHasErred ()) << str.MatchErrorStr ();
  EXPECT_STREQ ("one -- two -- three", str);

  str = "one two three";
  str.ReplaceAll ("(\\w+)", "-$1-");
  EXPECT_FALSE (str.MatchHasErred ()) << str.MatchErrorStr ();
  EXPECT_STREQ ("-one- -two- -three-", str);
}

TEST (RegexpStrTest, CopyAndMatch)
{
  Str str = "foo bar bash";
  EXPECT_TRUE (str.Match ("(\\w+)"));
  EXPECT_STREQ ("foo", str.MatchSlice ());

  Str str2 = str;
  EXPECT_TRUE (str.MatchAgain ());
  EXPECT_STREQ ("bar", str.MatchSlice ());

  EXPECT_TRUE (str2.MatchAgain ());
  EXPECT_STREQ ("bar", str.MatchSlice ());
}

TEST (RegexpStrTest, CopyAndRunoutMatch)
{
  Str str = "foo bar bash";
  str.Match ("(\\w+)");
  str.MatchAgain ();
  str.MatchAgain ();
  Str str2 = str;
  EXPECT_FALSE (str.MatchAgain ());
  EXPECT_FALSE (str2.MatchAgain ());
  EXPECT_FALSE (str.MatchAgain ());
  EXPECT_FALSE (str2.MatchAgain ());
}

TEST_F (RegexpStrTest1, CopyAndIllegalPattern)
{
  Str str = "foo bar bash";
  EXPECT_FALSE (str.Match ("foo)"));
  EXPECT_STREQ ("U_REGEX_MISMATCHED_PAREN", str.MatchErrorStr ());

  Str str2 = str;
  EXPECT_FALSE (str.Match ("foo)"));
}

TEST_F (RegexpStrTest1, MatchAgainAfterError)
{
  Str str = "foo bar bash";
  EXPECT_FALSE (str.Match ("foo)"));
  EXPECT_FALSE (str.MatchAgain ());
}

TEST (RegexpStrTest, MatchButShouldClearWithAppend)
{
  Str a = "world";
  a.Match ("d$");
  Str s;
  s = a + "hello";
  // the streq here isn't really the point. our assert should fail if
  // we have the bug that we always used to in StrMatchData.C's copy
  // utility function. ("bug in StrMatchData copy conceit")
  EXPECT_STREQ ("worldhello", s);
}

TEST_F (RegexpStrTest1, CopyMatchError)
{
  Str str = "foo bar bash";
  EXPECT_FALSE (str.Match ("foo)"));
  Str conceit (str);
  EXPECT_STREQ ("U_REGEX_MISMATCHED_PAREN", str.MatchErrorStr ());
  EXPECT_STREQ ("U_REGEX_MISMATCHED_PAREN", conceit.MatchErrorStr ());
}

TEST_F (RegexpStrTest1, Bug1257)
{
  Str *strs[2];
  strs[0] = new Str ("I reject your reality, and substitute my own");
  EXPECT_FALSE (strs[0]->Match ("([0-9])[A-Z])"));
  strs[1] = new Str (*strs[0]);
  for (int j = 0; j < 2; j++)
    EXPECT_FALSE (strs[j]->MatchAgain ());
  for (int j = 0; j < 2; j++)
    EXPECT_STREQ ("", strs[j]->MatchNthCaptureSlice (3));
  for (int i = 0; i < 2; i++)
    strs[i]->Delete ();
}

TEST (RegexpStrTest, RepeatedSliceFailure)
{
  Str *str = new Str (", ate a radio for science!");
  EXPECT_TRUE (str->Match ("(,+) "));
  EXPECT_STREQ ("", str->MatchNthCaptureSlice (-1));
  EXPECT_STREQ ("", str->MatchNthCaptureSlice (-2));
  str->Delete ();
}

TEST (RegexpStrTest, SliceSuccessAfterFailure)
{
  Str *str = new Str (",,,, ,,,");
  EXPECT_TRUE (str->Match ("(,+) "));
  EXPECT_STREQ ("", str->MatchNthCaptureSlice (-1));
  EXPECT_STREQ (",,,,", str->MatchNthCaptureSlice (1));
  str->Delete ();
}

TEST (RegexpStrTest, ReplaceFirst)
{
  Str s1 ("I'll wear a porcupine on my head on a WHIM.");
  Str s2 (s1);
  s1.ReplaceAll ("\\ba\\b", "the");
  s2.ReplaceFirst ("\\ba\\b", "the");
  EXPECT_STREQ ("I'll wear the porcupine on my head on the WHIM.", s1);
  EXPECT_STREQ ("I'll wear the porcupine on my head on a WHIM.", s2);
}

TEST (RegexpStrTest, MatchRange)
{
  Str s1 ("\360\235\224\270food");
  EXPECT_TRUE (s1.Match ("foo"));
  StrRange r = s1.MatchRange ();
  EXPECT_EQ (1, r.idx);
  EXPECT_EQ (3, r.len);
}

TEST (RegexpStrTest, MatchNthCaptureRange)
{
  Str s1 ("bar f\360\235\224\270o!!!o");
  EXPECT_TRUE (s1.Match ("f(.+)o(.+)o"));
  EXPECT_EQ (2, s1.MatchNumCaptures ());
  StrRange r1 = s1.MatchNthCaptureRange (1);
  StrRange r2 = s1.MatchNthCaptureRange (2);
  EXPECT_EQ (5, r1.idx);
  EXPECT_EQ (1, r1.len);
  EXPECT_EQ (7, r2.idx);
  EXPECT_EQ (3, r2.len);
}

static void upcase_matches (Str &s)
{
  StrRange r = s.MatchRange ();
  if (s.MatchAgain ())
    upcase_matches (s);
  s.Replace (r.idx, r.len, s.Slice (r.idx, r.len).Upcase ());
}

TEST (RegexpStrTest, ComputedReplace)
{
  Str c1 (0x149);
  Str c2 (0x1D543);
  Str s = Str::Format (
    "DwCDUZmdct XgVlUI%sdeO zHVPlmG%swy HubGdlLjVN XcgLduj%sUe yyUTesYmrw",
    c1.utf8 (), c2.utf8 (), c1.utf8 ());
  ASSERT_TRUE (s.Match ("\\bX[^ ]+"));
  upcase_matches (s);
  const Str expected ("DwCDUZmdct XGVLUI\312\274NDEO zHVPlmG\360\235\225\203wy"
                      " HubGdlLjVN XCGLDUJ\312\274NUE yyUTesYmrw");
  EXPECT_STREQ (expected, s);
}

static OB_UNUSED void printfor (int b)
{
  if (b > 1)
    printf ("for (int j = %d  ;  j < %d  ;  j++)\n  ", 0, b);
}

/**
 * Return true with 2/3 probability.
 * We use this to call "const" methods on only some of the strings
 * but not others, to make sure the behavior is still the same in
 * the future, whether we called the const method or not.
 */
static OB_UNUSED bool probably (ob_rand_t *r)
{
  return (0 == ob_rand_state_int32 (0, 3, r));
}

// These can be changed to printfor() and printf() to generate a test case.
#define PR4(n)
#define PRF(...)

static UChar32 random_char (ob_rand_t *r)
{
  UChar32 c = ob_rand_state_int32 (0x20, 0x7f, r);
#if 0  // well, these funny characters don't work so well
  if (c >= 0x100)
    c = 0x1D400 + (c - 0x100); // Yeah, get some chars outside the BMP!
  else if (c >= 0x7f  &&  c <= 0xa0)
    c = 0x1D5B0 + (c - 0x80);
#endif
  return c;
}

static void random_str_ops (const char *initial, int32 seed)
{
  Str *strs[10];
  int n = 1;
  strs[0] = new Str (initial);
  ob_rand_t *r = ob_rand_allocate_state (seed);

  PRF ("strs[0] = new Str (\"%s\");\n", initial);

  for (int i = 0; i < 1000; i++)
    {
      int x = ob_rand_state_int32 (0, 22, r);
      int64 len, k;
      UChar32 c;
      bool b;
      const char *m;
      Str s;
      int y, j;

      s.Sprintf ("initial = '%s', seed = %d, i = %d, string = '%s'\n", initial,
                 seed, i, strs[0]->utf8 ());
      SCOPED_TRACE (s.utf8 ());

      switch (x)
        {
          case 0:
            if (n < 10)
              {
                k = ob_rand_state_int32 (0, n, r);
                PRF ("strs[%d] = new Str (*strs[%" OB_FMT_64 "d]);\n", n, k);
                strs[n] = new Str (*strs[k]);
                n++;
              }
            break;
          case 1:
            len = strs[0]->Length ();
            PR4 (n);
            PRF ("EXPECT_EQ (%" OB_FMT_64 "d, strs[j] -> Length ());\n", len);
            for (j = 1; j < n; j++)
              if (probably (r))
                {
                  EXPECT_EQ (len, strs[j]->Length ())
                    << "j = " << j << ", str = '" << strs[j]->utf8 () << "'";
                }
            break;
          case 2:
            k = ob_rand_state_int32 (-20, 20, r);
            c = strs[0]->At (k);
            PR4 (n);
            PRF ("EXPECT_EQ (0x%x, strs[j] -> At (%" OB_FMT_64 "d));\n", c, k);
            for (j = 1; j < n; j++)
              if (probably (r))
                {
                  EXPECT_EQ (c, strs[j]->At (k))
                    << "j = " << j << ", k = " << k << ", str = '"
                    << strs[j]->utf8 () << "'";
                }
            break;
          case 3:
            c = random_char (r);
            PR4 (n);
            PRF ("strs[j] -> Append (0x%x);\n", c);
            for (j = 0; j < n; j++)
              strs[j]->Append (c);
            break;
          case 4:
            c = random_char (r);
            k = ob_rand_state_int32 (-20, 20, r);
            PR4 (n);
            PRF ("strs[j] -> Insert (%" OB_FMT_64 "d, 0x%x);\n", k, c);
            for (j = 0; j < n; j++)
              strs[j]->Insert (k, c);
            break;
          case 5:
            PR4 (n);
            PRF ("strs[j] -> Upcase ();\n");
            for (j = 0; j < n; j++)
              strs[j]->Upcase ();
            break;
          case 6:
            PR4 (n);
            PRF ("strs[j] -> Reverse ();\n");
            for (j = 0; j < n; j++)
              strs[j]->Reverse ();
            break;
          case 7:
            m = "([0-9])([A-Z])";
            b = strs[0]->Match (m);
            PR4 (n);
            PRF ("EXPECT_%s (strs[j] -> Match (\"%s\"));\n",
                 (b ? "TRUE" : "FALSE"), m);
            for (j = 1; j < n; j++)
              EXPECT_EQ (b, strs[j]->Match (m));
            break;
          case 8:
            b = strs[0]->MatchAgain ();
            PR4 (n);
            PRF ("EXPECT_%s (strs[j] -> MatchAgain ());\n",
                 (b ? "TRUE" : "FALSE"));
            for (j = 1; j < n; j++)
              EXPECT_EQ (b, strs[j]->MatchAgain ());
            break;
          case 9:
            // the no-argument Match() always returns false.
            // is there any point in the return value, then?
            PR4 (n);
            PRF ("EXPECT_FALSE (strs[j] -> Match ());\n");
            for (j = 0; j < n; j++)
              EXPECT_FALSE (strs[j]->Match ());
            break;
          case 10:
            b = strs[0]->MatchHasMatched ();
            PR4 (n);
            PRF ("EXPECT_%s (strs[j] -> MatchHasMatched ());\n",
                 (b ? "TRUE" : "FALSE"));
            for (j = 1; j < n; j++)
              if (probably (r))
                {
                  EXPECT_EQ (b, strs[j]->MatchHasMatched ());
                }
            break;
          case 11:
            b = strs[0]->MatchHasErred ();
            PR4 (n);
            PRF ("EXPECT_%s (strs[j] -> MatchHasErred ());\n",
                 (b ? "TRUE" : "FALSE"));
            for (j = 1; j < n; j++)
              if (probably (r))
                {
                  EXPECT_EQ (b, strs[j]->MatchHasErred ());
                }
            break;
          case 12:
            s = strs[0]->MatchErrorStr ();
            PR4 (n);
            PRF ("EXPECT_STREQ (\"%s\", strs[j] -> MatchErrorStr ());\n",
                 s.utf8 ());
            for (j = 1; j < n; j++)
              if (probably (r))
                {
                  EXPECT_STREQ (s, strs[j]->MatchErrorStr ());
                }
            break;
          case 13:
            s = strs[0]->MatchSlice ();
            PR4 (n);
            PRF ("EXPECT_STREQ (\"%s\", strs[j] -> MatchSlice ());\n",
                 s.utf8 ());
            for (j = 1; j < n; j++)
              if (probably (r))
                {
                  EXPECT_STREQ (s, strs[j]->MatchSlice ());
                }
            break;
          case 14:
            s = strs[0]->MatchPreSlice ();
            PR4 (n);
            PRF ("EXPECT_STREQ (\"%s\", strs[j] -> MatchPreSlice ());\n",
                 s.utf8 ());
            for (j = 1; j < n; j++)
              if (probably (r))
                {
                  EXPECT_STREQ (s, strs[j]->MatchPreSlice ());
                }
            break;
          case 15:
            s = strs[0]->MatchPostSlice ();
            PR4 (n);
            PRF ("EXPECT_STREQ (\"%s\", strs[j] -> MatchPostSlice ());\n",
                 s.utf8 ());
            for (j = 1; j < n; j++)
              if (probably (r))
                {
                  EXPECT_STREQ (s, strs[j]->MatchPostSlice ());
                }
            break;
          case 16:
            y = strs[0]->MatchNumCaptures ();
            PR4 (n);
            PRF ("EXPECT_EQ (%d, strs[j] -> MatchNumCaptures ());\n", y);
            for (j = 1; j < n; j++)
              if (probably (r))
                {
                  EXPECT_EQ (y, strs[j]->MatchNumCaptures ());
                }
            break;
          case 17:
            y = ob_rand_state_int32 (-2, 5, r);
            s = strs[0]->MatchNthCaptureSlice (y);
            PR4 (n);
            PRF (
              "EXPECT_STREQ (\"%s\", strs[j] -> MatchNthCaptureSlice (%d));\n",
              s.utf8 (), y);
            for (j = 1; j < n; j++)
              if (probably (r))
                {
                  EXPECT_STREQ (s, strs[j]->MatchNthCaptureSlice (y));
                }
            break;
          case 18:
            PR4 (n);
            PRF ("strs[j] -> ReplaceAll (\"[A-Z]+\", \",\");\n");
            for (j = 0; j < n; j++)
              strs[j]->ReplaceAll ("[A-Z]+", ",");
            break;
          case 19:
            PR4 (n);
            PRF ("strs[j] -> Split (\",\");\n");
            for (j = 0; j < n; j++)
              if (probably (r))
                {
                  strs[j]->Split (",");
                }
            break;
          case 20:
            // This is just like case 7, but with a mismatched paren, so I'm
            // assuming the mismatch was intentional.  Unfortunately, I wrote
            // this too long ago to remember my original intention.
            m = "([0-9])[A-Z])";
            b = strs[0]->Match (m);
            PR4 (n);
            PRF ("EXPECT_%s (strs[j] -> Match (\"%s\"));\n",
                 (b ? "TRUE" : "FALSE"), m);
            for (j = 1; j < n; j++)
              EXPECT_EQ (b, strs[j]->Match (m));
            break;
          case 21:
            m = "(,+) ";
            b = strs[0]->Match (m);
            PR4 (n);
            PRF ("EXPECT_%s (strs[j] -> Match (\"%s\"));\n",
                 (b ? "TRUE" : "FALSE"), m);
            for (j = 1; j < n; j++)
              EXPECT_EQ (b, strs[j]->Match (m));
            break;
        }
    }

  for (int i = 0; i < n; i++)
    strs[i]->Delete ();

  ob_rand_free_state (r);
}

static Str generate_funny_alphabet ()
{
  Str s;
  s.Append (0x1D538);
  s.Append (0x1D539);
  s.Append (0x2102);
  s.Append (0x1D53B);
  s.Append (0x1D53C);
  s.Append (0x1D53D);
  s.Append (0x1D53E);
  s.Append (0x210D);
  s.Append (0x1D540);
  s.Append (0x1D541);
  s.Append (0x1D542);
  s.Append (0x1D543);
  s.Append (0x1D544);
  s.Append (0x2115);
  s.Append (0x1D546);
  s.Append (0x2119);
  s.Append (0x211A);
  s.Append (0x211D);
  s.Append (0x1D54A);
  s.Append (0x1D54B);
  s.Append (0x1D54C);
  s.Append (0x1D54D);
  s.Append (0x1D54E);
  s.Append (0x1D54F);
  s.Append (0x1D550);
  s.Append (0x2124);
  for (int i = 0; i < 26; i++)
    s.Append (i + 0x1D552);
  return s;
}

/**
 * This tests for three things:
 *
 * 1) Can we do a bunch of random operations to a string without
 *    crashing, leaking memory, or otherwise doing something bad?
 *
 * 2) If we copy the string at any point in this sequence, will
 *    the copy continue to behave identically to the original?
 *
 * 3) Do "const" methods truly have no observable effect on the
 *    string?
 *
 * Note: we use fixed seeds (the second argument in the calls below),
 * so the operations are not technically "random".  We just use a
 * random number generator to produce an arbitrary sequence in a
 * repeatable way.  So the test should behave the same every time
 * you run it.
 */
TEST_F (RegexpStrTest1, RandomOperations)
{
  const Str funny_alphabet (generate_funny_alphabet ());
  for (int i = 0; i < 110; i += 11)
    {
      random_str_ops ("I reject your reality, and substitute my own", i + 100);
      random_str_ops ("Am I missing an eyebrow?", i + 101);
      random_str_ops ("I ate a radio for science!", i + 102);
      random_str_ops ("Jamie wants big boom.", i + 103);
      random_str_ops ("Is gasoline flammable?  You'll find out after this!",
                      i + 104);
      random_str_ops ("I always enjoy seeing Adam in pain.", i + 105);
      random_str_ops ("I wouldn't say Jamie's an evil genius", i + 106);
      random_str_ops ("Quack, damn you!", i + 107);
      random_str_ops ("If it's worth doing, it's worth over-doing.", i + 108);
      random_str_ops ("We often learn at the end of an episode of MythBusters, "
                      "everyday objects can, in fact, be made lethal if Jamie "
                      "builds a gun to shoot them.",
                      i + 109);
      if (ob_running_under_valgrind ())
        // XXX: this last one causes some valgrind errors, so I'm going
        // to skip it for now (yeah, sweep the problem under the carpet).
        // Plus, we don't want to go through the loop multiple times,
        // because it would take too long under valgrind.
        break;
      random_str_ops (funny_alphabet.utf8 (), i + 110);
    }
}
