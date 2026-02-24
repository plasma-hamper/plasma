
/* Copyright (C) 2001 Mignon Pelletier
 * Copyright (C) 2009 Oblong Industries */

#include <gtest/gtest.h>
#include <boost/version.hpp>
#include "libLoam/c++/ArgParse.h"

using namespace oblong::loam;

static bool checkArrays (const ArgParse::apintvec &foo,
                         const ArgParse::apfloatvec &bar,
                         const ArgParse::apstringvec &english,
                         const ArgParse::apstringvec &german)
{
  if (foo.Count () != 4 || bar.Count () != 4 || english.Count () != 4
      || german.Count () != 4)
    {
      return false;
    }

  for (int i = 0; i < 4; i++)
    {
      ArgParse::apstring en, de;
      switch (i + 1)
        {
          case 1:
            en = "one";
            de = "eins";
            break;
          case 2:
            en = "two";
            de = "zwei";
            break;
          case 3:
            en = "three";
            de = "drei";
            break;
          case 4:
            en = "four";
            de = "vier";
            break;
        }
      if (foo.Nth (i) != ArgParse::apint (i + 1)
          || bar.Nth (i) != ArgParse::apfloat (i + 1) || english.Nth (i) != en
          || german.Nth (i) != de)
        {
          return false;
        }
    }

  return true;
}

TEST (aptest1, FlagsAndIntegers)
{
  ArgParse::apflag showhelp = false;
  ArgParse::apint myint = 0;

  ArgParse ap;
  ap.AllowOneCharOptionsToBeCombined ();
  ap.UsageHeader (ArgParse::apstring ("Usage: aptest1 [options]"));
  ap.ArgFlag ("help", "\aprint this help, then exit", &showhelp);
  ap.ArgInt ("integer", "=value\aa random integer you can give for no reason",
             &myint);
  ap.Alias ("help", "h");
  ap.Alias ("integer", "i");

  myint = 3;
  showhelp = false;
  const char *av[3];

  EXPECT_TRUE (ap.Parse (0, av) && !showhelp && myint == 3
               && ap.Leftovers ().Count () == 0);

  av[0] = "-h";
  EXPECT_TRUE (ap.Parse (1, av) && showhelp && myint == 3
               && ap.Leftovers ().Count () == 0);

  showhelp = false;
  av[0] = "--help";
  EXPECT_TRUE (ap.Parse (1, av) && showhelp && myint == 3
               && ap.Leftovers ().Count () == 0);

  showhelp = false;
  av[0] = "--integer";
  av[1] = "5";
  EXPECT_TRUE (ap.Parse (2, av) && !showhelp && myint == 5
               && ap.Leftovers ().Count () == 0);

  myint = 3;
  av[0] = "-i";
  EXPECT_TRUE (ap.Parse (2, av) && !showhelp && myint == 5
               && ap.Leftovers ().Count () == 0);

  myint = 3;
  av[2] = "--help";
  EXPECT_TRUE (ap.Parse (3, av) && showhelp && myint == 5
               && ap.Leftovers ().Count () == 0);

  av[0] = "-h";
  av[1] = "-i";
  av[2] = "5";
  myint = 3;
  showhelp = false;
  EXPECT_TRUE (ap.Parse (3, av) && showhelp && myint == 5
               && ap.Leftovers ().Count () == 0);

  av[0] = "-hi";
  av[1] = "5";
  myint = 3;
  showhelp = false;
  EXPECT_TRUE (ap.Parse (2, av) && showhelp && myint == 5
               && ap.Leftovers ().Count () == 0);

  av[0] = "-hi=5";
  myint = 3;
  showhelp = false;
  EXPECT_TRUE (ap.Parse (1, av) && showhelp && myint == 5
               && ap.Leftovers ().Count () == 0);

  av[0] = "-help";
  EXPECT_TRUE (!ap.Parse (1, av)
               && ap.ErrorMessage () == "-help: 'e' is an unrecognized option");

  av[0] = "--integer=5";
  myint = 3;
  showhelp = false;
  EXPECT_TRUE (ap.Parse (1, av) && !showhelp && myint == 5
               && ap.Leftovers ().Count () == 0);

  av[0] = "--integer:5";
  myint = 3;
  EXPECT_TRUE (ap.Parse (1, av) && !showhelp && myint == 5
               && ap.Leftovers ().Count () == 0);

  av[0] = "-ih";
  av[1] = "5";
  EXPECT_TRUE (!ap.Parse (2, av)
               && ap.ErrorMessage () == "-ih: 'i' requires an argument");

  av[0] = "--help=5";
  EXPECT_TRUE (!ap.Parse (1, av)
               && ap.ErrorMessage () == "--help=5: doesn't take an argument");

  av[0] = "--nohelp";
  showhelp = true;
  myint = 3;
  EXPECT_TRUE (ap.Parse (1, av) && !showhelp && myint == 3
               && ap.Leftovers ().Count () == 0);

  av[1] = "--help";
  EXPECT_TRUE (ap.Parse (2, av) && showhelp && myint == 3
               && ap.Leftovers ().Count () == 0);
}

TEST (aptest1, StringsAndFloats)
{
  const char *av[5];
  ArgParse::apfloat tex;
  ArgParse::apfloat metafont;
  ArgParse::apstring kpathsea;
  ArgParse t1;
  t1.ArgFloat ("tex", "\aversion of TeX", &tex);
  t1.ArgFloat ("metafont", "\aversion of Metafont", &metafont);
  t1.ArgString ("kpathsea", "\aversion of Kpathsea", &kpathsea);

  av[0] = "-tex";
  av[1] = "3.14159";
  av[2] = "--metafont=2.7182";
  av[3] = "--kpathsea";
  av[4] = "3.3.1";
  EXPECT_TRUE (t1.Parse (5, av) && t1.Leftovers ().Count () == 0
               && tex == 3.14159 && metafont == 2.7182 && kpathsea == "3.3.1");

  av[1] = "blech";
  EXPECT_TRUE (!t1.Parse (5, av)
               && t1.ErrorMessage ()
                    == "-tex: \"blech\" is not a valid floating-point number");
}

TEST (aptest1, UsageMessage)
{
  ArgParse::apflag junkf;
  ArgParse t2;
  t2.UsageHeader ("GNU `tar' saves blah, blah, blah\n\n"
                  "Main operation mode:",
                  26);
  t2.ArgFlag ("t", "\alist the contents of an archive", &junkf);
  t2.ArgFlag ("x", "\aextract files from an archive", &junkf);
  t2.ArgFlag ("delete", "\adelete from the archive (not on mag tapes!)",
              &junkf);
  t2.Alias ("t", "list");
  t2.Alias ("x", "extract");
  t2.Alias ("x", "get");
  t2.UsageHeader ("\nOperation modifiers:", 29);
  t2.ArgFlag ("G", "\ahandle old GNU-format incremental backup", &junkf);
  t2.ArgFlag ("g", "=FILE\ahandle new GNU-format incremental backup", &junkf);
  t2.Alias ("G", "incremental");
  t2.Alias ("g", "listed-incremental");
  t2.UsageHeader ("\nArchive format selection:", 37);
  t2.ArgFlag ("V",
              "=NAME\acreate archive with volume name NAME\n      "
              "        PATTERN\aat list/extract time, a globbing PATTERN",
              &junkf);
  t2.Alias ("V", "label");
  t2.AllowOneCharOptionsToBeCombined ();
  EXPECT_TRUE (
    t2.UsageMessage ()
    == "GNU `tar' saves blah, blah, blah\n"
       "\n"
       "Main operation mode:\n"
       "  -t, --list              list the contents of an archive\n"
       "  -x, --get, --extract    extract files from an archive\n"
       "      --delete            delete from the archive (not on mag tapes!)\n"
       "\n"
       "Operation modifiers:\n"
       "  -G, --incremental          handle old GNU-format incremental backup\n"
       "  -g, --listed-incremental=FILE\n"
       "                             handle new GNU-format incremental backup\n"
       "\n"
       "Archive format selection:\n"
       "  -V, --label=NAME                   create archive with volume name "
       "NAME\n"
       "              PATTERN                at list/extract time, a globbing "
       "PATTERN\n");
}

TEST (aptest1, Arrays)
{
  const char *av[20];
  ArgParse::apintvec foo;
  ArgParse::apfloatvec bar;
  ArgParse::apstringvec baz;
  ArgParse t3;
  t3.ArgInts ("foo", "", &foo);
  t3.ArgFloats ("bar", "", &bar);
  t3.ArgStrings ("baz", "", &baz);

  av[0] = "eins";
  av[1] = "-baz=one";
  av[2] = "-foo=1";
  av[3] = "zwei";
  av[4] = "drei";
  av[5] = "-baz";
  av[6] = "two";
  av[7] = "vier";
  av[8] = "-foo=2";
  av[9] = "-bar=1";
  av[10] = "-bar=2";
  av[11] = "-bar";
  av[12] = "3";
  av[13] = "-bar:4";
  av[14] = "-foo=3";
  av[15] = "--baz=three";
  av[16] = "-baz:four";
  av[17] = "-foo=4";
  EXPECT_TRUE (t3.Parse (18, av)
               && checkArrays (foo, bar, baz, t3.Leftovers ()));

  foo.Empty ();
  bar.Empty ();
  baz.Empty ();
  ArgParse t4;
  t4.ArgInts ("foo", "", &foo, ArgParse::SEP_ARGV);
  t4.ArgFloats ("bar", "", &bar, ArgParse::SEP_ARGV);
  t4.ArgStrings ("baz", "", &baz, ArgParse::SEP_ARGV);
  av[0] = "eins";
  av[1] = "zwei";
  av[2] = "-baz=one";
  av[3] = "two";
  av[4] = "-foo";
  av[5] = "1";
  av[6] = "2";
  av[7] = "3";
  av[8] = "-baz=three";
  av[9] = "-bar=1";
  av[10] = "-bar=2";
  av[11] = "-bar";
  av[12] = "3";
  av[13] = "4";
  av[14] = "-foo=4";
  av[15] = "--baz";
  av[16] = "four";
  av[17] = "--";
  av[18] = "drei";
  av[19] = "vier";
  EXPECT_TRUE (t4.Parse (20, av)
               && checkArrays (foo, bar, baz, t4.Leftovers ()));

  foo.Empty ();
  bar.Empty ();
  baz.Empty ();
  ArgParse t5;
  t5.ArgInts ("foo", "", &foo, ',');
  t5.ArgFloats ("bar", "", &bar, ',');
  t5.ArgStrings ("baz", "", &baz, ',');
  av[0] = "-foo";
  av[1] = "1,2";
  av[2] = "eins";
  av[3] = "-baz=one,two,three";
  av[4] = "zwei";
  av[5] = "-bar:1,2,3,4";
  av[6] = "--baz=four";
  av[7] = "drei";
  av[8] = "-foo";
  av[9] = "3,4";
  av[10] = "vier";
  EXPECT_TRUE (t5.Parse (11, av)
               && checkArrays (foo, bar, baz, t5.Leftovers ()));
}

TEST (aptest1, ReAlias)
{
  ArgParse ap;
  ArgParse::apint foo = 1, foobar = 2;

  const char *test1 = "-f=7";
  const char *test2 = "-f=8";

  ap.ArgInt ("foo", "this is not the option you're looking for", &foo);
  ap.ArgInt ("foobar", "this is the option we want", &foobar);

  ap.Alias ("foo", "f", true);

  bool success = ap.Parse (1, &test1);
  ASSERT_TRUE (success);
  ASSERT_EQ (7, foo);
  ASSERT_EQ (2, foobar);

  ap.Alias ("foobar", "f", true);

  success = ap.Parse (1, &test2);
  ASSERT_TRUE (success);
  ASSERT_EQ (7, foo);
  ASSERT_EQ (8, foobar);
}

TEST (aptest1, Query)
{
  ArgParse ap;
  ArgParse::apflag foo = false, bar = false;

  const char *foosage = "xyzxyzxyz";
  const char *barsage = "abcabcabc";

  ap.ArgFlag ("foo", foosage, &foo, true);
  ap.ArgFlag ("bar", barsage, &bar, false);
  ap.Alias ("foo", "f", true);

  ArgParse::ArgInfo afoo = ap.Query ("foo");
  ArgParse::ArgInfo abar = ap.Query ("bar");
  ArgParse::ArgInfo anofoo = ap.Query ("nofoo");
  ArgParse::ArgInfo anobar = ap.Query ("nobar");
  ArgParse::ArgInfo af = ap.Query ("f");
  ArgParse::ArgInfo ab = ap.Query ("b");

  ASSERT_EQ (ArgParse::TYPE_REAL_OPTION, afoo.typ);
  ASSERT_EQ (ArgParse::TYPE_REAL_OPTION, abar.typ);
  ASSERT_EQ (ArgParse::TYPE_NEGATED, anofoo.typ);
  ASSERT_EQ (ArgParse::TYPE_NONEXISTENT, anobar.typ);
  ASSERT_EQ (ArgParse::TYPE_ALIAS, af.typ);
  ASSERT_EQ (ArgParse::TYPE_NONEXISTENT, ab.typ);

  ASSERT_STREQ ("", afoo.aliasFor.utf8 ());
  ASSERT_STREQ ("", abar.aliasFor.utf8 ());
  ASSERT_STREQ ("", anofoo.aliasFor.utf8 ());
  ASSERT_STREQ ("", anobar.aliasFor.utf8 ());
  ASSERT_STREQ ("foo", af.aliasFor.utf8 ());
  ASSERT_STREQ ("", ab.aliasFor.utf8 ());

  ASSERT_STREQ (foosage, afoo.usage.utf8 ());
  ASSERT_STREQ (barsage, abar.usage.utf8 ());
  ASSERT_STREQ (foosage, anofoo.usage.utf8 ());
  ASSERT_STREQ ("", anobar.usage.utf8 ());
  ASSERT_STREQ (foosage, af.usage.utf8 ());
  ASSERT_STREQ ("", ab.usage.utf8 ());

  ASSERT_EQ (1, afoo.aliases.Count ());
  ASSERT_EQ (0, abar.aliases.Count ());
}

TEST (aptest1, TroveParse)
{
  ArgParse ap;
  ArgParse::apflag cc = false;
  ap.AllowOneCharOptionsToBeCombined ();
  ap.UsageHeader ("Usage: man [options] [[section] page ...] ...");
  ap.ArgFlag ("create-cylons", "\astart war", &cc);

  ArgParse::apstringvec args;
  EXPECT_EQ (OB_OK, args.Append ("2").Code ());
  EXPECT_EQ (OB_OK, args.Append ("mkdir").Code ());
  EXPECT_EQ (OB_OK, args.Append ("--create-cylons").Code ());

  EXPECT_STREQ ("Usage: man [options] [[section] page ...] ...\n"
                "      --create-cylons    start war\n",
                ap.UsageMessage ());

  ObRetort tort = ap.Parse (args);
  EXPECT_EQ (OB_OK, tort.Code ());
  EXPECT_STREQ ("OB_OK", tort.Description ());

  ArgParse::apstringvec lefto = ap.Leftovers ();

  EXPECT_EQ (2, lefto.Count ());
  EXPECT_STREQ ("2", lefto.Nth (0));
  EXPECT_STREQ ("mkdir", lefto.Nth (1));
  EXPECT_TRUE (cc);
}

TEST (aptest1, TroveParseError)
{
  ArgParse ap;
  ArgParse::apflag cc = false;
  ap.AllowOneCharOptionsToBeCombined ();
  ap.UsageHeader ("Usage: man [options] [[section] page ...] ...");
  ap.ArgFlag ("create-cylons", "\astart war", &cc);

  ArgParse::apstringvec args;
  EXPECT_EQ (OB_OK, args.Append ("2").Code ());
  EXPECT_EQ (OB_OK, args.Append ("mkdir").Code ());
  EXPECT_EQ (OB_OK, args.Append ("-create-cylons").Code ());

  ObRetort tort = ap.Parse (args);
  EXPECT_EQ (OB_ARGUMENT_PARSING_FAILED, tort.Code ());
  EXPECT_STREQ ("OB_ARGUMENT_PARSING_FAILED", tort.Description ());
  EXPECT_STREQ ("-create-cylons: 'c' is an unrecognized option",
                ap.ErrorMessage ());
}

TEST (aptest1, DoubleDoubleYourOptionmint)
{
  ArgParse ap;
  ArgParse::apflag problem = false;
  ArgParse::apstring color = "green";
  ap.AllowOneCharOptionsToBeCombined ();
  ap.UsageHeader ("Usage: dashpot [options]");
  ap.ArgFlag ("problem", "\auh-oh!", &problem);
  ap.ArgString ("color", "\awavelength of photons", &color);

  ArgParse::apstringvec args;
  EXPECT_EQ (OB_OK, args.Append ("--color=blue").Code ());
  EXPECT_EQ (OB_OK, args.Append ("--problem").Code ());
  EXPECT_EQ (OB_OK, args.Append ("--color=red").Code ());
  EXPECT_EQ (OB_OK, args.Append ("--noproblem").Code ());

  ObRetort tort = ap.Parse (args);
  EXPECT_EQ (OB_OK, tort.Code ()) << ap.ErrorMessage ();

  ArgParse::apstringvec lefto = ap.Leftovers ();
  EXPECT_EQ (0, lefto.Count ());

  EXPECT_FALSE (problem);
  EXPECT_STREQ ("red", color);
}

using boost::logic::tribool;
using boost::logic::indeterminate;

TEST (aptest1, TrinaryTrue)
{
  ArgParse ap;
  tribool ihtfp = indeterminate;
  ap.AllowOneCharOptionsToBeCombined ();
  ap.UsageHeader ("Usage: mit [options]");
  ap.ArgFlag ("ihtfp", "\ahow i feel", &ihtfp);

  ArgParse::apstringvec args;
  EXPECT_EQ (OB_OK, args.Append ("--ihtfp").Code ());

  ObRetort tort = ap.Parse (args);
  EXPECT_EQ (OB_OK, tort.Code ()) << ap.ErrorMessage ();

  ArgParse::apstringvec lefto = ap.Leftovers ();
  EXPECT_EQ (0, lefto.Count ());

  EXPECT_TRUE (true == ihtfp);
  EXPECT_TRUE (false != ihtfp);
  EXPECT_FALSE (indeterminate (ihtfp));
}

TEST (aptest1, TrinaryFalse)
{
  ArgParse ap;
  tribool ihtfp = indeterminate;
  ap.AllowOneCharOptionsToBeCombined ();
  ap.UsageHeader ("Usage: mit [options]");
  ap.ArgFlag ("ihtfp", "\ahow i feel", &ihtfp);

  ArgParse::apstringvec args;
  EXPECT_EQ (OB_OK, args.Append ("--noihtfp").Code ());

  ObRetort tort = ap.Parse (args);
  EXPECT_EQ (OB_OK, tort.Code ()) << ap.ErrorMessage ();

  ArgParse::apstringvec lefto = ap.Leftovers ();
  EXPECT_EQ (0, lefto.Count ());

  EXPECT_TRUE (true != ihtfp);
  EXPECT_TRUE (false == ihtfp);
  EXPECT_FALSE (indeterminate (ihtfp));
}

TEST (aptest1, TrinaryIndeterminate)
{
  ArgParse ap;
  tribool ihtfp = indeterminate;
  ap.AllowOneCharOptionsToBeCombined ();
  ap.UsageHeader ("Usage: mit [options]");
  ap.ArgFlag ("ihtfp", "\ahow i feel", &ihtfp);

  ArgParse::apstringvec args;

  ObRetort tort = ap.Parse (args);
  EXPECT_EQ (OB_OK, tort.Code ()) << ap.ErrorMessage ();

  ArgParse::apstringvec lefto = ap.Leftovers ();
  EXPECT_EQ (0, lefto.Count ());

  bool ugh;
#if BOOST_VERSION < 106900
  // Before boost 1.69, you could compare tribools directly to bools.
  ugh = (true == ihtfp);
  EXPECT_FALSE (ugh);
  ugh = (false == ihtfp);
  EXPECT_FALSE (ugh);
#endif
  // As of boost 1.69, you have to do an explicit cast... and
  // unfortunately, that converts indeterminate to false?!
  ugh = (true == static_cast<bool> (ihtfp));
  EXPECT_FALSE (ugh);
  ugh = (false == static_cast<bool> (ihtfp));
  EXPECT_TRUE (ugh);

  EXPECT_TRUE (indeterminate (ihtfp));
}

TEST (aptest1, UsageNotCombined)
{
  ArgParse ap;
  bool help = false;
  ap.UsageHeader ("Usage: foo [options]");
  ap.ArgFlag ("help", "\ashow this help message", &help, false);
  EXPECT_STREQ ("Usage: foo [options]\n"
                "  -help                  show this help message\n",
                ap.UsageMessage ());
}

TEST (aptest1, UsageCombined)
{
  ArgParse ap;
  bool help = false;
  ap.UsageHeader ("Usage: foo [options]");
  ap.ArgFlag ("help", "\ashow this help message", &help, false);
  ap.AllowOneCharOptionsToBeCombined ();
  EXPECT_STREQ ("Usage: foo [options]\n"
                "      --help             show this help message\n",
                ap.UsageMessage ());
}

TEST (aptest1, NullPtrs)
{
  ArgParse ap;
  ap.UsageHeader ("Usage: foo [options]");
  ap.ArgFlag ("flag1", "\agive me a flag", nullptr, false);
  ap.ArgFlag ("flag2", "\agive me another flag", nullptr, true);
  ap.ArgInt ("int1", "\agive me an integer", nullptr, false);
  ap.ArgInt ("int2", "\agive me another integer", nullptr, true);
  ap.ArgInts ("ints1", "\agive me some integers", nullptr, false);
  ap.ArgInts ("ints2", "\agive me some other integers", nullptr, true);
  ap.ArgFloat ("float1", "\agive me a float", nullptr, false);
  ap.ArgFloat ("float2", "\agive me another float", nullptr, true);
  ap.ArgFloats ("floats1", "\agive me some floats", nullptr, false);
  ap.ArgFloats ("floats2", "\agive me some other floats", nullptr, true);
  ap.ArgString ("string1", "\agive me a string", nullptr, false);
  ap.ArgString ("string2", "\agive me another string", nullptr, true);
  ap.ArgStrings ("strings1", "\agive me some strings", nullptr, false);
  ap.ArgStrings ("strings2", "\agive me some other strings", nullptr, true);

  ArgParse::apstringvec args;
  ObRetort tort = ap.Parse (args);
  EXPECT_EQ (OB_OK, tort.Code ()) << ap.ErrorMessage ();

  ArgParse::apstringvec lefto = ap.Leftovers ();
  EXPECT_EQ (0, lefto.Count ());
}

TEST (aptest1, NullPtrsWithArgs)
{
  ArgParse ap;
  ap.UsageHeader ("Usage: foo [options]");
  ap.ArgFlag ("flag1", "\agive me a flag", nullptr, false);
  ap.ArgFlag ("flag2", "\agive me another flag", nullptr, true);
  ap.ArgInt ("int1", "\agive me an integer", nullptr, false);
  ap.ArgInt ("int2", "\agive me another integer", nullptr, true);
  ap.ArgInts ("ints1", "\agive me some integers", nullptr, false);
  ap.ArgInts ("ints2", "\agive me some other integers", nullptr, true);
  ap.ArgFloat ("float1", "\agive me a float", nullptr, false);
  ap.ArgFloat ("float2", "\agive me another float", nullptr, true);
  ap.ArgFloats ("floats1", "\agive me some floats", nullptr, false);
  ap.ArgFloats ("floats2", "\agive me some other floats", nullptr, true);
  ap.ArgString ("string1", "\agive me a string", nullptr, false);
  ap.ArgString ("string2", "\agive me another string", nullptr, true);
  ap.ArgStrings ("strings1", "\agive me some strings", nullptr, false);
  ap.ArgStrings ("strings2", "\agive me some other strings", nullptr, true);

  const char *av[22];
  av[0] = "foo";
  av[1] = "baz";
  av[2] = "--flag1";
  av[3] = "--flag2";
  av[4] = "--int1=1";
  av[5] = "--int2=2";
  av[6] = "--ints1=1";
  av[7] = "--ints1=2";
  av[8] = "--ints2=1";
  av[9] = "--ints2=2";
  av[10] = "--float1=1.0";
  av[11] = "--float2=2.0";
  av[12] = "--floats1=1.0";
  av[13] = "--floats1=2.0";
  av[14] = "--floats2=1.0";
  av[15] = "--floats2=2.0";
  av[16] = "--string1=asdf";
  av[17] = "--string2=jkl;";
  av[18] = "--strings1=asdf";
  av[19] = "--strings1=jkl;";
  av[20] = "--strings2=asdf";
  av[21] = "--strings2=jkl;";

  bool ret = ap.Parse (22, av);
  EXPECT_TRUE (ret) << ap.ErrorMessage ();

  ArgParse::apstringvec lefto = ap.Leftovers ();
  EXPECT_EQ (2, lefto.Count ());
}
