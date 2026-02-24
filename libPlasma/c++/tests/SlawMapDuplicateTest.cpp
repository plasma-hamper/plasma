
/* (c)  oblong industries */

// This is a close cousin of libPlasma/c/tests/nonconformist.c

#include <gtest/gtest.h>
#include "libLoam/c++/LoamStreams.h"
#include "libPlasma/c++/Plasma.h"

using namespace oblong::plasma;

namespace {

const char *const words[] = {
  "mortals",    "foolish",       "pallor",      "cadaverous",  "aura",
  "foreboding", "metamorphosis", "disquieting", "observation", "dismaying",
  "no",         "windows",       "no",          "doors",       "challenge",
  "chilling",   "way",           "out",         "way",         "my",
  "frighten",   "prematurely"};

const int nwords = sizeof (words) / sizeof (words[0]);
const int npairs = nwords / 2;
const int nunique = npairs - 2;

typedef std::vector<Slaw> Slector;

void verify_c (bool (*func) (bslaw s), bslaw x, int n, const char *aperture,
               const char *way)
{
  EXPECT_TRUE (func (x));
  EXPECT_TRUE (slaw_is_list_or_map (x));
  EXPECT_EQ (n, slaw_list_count (x));

  bslaw found;
#define DO_CHECK_C(key, spect)                                                 \
  found = slaw_map_find_c (x, key);                                            \
  EXPECT_NE (bslaw (NULL), found);                                             \
  EXPECT_STREQ (spect, slaw_string_emit (found))

  DO_CHECK_C ("mortals", "foolish");
  DO_CHECK_C ("pallor", "cadaverous");
  DO_CHECK_C ("aura", "foreboding");
  DO_CHECK_C ("metamorphosis", "disquieting");
  DO_CHECK_C ("observation", "dismaying");
  DO_CHECK_C ("no", aperture);
  DO_CHECK_C ("challenge", "chilling");
  DO_CHECK_C ("way", way);
  DO_CHECK_C ("frighten", "prematurely");

#undef DO_CHECK_C
}

void verify_cxx (Slaw x, int n, const char *aperture, const char *way,
                 const char *expectedKeys)
{
  EXPECT_EQ (n, x.Count ());

  Slaw found;
  Str s;
#define DO_CHECK_CXX(key, spect)                                               \
  found = x.Find (key);                                                        \
  EXPECT_EQ (found, x.MapFind (key));                                          \
  EXPECT_TRUE (found.Into (s));                                                \
  EXPECT_STREQ (spect, s)

  DO_CHECK_CXX ("mortals", "foolish");
  DO_CHECK_CXX ("pallor", "cadaverous");
  DO_CHECK_CXX ("aura", "foreboding");
  DO_CHECK_CXX ("metamorphosis", "disquieting");
  DO_CHECK_CXX ("observation", "dismaying");
  DO_CHECK_CXX ("no", aperture);
  DO_CHECK_CXX ("challenge", "chilling");
  DO_CHECK_CXX ("way", way);
  DO_CHECK_CXX ("frighten", "prematurely");

  Slaw keys (x.MapKeys ());
  Slaw expected (slaw_list_f (slabu_of_strings_from_split (expectedKeys, " ")));
  EXPECT_EQ (expected, keys);

#undef DO_CHECK_CXX
}

void verify (bool (*func) (bslaw s), Slaw x, int n, const char *aperture,
             const char *way, const char *expectedKeys)
{
  verify_cxx (x, n, aperture, way, expectedKeys);
  verify_c (func, x.SlawValue (), n, aperture, way);
}


TEST (SlawMapDuplicateTest, MapCollect)
{
  //Slector v (words, words + nwords);
  //MSVC doesn't like this

  //just built it iteratively
  Slector v;
  for (int i = 0; i < nwords; i++)
    v.push_back (Slaw (words[i]));

  Slaw m = Slaw::MapCollect (v.begin (), v.end ());

  verify (slaw_is_map, m, nunique, "doors", "my",
          "mortals pallor aura metamorphosis observation "
          "no challenge way frighten");
}

TEST (SlawMapDuplicateTest, ListCollect)
{
  Slector v;

  for (int i = 0; i < nwords; i += 2)
    v.push_back (Slaw::Cons (words[i], words[i + 1]));

  Slaw l = Slaw::ListCollect (v.begin (), v.end ());
  verify (slaw_is_list, l, npairs, "doors", "my",
          "mortals pallor aura metamorphosis observation "
          "no challenge way frighten");
}

TEST (SlawMapDuplicateTest, MapPut)
{
  Slaw m;

  for (int i = 0; i < nwords; i += 2)
    m = m.MapPut (words[i], words[i + 1]);
  verify (slaw_is_map, m, nunique, "doors", "my",
          "mortals pallor aura metamorphosis observation "
          "no challenge way frighten");
}

TEST (SlawMapDuplicateTest, MapMerge)
{
  Slector v[2];

  // create two maps ("no" is split across them, but "way" is all in
  // the 2nd) and merge them

  for (int i = 0; i < nwords; i += 2)
    v[i / 12].push_back (Slaw::Cons (words[i], words[i + 1]));

  Slaw l[2];
  for (int i = 0; i < 2; i++)
    l[i] = Slaw::ListCollect (v[i].begin (), v[i].end ());

  Slaw m = l[0].MapMerge (l[1]);
  verify (slaw_is_map, m, nunique, "doors", "my",
          "mortals pallor aura metamorphosis observation "
          "no challenge way frighten");
}

TEST (SlawMapDuplicateTest, MapMerge2)
{
  Slector v[2];

  // create two maps, split even and odd, so that the two
  // "no"s will be in different maps, and the two "way"s
  // will also be in different maps.

  for (int i = 0; i < nwords; i += 2)
    v[(i & 2) >> 1].push_back (Slaw::Cons (words[i], words[i + 1]));

  Slaw l[2];
  for (int i = 0; i < 2; i++)
    l[i] = Slaw::ListCollect (v[i].begin (), v[i].end ());

  Slaw m = l[0].MapMerge (l[1]);
  verify (slaw_is_map, m, nunique, "windows", "my",
          "mortals aura observation no way frighten "
          "pallor metamorphosis challenge");
}

TEST (SlawMapDuplicateTest, NonContiguousDuplicates)
{
  Slaw list (Slaw::List ("dup-key", "v", "key1", "v1", "key2", 3, "dup-key",
                         "v2", "key3", 3));
  list = list.ListAppend ("key1").ListAppend ("dup-key").ListAppend ("astray");

  Slaw map = list.MapRemove ("dup-key");

  EXPECT_EQ (list.Count () / 2 - 3, map.Count ());
  EXPECT_TRUE (map.Find ("dup-key").IsNull ());
  EXPECT_EQ (Slaw::List ("key1", "key2", "key3"), map.MapKeys ());
  EXPECT_EQ (Slaw::List ("dup-key", 3, 3), map.MapValues ());
}

}  // namespace
