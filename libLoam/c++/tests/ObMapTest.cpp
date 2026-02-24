
/* (c)  oblong industries */

#include <gtest/gtest.h>
#include "libLoam/c/ob-rand.h"
#include "libLoam/c++/LoamStreams.h"
#include "libLoam/c++/ObMap.h"
#include "libLoam/c++/Str.h"

using namespace oblong::loam;

static int livecount = 0;
class Dorf : public AnkleObject
{
  PATELLA_SUBCLASS (Dorf, AnkleObject);

 public:
  Str s;
  Dorf (Str s_) : s (s_) { livecount++; }
  ~Dorf () { livecount--; }
};

TEST (ObMapTest, RangedForLoopInts)
{
  ObMap<int64, int64> inttrove;
  inttrove.Put (1, 1);
  inttrove.Put (2, 2);
  inttrove.Put (3, 3);
  inttrove.Put (4, 4);
  inttrove.Put (5, 5);
  int sum = 0;
  for (auto i : inttrove)
    {
      static_assert (
        std::is_same<decltype (i), ObCons<int64, int64, UnspecifiedMemMgmt,
                                          UnspecifiedMemMgmt> *>::value,
        "std::begin(ObMap<int64, int64>) should return an "
        "iterator with elements of type ObCons<int64,int64, "
        "UnspecifiedMemMgmt, UnspecifiedMemMgmt>");
      sum += i->Car ();
    }
  EXPECT_EQ (5, inttrove.Count ());
  EXPECT_EQ (15, sum);
}

TEST (ObMapTest, RangedForLoopObRefs)
{
  ObMap<Dorf *, Dorf *> dorves;
  ObMap<Str, Str> strings;
  Dorf *tmp = new Dorf ("avert");
  dorves.Put (tmp, tmp);
  tmp = new Dorf ("bituminous");
  dorves.Put (tmp, tmp);
  tmp = new Dorf ("catastrophe");
  dorves.Put (tmp, tmp);

  for (auto dorf : dorves)
    {
      static_assert (
        std::is_same<decltype (dorf), ObCons<Dorf *, Dorf *, UnspecifiedMemMgmt,
                                             UnspecifiedMemMgmt> *>::value,
        "std::begin(ObMap<AnkleObject *, AnkleObject *>) should "
        "return an iterator with elements of type "
        "ObCons<AnkleObject *,AnkleObject *, UnspecifiedMemMgmt, "
        "UnspecifiedMemMgmt>");
      strings.Put (dorf->Car ()->s, dorf->Cdr ()->s);
    }
  EXPECT_EQ (strings.NthKey (0), "avert");
  EXPECT_EQ (strings.NthKey (1), "bituminous");
  EXPECT_EQ (strings.NthKey (2), "catastrophe");
}

TEST (ObMapTest, SelfCopy)
{
  ObMap<Str, Dorf *> dorves;

  dorves.Put ("one", new Dorf ("one"));
  dorves.Put ("two", new Dorf ("two"));
  dorves.Put ("three", new Dorf ("three"));
  dorves.Put ("four", new Dorf ("four"));
  dorves.Put ("five", new Dorf ("five"));

  ASSERT_EQ (livecount, dorves.Count ());

  dorves =
    *&dorves;  // annotate with *& to tell clang we intend self-assignment
  ASSERT_EQ (livecount, dorves.Count ());
}

TEST (ObMapTest, CopyCopy)
{
  ObMap<Str, Dorf *> dorves;

  dorves.Put ("one", new Dorf ("one"));
  dorves.Put ("two", new Dorf ("two"));
  dorves.Put ("three", new Dorf ("three"));
  dorves.Put ("four", new Dorf ("four"));
  dorves.Put ("five", new Dorf ("five"));

  ASSERT_EQ (livecount, dorves.Count ());

  ObMap<Str, Dorf *> dorves_the_sequel;
  dorves_the_sequel = dorves;
  ASSERT_EQ (livecount, dorves_the_sequel.Count ());
}
