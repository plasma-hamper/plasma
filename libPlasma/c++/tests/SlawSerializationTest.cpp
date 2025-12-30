
/* (c)  oblong industries */

// Unit tests for Slaw serialization to/from strings and files

#include <Slaw.h>

#include <libPlasma/c/slaw-io.h>

#include <gtest/gtest.h>
#include <libLoam/c/ob-sys.h>

#include "libLoam/c++/LoamStreams.h"
#include "libPlasma/c++/PlasmaStreams.h"
#include "libPlasma/c++/SlawIterator.h"

using namespace oblong::plasma;
using namespace oblong::loam;

namespace {

void test_string (const Slaw &s, const Slaw::YAMLFormat &fmt,
                  const Slaw &expected = Slaw ())
{
  ObRetort err (OB_OK);
  Str str = s.ToString (&err, fmt);
  EXPECT_EQ (OB_OK, err);
  EXPECT_EQ (expected.IsNull () ? s : expected, Slaw::FromString (str, &err));
  EXPECT_EQ (OB_OK, err);
}

void test_file (const Slaw &s, const Slaw::OutputFormat &fmt)
{
  Str file ("____slaw_serialization_test.slaw");
  ObRetort err (OB_OK);
  EXPECT_TRUE (s.ToFile (file, &err, fmt));
  EXPECT_EQ (OB_OK, err);
  EXPECT_EQ (s, Slaw::FromFile (file, &err));
  EXPECT_EQ (OB_OK, err);
  EXPECT_EQ (0L, unlink (file));
}

void test_serialization (const Slaw &s)
{
  for (int dirs = 0; dirs < 2; ++dirs)
    for (int ord = 0; ord < 2; ++ord)
      {
        test_string (s, Slaw::YAMLFormat (true, dirs, ord));
        test_file (s, Slaw::YAMLFormat (true, dirs, ord));
      }
  test_file (s, Slaw::BinaryFormat ());
}

TEST (SlawSerializationTest, Null)
{
  ObRetort err (OB_OK);
  EXPECT_FALSE (Slaw ().ToFile ("a_file", &err, Slaw::BinaryFormat ()));
  EXPECT_EQ (OB_ARGUMENT_WAS_NULL, err);
  err = OB_OK;
  EXPECT_FALSE (Slaw ().ToFile ("a_file", &err, Slaw::YAMLFormat ()));
  EXPECT_EQ (OB_ARGUMENT_WAS_NULL, err);
  err = OB_OK;
  EXPECT_TRUE (Slaw ().ToStringSlaw (&err).IsNull ());
  EXPECT_EQ (OB_ARGUMENT_WAS_NULL, err);
}

TEST (SlawSerializationTest, LousySerialization)
{
  struct
  {
    Slaw from;
    Slaw to;
  } data[] = {
    {Slaw (unt16 (124)), Slaw (int64 (124))},
    {Slaw (float32 (3.141)), Slaw (float64 (3.141))},
    {Slaw (int64 (12321312)), Slaw ()},
    {Slaw (int64 (1)), Slaw ()},
    {Slaw ("a string"), Slaw ()},
    {Slaw::List ("a", "b", int8 (2)), Slaw::List ("a", "b", int64 (2))}};

  const int N (sizeof (data) / sizeof (data[0]));

  for (int i = 0; i < N; ++i)
    for (int dirs = 0; dirs < 2; ++dirs)
      for (int ord = 0; ord < 2; ++ord)
        test_string (data[i].from, Slaw::YAMLFormat (false, dirs, ord),
                     data[i].to);
}

TEST (SlawSerializationTest, FromWithMultipleSlax)
{
  Str doubleTrouble = "%TAG ! tag:oblong.com,2009:slaw/ \n"
                      "--- \n"
                      "baz \n"
                      "--- \n"
                      "foo \n";
  Slaw fooSlaw ("baz");
  ObRetort err (OB_OK);
  EXPECT_EQ (fooSlaw, Slaw::FromString (doubleTrouble, &err));
  EXPECT_EQ (OB_OK, err);
}

TEST (SlawSerializationTest, Atomic)
{
  test_serialization (Slaw (unt8 (3)));
  test_serialization (Slaw ("foo"));
  test_serialization (Slaw (float64 (3.141592653589793)));
}

TEST (SlawSerializationTest, Cons)
{
  test_serialization (Slaw::Cons (3, "a"));
}

TEST (SlawSerializationTest, List)
{
  test_serialization (Slaw::List ());
  test_serialization (Slaw::List (Vect (2.1, 3, 8.0)));
  test_serialization (Slaw::List (5, "a", 2.3434, make_v2int32 (2, 3)));
}

TEST (SlawSerializationTest, Map)
{
  test_serialization (Slaw::Map ());
  test_serialization (Slaw::Map ("k0", "v0", 3, make_v3float64 (2.1, 1.2, 9)));
}

TEST (SlawSerializationTest, Protein)
{
  test_serialization (Slaw::Protein ());
  test_serialization (Slaw::Protein (Slaw::Nil (), Slaw::Map ("a", 3)));
}


void test_printable (const char *res, Slaw s)
{
  EXPECT_STREQ (res, s.ToPrintableString ());
}

static const char brandon[] =
  "%YAML 1.1\n"
  "%TAG ! tag:oblong.com,2009:slaw/\n"
  "--- !protein\n"
  "descrips:\n"
  "- grid-file-metadata\n"
  "ingests: !!omap\n"
  "- bbox-min-ISE: !vector [!f64 0, !f64 0, !f64 82962.4609375]\n"
  "- bbox-max-ISE: !vector [!f64 65606.2109375, !f64 226167.515625, !f64 "
  "119324.7578125]\n"
  "- version: !f64 1\n"
  "...\n";

TEST (SlawSerializationTest, ToPrintableString)
{
  test_printable ("10", Slaw (int32 (10)));
  test_printable ("-9999", Slaw (-9999));
  test_printable ("42", Slaw (unt8 (42)));
  test_printable ("100000", Slaw (unt64 (100000)));
  // test_printable ("1.0", Slaw (1.0));
  test_printable ("true", Slaw (true));
  test_printable ("false", Slaw (false));
  test_printable ("stringy", Slaw ("stringy"));
  test_printable ("(hola, 3)", Slaw::Cons ("hola", 3));
  test_printable ("[h, a, 0, [li]]",
                  Slaw::List ("h", "a", 0).ListAppend (Slaw::List ("li")));
  test_printable ("[(key, value), (key2, 3)]",
                  Slaw::Map ("key", "value", "key2", 3));
  ObRetort err (OB_OK);
  test_printable ("{[grid-file-metadata], [(bbox-min-ISE, <0, 0, "
                  "82962.4609375>), (bbox-max-ISE, <65606.2109375, "
                  "226167.515625, 119324.7578125>), (version, 1.000000)]}",
                  Slaw::FromString (brandon, &err));
  EXPECT_EQ (OB_OK, err);
}

TEST (SlawSerializationTest, Long)
{
  Str str (0x1D540);
  Slaw::YAMLFormat fmt;
  Slaw s (Slaw::List (str));
  for (int64 i = 0; i < 12; i++)
    {
      s = s.ListAppend (s);
      s = s.ListPrepend (i);
    }
  test_string (s, fmt);
  test_file (s, fmt);
}

}  // namespace
