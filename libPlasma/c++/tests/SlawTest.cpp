
/* (c)  oblong industries */

// Unit tests for Slaw

#include "SlawTypesTest.h"

#include "libLoam/c++/LoamStreams.h"
#include "libPlasma/c++/PlasmaStreams.h"
#include <libPlasma/c++/Slaw.h>
#include <libPlasma/c++/SlawIterator.h>
#include <libPlasma/c++/Protein.h>

#include <iostream>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <map>

using namespace oblong::plasma;
using namespace oblong::loam;

namespace {

class SlawTest : public SlawTypesTest
{
 protected:
  void assignment_tests (const Callbacks &c)
  {
    for (size_t j = 0; j < c.Count (); ++j)
      {
        Slaw s (c.SlawFromNthValue (j));
        EXPECT_NE (s, Slaw ());
        EXPECT_NE (Slaw (), s);
        EXPECT_TRUE (c.HasType (s));
        EXPECT_TRUE (c.CanEmit (s));
        EXPECT_TRUE (c.CheckNthValue (s, j));
        EXPECT_TRUE (s.IsAtomic ());
        EXPECT_FALSE (s.IsArray ());
        EXPECT_FALSE (s.IsComposite ());
        Slaw t (s);
        Slaw u (t);
        EXPECT_EQ (t, u);
        s = u;
        t = t = u;
        EXPECT_TRUE (t.IsAtomic ());
        EXPECT_TRUE (u.IsAtomic ());
        EXPECT_EQ (t, t);
        EXPECT_EQ (u, t);
        EXPECT_EQ (s, t);
      }
  }

  void containment_tests (const Callbacks &c)
  {
    std::vector<Slaw> sl;
    std::map<Str, Slaw> ml;
    std::vector<Str> keys;
    for (size_t j = 0; j < c.Count (); ++j)
      {
        std::ostringstream os ("arg");
        os << j;
        keys.push_back (Str (os.str ().c_str ()));
      }
    for (size_t j = 0; j < c.Count (); ++j)
      {
        slaw s (c.CSlawFromNthValue (j));
        Slaw ss (s);
        ml[keys[j]] = ss;
        sl.push_back (ss);
      }
    for (size_t j = 0; j < c.Count (); ++j)
      {
        EXPECT_TRUE (c.CheckNthValue (sl[j], j));
        EXPECT_TRUE (c.CheckNthValue (ml[keys[j]], j));
      }
  }

  void wrapped_slaw_test (const Callbacks &c) override
  {
    assignment_tests (c);
    containment_tests (c);
  }
};

DEFINE_SLAW_TESTS_FOR_CLASS (SlawTest);

TEST_F (SlawTest, Nullity)
{
  EXPECT_TRUE (Slaw ().IsNull ());
  EXPECT_EQ (Slaw (), Slaw ());

  EXPECT_EQ (Slaw::Nil (), Slaw::Nil ());
  EXPECT_NE (Slaw (), Slaw::Nil ());
  EXPECT_NE (Slaw (0), Slaw::Nil ());
  EXPECT_NE (Slaw (0), Slaw::Null ());
}

TEST_F (SlawTest, SelfEmit)
{
  Slaw s (33);
  Slaw t (Vect4 (1.2, 2.7182818, 3.14159, 0));
  Slaw v;
  EXPECT_TRUE (s.Into (v));
  EXPECT_EQ (s, v);
  EXPECT_TRUE (t.Into (v));
  EXPECT_EQ (t, v);
  EXPECT_NE (s, t);
}

TEST_F (SlawTest, CanEmit)
{
  Slaw str ("str");
  Slaw i32 (int32 (42));
  Slaw flt (float64 (3.141592653589793));

  EXPECT_FALSE (str.CanEmit<int8> ());
  EXPECT_FALSE (str.CanEmit<unt16> ());
  EXPECT_FALSE (str.CanEmit<int32> ());
  EXPECT_FALSE (str.CanEmit<float64> ());

  EXPECT_TRUE (i32.CanEmit<unt32> ());
  EXPECT_TRUE (i32.CanEmit<int64> ());
  EXPECT_FALSE (i32.CanEmit<const char *> ());
  EXPECT_FALSE (i32.CanEmit<v2int32> ());

  EXPECT_FALSE (flt.CanEmit<int8> ());
  EXPECT_FALSE (flt.CanEmit<unt16> ());
  EXPECT_FALSE (flt.CanEmit<int32> ());
  EXPECT_TRUE (flt.CanEmit<float32> ());

  Slaw istr (Str ("18"));
  EXPECT_TRUE (istr.CanEmit<int8> ());
  EXPECT_TRUE (istr.CanEmit<int16> ());
  EXPECT_TRUE (istr.CanEmit<int32> ());
  EXPECT_TRUE (istr.CanEmit<int64> ());
  EXPECT_TRUE (istr.CanEmit<unt8> ());
  EXPECT_TRUE (istr.CanEmit<unt16> ());
  EXPECT_TRUE (istr.CanEmit<unt32> ());
  EXPECT_TRUE (istr.CanEmit<unt64> ());
  EXPECT_TRUE (istr.CanEmit<float32> ());
  EXPECT_TRUE (istr.CanEmit<float64> ());
  EXPECT_EQ (18, istr.Emit<int8> ());

  Slaw vstr ("12,13");
  EXPECT_TRUE (vstr.CanEmit<v2int8> ());
  EXPECT_TRUE (vstr.CanEmit<v2unt64> ());
  EXPECT_TRUE (vstr.CanEmit<Vect> ());
  EXPECT_TRUE (vstr.CanEmit<Str> ());

  Slaw vstr3 ("12,13,23.18");
  EXPECT_TRUE (vstr3.CanEmit<v3float32> ());
  EXPECT_TRUE (vstr3.CanEmit<v3float64> ());
  EXPECT_TRUE (vstr3.CanEmit<Vect> ());
  EXPECT_TRUE (vstr3.CanEmit<Str> ());
}

// XXX: Google Test won't let me use "SlawTest" for a TEST,
// since "SlawTest" was already used with TEST_F.
TEST (SlawTest1, Spew)
{
  Slaw s = Slaw::Map ("type", Str ("Blank"), "num", (int32) 96);
  std::ostringstream silly_string;
  s.Spew (silly_string);
  Str actual (silly_string.str ().c_str ());
  Str expected ("#MAP(2)<\nKEY: slaw[xxx]: STR(4): \"type\"\n"
                "VALUE: slaw[xxx]: STR(5): \"Blank\"\n"
                "KEY: slaw[xxx]: STR(3): \"num\"\n"
                "VALUE: slaw[xxx]: INT32 = 96\n>");
  actual.ReplaceAll ("slaw\\[\\d+[oq]\\.[0-9A-Fa-fxX]+\\]:", "slaw[xxx]:");
  EXPECT_STREQ (expected, actual);
}

class SomeSlaw
{
 public:
  Slaw meaning;
  Slaw amusing_number;
  Slaw png_comment;
  Slaw south_park;
  Slaw nuthin;
  Slaw really_nuthin;
  Slaw larger_meaning;
  Slaw negativity;
  Slaw sixty_four_bits;
  Slaw really;
  Slaw not_so_much;
  Slaw unready;
  Slaw foo;
  Slaw empty;

  SomeSlaw ();
};

SomeSlaw::SomeSlaw ()
    : meaning (int8 (42)),
      amusing_number (unt64 (OB_CONST_U64 (0xdadaddedbaddecaf))),
      png_comment ("Not the Borland DOS special memory handler"),
      south_park ("Osama bin Laden Has Farty Pants"),
      nuthin (slaw_nil ()),
      really_nuthin (),
      larger_meaning (int32 (42)),
      negativity (int8 (-1)),
      sixty_four_bits (unt64 (OB_CONST_U64 (0xdefacedbadfacade))),
      really (bool (true)),
      not_so_much (bool (false)),
      unready ("\303\246thelred-the-unready"),
      foo ("foo"),
      empty ("")
{
}

TEST (SlawTest1, StdMap)
{
  SomeSlaw some1;
  SomeSlaw some2;
  std::map<Slaw, Slaw> m;
  m[some1.meaning] = some1.amusing_number;
  m[some1.amusing_number] = some1.png_comment;
  m[some1.png_comment] = some1.south_park;
  m[some1.south_park] = some1.nuthin;
  m[some1.nuthin] = some1.really_nuthin;
  m[some1.really_nuthin] = some1.larger_meaning;
  m[some1.larger_meaning] = some1.negativity;
  m[some1.negativity] = some1.sixty_four_bits;
  m[some1.sixty_four_bits] = some1.really;
  m[some1.really] = some1.not_so_much;
  m[some1.not_so_much] = some1.unready;
  m[some1.unready] = some1.foo;
  m[some1.foo] = some1.empty;
  m[some1.empty] = some1.meaning;

  EXPECT_EQ (some2.amusing_number, m[some2.meaning]);
  EXPECT_EQ (some2.png_comment, m[some2.amusing_number]);
  EXPECT_EQ (some2.south_park, m[some2.png_comment]);
  EXPECT_EQ (some2.nuthin, m[some2.south_park]);
  EXPECT_EQ (some2.really_nuthin, m[some2.nuthin]);
  EXPECT_EQ (some2.larger_meaning, m[some2.really_nuthin]);
  EXPECT_EQ (some2.negativity, m[some2.larger_meaning]);
  EXPECT_EQ (some2.sixty_four_bits, m[some2.negativity]);
  EXPECT_EQ (some2.really, m[some2.sixty_four_bits]);
  EXPECT_EQ (some2.not_so_much, m[some2.really]);
  EXPECT_EQ (some2.unready, m[some2.not_so_much]);
  EXPECT_EQ (some2.foo, m[some2.unready]);
  EXPECT_EQ (some2.empty, m[some2.foo]);
  EXPECT_EQ (some2.meaning, m[some2.empty]);
}

TEST (SlawTest1, StdUnorderedMap)
{
  SomeSlaw some1;
  SomeSlaw some2;
  std::unordered_map<Slaw, Slaw> m;
  m[some1.meaning] = some1.amusing_number;
  m[some1.amusing_number] = some1.png_comment;
  m[some1.png_comment] = some1.south_park;
  m[some1.south_park] = some1.nuthin;
  m[some1.nuthin] = some1.really_nuthin;
  m[some1.really_nuthin] = some1.larger_meaning;
  m[some1.larger_meaning] = some1.negativity;
  m[some1.negativity] = some1.sixty_four_bits;
  m[some1.sixty_four_bits] = some1.really;
  m[some1.really] = some1.not_so_much;
  m[some1.not_so_much] = some1.unready;
  m[some1.unready] = some1.foo;
  m[some1.foo] = some1.empty;
  m[some1.empty] = some1.meaning;

  EXPECT_EQ (some2.amusing_number, m[some2.meaning]);
  EXPECT_EQ (some2.png_comment, m[some2.amusing_number]);
  EXPECT_EQ (some2.south_park, m[some2.png_comment]);
  EXPECT_EQ (some2.nuthin, m[some2.south_park]);
  EXPECT_EQ (some2.really_nuthin, m[some2.nuthin]);
  EXPECT_EQ (some2.larger_meaning, m[some2.really_nuthin]);
  EXPECT_EQ (some2.negativity, m[some2.larger_meaning]);
  EXPECT_EQ (some2.sixty_four_bits, m[some2.negativity]);
  EXPECT_EQ (some2.really, m[some2.sixty_four_bits]);
  EXPECT_EQ (some2.not_so_much, m[some2.really]);
  EXPECT_EQ (some2.unready, m[some2.not_so_much]);
  EXPECT_EQ (some2.foo, m[some2.unready]);
  EXPECT_EQ (some2.empty, m[some2.foo]);
  EXPECT_EQ (some2.meaning, m[some2.empty]);
}

TEST (SlawTest1, Foreach)
{
  std::ostringstream os;
  Slaw p (Slaw::List ("a", "b", "c"));
  for (const Slaw &descrip : p)
    {
      descrip.Spew (os);
      os << std::endl;
    }
  Str actual (os.str ().c_str ());
  actual.ReplaceAll ("slaw\\[\\d+[oq]\\.[0-9A-Fa-fxX]+\\]:", "slaw[xxx]:");
  const Str expected ("slaw[xxx]: STR(1): \"a\"\nslaw[xxx]: "
                      "STR(1): \"b\"\nslaw[xxx]: STR(1): \"c\"\n");
  EXPECT_STREQ (expected, actual);
}

}  // namespace
