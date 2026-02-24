
/* (c)  oblong industries */

// Unit tests for SlawList

#include "SlawTypesTest.h"
#include "libLoam/c++/LoamStreams.h"
#include "libPlasma/c++/PlasmaStreams.h"
#include "libPlasma/c++/SlawIterator.h"

class SlawListTest : public SlawTypesTest
{
  void collect_construction_test (const Callbacks &c)
  {
    std::vector<slaw> slawx;
    std::vector<Slaw> slawxx;
    for (size_t i = 0; i < c.Count (); ++i)
      {
        slawx.push_back (c.CSlawFromNthValue (i));
        slawxx.push_back (c.SlawFromNthValue (i));
      }
    Slaw lc = Slaw::ListCollect (slawx.begin (), slawx.end ());
    Slaw lcs = Slaw::ListCollect (slawxx.begin (), slawxx.end ());
    EXPECT_EQ (lc, lcs);
  }

  void basic_construction_test (const Slaw &lst, const Callbacks &c)
  {
    Slaw clst (lst);
    Slaw cclst;
    EXPECT_TRUE (lst.IsList ());
    EXPECT_TRUE (clst.IsList ());
    EXPECT_EQ (0, cclst.Count ());
    EXPECT_NE (cclst, clst);
    EXPECT_NE (cclst, lst);
    cclst = lst;
    EXPECT_EQ (c.Count (), size_t (lst.Count ()));
    EXPECT_EQ (lst, clst);
    EXPECT_EQ (lst, cclst);
    EXPECT_EQ (clst, cclst);
    EXPECT_TRUE (lst.Nth (c.Count ()).IsNull ());
    EXPECT_TRUE (lst.Nth (c.Count () + 10).IsNull ());
    EXPECT_TRUE (lst.Nth (-int (c.Count () + 1)).IsNull ());
    EXPECT_TRUE (lst.Nth (-int (c.Count () + 10)).IsNull ());
    c.CheckElements (lst);
    for (int64 i = 0, n = int64 (c.Count ()); i < n; ++i)
      {
        Slaw s = lst.Nth (i);
        EXPECT_EQ (lst[i], s);
        EXPECT_EQ (clst[i], s);
        EXPECT_TRUE (c.CheckNthValue (s, i));
        EXPECT_EQ (cclst[i], s);
        EXPECT_EQ (lst[n - 1 - i], lst[-(1 + i)]);
      }
  }

  void concat_test (const Slaw &lst)
  {
    Slaw list2 (
      Slaw::List (Slaw (3), Slaw ("foo"), Slaw (32.12), Slaw ("bar")));
    Slaw lst02 = lst.ListConcat (list2);
    EXPECT_EQ (lst.Count () + list2.Count (), lst02.Count ());
    int64 i = 0;
    for (int64 s = lst.Count (); i < s; ++i)
      EXPECT_EQ (lst[i], lst02[i]);
    for (int64 s = lst02.Count (), off = i; i < s; ++i)
      EXPECT_EQ (list2[i - off], lst02[i]);

    EXPECT_EQ (lst, lst.ListConcat (Slaw ()));
    EXPECT_EQ (lst, Slaw ().ListConcat (lst));
    EXPECT_EQ (lst, lst.ListConcat (Slaw::List ()));
    EXPECT_EQ (lst, Slaw::List ().ListConcat (lst));
  }

  void append_test (const Slaw &lst)
  {
    EXPECT_EQ (lst, lst.ListAppend (Slaw ()));

    Slaw list2 = Slaw::List ("foo", "bar", 3.13, 2.7182818);

    int64 list2c = list2.Count ();
    Slaw c = list2.ListAppend (lst);

    EXPECT_EQ (list2c, list2.Count ());
    EXPECT_TRUE (c.IsList ());
    EXPECT_EQ (list2c + 1, c.Count ());
    EXPECT_EQ (lst, c[-1]);

    Slaw cs (c.Slice (0, -1));
    EXPECT_EQ (cs.Count (), c.Count () - 1);
    for (int64 i = 0, s = c.Count () - 1; i < s; ++i)
      {
        EXPECT_EQ (list2[i], c[i]);
        EXPECT_EQ (list2[i], cs[i]);
      }
    EXPECT_EQ (list2.Count (), cs.Count ());
    EXPECT_TRUE (cs.IsList ());
    EXPECT_EQ (list2, cs);
  }

  void prepend_test (const Slaw &lst)
  {
    EXPECT_EQ (lst, lst.ListPrepend (Slaw ()));

    Slaw list2 = Slaw::List ("foo", "bar", 3.13, 2.7182818);

    Slaw c = list2.ListAppend (lst);
    Slaw c2 = c.ListPrepend (lst[0]);

    EXPECT_TRUE (c2.IsList ());
    EXPECT_EQ (c.Count () + 1, c2.Count ());
    EXPECT_EQ (lst[0], c2.Car ());
    for (int64 i = 1, s = c2.Count () - 1; i < s; ++i)
      EXPECT_EQ (c[i - 1], c2[i]);
    EXPECT_EQ (c, c2.Slice (1, c2.Count ()));
  }

  void list_insert_test (const Slaw &lst, const Callbacks &c)
  {
    for (int64 i = 0, n = int64 (c.Count ()); i < n; ++i)
      {
        EXPECT_EQ (lst, lst.ListInsert (Slaw (), i));
        EXPECT_EQ (Slaw::List (lst[i]), Slaw ().ListInsert (lst[i], i));
        Slaw list2 (lst.ListInsert (lst.Nth (i), i));
        EXPECT_EQ (n + 1, list2.Count ());
        EXPECT_EQ (lst.Nth (i), list2.Nth (i));
        EXPECT_EQ (lst.Slice (0, i), list2.Slice (0, i));
        EXPECT_EQ (lst.Slice (i, n), list2.Slice (i + 1, n + 1));
        Slaw list3 (list2.ListInsert ("foo", i));
        EXPECT_EQ (n + 2, list3.Count ());
        Slaw snth = list3.Nth (i);
        EXPECT_STREQ ("foo", snth.Emit<const char *> ());
        EXPECT_EQ (list2.Slice (0, i), list3.Slice (0, i));
        EXPECT_EQ (list2.Slice (i, n + 1), list3.Slice (i + 1, n + 2));
      }
    EXPECT_EQ (lst.ListAppend (5), lst.ListInsert (5, c.Count ()));
    EXPECT_EQ (lst.ListAppend ("bar"), lst.ListInsert ("bar", c.Count () + 2));
    EXPECT_EQ (lst.ListPrepend (5), lst.ListInsert (5, -int64 (c.Count ())));
    EXPECT_EQ (lst.ListPrepend ("bar"),
               lst.ListInsert ("bar", -int64 (c.Count ()) - 2));
  }

  void list_remove_test (const Slaw &lst, const Callbacks &c)
  {
    for (int64 i = 0, n = int64 (c.Count ()); i < n; ++i)
      {
        Slaw list2 (lst.ListRemoveNth (i));
        EXPECT_EQ (n - 1, list2.Count ());
        EXPECT_EQ (lst.Slice (0, i), list2.Slice (0, i));
        EXPECT_EQ (lst.Slice (i + 1, n), list2.Slice (i, n));
        Slaw list3 (lst.ListRemoveFirst (c.SlawFromNthValue (i)));
        EXPECT_EQ (n - 1, list3.Count ());
        for (int64 k = 0; k < i; ++k)
          EXPECT_GE (c.IndexOfNthValue (list3, k), 0);
        for (int64 k = i + 1; k < n; ++k)
          EXPECT_GE (c.IndexOfNthValue (list3, k), 0);
        Slaw list4 = c.MakeSlawList (i, i, i);
        list4 = list4.ListRemove (c.SlawFromNthValue (i));
        EXPECT_EQ (0, list4.Count ());
      }
    EXPECT_EQ (lst, lst.ListRemoveNth (c.Count ()));
    EXPECT_EQ (lst, lst.ListRemoveNth (c.Count () + 200));
    EXPECT_EQ (lst, lst.ListRemoveNth (-int64 (c.Count ()) - 1));
    EXPECT_EQ (lst, lst.ListRemoveNth (-int64 (c.Count ()) - 42));
    EXPECT_EQ (lst, lst.ListRemove (Slaw::Nil ()));
    EXPECT_EQ (lst, lst.ListRemoveFirst (Slaw::Nil ()));
    EXPECT_EQ (lst, lst.ListRemove (Slaw ()));
    EXPECT_EQ (lst, lst.ListRemoveFirst (Slaw ()));
  }

  void list_replace_test (const Slaw &lst, const Callbacks &c)
  {
    for (int64 i = 0, n = int64 (c.Count ()); i < n; ++i)
      {
        Slaw ab ("abracadabra");
        Slaw list2 (lst.ListReplaceNth (i, ab));
        EXPECT_EQ (n, list2.Count ());
        EXPECT_EQ (ab, list2.Nth (i));
        EXPECT_EQ (lst.Slice (0, i), list2.Slice (0, i));
        EXPECT_EQ (lst.Slice (i + 1, n), list2.Slice (i + 1, n));
        Slaw list3 (lst.ListReplaceFirst (c.SlawFromNthValue (i), ab));
        EXPECT_EQ (n, list3.Count ());
        EXPECT_EQ (ab, list2.Nth (i));
        for (int64 k = 0; k < i; ++k)
          EXPECT_GE (c.IndexOfNthValue (list3, k), 0);
        for (int64 k = i + 1; k < n; ++k)
          EXPECT_GE (c.IndexOfNthValue (list3, k), 0);
        Slaw list4 = c.MakeSlawList (i, i, i);
        list4 = list4.ListReplace (c.SlawFromNthValue (i), ab);
        EXPECT_EQ (3, list4.Count ());
        for (int64 k = 0; k < 3; ++k)
          EXPECT_EQ (ab, list4.Nth (k));
        Slaw v (c.SlawFromNthValue (i));
        EXPECT_EQ (lst, lst.ListReplaceNth (n, v));
        EXPECT_EQ (lst, lst.ListReplaceNth (n + 200, v));
        EXPECT_EQ (lst, lst.ListReplaceNth (-n - 1, v));
        EXPECT_EQ (lst, lst.ListReplaceNth (-n - 42, v));
        EXPECT_EQ (lst, lst.ListReplace (Slaw (), v));
        EXPECT_EQ (lst, lst.ListReplaceFirst (Slaw (), v));
        EXPECT_EQ (lst, lst.ListReplace (v, Slaw ()));
        EXPECT_EQ (lst, lst.ListReplaceFirst (v, Slaw ()));
      }
  }

  void index_of_list_test (const Slaw &lst, const Callbacks &c)
  {
    for (int64 i = 0, n = int64 (c.Count ()); i < n; ++i)
      {
        Slaw sublst (c.FillList (i, n - i));
        int64 p = lst.IndexOfList (sublst);
        EXPECT_TRUE (p >= 0);
        EXPECT_EQ (sublst, lst.Slice (p, p + n - i));
        EXPECT_GE (lst.IndexOfList (sublst, false), 0);
      }
    if (c.Count () > 3)
      {
        Slaw sublst = c.MakeSlawList (0, c.Count () / 2, c.Count () - 1);
        EXPECT_EQ (0, lst.IndexOfList (sublst, false));
      }
  }

  void slicing_test (const Slaw &lst, const Slaw &clst, const Callbacks &c)
  {
    EXPECT_EQ (clst, lst.Slice (0, lst.Count ()));
    EXPECT_EQ (lst.Slice (1, lst.Count ()), clst.Slice (1, lst.Count ()));

    for (int64 i = 0, n = int64 (c.Count ()); i < n; ++i)
      {
        EXPECT_EQ (n - i, lst.Slice (i, n).Count ());
        EXPECT_EQ (n - i, lst.Slice (i, n + 1).Count ());
        EXPECT_EQ (n - i, lst.Slice (i, n + 10).Count ());
        for (int64 k = 0; k < i; ++k)
          {
            Slaw sl = lst.Slice (k, i);
            EXPECT_TRUE (sl.IsList ());
            EXPECT_EQ (i - k, sl.Count ());
            for (int64 j = k; j < i; ++j)
              {
                EXPECT_TRUE (c.CheckNthValue (sl[j - i], j));
                EXPECT_EQ (lst[j], sl[j - i]);
              }
            EXPECT_EQ (sl, lst.Slice (k - n, i - n));
            EXPECT_GE (lst.IndexOfList (sl, true), 0);
            EXPECT_GE (lst.IndexOfList (sl, false), 0);
          }
        c.CheckElements (lst);
      }
  }

  void wrapped_slaw_test (const Callbacks &c) override
  {
    collect_construction_test (c);
    basic_construction_test (c.FillList (0, c.Count ()), c);

    Slaw lst (c.FillList (0, c.Count ()));
    list_insert_test (lst, c);
    list_remove_test (lst, c);
    list_replace_test (lst, c);
    append_test (lst);
    prepend_test (lst);
    concat_test (lst);

    Slaw clst (c.FillList (0, c.Count ()));
    slicing_test (lst, clst, c);
    index_of_list_test (lst, c);
  }
};

DEFINE_SLAW_TESTS_FOR_CLASS (SlawListTest);

namespace {

void check_list_elements (slaw *values, size_t n, const Slaw &l)
{
  for (size_t i = 0; i < n; ++i)
    {
      EXPECT_TRUE (slawx_equal (l[i].SlawValue (), values[i]));
      EXPECT_TRUE (slawx_equal (l.Nth (i).SlawValue (), values[i]));
    }
}

TEST (SlawListBasicTest, EmptyList)
{
  Slaw list (Slaw::List ());
  EXPECT_TRUE (list.IsList ());
  EXPECT_FALSE (list.IsNull ());
  EXPECT_EQ (0L, list.Count ());
  EXPECT_TRUE (slaw_is_list (list.SlawValue ()));
}

TEST (SlawListBasicTest, Equality)
{
  Slaw lst, list2;
  EXPECT_EQ (lst, list2);
  lst = Slaw::List ("a", 3, "b", 3.1);
  list2 = Slaw::List ("a", 3, "b");
  EXPECT_NE (lst, list2);
  list2 = list2.ListAppend (3.1);
  EXPECT_EQ (lst, list2);
}

TEST (SlawListBasicTest, ExplicitConstruction)
{
  slaw sls[7] = {
    make_slaw (3),     make_slaw ("foo"),        make_slaw (3.14),
    make_slaw ("bar"), make_slaw (int64 (2000)), make_slaw (unt32 (2)),
    make_slaw ("baz")};

  Slaw Sls[7];
  for (size_t i = 0; i < 7; ++i)
    Sls[i] = Slaw (slaw_dup (sls[i]));

  Slaw l = Slaw::List (slaw_dup (sls[0]), slaw_dup (sls[1]), slaw_dup (sls[2]),
                       slaw_dup (sls[3]), slaw_dup (sls[4]), slaw_dup (sls[5]),
                       slaw_dup (sls[6]));
  check_list_elements (sls, 7, l);
  EXPECT_EQ (7, l.Count ());
  l = Slaw::List (Sls[0], Sls[1], Sls[2], Sls[3], Sls[4], Sls[5], Sls[6]);
  check_list_elements (sls, 7, l);
  EXPECT_EQ (7, l.Count ());

  l = Slaw::List (slaw_dup (sls[0]), slaw_dup (sls[1]), slaw_dup (sls[2]),
                  slaw_dup (sls[3]), slaw_dup (sls[4]), slaw_dup (sls[5]));
  check_list_elements (sls, 6, l);
  EXPECT_EQ (6, l.Count ());
  l = Slaw::List (Sls[0], Sls[1], Sls[2], Sls[3], Sls[4], Sls[5]);
  check_list_elements (sls, 6, l);
  EXPECT_EQ (6, l.Count ());

  l = Slaw::List (slaw_dup (sls[0]), slaw_dup (sls[1]), slaw_dup (sls[2]),
                  slaw_dup (sls[3]), slaw_dup (sls[4]));
  check_list_elements (sls, 5, l);
  EXPECT_EQ (5, l.Count ());
  l = Slaw::List (Sls[0], Sls[1], Sls[2], Sls[3], Sls[4]);
  check_list_elements (sls, 5, l);
  EXPECT_EQ (5, l.Count ());

  l = Slaw::List (slaw_dup (sls[0]), slaw_dup (sls[1]), slaw_dup (sls[2]),
                  slaw_dup (sls[3]));
  check_list_elements (sls, 4, l);
  EXPECT_EQ (4, l.Count ());
  l = Slaw::List (Sls[0], Sls[1], Sls[2], Sls[3]);
  check_list_elements (sls, 4, l);
  EXPECT_EQ (4, l.Count ());

  l = Slaw::List (slaw_dup (sls[0]), slaw_dup (sls[1]), slaw_dup (sls[2]));
  check_list_elements (sls, 3, l);
  EXPECT_EQ (3, l.Count ());
  l = Slaw::List (Sls[0], Sls[1], Sls[2]);
  check_list_elements (sls, 3, l);
  EXPECT_EQ (3, l.Count ());

  l = Slaw::List (slaw_dup (sls[0]), slaw_dup (sls[1]));
  check_list_elements (sls, 2, l);
  EXPECT_EQ (2, l.Count ());
  l = Slaw::List (Sls[0], Sls[1]);
  check_list_elements (sls, 2, l);
  EXPECT_EQ (2, l.Count ());

  for (size_t i = 0; i < 7; ++i)
    slaw_free (sls[i]);
}

TEST (SlawListBasicTest, DuplicateConstructorArguments)
{
  slaw s (make_slaw ("foo"));
  Slaw list = Slaw::List (s, s, s, s);
  EXPECT_EQ (4, list.Count ());
  for (int64 i = 0; i < list.Count (); ++i)
    EXPECT_STREQ (list[i].Emit<const char *> (), "foo");

  slaw cs (make_slaw ("foo"));
  Slaw clist = Slaw::List (cs, cs, cs, cs, cs, cs);
  EXPECT_EQ (6, clist.Count ());
  for (int64 i = 0; i < clist.Count (); ++i)
    EXPECT_STREQ (clist[i].Emit<const char *> (), "foo");
}

TEST (SlawListBasicTest, IndexOf)
{
  v2int32 v = {42, 1};
  Slaw lst (Slaw::List (1, "foo", "bar", 3.45, v));
  EXPECT_TRUE (lst.ListContains (1));
  EXPECT_FALSE (lst.ListContains (1, 1));
  EXPECT_FALSE (lst.ListContains (1, 8));
  EXPECT_TRUE (lst.ListContains (v, 3));
  EXPECT_TRUE (lst.ListContains (Slaw ("bar"), 2));
  EXPECT_FALSE (lst.ListContains (Slaw ("bar"), 3));
  EXPECT_LE (lst.IndexOfList (Slaw::List ("foo", "bar", v)), -1);
  EXPECT_EQ (1, lst.IndexOfList (Slaw::List ("foo", "bar", v), false));
  EXPECT_EQ (4, lst.IndexOfList (Slaw::List (v)));
  EXPECT_EQ (4, lst.IndexOfList (Slaw::List (v), false));
  EXPECT_LE (lst.IndexOfList (Slaw::List ("bar", "foo")), -1);
  EXPECT_LE (lst.IndexOfList (Slaw::List ("bar", "foo"), false), -1);
  EXPECT_LE (lst.IndexOfList (Slaw::List (v, 34), false), -1);
}

TEST (SlawListBasicTest, VstructCoercion)
{
  Slaw list = Slaw::List (9, 34, 12);
  EXPECT_TRUE (list.CanEmit<v3int8> ());
  EXPECT_TRUE (list.CanEmit<v3int16> ());
  EXPECT_TRUE (list.CanEmit<v3int32> ());
  EXPECT_TRUE (list.CanEmit<v3int64> ());
  EXPECT_TRUE (list.CanEmit<v3unt8> ());
  EXPECT_TRUE (list.CanEmit<v3unt16> ());
  EXPECT_TRUE (list.CanEmit<v3unt32> ());
  EXPECT_TRUE (list.CanEmit<v3unt64> ());
  EXPECT_TRUE (list.CanEmit<v3float32> ());
  EXPECT_TRUE (list.CanEmit<v3float64> ());

  v3int32 v;
  v = list.Emit<v3int32> ();
  EXPECT_EQ (list.Nth (0).Emit<int32> (), v.x);
  EXPECT_EQ (list.Nth (1).Emit<int32> (), v.y);
  EXPECT_EQ (list.Nth (2).Emit<int32> (), v.z);

  v3float64 fv;
  fv = list.Emit<v3float64> ();
  EXPECT_DOUBLE_EQ (list.Nth (0).Emit<float64> (), fv.x);
  EXPECT_DOUBLE_EQ (list.Nth (1).Emit<float64> (), fv.y);
  EXPECT_DOUBLE_EQ (list.Nth (2).Emit<float64> (), fv.z);
}

TEST (SlawListBasicTest, NullOperations)
{
  Slaw null;
  Slaw empty (Slaw::List ());
  EXPECT_EQ (empty, null.ListAppend (null));
  EXPECT_EQ (empty, null.ListPrepend (null));
  EXPECT_EQ (empty, null.ListInsert (null, 0));
  EXPECT_EQ (empty, null.ListReplace (null, null));
  EXPECT_EQ (empty, null.ListReplaceFirst (null, null));
  EXPECT_EQ (empty, null.ListRemove (null));
  EXPECT_EQ (empty, null.ListRemoveFirst (null));
  EXPECT_EQ (empty, null.ListRemoveNth (3));
  EXPECT_EQ (empty, null.Slice (0, 1));

  EXPECT_EQ (Slaw::List (5), null.ListAppend (5));
  EXPECT_EQ (Slaw::List ("foo"), null.ListPrepend ("foo"));
}

TEST (SlawListBasicTest, FindListBslaw)
{
  Slaw s (Slaw::List ("This Is Sp\304\261n\314\210al Tap",
                      "Blue \303\226yster Cult", "M\303\266tley Cr\303\274e",
                      "Blue \303\226yster Cult"));
  slaw freeme[4];
  bslaw spinal_tap = freeme[0] =
    slaw_string ("This Is Sp\304\261n\314\210al Tap");
  bslaw blue_oyster_cult = freeme[1] = slaw_string ("Blue \303\226yster Cult");
  bslaw motley_crue = freeme[2] = slaw_string ("M\303\266tley Cr\303\274e");
  bslaw queensryche = freeme[3] = slaw_string ("Queensr\303\277che");

  EXPECT_EQ (0, s.ListFind (spinal_tap));
  EXPECT_EQ (1, s.ListFind (blue_oyster_cult));
  EXPECT_EQ (2, s.ListFind (motley_crue));
  EXPECT_EQ (-1, s.ListFind (queensryche));

  for (int i = 0; i < 4; i++)
    slaw_free (freeme[i]);
}

TEST (SlawListBasicTest, FindListSlaw)
{
  Slaw s (
    Slaw::List ("libAfferent", "libAfferent", "libAfferent", "libNoodoo"));
  slaw freeme[3];
  freeme[0] = slaw_string ("libNoodoo");
  freeme[1] = slaw_string ("libImpetus");
  freeme[2] = slaw_string ("libAfferent");

  EXPECT_EQ (3, s.ListFind (freeme[0]));
  EXPECT_EQ (-1, s.ListFind (freeme[1]));
  EXPECT_EQ (0, s.ListFind (freeme[2]));

  for (int i = 0; i < 3; i++)
    slaw_free (freeme[i]);
}

TEST (SlawListBasicTest, FindListCapitalSlaw)
{
  Slaw s (Slaw::List ("Hydrogen", "Helium", "Lithium"));
  Slaw hydrogen ("Hydrogen");
  Slaw lithium ("Lithium");
  Slaw unobtainium ("Unobtainium");

  EXPECT_EQ (0, s.ListFind (hydrogen));
  EXPECT_EQ (2, s.ListFind (lithium));
  EXPECT_EQ (-1, s.ListFind (unobtainium));
}

TEST (SlawListBasicTest, FindListTemplate)
{
  Slaw s (Slaw::List (107, 111, 112));

  EXPECT_EQ (0, s.ListFind (107));
  EXPECT_EQ (1, s.ListFind (111));
  EXPECT_EQ (2, s.ListFind (112));
  EXPECT_EQ (-1, s.ListFind (115));
}

TEST (SlawListBasicTest, Bug2123)
{
  ObTrove<Str> troveOfStr = Str ("a b c").Split (" ");
  Slaw listy (troveOfStr);
  EXPECT_STREQ ("[a, b, c]", listy.ToPrintableString ());
}

TEST (SlawListBasicTest, InverseBug2123)
{
  Slaw s (Slaw::List ("a", "b", "c"));
  ObTrove<Str> t = s.Emit<ObTrove<Str>> ();
  EXPECT_EQ (3, t.Count ());
  Str r;
  for (int64 i = 0; i < t.Count (); i++)
    r.Append (t.Nth (i));
  EXPECT_STREQ ("abc", r);
}

}  // namespace
