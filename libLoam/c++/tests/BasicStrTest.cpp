
/* (c)  oblong industries */

#ifdef _MSC_VER
// On Windows, this file causes the warning:
// C4309 truncation of constant value
#pragma warning(disable : 4309)
#endif


#include <gtest/gtest.h>
#include <libLoam/c++/Str.h>
#include <unicode/unistr.h>

#include <unordered_map>

#include <type_traits>

using namespace oblong::loam;


class BasicStrTest : public ::testing::Test
{
 public:
  static const Str german;
  static const Str arabic;
  static const Str german_utf8;
  static const Str arabic_utf8;

 protected:
  void OutOfRangeTest (const Str &str, const char *trace)
  {
    SCOPED_TRACE (trace);
    EXPECT_EQ (0, str.At (str.end_bounds_marker ()));
    EXPECT_EQ (0, str.At (-(str.Length () + 1)));
    EXPECT_EQ (0, str.At (str.Length ()));
  }

  void ValidatePalindrome (const Str &str, const char *trace)
  {
    SCOPED_TRACE (trace);
    const int64 len = str.Length ();
    for (int64 i = 0; i < len; i++)
      {
        SCOPED_TRACE (i);
        EXPECT_EQ (str.At (i), str.At (-(i + 1)));
      }
  }
};

const Str BasicStrTest::german (
  UNICODE_STRING_SIMPLE ("Sch\\u00f6nes Auto: \\u20ac 11240.").unescape ());
const Str BasicStrTest::arabic (
  UNICODE_STRING_SIMPLE ("\\u0627\\u0628\\u0629\\u062a\\u062b").unescape ());
const Str BasicStrTest::german_utf8 (BasicStrTest::german.utf8 ());
const Str BasicStrTest::arabic_utf8 (BasicStrTest::arabic.utf8 ());

TEST_F (BasicStrTest, SingleCharacterAccess)
{
  EXPECT_EQ (0x062b, arabic.At (4));
  EXPECT_EQ (0x062b, arabic.At (-1));
  EXPECT_EQ (0x062b, arabic_utf8.At (4));
  EXPECT_EQ (0x062b, arabic_utf8.At (-1));

  EXPECT_EQ (0x20ac, german.At (-8));
  EXPECT_EQ (0x20ac, german.At (14));
  EXPECT_EQ (0x20ac, german_utf8.At (-8));
  EXPECT_EQ (0x20ac, german_utf8.At (14));
}

TEST_F (BasicStrTest, OutOfRangeSingleCharacterAccess)
{
  OutOfRangeTest (arabic, "Arabic string");
  OutOfRangeTest (arabic_utf8, "Arabic UTF-8 string");
  OutOfRangeTest (german, "German string");
  OutOfRangeTest (german_utf8, "German UTF-8 string");
}

TEST_F (BasicStrTest, LengthTest)
{
  EXPECT_EQ (arabic.ByteLength (), strlen (arabic_utf8));
  EXPECT_EQ (german.ByteLength (), strlen (german_utf8));

  EXPECT_NE (arabic.ByteLength (), arabic.Length ());
  EXPECT_NE (german.ByteLength (), german.Length ());
}

TEST_F (BasicStrTest, Equality)
{
  // icu
  Str str (arabic);
  EXPECT_EQ (str, arabic);
  str += "!";
  EXPECT_NE (str, arabic);

  // utf8
  str = "abcde";
  Str str2_eq = "abcde";
  Str str2_neq = "abcd";
  const char *eq_chars = "abcde";
  const char *neq_chars = "abcd";
  EXPECT_TRUE (str == str2_eq);
  EXPECT_TRUE (str != str2_neq);
  EXPECT_TRUE (str == eq_chars);
  EXPECT_TRUE (str != neq_chars);
  EXPECT_TRUE (eq_chars == str);
  EXPECT_TRUE (neq_chars != str);
  EXPECT_FALSE (str != str2_eq);
  EXPECT_FALSE (str == str2_neq);
  EXPECT_FALSE (str != eq_chars);
  EXPECT_FALSE (str == neq_chars);
  EXPECT_FALSE (eq_chars != str);
  EXPECT_FALSE (neq_chars == str);

  // check common prefixes
  str = "abcd";
  str2_eq = "abcd";
  str2_neq = "abcde";
  eq_chars = "abcd";
  neq_chars = "abcde";
  EXPECT_TRUE (str == str2_eq);
  EXPECT_TRUE (str != str2_neq);
  EXPECT_TRUE (str == eq_chars);
  EXPECT_TRUE (str != neq_chars);
  EXPECT_TRUE (eq_chars == str);
  EXPECT_TRUE (neq_chars != str);
  EXPECT_FALSE (str != str2_eq);
  EXPECT_FALSE (str == str2_neq);
  EXPECT_FALSE (str != eq_chars);
  EXPECT_FALSE (str == neq_chars);
  EXPECT_FALSE (eq_chars != str);
  EXPECT_FALSE (neq_chars == str);

  // check empty strings
  str = "";
  str2_eq = "";
  str2_neq = "a";
  eq_chars = "";
  neq_chars = "a";
  EXPECT_TRUE (str == str2_eq);
  EXPECT_TRUE (str != str2_neq);
  EXPECT_TRUE (str == eq_chars);
  EXPECT_TRUE (str != neq_chars);
  EXPECT_TRUE (eq_chars == str);
  EXPECT_TRUE (neq_chars != str);
  EXPECT_FALSE (str != str2_eq);
  EXPECT_FALSE (str == str2_neq);
  EXPECT_FALSE (str != eq_chars);
  EXPECT_FALSE (str == neq_chars);
  EXPECT_FALSE (eq_chars != str);
  EXPECT_FALSE (neq_chars == str);
}

TEST_F (BasicStrTest, LengthLimitedConstruction)
{
  UnicodeString german_short =
    UNICODE_STRING_SIMPLE ("Sch\\u00f6nes Auto").unescape ();
  Str str (german_short, 0, 12);
  EXPECT_EQ (Str (german_short), str);
}

// The previous test seems a little weird to me, since it's only passing
// the short string to the length-limited constructor.  In other words,
// the Str constructor could be completely ignoring the "12", and that
// test would still pass.  But this one wouldn't.
TEST_F (BasicStrTest, LengthLimitedConstruction2)
{
  UnicodeString german_short =
    UNICODE_STRING_SIMPLE ("Sch\\u00f6nes Auto").unescape ();
  Str str (german.ICUUnicodeString (), 0, 12);
  EXPECT_EQ (Str (german_short), str);
}

// And now let's try one that doesn't start at 0
TEST_F (BasicStrTest, LengthLimitedConstruction3)
{
  Str str (german.ICUUnicodeString (), 14, 1);
  EXPECT_EQ (Str (0x20ac), str);
}

TEST_F (BasicStrTest, Slice)
{
  Str slice = arabic.Slice (1, 3);
  EXPECT_EQ (3, slice.Length ());
  EXPECT_EQ (0x0628, slice[0]);
  EXPECT_EQ (0x0629, slice[1]);
  EXPECT_EQ (0x062a, slice[-1]);

  slice = german.Slice (14, 500);
  EXPECT_EQ (8, slice.Length ());
  EXPECT_EQ (0x20ac, slice[0]);
  EXPECT_EQ (' ', slice[1]);
  EXPECT_EQ ('.', slice[-1]);

  slice = german.Slice (14);
  EXPECT_EQ (8, slice.Length ());
  EXPECT_EQ (0x20ac, slice[0]);
  EXPECT_EQ (' ', slice[1]);
  EXPECT_EQ ('.', slice[-1]);

  slice = german.Slice (-8);
  EXPECT_EQ (8, slice.Length ());
  EXPECT_EQ (0x20ac, slice[0]);
  EXPECT_EQ (' ', slice[1]);
  EXPECT_EQ ('.', slice[-1]);
}

TEST_F (BasicStrTest, Append)
{
  Str str = "one";
  str.Append ("two");
  EXPECT_STREQ ("onetwo", str);
  EXPECT_EQ (6, str.Length ());
  EXPECT_EQ (strlen (str.utf8 ()), str.ByteLength ());

  str = "one";
  str.Append ('c');
  EXPECT_STREQ ("onec", str);
  EXPECT_EQ (4, str.Length ());
  EXPECT_EQ (strlen (str.utf8 ()), str.ByteLength ());

  UChar32 third_char = str[2];
  str.Append (third_char);
  EXPECT_STREQ ("onece", str);
  EXPECT_EQ (strlen (str.utf8 ()), str.ByteLength ());
}

TEST_F (BasicStrTest, Concat)
{
  Str str1 = "one";
  Str str2 = "two";
  Str str = str1 + str2;
  EXPECT_STREQ ("onetwo", str);
  EXPECT_EQ (6, str.Length ());
  EXPECT_EQ (strlen (str.utf8 ()), str.ByteLength ());

  str = str1 + "two";
  EXPECT_STREQ ("onetwo", str);

  str = str1 + 'j';
  EXPECT_STREQ ("onej", str);

  UChar32 second_char = str1[1];
  str = str1 + second_char;
  EXPECT_STREQ ("onen", str);

  Str snull;
  char *sc = NULL;
  EXPECT_TRUE (snull.IsNull ());

  str = str1 + snull;
  EXPECT_STREQ ("one", str);
  str = snull + str2;
  EXPECT_STREQ ("two", str);
  str = str1 + snull + str2;
  EXPECT_STREQ ("onetwo", str);
  str = "<<" + snull + ">>";
  EXPECT_STREQ ("<<>>", str);
  str = snull + "->";
  EXPECT_STREQ ("->", str);
  str = str1 + sc;
  EXPECT_STREQ ("one", str);
  str = sc + str2;
  EXPECT_STREQ ("two", str);
}

TEST_F (BasicStrTest, Insert)
{
  Str str1 = "onethree";
  Str str2 = "two";
  str1.Insert (3, str2);
  EXPECT_STREQ ("onetwothree", str1);
  //     at the beginning
  str1 = "twothree";
  str2 = "one";
  str1.Insert (0, str2);
  EXPECT_STREQ ("onetwothree", str1);
  //     at the end
  str1 = "onetwo";
  str2 = "three";
  str1.Insert (6, str2);
  EXPECT_STREQ ("onetwothree", str1);
  //     off the end should be the same as at the end
  str1 = "onetwo";
  str2 = "three";
  str1.Insert (8, str2);
  EXPECT_STREQ ("onetwothree", str1);
}

TEST_F (BasicStrTest, Comparison)
{
  Str str1 = "one";
  Str str2 = str1;
  EXPECT_EQ (str1, str2);

  str2 = "One";
  EXPECT_NE (str1, str2);
  EXPECT_EQ (0, str1.CaseCmp (str2));

  str1 = "";
  str2 = Str ("");
  EXPECT_EQ (str1, str2);
  EXPECT_STREQ ("", str1);
  EXPECT_TRUE (str1 == "");
  EXPECT_STREQ ("", str2);
  EXPECT_TRUE (str2 == "");
}

TEST_F (BasicStrTest, Nullishness)
{
  Str str1 ("");
  EXPECT_FALSE (str1.IsNull ());
  EXPECT_TRUE (str1.IsEmpty ());

  Str str2;
  EXPECT_TRUE (str2.IsNull ());
  EXPECT_TRUE (str2.IsEmpty ());
  EXPECT_NE (str1, str2);

  str1 = NULL;
  EXPECT_TRUE (str1.IsNull ());
  EXPECT_TRUE (str1.IsEmpty ());
  EXPECT_EQ (str1, str2);

  str2 = "foo";
  str2.SetToNull ();
  EXPECT_TRUE (str2.IsNull ());
  str2 = "foo";
  str2.Set (NULL);
  EXPECT_TRUE (str2.IsNull ());
  EXPECT_EQ (str1, str2);

  str2 = "foo";
  str2 = str1;
  EXPECT_TRUE (str2.IsNull ());
  EXPECT_EQ (str1, str2);

  str1 = str2.Dup ();
  EXPECT_TRUE (str1.IsNull ());
  EXPECT_TRUE (str2.IsNull ());
  EXPECT_EQ (str1, str2);

  str1.Append ("");
  EXPECT_FALSE (str1.IsNull ());
  str1.SetToNull ();
  str2.SetToNull ();
  str1.Append (str2);
  EXPECT_FALSE (str1.IsNull ());
  EXPECT_TRUE (str2.IsNull ());

  str1.SetToNull ();
  EXPECT_EQ (str1.Length (), 0);
  EXPECT_TRUE (str1.IsNull ());
  EXPECT_STREQ (str1.utf8 (), "");
  EXPECT_TRUE (str1.IsNull ());
  EXPECT_EQ (str1[0], 0);
  EXPECT_EQ (str1.At (12), 0);
  EXPECT_TRUE (str1.IsNull ());
  EXPECT_EQ (str1.Slice (0, 5), Str (""));
  EXPECT_TRUE (str1.IsNull ());
  EXPECT_EQ (str1.Index ("a"), -1);
  EXPECT_EQ (str1.Rindex ("a"), -1);
  EXPECT_TRUE (str1.IsNull ());
  EXPECT_FALSE (str1.Match (""));
  EXPECT_FALSE (str1.MatchHasMatched ());
  EXPECT_TRUE (str1.IsNull ());
  EXPECT_FALSE (str1.MatchAgain ());
  EXPECT_TRUE (str1.IsNull ());
  EXPECT_FALSE (str1.Match ());
  EXPECT_TRUE (str1.IsNull ());
}


TEST_F (BasicStrTest, UpAndDownCase)
{
  Str str = "One";
  EXPECT_EQ (Str ("ONE"), str.Upcase ());
  EXPECT_EQ (Str ("one"), str.Downcase ());
}

TEST_F (BasicStrTest, Strip)
{
  Str str = "  some text  \n";
  EXPECT_STREQ ("some text", str.Dup ().Strip ());
}

TEST_F (BasicStrTest, Chomp)
{
  Str str = "  some text  \n";
  EXPECT_EQ (Str ("  some text  "), str.Dup ().Chomp ());

  str = "  some text  ";
  EXPECT_STREQ ("  some text  ", str.Dup ().Chomp ());
}

TEST_F (BasicStrTest, Reverse)
{
  Str str = "abcdefg";
  EXPECT_STREQ ("gfedcba", str.Reverse ());
}

TEST_F (BasicStrTest, Indexing)
{
  Str str = "one two three three two one";
  EXPECT_EQ (8, str.Index ("three"));
  str = "one two three three two one";
  EXPECT_EQ (14, str.Rindex ("three"));
}

TEST_F (BasicStrTest, SplicyReplacement)
{
  Str str = "one two three";
  str.Replace (0, 3, "xxx");
  EXPECT_STREQ ("xxx two three", str);

  str = "one two three";
  str.Replace (0, 3, "y");
  EXPECT_STREQ ("y two three", str);

  str = "one two three";
  str.Replace (0, 3, "");
  EXPECT_STREQ (" two three", str);

  str = "one two three";
  str.Replace (0, 3, "carrots");
  EXPECT_STREQ ("carrots two three", str);

  str = "one two three";
  str.Replace (-5, 5, "golly");
  EXPECT_STREQ ("one two golly", str);

  str = "one two three";
  str.Replace (0, 100, "granola");
  EXPECT_STREQ ("granola", str);

  str = "abcdefg";
  str.Replace (3, 'D');
  EXPECT_STREQ ("abcDefg", str);
}

TEST_F (BasicStrTest, Splitnl)
{
  Str s ("foo\n\bar\nsome more stuff\nanother line");
  str_array strs = s.Split ("\n");
  ASSERT_EQ (int64 (4), strs.Count ());
}

TEST_F (BasicStrTest, Split)
{
  Str str = "one two   three";
  str_array strs = str.Split ("\\s+");
  ASSERT_EQ (int64 (3), strs.Count ());
  EXPECT_STREQ ("one", strs.Nth (0));
  EXPECT_STREQ ("two", strs.Nth (1));
  EXPECT_STREQ ("three", strs.Nth (2));
}

TEST_F (BasicStrTest, SplitWithCaptureGroup)
{
  // and with a capture group indicating that the separators should be
  // part of the returned list
  Str str = "one two   three";
  str_array strs = str.Split ("(\\s+)");
  ASSERT_EQ (int64 (5), strs.Count ());
  EXPECT_STREQ ("one", strs.Nth (0));
  EXPECT_STREQ (" ", strs.Nth (1));
  EXPECT_STREQ ("two", strs.Nth (2));
  EXPECT_STREQ ("   ", strs.Nth (3));
  EXPECT_STREQ ("three", strs.Nth (4));
}

TEST_F (BasicStrTest, SplitWithPatternNotFound)
{
  // bug 550: Tom asserts this should work like Perl, and you should just
  // get back the string you put in.
  Str tom ("filename");
  str_array banana = tom.Split ("/");
  EXPECT_EQ (int64 (1), banana.Count ());
  EXPECT_STREQ ("filename", banana.Nth (0));
  banana = german.Split ("/");
  EXPECT_EQ (int64 (1), banana.Count ());
  EXPECT_STREQ (german_utf8, banana.Nth (0));
  banana = arabic.Split ("(\\s+)");
  EXPECT_EQ (int64 (1), banana.Count ());
  EXPECT_STREQ (arabic_utf8, banana.Nth (0));
}

TEST_F (BasicStrTest, SplitWithPatternNotFound2)
{
  str_array banana = german_utf8.Split (arabic);
  EXPECT_EQ (int64 (1), banana.Count ());
  EXPECT_STREQ (german, banana.Nth (0));
  banana = arabic_utf8.Split (Str (UChar32 (0x262F)));
  EXPECT_EQ (int64 (1), banana.Count ());
  EXPECT_STREQ (arabic, banana.Nth (0));
}

TEST_F (BasicStrTest, SplitShouldBeNonDestructive)
{
  // bug 949
  Str a ("hello world");
  str_array parts = a.Split ("o");
  Str b = a;
  EXPECT_STREQ ("hello world", b);
}

TEST_F (BasicStrTest, SplitEmptyString)
{
  Str empty ("");
  Str nulll;

  EXPECT_EQ (0, empty.Split ("/").Count ());
  EXPECT_EQ (0, empty.Split ("(\\s+)").Count ());
  EXPECT_EQ (0, nulll.Split ("/").Count ());
  EXPECT_EQ (0, nulll.Split ("(\\s+)").Count ());
}

TEST_F (BasicStrTest, ConstCharConversion)
{
  Str str = "some collection of ascii chars";
  int64 lstr = strlen (str);
  EXPECT_EQ (lstr, str.Length ());

  char *freeMe;
  const char *str_c = (freeMe = strdup (str));
  EXPECT_STREQ (str_c, str);
  EXPECT_STREQ (str_c, str);
  free (freeMe);
}

TEST_F (BasicStrTest, LessThan)
{
  Str str1 = "bark";
  Str str2 = "bite";
  EXPECT_LT (str1, str2);
  EXPECT_FALSE (str2 < str1);
}

TEST_F (BasicStrTest, Sprintf)
{
  Str s = Str ().Sprintf ("-> %d", 23);
  EXPECT_EQ (s, "-> 23");

  Str format = "%s%s%s";
  Str repeatee = "/// call (or a cast) is required when passing a Str "
                 "object to printf or any other method that uses varargs";

  s.Sprintf (format, repeatee.utf8 (), repeatee.utf8 (), repeatee.utf8 ());
  EXPECT_EQ (s, (repeatee + repeatee + repeatee));
}

TEST_F (BasicStrTest, Format)
{
  Str s =
    Str::Format ("(%x)bort, (%c)etry, (%c)gnore, (%x)ail, (%c)ive up, (%x)ry",
                 0xa, 'r', 'i', 0xf, 'g', 0xc);
  EXPECT_STREQ ("(a)bort, (r)etry, (i)gnore, (f)ail, (g)ive up, (c)ry", s);
  s = Str::Format ("no arguments");
  EXPECT_STREQ ("no arguments", s);
  s = Str::Format ("si%sm%s", "so", "so");
  s.Reverse ();
  EXPECT_STREQ ("osmosis", s);
}

/* I have to cast the values greater than 127 to "char" explicitly,
 * to keep C++11 from complaining?  That blows! */

const char eins[] = {0x24, 0x00};
const char zwei[] = {char (0xC2), char (0xA2), 0x00};
const char drei[] = {char (0xE2), char (0x82), char (0xAC), 0x00};
const char vier[] = {char (0xF0), char (0xA4), char (0xAD), char (0xA2), 0x00};

TEST_F (BasicStrTest, FromUTF8)
{
  Str s (eins);
  EXPECT_EQ (0x24, s.At (0));
  s += "";
  EXPECT_EQ (0x24, s.At (0));
  s = zwei;
  EXPECT_EQ (0xa2, s.At (0));
  s += "";
  EXPECT_EQ (0xa2, s.At (0));
  s = drei;
  EXPECT_EQ (0x20ac, s.At (0));
  s += "";
  EXPECT_EQ (0x20ac, s.At (0));
  s = vier;
  EXPECT_EQ (0x24B62, s.At (0));
  s += "";
  EXPECT_EQ (0x24B62, s.At (0));
}

// This tests bug 1240 comment 0
TEST_F (BasicStrTest, ToUTF8)
{
  Str s1 (0x24);
  EXPECT_STREQ (eins, s1.utf8 ());
  Str s2 (0xa2);
  EXPECT_STREQ (zwei, s2.utf8 ());
  Str s3 (0x20ac);
  EXPECT_STREQ (drei, s3.utf8 ());
  Str s4 (0x24B62);
  EXPECT_STREQ (vier, s4.utf8 ());
}

// This tests bug 1240 comment 1
TEST_F (BasicStrTest, SurrogatePairsAndInsertion)
{
  Str s;

  s.Append (0x1D540);
  s.Append (0x1D541);
  s.Append (0x1D542);
  s.Append (0x1D543);
  s.Append (0x1D544);
  s.Insert (3, 0x63);
  EXPECT_EQ (0x63, s.At (3));
  EXPECT_EQ (size_t (21), s.ByteLength ());
  EXPECT_EQ (size_t (21), strlen (s.utf8 ()));
}

TEST_F (BasicStrTest, SurrogatePairsAndReplacement1)
{
  Str s;

  s.Append (0x1D540);
  s.Append (0x1D541);
  s.Append (0x1D542);
  s.Append (0x1D543);
  s.Append (0x1D544);
  s.Replace (3, 0x63);
  EXPECT_EQ (0x63, s.At (3));
  EXPECT_EQ (size_t (17), s.ByteLength ());
  EXPECT_EQ (size_t (17), strlen (s.utf8 ()));
}

TEST_F (BasicStrTest, SurrogatePairsAndReplacement2)
{
  Str s;

  s.Append (0x1D540);
  s.Append (0x1D541);
  s.Append (0x1D542);
  s.Append (0x1D543);
  s.Append (0x1D544);
  s.Replace (3, 2, "c");
  EXPECT_EQ (0x63, s.At (3));
  EXPECT_EQ (size_t (13), s.ByteLength ());
  EXPECT_EQ (size_t (13), strlen (s.utf8 ()));
}

TEST_F (BasicStrTest, SurrogatePairsAndReplacement3)
{
  Str s ("Policy : yciloP");

  ValidatePalindrome (s, "before");
  s.Replace (7, 0x1D540);
  ValidatePalindrome (s, "between1");
  s.Replace (3, 0x1D541);
  s.Replace (11, 0x1D541);
  ValidatePalindrome (s, "between2");
  s.Replace (-8, 0x1D542);
  ValidatePalindrome (s, "after");
  EXPECT_STREQ ("Pol\360\235\225\201cy \360\235\225\202 yc\360\235\225\201loP",
                s);
}

TEST_F (BasicStrTest, OutOfBoundsReplacement)
{
  Str s1, s2;
  Str s3 ("");
  Str s4 ("");
  Str s5 ("foo");
  Str s6 ("foo");
  Str s7 ("");
  Str s8 ("");
  Str s9 ("foo");
  Str s10 ("foo");
  Str s11 ("foo");
  Str s12 ("foo");
  Str s13 ("foo");
  Str s14 ("foo");
  Str s15 ("foo");
  Str s16 ("foo");
  Str s17 ("foo");
  Str s18 ("foo");
  Str s19 ("foo");
  Str s20 ("foo");

  // null strings remain null
  s1.Replace (0, 0, "bar");
  s2.Replace (0, 0x263a);
  EXPECT_STREQ ("", s1);
  EXPECT_STREQ ("", s2);

  // but now do the same thing to empty strings
  s3.Replace (0, 0, "bar");
  s4.Replace (0, 0x263a);
  EXPECT_STREQ ("bar", s3);
  EXPECT_STREQ ("\342\230\272", s4);

  // try inserting past the end of the string
  s5.Replace (3, 0, "bar");
  s6.Replace (3, 0x263a);
  EXPECT_STREQ ("foobar", s5);
  EXPECT_STREQ ("foo\342\230\272", s6);

  // negative
  s7.Replace (-1, 0, "bar");
  s8.Replace (-1, 0x263a);
  EXPECT_STREQ ("bar", s7);
  EXPECT_STREQ ("\342\230\272", s8);

  // negative on non-empty strings
  s9.Replace (-100, 0, "bar");
  s10.Replace (-100, 0x263a);
  EXPECT_STREQ ("barfoo", s9);
  EXPECT_STREQ ("\342\230\272oo", s10);

  // how about way past the end of the string
  s11.Replace (100, 0, "bar");
  s12.Replace (100, 0x263a);
  EXPECT_STREQ ("foobar", s11);
  EXPECT_STREQ ("foo\342\230\272", s12);

  // for the multi-character variant, let's try different lengths
  s13.Replace (3, 100, "bar");
  s14.Replace (-1, 100, "bar");
  s15.Replace (-100, 2, "bar");
  s16.Replace (-100, 4, "bar");
  EXPECT_STREQ ("foobar", s13);
  EXPECT_STREQ ("fobar", s14);
  EXPECT_STREQ ("baro", s15);
  EXPECT_STREQ ("bar", s16);

  // including some negative lengths
  s17.Replace (3, -1, "bar");
  s18.Replace (-1, -100, "bar");
  s19.Replace (2, -2, "bar");
  s20.Replace (100, -3, "bar");
  EXPECT_STREQ ("foobar", s17);
  EXPECT_STREQ ("fobaro", s18);
  EXPECT_STREQ ("fobaro", s19);
  EXPECT_STREQ ("foobar", s20);

  // make sure all the lengths came out okay
  EXPECT_EQ (0, s1.Length ());
  EXPECT_EQ (0, s2.Length ());
  EXPECT_EQ (3, s3.Length ());
  EXPECT_EQ (1, s4.Length ());
  EXPECT_EQ (6, s5.Length ());
  EXPECT_EQ (4, s6.Length ());
  EXPECT_EQ (3, s7.Length ());
  EXPECT_EQ (1, s8.Length ());
  EXPECT_EQ (6, s9.Length ());
  EXPECT_EQ (3, s10.Length ());
  EXPECT_EQ (6, s11.Length ());
  EXPECT_EQ (4, s12.Length ());
  EXPECT_EQ (6, s13.Length ());
  EXPECT_EQ (5, s14.Length ());
  EXPECT_EQ (4, s15.Length ());
  EXPECT_EQ (3, s16.Length ());
  EXPECT_EQ (6, s17.Length ());
  EXPECT_EQ (6, s18.Length ());
  EXPECT_EQ (6, s19.Length ());
  EXPECT_EQ (6, s20.Length ());
}

TEST_F (BasicStrTest, SurrogatePairsAndReverse1)
{
  Str s ("I wouldn't say Jam\360\235\224\270ie's an evil genius");
  s.Reverse ();
  EXPECT_STREQ ("suineg live na s'ei\360\235\224\270maJ yas t'ndluow I",
                s.utf8 ());
}

// This demonstrates bug 1255
TEST_F (BasicStrTest, SurrogatePairsAndReverse2)
{
  Str s ("I wouldn't say Jami\360\235\224\270e's an evil genius");
  s.Reverse ();
  EXPECT_STREQ ("suineg live na s'e\360\235\224\270imaJ yas t'ndluow I",
                s.utf8 ());
}

// This tests bug 1233
TEST_F (BasicStrTest, UpcaseLengthChange)
{
  Str a (0x149);
  Str b (0x149);

  a.utf8 ();  // this is what makes the two strings different

  a.Upcase ();
  b.Upcase ();

  EXPECT_EQ (2, a.Length ());
  EXPECT_EQ (2, b.Length ());
}

TEST_F (BasicStrTest, SetUnicodeStringWhenPreexisting)
{
  Str s ("racecar");
  s.Reverse ();  // calling Reverse ensures s has allocated its uString
  UnicodeString u ("[mo^", "ebcdic-xml-us");
  s.Set (u);
  EXPECT_STREQ ("$_?;", s.utf8 ());
}

TEST_F (BasicStrTest, SetUnicodeStringWhenNotPreexisting)
{
  Str s;
  UnicodeString u ("[mo^", "ebcdic-xml-us");
  s.Set (u);
  EXPECT_STREQ ("$_?;", s.utf8 ());
}

TEST_F (BasicStrTest, SurrogatePairsAndIndex)
{
  Str s ("\360\235\224\270BCDABCD");
  int64 idx = s.Index ("C");
  EXPECT_EQ (2, idx);
}

TEST_F (BasicStrTest, SurrogatePairsAndRindex)
{
  Str s ("\360\235\224\270BCDABCD");
  int64 idx = s.Rindex ("C");
  EXPECT_EQ (6, idx);
}

TEST_F (BasicStrTest, SurrogatePairsAndChomp1)
{
  Str s ("I wouldn't say Jam\360\235\224\270ie's an evil genius\n");
  s.Chomp ();
  EXPECT_STREQ ("I wouldn't say Jam\360\235\224\270ie's an evil genius",
                s.utf8 ());
}

TEST_F (BasicStrTest, SurrogatePairsAndChomp2)
{
  Str s ("I wouldn't say Jamie's an evil genius\360\235\224\270");
  s.Chomp ("\360\235\224\270");
  EXPECT_STREQ ("I wouldn't say Jamie's an evil genius", s.utf8 ());
}

TEST_F (BasicStrTest, NullAndEmptyComparisons)
{
  Str nullStr;
  Str emptyStr ("");

  EXPECT_TRUE (nullStr.IsNull ());
  EXPECT_TRUE (nullStr.IsEmpty ());
  EXPECT_EQ (-1, nullStr.Compare (emptyStr));
  EXPECT_EQ (-1, nullStr.Compare (""));
  EXPECT_FALSE (nullStr == emptyStr);
  EXPECT_FALSE (nullStr == "");
  EXPECT_TRUE (nullStr == NULL);
  EXPECT_EQ (0, nullStr.Compare (nullptr));
  EXPECT_EQ (0, nullStr.Compare (nullStr));
  EXPECT_TRUE (nullStr == nullStr);
  EXPECT_FALSE ("" == nullStr);
  EXPECT_TRUE (NULL == nullStr);

  EXPECT_FALSE (emptyStr.IsNull ());
  EXPECT_TRUE (emptyStr.IsEmpty ());
  EXPECT_EQ (0, emptyStr.Compare (emptyStr));
  EXPECT_EQ (0, emptyStr.Compare (""));
  EXPECT_TRUE (emptyStr == emptyStr);
  EXPECT_TRUE (emptyStr == "");
  EXPECT_FALSE (emptyStr == NULL);
  EXPECT_EQ (1, emptyStr.Compare (nullptr));
  EXPECT_EQ (1, emptyStr.Compare (nullStr));
  EXPECT_FALSE (emptyStr == nullStr);
  EXPECT_TRUE ("" == emptyStr);
  EXPECT_FALSE (NULL == emptyStr);
}

TEST_F (BasicStrTest, Hash)
{
  Str nullStr;
  Str emptyStr ("");

  EXPECT_EQ (german.Hash (), german_utf8.Hash ());
  EXPECT_EQ (arabic.Hash (), arabic_utf8.Hash ());
  EXPECT_NE (german.Hash (), arabic.Hash ());
  EXPECT_NE (nullStr.Hash (), emptyStr.Hash ());
}

TEST_F (BasicStrTest, BoostUnorderedMap)
{
  std::unordered_map<Str, int64, Str_hash> map;
  map[german] = 49;
  map[arabic] = 966;
  EXPECT_EQ (49, map[german_utf8]);
  EXPECT_EQ (966, map[arabic_utf8]);
}

TEST_F (BasicStrTest, Tr1UnorderedMap)
{
  std::unordered_map<Str, int64, Str_hash> map;
  map[german] = 49;
  map[arabic] = 966;
  EXPECT_EQ (49, map[german_utf8]);
  EXPECT_EQ (966, map[arabic_utf8]);
}

TEST_F (BasicStrTest, AppendUChar32)  // bug 1729
{
  Str s = "";
  s += (UChar32) 'W';
  s += (UChar32) 'O';
  s += (UChar32) 'R';
  s += (UChar32) 'K';
  s += (UChar32) 'S';
  s += (UChar32) 'F';
  s += (UChar32) 'O';
  s += (UChar32) 'R';
  s += (UChar32) 'M';
  s += (UChar32) 'E';
  EXPECT_STREQ ("WORKSFORME", s.utf8 ());
}

TEST_F (BasicStrTest, SelfAssignment)
{
  Str s = "I am myself";
  s = *&s;  // annotate with *& to tell clang we intend self-assignment
  EXPECT_STREQ ("I am myself", s.utf8 ());
}

TEST_F (BasicStrTest, SurrogatePairsAndSlice)  // bug 2296
{
  Str s;
  s.Append (0x1D540);
  s.Append (0x1D541);
  const Str e1 (s);
  s.Append (0x1D542);
  s.Append (0x1D543);
  s.Append (0x1D544);

  Str a1 = s.Slice (0, 2);
  Str a2 = s.Slice (2, 1);
  Str a3 = s.Slice (3);

  EXPECT_STREQ (e1, a1);
  EXPECT_EQ (1, a2.Length ());
  EXPECT_EQ (0x1D542, a2.At (0));
  EXPECT_STREQ (s.utf8 () + 12, a3);
}

TEST_F (BasicStrTest, RepeatedlyCallUtf8)  // bug 2299
{
  Str a ('a');
  Str s = Str::Format ("%s man %s plan %s canal panama", a.utf8 (), a.utf8 (),
                       a.utf8 ());
  EXPECT_STREQ ("a man a plan a canal panama", s);
}

TEST_F (BasicStrTest, InvalidUTF8)  // bug 2865
{
  ob_suppress_message (OBLV_WARN, 0x11000004);

  static const char notutf8[] = {82, 101, 121,        107, 106,
                                 97, 118, char (237), 107, 0};
  Str unfortunate (notutf8);
  EXPECT_EQ (9, unfortunate.Length ());
  EXPECT_EQ ('j', unfortunate.At (4));
  EXPECT_EQ (0xfffd, unfortunate.At (7));
  EXPECT_EQ ('k', unfortunate.At (8));
  unfortunate.Reverse ();
  EXPECT_EQ ('R', unfortunate.At (8));
  EXPECT_EQ (0xfffd, unfortunate.At (1));
  unfortunate.Reverse ();
  EXPECT_EQ (9, unfortunate.Length ());
  EXPECT_EQ ('j', unfortunate.At (4));
  EXPECT_EQ (0xfffd, unfortunate.At (7));
  EXPECT_EQ ('k', unfortunate.At (8));
  EXPECT_EQ (strlen (unfortunate.utf8 ()), unfortunate.ByteLength ());
}

TEST_F (BasicStrTest, SetWithLength)
{
  Str s ("foo");
  s.Set ("barn door", 3);
  EXPECT_STREQ ("bar", s.utf8 ());
}

TEST_F (BasicStrTest, BlankSplitFields)
{
  Str s (':');  // this creates a UTF-16 string
  for (int i = 0; i < 2; i++)
    {
      SCOPED_TRACE (i);
      EXPECT_EQ (6, Str (":::::").Split (s).Count ());
      EXPECT_EQ (6, Str ("foo:::::").Split (s).Count ());
      EXPECT_EQ (6, Str ("foo:bar::::").Split (s).Count ());
      EXPECT_EQ (6, Str ("foo:bar:baz:::").Split (s).Count ());
      EXPECT_EQ (6, Str ("foo:bar:baz:bob::").Split (s).Count ());
      EXPECT_EQ (6, Str ("foo:bar:baz:bob:buf:").Split (s).Count ());
      EXPECT_EQ (6, Str ("foo:bar:baz:bob:buf:fud").Split (s).Count ());
      EXPECT_EQ (6, Str (":bar:baz:bob:buf:fud").Split (s).Count ());
      EXPECT_EQ (6, Str ("::baz:bob:buf:fud").Split (s).Count ());
      EXPECT_EQ (6, Str (":::bob:buf:fud").Split (s).Count ());
      EXPECT_EQ (6, Str ("::::buf:fud").Split (s).Count ());
      EXPECT_EQ (6, Str (":::::fud").Split (s).Count ());
      s.utf8 ();  // caches the UTF-8 value for the second round
    }
}

struct answer
{
  int64 count;
  char fields[6][32];
};

static const answer answers[] = {
  {6, {"5", "/4~6", "", "5_3  7`81%3882\t5@3//1", "", ""}},
  {2, {"50/4~6005_3  7", "81%3882\t5@3//100"}},
  {2, {"50/4~6005_3  7`81%3882\t5", "3//100"}},
  {4, {"50/4~6005_", "  7`81%", "882\t5@", "//100"}},
  {2, {"50/4~", "005_3  7`81%3882\t5@3//100"}},
  {2, {"50/4~6005", "3  7`81%3882\t5@3//100"}},
  {2, {"50/4~6005_3  7`81%388", "\t5@3//100"}},
  {4, {"", "0/4~600", "_3  7`81%3882\t", "@3//100"}},
  {2, {"50/4~6005_3  7`81", "3882\t5@3//100"}},
  {4, {"50/4~6005_3  7`", "1%3", "", "2\t5@3//100"}},
  {2, {"50/4", "6005_3  7`81%3882\t5@3//100"}},
  {3, {"50/4~6005_3  7`8", "%3882\t5@3//", "00"}},
  {2, {"50/", "~6005_3  7`81%3882\t5@3//100"}},
  {2, {"50/4~6005_3  ", "`81%3882\t5@3//100"}},
  {1, {"50/4~6005_3  7`81%3882\t5@3//100"}},
  {3, {"50/4~6005_3", "", "7`81%3882\t5@3//100"}},
  {4, {"50", "4~6005_3  7`81%3882\t5@3", "", "100"}},
  {2, {"50/4~6005_3  7`81%3882", "5@3//100"}},
};

TEST_F (BasicStrTest, SingleCharSplit)
{
  const Str s ("50/4~6005_3  7`81%3882\t5@3//100");
  const Str nub ("0`@36_25%8~147] /\t");
  for (int64 i = 0; i < nub.Length (); i++)
    {
      SCOPED_TRACE (i);
      const UChar32 c = nub.At (i);
      Str p (c);
      str_array a1 = s.Split (p);  // single character UnicodeString
      Str q = Str::Format ("%s|%s", p.utf8 (), p.utf8 ());
      str_array a2 = s.Split (p);  // single character utf8
      str_array a3 = s.Split (q);  // degenerate regexp
      ASSERT_EQ (answers[i].count, a1.Count ());
      ASSERT_EQ (answers[i].count, a2.Count ());
      ASSERT_EQ (answers[i].count, a3.Count ());
      for (int64 j = 0; j < a1.Count (); j++)
        {
          SCOPED_TRACE (j);
          EXPECT_STREQ (answers[i].fields[j], a1.Nth (j));
          EXPECT_STREQ (answers[i].fields[j], a2.Nth (j));
          EXPECT_STREQ (answers[i].fields[j], a3.Nth (j));
        }
    }
}

#if defined __linux__ && U_ICU_VERSION_MAJOR_NUM >= 60
TEST_F (BasicStrTest, Char16)
{
  UnicodeString us{"beeeeeeeeeeeeeep"};

  const bool is_c16 =
    std::is_same<const char16_t *, decltype (us.getBuffer ())>::value;
  ASSERT_TRUE (is_c16);

  const char16_t *buff = us.getBuffer ();
  Str c16_str{buff};

  ASSERT_TRUE (c16_str.ICUUnicodeString () == us);
}
#endif

#if defined _MSC_VER
#include <cwchar>
#include <iostream>

TEST_F (BasicStrTest, DubyaChar)
{
  const wchar_t *text = L"beeeep";
  const size_t text_length = wcslen (text);

  Str wch_str{text};
  ASSERT_TRUE (wch_str.ICUUnicodeString ().getBuffer ());
  const UChar *uc_buf = wch_str.ICUUnicodeString ().getBuffer ();
  const wchar_t *uc_text = reinterpret_cast<const wchar_t *> (uc_buf);
  const int32 uc_text_length = wch_str.ICUUnicodeString ().length ();
  EXPECT_EQ (text_length, uc_text_length);
  EXPECT_EQ (0, std::wcsncmp (text, uc_text, text_length));
}
#endif
