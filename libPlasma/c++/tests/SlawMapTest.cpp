
/* (c)  oblong industries */

// Unit tests for SlawMap

#include "SlawTypesTest.h"

#include "libLoam/c++/LoamStreams.h"
#include "libPlasma/c++/PlasmaStreams.h"
#include <libPlasma/c++/SlawIterator.h>
#include <libPlasma/c++/Slaw.h>

using namespace oblong::plasma;

namespace {

class SlawMapTest : public SlawTypesTest
{
 protected:
  void basic_put_find_test (const Callbacks &c)
  {
    Slaw m (c.MapWithValues ("foo"));
    Slaw rm (c.MapWithKeys ("foo"));
    EXPECT_EQ (1, m.Count ());
    EXPECT_FALSE (m.Find ("foo").IsNull ());
    Slaw sfnd = m.Find ("foo");
    EXPECT_TRUE (c.CheckNthValue (sfnd, c.Count () - 1));
    const Slaw value = Slaw ("foo");
    for (size_t i = 0; i < c.Count (); ++i)
      {
        Slaw key (c.SlawFromNthValue (i));
        EXPECT_FALSE (rm.Find (key).IsNull ());
        EXPECT_FALSE (rm.MapFind (key).IsNull ());
        EXPECT_EQ (value, rm.Find (key));
        EXPECT_EQ (value, rm.MapFind (key));
      }
  }

  void map_collect_slaw_test (const Callbacks &c)
  {
    const size_t M (c.Count ());
    std::vector<Slaw> slawx;
    for (size_t i = 0; i < M; ++i)
      slawx.push_back (c.SlawFromNthValue (i));
    Slaw map = Slaw::MapCollect (slawx.begin (), slawx.end ());
    for (size_t i = 0; i < (M & ~1UL); i += 2)
      {
        Slaw key (c.SlawFromNthValue (i));
        Slaw value = map.Find (key);
        EXPECT_FALSE (value.IsNull ());
        EXPECT_EQ (value, map.MapFind (key));
        if (i < c.Count () - 1)
          {
            EXPECT_TRUE (c.CheckNthValue (value, i + 1)) << map;
          }
      }
  }

  void wrapped_slaw_test (const Callbacks &c) override
  {
    basic_put_find_test (c);
    map_collect_slaw_test (c);
  }
};

DEFINE_SLAW_TESTS_FOR_CLASS (SlawMapTest);

TEST_F (SlawMapTest, EmptyMap)
{
  Slaw map (Slaw::Map ());
  EXPECT_TRUE (map.IsMap ());
  EXPECT_FALSE (map.IsNull ());
  EXPECT_EQ (0L, map.Count ());
  EXPECT_TRUE (slaw_is_map (map.SlawValue ()));
  EXPECT_EQ (Slaw (), map.Find ("fol"));

  EXPECT_EQ (map, map.MapPut (map));
}

TEST_F (SlawMapTest, BasicTests)
{
  Slaw k ("foo");
  Slaw v (int32 (42));
  Slaw m = Slaw ().MapPut (k, v);
  EXPECT_TRUE (m.IsMap ());
  Slaw vv = m.Find (k);
  EXPECT_FALSE (vv.IsNull ());
  EXPECT_TRUE (vv.CanEmit<int32> ());
  EXPECT_EQ (42, vv.Emit<int32> ());
  EXPECT_EQ (42, m.Find (k).Emit<int32> ());

  EXPECT_EQ (m, m.MapPut (Slaw::Null ()));
  Slaw m2 =
    m.MapPut ("foo", 12).MapPut ("foo", int32 (58)).MapPut (int32 (3), 5);
  EXPECT_TRUE (m2.IsMap ());
  EXPECT_EQ (2, m2.Count ());
  EXPECT_FALSE (m2.Find ("foo").IsNull ());
  EXPECT_FALSE (m2.MapFind ("foo").IsNull ());
  EXPECT_EQ (58, m2.Find (k).Emit<int32> ());
  EXPECT_EQ (58, m2.MapFind (k).Emit<int32> ());
  EXPECT_FALSE (m2.Find (int32 (3)).IsNull ());
  EXPECT_FALSE (m2.MapFind (int32 (3)).IsNull ());

  m2 = m2.MapRemove (k);
  EXPECT_EQ (1, m2.Count ());
  EXPECT_TRUE (m2.Find (k).IsNull ());
  EXPECT_TRUE (m2.MapFind (k).IsNull ());
  EXPECT_FALSE (m2.Find (int32 (3)).IsNull ());
  EXPECT_FALSE (m2.MapFind (int32 (3)).IsNull ());
  m2 = m2.MapRemove ("bar");
  EXPECT_EQ (1, m2.Count ());
  EXPECT_TRUE (m2.Find ("foo").IsNull ());
  EXPECT_TRUE (m2.MapFind ("foo").IsNull ());
  EXPECT_FALSE (m2.Find (int32 (3)).IsNull ());
  EXPECT_FALSE (m2.MapFind (int32 (3)).IsNull ());

  EXPECT_EQ (m, m.MapRemove (Slaw ()));
  EXPECT_EQ (m2, m2.MapRemove (Slaw ()));
}

TEST_F (SlawMapTest, KeyValueDups)
{
  Slaw m (Slaw::Map ("a", "b", "foo", 3, "b", 0));
  EXPECT_EQ (Slaw (0), m.Find ("b"));
}

TEST_F (SlawMapTest, KeyValueConstruction)
{
  slaw k = make_slaw ("foo");
  slaw v = make_slaw (float64 (1.0001));
  slaw k2 = make_slaw (int64 (56));
  slaw v2 = make_slaw ("bar");
  const slaw kvs[] = {k, v, k2, v2};
  const slaw kvs2[] = {slaw_dup (k), slaw_dup (v), slaw_dup (k2),
                       slaw_dup (v2)};

  Slaw m (Slaw::MapCollect (kvs, kvs + 4));
  Slaw m2 (Slaw::MapCollect (kvs2, kvs2 + 4));

  EXPECT_EQ (m, m2);
  EXPECT_EQ (m2, m);
  EXPECT_EQ (2, m2.Count ());
  EXPECT_TRUE (slawx_equal (m2.Find ("foo").SlawValue (), v));
  EXPECT_TRUE (slawx_equal (m2.Find (k2).SlawValue (), v2));

  EXPECT_EQ (Slaw::Map (), Slaw::Map (make_slaw ("key"), slaw (NULL)));
}

void check_elements (slaw *values, size_t n, const Slaw &m)
{
  Slaw keys (m.MapKeys ());
  Slaw vals (m.MapValues ());
  EXPECT_EQ (int64 (n / 2), m.Count ());
  EXPECT_EQ (int64 (n / 2), keys.Count ());
  EXPECT_EQ (int64 (n / 2), vals.Count ());

  for (size_t i = 0; i < n; i += 2)
    {
      EXPECT_TRUE (
        slawx_equal (m.Find (values[i]).SlawValue (), values[i + 1]));
      EXPECT_LE (0L, keys.IndexOf (values[i]));
      EXPECT_LE (0L, vals.IndexOf (values[i + 1]));
    }
}

TEST_F (SlawMapTest, ExplicitConstructors)
{
  slaw sls[14] = {
    make_slaw (3),       make_slaw (4),         make_slaw ("foo"),
    make_slaw ("e"),     make_slaw (3.14),      make_slaw (3.14),
    make_slaw ("bar"),   make_slaw (2),         make_slaw (int64 (2000)),
    make_slaw ("int64"), make_slaw (unt32 (2)), make_slaw ("aha"),
    make_slaw ("baz"),   make_slaw (1234543)};

  Slaw Sls[14];
  for (size_t i = 0; i < 14; ++i)
    Sls[i] = Slaw (slaw_dup (sls[i]));

  Slaw m = Slaw::Map (slaw_dup (sls[0]), slaw_dup (sls[1]), slaw_dup (sls[2]),
                      slaw_dup (sls[3]), slaw_dup (sls[4]), slaw_dup (sls[5]),
                      slaw_dup (sls[6]), slaw_dup (sls[7]), slaw_dup (sls[8]),
                      slaw_dup (sls[9]), slaw_dup (sls[10]), slaw_dup (sls[11]),
                      slaw_dup (sls[12]), slaw_dup (sls[13]));
  check_elements (sls, 14, m);
  EXPECT_EQ (7, m.Count ());
  m = Slaw::Map (Sls[0], Sls[1], Sls[2], Sls[3], Sls[4], Sls[5], Sls[6], Sls[7],
                 Sls[8], Sls[9], Sls[10], Sls[11], Sls[12], Sls[13]);
  check_elements (sls, 14, m);
  EXPECT_EQ (7, m.Count ());

  m = Slaw::Map (slaw_dup (sls[0]), slaw_dup (sls[1]), slaw_dup (sls[2]),
                 slaw_dup (sls[3]), slaw_dup (sls[4]), slaw_dup (sls[5]),
                 slaw_dup (sls[6]), slaw_dup (sls[7]), slaw_dup (sls[8]),
                 slaw_dup (sls[9]), slaw_dup (sls[10]), slaw_dup (sls[11]));
  check_elements (sls, 12, m);
  EXPECT_EQ (6, m.Count ());
  m = Slaw::Map (Sls[0], Sls[1], Sls[2], Sls[3], Sls[4], Sls[5], Sls[6], Sls[7],
                 Sls[8], Sls[9], Sls[10], Sls[11]);
  check_elements (sls, 12, m);
  EXPECT_EQ (6, m.Count ());

  m = Slaw::Map (slaw_dup (sls[0]), slaw_dup (sls[1]), slaw_dup (sls[2]),
                 slaw_dup (sls[3]), slaw_dup (sls[4]), slaw_dup (sls[5]),
                 slaw_dup (sls[6]), slaw_dup (sls[7]), slaw_dup (sls[8]),
                 slaw_dup (sls[9]));
  check_elements (sls, 10, m);
  EXPECT_EQ (5, m.Count ());
  m = Slaw::Map (Sls[0], Sls[1], Sls[2], Sls[3], Sls[4], Sls[5], Sls[6], Sls[7],
                 Sls[8], Sls[9]);
  check_elements (sls, 10, m);
  EXPECT_EQ (5, m.Count ());

  m = Slaw::Map (slaw_dup (sls[0]), slaw_dup (sls[1]), slaw_dup (sls[2]),
                 slaw_dup (sls[3]), slaw_dup (sls[4]), slaw_dup (sls[5]),
                 slaw_dup (sls[6]), slaw_dup (sls[7]));
  check_elements (sls, 8, m);
  EXPECT_EQ (4, m.Count ());
  m =
    Slaw::Map (Sls[0], Sls[1], Sls[2], Sls[3], Sls[4], Sls[5], Sls[6], Sls[7]);
  check_elements (sls, 8, m);
  EXPECT_EQ (4, m.Count ());

  m = Slaw::Map (slaw_dup (sls[0]), slaw_dup (sls[1]), slaw_dup (sls[2]),
                 slaw_dup (sls[3]), slaw_dup (sls[4]), slaw_dup (sls[5]));
  check_elements (sls, 6, m);
  EXPECT_EQ (3, m.Count ());
  m = Slaw::Map (Sls[0], Sls[1], Sls[2], Sls[3], Sls[4], Sls[5]);
  check_elements (sls, 6, m);
  EXPECT_EQ (3, m.Count ());

  m = Slaw::Map (slaw_dup (sls[0]), slaw_dup (sls[1]), slaw_dup (sls[2]),
                 slaw_dup (sls[3]));
  check_elements (sls, 4, m);
  EXPECT_EQ (2, m.Count ());
  m = Slaw::Map (Sls[0], Sls[1], Sls[2], Sls[3]);
  check_elements (sls, 4, m);
  EXPECT_EQ (2, m.Count ());

  m = Slaw::Map (slaw_dup (sls[0]), slaw_dup (sls[1]));
  check_elements (sls, 2, m);
  EXPECT_EQ (1, m.Count ());
  m = Slaw::Map (Sls[0], Sls[1]);
  check_elements (sls, 2, m);
  EXPECT_EQ (1, m.Count ());

  for (size_t i = 0; i < 14; ++i)
    slaw_free (sls[i]);
}

TEST_F (SlawMapTest, UnorderedEquality)
{
  int32 v = 32;
  Slaw a = Slaw::Cons ("foo", v);
  Slaw b = Slaw::Cons (3.14, "bar");
  Slaw c = Slaw::Cons ("qux", "qux");

  Slaw m = a.MapPut (b).MapPut (c);
  Slaw m2 = b.MapPut (c).MapPut (a).MapPut (b);

  EXPECT_EQ (3, m.Count ()) << m;
  EXPECT_EQ (3, m2.Count ()) << m;

  EXPECT_EQ (a.Cdr (), m.Find (a.Car ()));
  EXPECT_EQ (b.Cdr (), m.Find (b.Car ()));
  EXPECT_EQ (c.Cdr (), m.Find (c.Car ()));

  EXPECT_EQ (a.Cdr (), m2.Find (a.Car ()));
  EXPECT_EQ (b.Cdr (), m2.Find (b.Car ()));
  EXPECT_EQ (c.Cdr (), m2.Find (c.Car ()));

  EXPECT_NE (m, m2);
  EXPECT_NE (m2, m);

  Slaw m3 = Slaw ().MapPut (a, b);
  EXPECT_NE (m, m3);
  EXPECT_NE (m3, m);
  EXPECT_NE (m2, m3);
  EXPECT_NE (m3, m2);

  Slaw m4 (Slaw::Map ("a", 1, "b", 2, "c", 3, "d", 4));
  Slaw m5 (Slaw::Map ("a", 1, "b", 1, "c", 3, "d", 4));
  Slaw m6 (Slaw::Map ("b", 1, "a", 1, "d", 4, "c", 3));
  Slaw m7 (Slaw::Map ("b", 1, "a", 3, "a", 1, "d", 4, "c", 3));
  Slaw m8 (Slaw::Map ("b", 1, "a", 3, "d", 4, "a", 1, "c", 3));
  EXPECT_NE (m4, m5);
  EXPECT_NE (m4, m6);
  EXPECT_NE (m5, m6);
  EXPECT_EQ (m6, m7);
  EXPECT_EQ (m7, m6);
  EXPECT_EQ (m6, m8);
  EXPECT_EQ (m8, m7);

  Slaw l (Slaw::List (Slaw::Cons ("b", 1), Slaw::Cons ("a", 1),
                      Slaw::Cons ("d", 4), Slaw::Cons ("c", 3)));
  EXPECT_EQ (m7, l);
  EXPECT_EQ (l, m7);
  l = l.ListAppend (Slaw::Cons ("a", 1));
  EXPECT_NE (m7, l);
  EXPECT_NE (l, m7);

  Slaw l2 (Slaw::List ("b", 1, "a", 1, "d", 4, "c").ListAppend (3));
  EXPECT_NE (l2, m7);
  EXPECT_NE (m7, l2);
}

TEST_F (SlawMapTest, ContructionWithDuplicates)
{
  slaw k = make_slaw ("foo");
  slaw kvs[] = {k, k, k, k};
  Slaw m (Slaw::MapCollect (kvs, kvs + 4));
  EXPECT_EQ (1L, m.Count ());
  EXPECT_STREQ ("foo", m.Find (k).Emit<const char *> ()) << m[0][0];

  Slaw m2 (Slaw::Map ("foo", 0, 1, 1, 2, 2, "foo", 1, 1, "bar"));
  EXPECT_EQ (3L, m2.Count ()) << m2;
}

TEST_F (SlawMapTest, OddConstruction)
{
  slaw kvs[6] = {make_slaw ("foo"), make_slaw (int32 (12)),
                 make_slaw ("bar"), NULL,
                 make_slaw ("baz"), make_slaw ("whatever")};
  Slaw m (Slaw::MapCollect (kvs, kvs + 6));
  EXPECT_EQ (2, m.Count ());
  EXPECT_TRUE (slawx_equal (m.Find (kvs[0]).SlawValue (), kvs[1]));
  EXPECT_TRUE (m.Find ("bar").IsNull ());
  EXPECT_TRUE (slawx_equal (m.Find (kvs[4]).SlawValue (), kvs[5]));

  slaw mkvs[3] = {make_slaw (3), make_slaw (false), make_slaw ("odd_key")};
  m = Slaw::MapCollect (mkvs, mkvs + 3);
  EXPECT_EQ (1L, m.Count ());
  EXPECT_TRUE (slawx_equal (m.Find (mkvs[0]).SlawValue (), mkvs[1]));
  EXPECT_TRUE (m.Find ("odd_key").IsNull ());
}

TEST_F (SlawMapTest, FromSlawList)
{
  slaw kvs[6] = {make_slaw ("foo"), make_slaw (int32 (12)),
                 make_slaw ("bar"), make_slaw (3),
                 make_slaw ("baz"), make_slaw ("whatever")};

  slabu *sb (slabu_new ());
  for (int i = 0; i < 6; ++i)
    slabu_list_add (sb, kvs[i]);

  Slaw map (slaw_list_f (sb));

  EXPECT_TRUE (map.IsList ());
  EXPECT_EQ (6, map.Count ());

  EXPECT_EQ (Slaw::List (), map.MapKeys ());
  EXPECT_EQ (Slaw::List (), map.MapValues ());

  for (int i = 0; i < 6; ++i)
    {
      Slaw s (kvs[i]);
      EXPECT_EQ (Slaw (), map.Find (s));
      EXPECT_EQ (Slaw (), map.MapFind (s));
    }
}


TEST_F (SlawMapTest, FromMixedSlawList)
{
  Slaw cmps[] = {Slaw (3), Slaw::Cons ("key", "value"), Slaw ("a"),
                 Slaw::Cons ("a", 2)};
  Slaw list = Slaw::ListCollect (cmps, cmps + 4);
  EXPECT_EQ (Slaw::List ("key", "a"), list.MapKeys ());
  EXPECT_EQ (Slaw::List ("value", 2), list.MapValues ());
  EXPECT_EQ (Slaw (), list.Find (3));
  EXPECT_EQ (Slaw ("value"), list.Find ("key"));
  EXPECT_EQ (Slaw (2), list.Find ("a"));

  EXPECT_EQ (Slaw (), list.MapFind (3));
  EXPECT_EQ (Slaw ("value"), list.MapFind ("key"));
  EXPECT_EQ (Slaw (2), list.MapFind ("a"));
}

TEST_F (SlawMapTest, FromConsList)
{
  slaw kvs[6] = {make_slaw ("foo"), make_slaw (int32 (12)),
                 make_slaw ("bar"), make_slaw (3),
                 make_slaw ("baz"), make_slaw ("whatever")};
  slabu *sb (slabu_new ());
  for (int i = 0; i < 6; i += 2)
    slabu_list_add_f (sb, slaw_cons (kvs[i], kvs[i + 1]));
  Slaw map (slaw_list_f (sb));
  EXPECT_TRUE (map.IsList ());
  EXPECT_EQ (3, map.Count ());
  EXPECT_EQ (Slaw (kvs[1]), map.Find ("foo"));
  EXPECT_EQ (Slaw (kvs[3]), map.Find ("bar"));
  EXPECT_EQ (Slaw (kvs[5]), map.Find ("baz"));

  EXPECT_EQ (map.Find ("foo"), map.MapFind ("foo"));
  EXPECT_EQ (map.Find ("bar"), map.MapFind ("bar"));
  EXPECT_EQ (map.Find ("baz"), map.MapFind ("baz"));

  for (int i = 0; i < 6; i += 2)
    slaw_free (kvs[i]);
}

TEST_F (SlawMapTest, FromSlawConsWithDups)
{
  slaw kvs[10] = {make_slaw ("foo"), make_slaw (int32 (12)),
                  make_slaw ("baz"), make_slaw ("whatever"),
                  make_slaw ("bar"), make_slaw (3),
                  make_slaw ("bar"), make_slaw (5),
                  make_slaw ("foo"), make_slaw ("wins")};
  slabu *sb (slabu_new ());
  for (int i = 0; i < 10; i += 2)
    slabu_list_add_f (sb, slaw_cons (kvs[i], kvs[i + 1]));

  Slaw map (slaw_list_f (sb));
  EXPECT_TRUE (map.IsList ());

  EXPECT_EQ (5, map.Count ());
  EXPECT_EQ (3, map.MapKeys ().Count ());
  EXPECT_EQ (Slaw::List ("foo", "baz", "bar"), map.MapKeys ());
  EXPECT_EQ (3, map.MapValues ().Count ());
  EXPECT_EQ (Slaw::List ("wins", "whatever", 5), map.MapValues ());

  EXPECT_EQ (Slaw (kvs[9]), map.Find ("foo"));
  EXPECT_EQ (Slaw (kvs[7]), map.Find ("bar"));
  EXPECT_EQ (Slaw (kvs[3]), map.Find ("baz"));

  EXPECT_EQ (map.Find ("foo"), map.MapFind ("foo"));
  EXPECT_EQ (map.Find ("bar"), map.MapFind ("bar"));
  EXPECT_EQ (map.Find ("baz"), map.MapFind ("baz"));

  for (int i = 0; i < 10; i += 2)
    slaw_free (kvs[i]);
  slaw_free (kvs[1]);
  slaw_free (kvs[5]);
}

TEST_F (SlawMapTest, MapRemove)
{
  Slaw alist (Slaw::List (Slaw::Cons ("a", "b"), Slaw::Cons ("c", "d"),
                          Slaw::Cons ("a", 4), Slaw::Cons ("e", 5)));
  EXPECT_EQ (4, alist.Count ());

  Slaw map (alist.MapRemove ("a"));
  EXPECT_EQ (2, map.Count ());
  EXPECT_EQ (Slaw::List ("c", "e"), map.MapKeys ());
  EXPECT_EQ (Slaw::List ("d", 5), map.MapValues ());

  EXPECT_EQ (Slaw::MapCollect (alist.begin (), alist.end ()),
             alist.MapRemove (Slaw ()));

  EXPECT_EQ (map, map.MapRemove (Slaw ()));

  EXPECT_EQ (Slaw::Map (), Slaw ().MapRemove (Slaw ()));
  EXPECT_EQ (Slaw::Map (), Slaw ().MapRemove (Slaw (42)));

  map = Slaw::Map ("a", 0, 0, "a", "b", 0);
  map = map.MapRemove (0);
  EXPECT_EQ (2L, map.Count ());
  EXPECT_EQ (Slaw (0), map.Find ("a"));
  EXPECT_EQ (Slaw (), map.Find (0));
  EXPECT_EQ (Slaw (0), map.Find ("b"));
}

TEST_F (SlawMapTest, Nth)
{
  const int32 SIZE (5);
  slabu *sb (slabu_new ());
  for (int32 i = 0; i < SIZE; ++i)
    slabu_map_put_ff (sb, slaw_int32 (i), slaw_int32 (2 * i));
  Slaw map (slaw_map_f (sb));
  EXPECT_TRUE (map.IsMap ());
  EXPECT_EQ (SIZE, map.Count ());
  for (int i = 0; i < SIZE; ++i)
    {
      EXPECT_TRUE (map.Nth (i).IsCons ()) << map.Nth (i);
      EXPECT_EQ (i, map.Nth (i).Car ().Emit<int32> ());
      EXPECT_EQ (2 * i, map.Nth (i).Cdr ().Emit<int32> ());
    }
}

TEST_F (SlawMapTest, MapOfMaps)
{
  Slaw map = Slaw::Map (
    unt32 (2), Slaw::Map ("blue", unt32 (0), "logo", unt32 (1)), unt32 (3),
    Slaw::Map ("yellow", unt32 (0), "green", unt32 (1), "blue", unt32 (2)));

  Slaw keys (Slaw::List (unt32 (2), unt32 (3)));
  Slaw vals (Slaw::List (map.Find (unt32 (2)), map.Find (unt32 (3))));
  EXPECT_EQ (keys, map.MapKeys ());
  EXPECT_EQ (vals, map.MapValues ());

  Slaw map2 (map.Find (unt32 (2)));
  EXPECT_TRUE (map2.IsMap ());
  EXPECT_EQ (2L, map2.Count ());
  EXPECT_EQ (Slaw (unt32 (0)), map2.Find ("blue"));
  EXPECT_EQ (Slaw (unt32 (1)), map2.Find ("logo"));

  Slaw map3 (map.Find (unt32 (3)));
  EXPECT_TRUE (map3.IsMap ());
  EXPECT_EQ (3L, map3.Count ());
  EXPECT_EQ (Slaw (unt32 (0)), map3.Find ("yellow"));
  EXPECT_EQ (Slaw (unt32 (2)), map3.Find ("blue"));
  EXPECT_EQ (Slaw (unt32 (1)), map3.Find ("green"));
}

void test_unordered_equal (Slaw a, Slaw b)
{
  EXPECT_EQ (a.Count (), b.Count ());
  for (int64 i = 0, s = a.Count (); i < s; ++i)
    {
      EXPECT_LT (-1, b.IndexOf (a[i]));
      EXPECT_LT (-1, a.IndexOf (b[i]));
    }
}

TEST_F (SlawMapTest, Merge)
{
  Slaw m0 (Slaw::Map ("foo", int32 (1), "bar", int32 (2)));
  Slaw m1;
  Slaw m01 (m0.MapMerge (m1));
  EXPECT_EQ (m0, m01);
  m01 = m1.MapMerge (m0);
  EXPECT_EQ (m0, m01);

  m1 = Slaw::Map ("baz", int32 (3), "qux", int32 (4));
  m01 = m1.MapMerge (m0);
  Slaw m10 = m0.MapMerge (m1);
  test_unordered_equal (m01.MapKeys (), m10.MapKeys ());
  test_unordered_equal (m01.MapValues (), m10.MapValues ());
  EXPECT_EQ (m0.Count () + m1.Count (), m01.Count ());
  for (int64 i = 0, N = m0.Count (); i < N; ++i)
    {
      Slaw cons (m0[i]);
      EXPECT_EQ (cons.Cdr (), m01.Find (cons.Car ()));
      EXPECT_EQ (cons.Cdr (), m10.Find (cons.Car ()));
    }
  for (int64 i = 0, N = m1.Count (); i < N; ++i)
    {
      Slaw cons (m1[i]);
      EXPECT_EQ (cons.Cdr (), m01.Find (cons.Car ()));
      EXPECT_EQ (cons.Cdr (), m10.Find (cons.Car ()));
    }

  Slaw m0110 = m01.MapMerge (m10);
  EXPECT_EQ (m01, m0110);

  m1 = m1.MapPut ("baz", int32 (5));
  m0110 = m0110.MapMerge (m1);
  EXPECT_EQ (m01.Count (), m0110.Count ());
  EXPECT_NE (m01, m0110);
  EXPECT_EQ (m1.Find ("baz"), m0110.Find ("baz"));

  EXPECT_EQ (m1, m1.MapMerge (Slaw ()));
  EXPECT_EQ (m1, Slaw ().MapMerge (m1));
}


TEST_F (SlawMapTest, ConstructVersusMerge)
{
  Slaw s0 =
    Slaw::Map ("aspic", 1, "bunty", 2, "crepe", 3, "dirge", 4, "elbow", 5);

  Slaw sa = Slaw::Map ("aspic", 1, "bunty", 2, "crepe", 3);
  Slaw sb = Slaw::Map ("dirge", 4, "elbow", 5);

  Slaw sab = sa.MapMerge (sb);
  Slaw sba = sb.MapMerge (sa);

  EXPECT_TRUE (s0 == sab);
  EXPECT_FALSE (s0 == sba);

  bslaw s_0 = s0.SlawValue ();
  bslaw s_ab = sab.SlawValue ();
  bslaw s_ba = sba.SlawValue ();

  EXPECT_TRUE (slawx_equal (s_0, s_ab));
  EXPECT_FALSE (slawx_equal (s_0, s_ba));
}


TEST_F (SlawMapTest, MergeOverlap)
{
  Slaw m0 (Slaw::Map (0, 0, 1, 1, 2, 2, 3, 3));
  Slaw m1 (Slaw::Map (1, 1, 2, 2, 3, 3, 4, 4));
  Slaw m01 (m0.MapMerge (m1));
  EXPECT_EQ (m0.Count () + 1, m01.Count ()) << m01;
}

TEST_F (SlawMapTest, MergeWithCSlaw)
{
  Slaw m0 (Slaw::Map ("foo", int32 (1), "bar", int32 (2)));
  Slaw m1 = Slaw::Map ("baz", int32 (3), "qux", int32 (4));
  m0 = Slaw (slaw_dup (m0.SlawValue ()));
  m1 = Slaw (slaw_dup (m1.SlawValue ()));
  Slaw m01 = m0.MapMerge (m1);
  EXPECT_EQ (m0.Count () + m1.Count (), m01.Count ());
  for (int64 i = 0, N = m0.Count (); i < N; ++i)
    {
      Slaw cons (m0[i]);
      EXPECT_EQ (cons.Cdr (), m01.Find (cons.Car ()));
    }
  for (int64 i = 0, N = m1.Count (); i < N; ++i)
    {
      Slaw cons (m1[i]);
      EXPECT_EQ (cons.Cdr (), m01.Find (cons.Car ()));
    }
}

TEST_F (SlawMapTest, WithoutRedHerring)
{
  Slaw m1 = Slaw::Map ("herring", "silver", "red", "flag");
  Slaw m2 = Slaw::Map ("red", "blood");

  Slaw m3 = m1.MapMerge (m2);
  Slaw m4 = m2.MapMerge (m1);
  Slaw m5 = m1.MapMerge (m1);
  Slaw m6 = m2.MapMerge (m2);

  EXPECT_EQ (2, m3.Count ()) << m3;
  EXPECT_EQ (2, m4.Count ()) << m4;
  EXPECT_EQ (2, m5.Count ()) << m5;
  EXPECT_EQ (1, m6.Count ()) << m6;
}

TEST_F (SlawMapTest, WithRedHerring)
{
  Slaw m1 = Slaw::Map ("herring", "red", "red", "flag");
  Slaw m2 = Slaw::Map ("red", "blood");

  Slaw m3 = m1.MapMerge (m2);
  Slaw m4 = m2.MapMerge (m1);
  Slaw m5 = m1.MapMerge (m1);
  Slaw m6 = m2.MapMerge (m2);

  EXPECT_EQ (2, m3.Count ()) << m3;
  EXPECT_EQ (2, m4.Count ()) << m4;
  EXPECT_EQ (2, m5.Count ()) << m5;
  EXPECT_EQ (1, m6.Count ()) << m6;
}

}  // namespace
