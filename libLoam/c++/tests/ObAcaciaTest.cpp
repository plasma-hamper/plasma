
/* (c)  oblong industries */

#include <gtest/gtest.h>

#include <libLoam/c++/ObAcacia.h>
#include <libLoam/c++/ObTrove.h>
#include <libLoam/c++/AnkleObject.h>
#include <libLoam/c++/Str.h>


using namespace oblong::loam;


TEST (ObAcacia, BasicsEins)
{
  {
    ObAcacia<int32> acac;

    acac.Append (40);
    acac.Append (60);
    acac.Append (20);
    acac.Append (10);
    acac.Append (30);
    EXPECT_EQ (3, acac.Depth ());
    EXPECT_EQ (5, acac.Count ());

    acac.Append (25);
    EXPECT_EQ (3, acac.Depth ());
    acac.Append (35);
    EXPECT_EQ (3, acac.Depth ());

    acac.Append (5);
    EXPECT_EQ (4, acac.Depth ());
    EXPECT_EQ (8, acac.Count ());

    EXPECT_EQ (OB_NOT_FOUND, acac.Remove (27).NumericRetort ());
    EXPECT_EQ (8, acac.Count ());

    EXPECT_EQ (OB_OK, acac.Remove (30).NumericRetort ());
    EXPECT_EQ (7, acac.Count ());

    ObCrawl<int32> cr = acac.Crawl ();
    EXPECT_FALSE (cr.isempty ());

    EXPECT_EQ (5, cr.fore ());
    EXPECT_EQ (60, cr.aft ());

    EXPECT_EQ (5, cr.popfore ());
    EXPECT_EQ (10, cr.popfore ());
    EXPECT_EQ (20, cr.popfore ());
    EXPECT_EQ (25, cr.popfore ());
    EXPECT_EQ (35, cr.popfore ());
    EXPECT_EQ (40, cr.popfore ());
    EXPECT_EQ (60, cr.popfore ());

    EXPECT_EQ (true, cr.isempty ());
    cr.reload ();
    EXPECT_FALSE (cr.isempty ());

    EXPECT_EQ (5, cr.fore ());
    EXPECT_EQ (60, cr.aft ());

    EXPECT_EQ (60, cr.popaft ());
    EXPECT_EQ (40, cr.popaft ());
    EXPECT_EQ (35, cr.popaft ());
    EXPECT_EQ (25, cr.popaft ());
    EXPECT_EQ (20, cr.popaft ());
    EXPECT_EQ (10, cr.popaft ());
    EXPECT_EQ (5, cr.popaft ());

    cr.reload ();

    EXPECT_EQ (5, cr.popfore ());
    EXPECT_EQ (60, cr.popaft ());
    EXPECT_EQ (10, cr.popfore ());
    EXPECT_EQ (40, cr.popaft ());
    EXPECT_EQ (20, cr.popfore ());
    EXPECT_EQ (35, cr.popaft ());
    EXPECT_EQ (25, cr.popfore ());

    EXPECT_EQ (true, cr.isempty ());
  }

  {
    ObAcacia<AnkleObject *> ac;

    ac.Append (new AnkleObject);
  }
}


TEST (ObAcacia, BasicsDeux)
{
  ObAcacia<unt16> ac;

  ac.Append (9);
  ac.Append (4);
  ac.Append (7);
  ac.Append (1);
  ac.Append (4);
  ac.Append (14);
  ac.Append (15);
  ac.Append (2);
  ac.Append (9);
  ac.Append (7);
  ac.Append (2);
  ac.Append (3);
  ac.Append (2);
  ac.Append (2);
  ac.Append (0);
  ac.Append (14);

  EXPECT_TRUE (ac.Contains (9));
  EXPECT_TRUE (ac.Find (9) > 0);

  EXPECT_FALSE (ac.Contains (7734));
  EXPECT_TRUE (ac.Find (7734) < 0);

  ac.Remove (7);
  ac.Remove (10);
  ac.Remove (1);
  ac.Remove (15);

  EXPECT_EQ (6, ac.Count ());
}


TEST (ObAcacia, CrawlIntegrity)
{
  ObAcacia<unt16> ac;

  ac.Append (0);
  ac.Append (1);
  ac.Append (6);
  ac.Append (0);
  ac.Append (7);
  ac.Append (3);
  ac.Append (5);
  ac.Append (6);
  ac.Append (4);
  ac.Append (5);

  EXPECT_EQ (7, ac.Count ());

  ObCrawl<unt16> cr = ac.Crawl ();

  EXPECT_EQ (0, cr.popfore ());
  EXPECT_EQ (1, cr.popfore ());
  EXPECT_EQ (3, cr.popfore ());
  EXPECT_EQ (4, cr.popfore ());
  EXPECT_EQ (5, cr.popfore ());
  EXPECT_EQ (6, cr.popfore ());
  EXPECT_EQ (7, cr.popfore ());

  ac.Empty ();
  EXPECT_EQ (0, ac.Count ());
  EXPECT_EQ (0, ac.Depth ());

  ac.Append (17);
  EXPECT_EQ (1, ac.Count ());
  EXPECT_EQ (1, ac.Depth ());

  cr = ac.Crawl ();

  EXPECT_EQ (17, cr.fore ());
  EXPECT_EQ (17, cr.aft ());

  EXPECT_EQ (17, cr.popfore ());
  EXPECT_TRUE (cr.isempty ());
}


TEST (ObAcacia, TopologicalIntegrityOne)
{
  ObAcacia<unt16> ac;

  ac.Append (3);
  ac.Append (9);
  ac.Append (6);
  ac.Append (7);
  ac.Append (2);
  ac.Append (4);
  ac.Append (9);
  ac.Append (0);
  ac.Append (6);
  ac.Append (7);

  EXPECT_EQ (7, ac.Count ());

  ObCrawl<unt16> cr = ac.Crawl ();

  EXPECT_EQ (0, cr.popfore ());
  EXPECT_EQ (2, cr.popfore ());
  EXPECT_EQ (3, cr.popfore ());
  EXPECT_EQ (4, cr.popfore ());
  EXPECT_EQ (6, cr.popfore ());
  EXPECT_EQ (7, cr.popfore ());
  EXPECT_EQ (9, cr.popfore ());
  EXPECT_TRUE (cr.isempty ());

  ac.Remove (7);
  ac.Remove (5);
  ac.Remove (7);
  ac.Remove (6);
  ac.Remove (5);
  ac.Remove (8);
  ac.Remove (1);
  ac.Remove (0);
  ac.Remove (0);
  ac.Remove (3);

  EXPECT_EQ (3, ac.Count ());

  cr = ac.Crawl ();

  EXPECT_EQ (2, cr.popfore ());
  EXPECT_EQ (4, cr.popfore ());
  EXPECT_EQ (9, cr.popfore ());
}


TEST (ObAcacia, TopologicalIntegrityTwo)
{
  ObAcacia<unt16> ac;

  ac.Append (2);
  ac.Append (7);
  ac.Append (1);
  ac.Append (8);
  ac.Append (6);
  ac.Append (0);
  ac.Append (1);
  ac.Append (11);
  ac.Append (11);
  ac.Append (10);
  ac.Append (11);
  ac.Append (9);
  ac.Append (4);

  EXPECT_EQ (10, ac.Count ());

  {
    ObCrawl<unt16> cr = ac.Crawl ();
    EXPECT_EQ (0, cr.popfore ());
    EXPECT_EQ (1, cr.popfore ());
    EXPECT_EQ (2, cr.popfore ());
    EXPECT_EQ (4, cr.popfore ());
    EXPECT_EQ (6, cr.popfore ());
    EXPECT_EQ (7, cr.popfore ());
    EXPECT_EQ (8, cr.popfore ());
    EXPECT_EQ (9, cr.popfore ());
    EXPECT_EQ (10, cr.popfore ());
    EXPECT_EQ (11, cr.popfore ());
  }

  ac.Remove (2);
  ac.Remove (12);
  ac.Remove (3);
  ac.Remove (5);
  ac.Remove (2);
  ac.Remove (3);
  ac.Remove (6);
  ac.Remove (2);
  ac.Remove (5);
  ac.Remove (2);
  ac.Remove (3);
  ac.Remove (3);
  ac.Remove (5);

  EXPECT_EQ (8, ac.Count ());

  {
    ObCrawl<unt16> cr = ac.Crawl ();
    EXPECT_EQ (0, cr.popfore ());
    EXPECT_EQ (1, cr.popfore ());
    EXPECT_EQ (4, cr.popfore ());
  }
}


TEST (ObAcacia, WorstCaseDeletionTopology)
{
  ObAcacia<unt16> ac;

  ac.Append (120);

  ac.Append (12);
  ac.Append (320);

  ac.Append (2);
  ac.Append (32);
  ac.Append (220);
  ac.Append (420);

  ac.Append (1);
  ac.Append (3);
  ac.Append (22);
  ac.Append (42);
  ac.Append (202);
  ac.Append (232);
  ac.Append (402);
  ac.Append (432);

  ac.Append (4);
  ac.Append (21);
  ac.Append (23);
  ac.Append (41);
  ac.Append (43);
  ac.Append (201);
  ac.Append (203);
  ac.Append (222);
  ac.Append (242);
  ac.Append (401);
  ac.Append (403);
  ac.Append (422);
  ac.Append (442);

  ac.Append (24);
  ac.Append (44);
  ac.Append (204);
  ac.Append (221);
  ac.Append (223);
  ac.Append (241);
  ac.Append (243);
  ac.Append (404);
  ac.Append (421);
  ac.Append (423);
  ac.Append (441);
  ac.Append (443);

  ac.Append (224);
  ac.Append (244);
  ac.Append (424);
  ac.Append (444);

  EXPECT_EQ (44, ac.Count ());
  EXPECT_EQ (7, ac.Depth ());

  {
    ObCrawl<unt16> cr = ac.Crawl ();
    EXPECT_EQ (1, cr.popfore ());
    EXPECT_EQ (2, cr.popfore ());
    EXPECT_EQ (3, cr.popfore ());
    EXPECT_EQ (4, cr.popfore ());
    EXPECT_EQ (12, cr.popfore ());
    EXPECT_EQ (21, cr.popfore ());
    EXPECT_EQ (22, cr.popfore ());
    EXPECT_EQ (23, cr.popfore ());
    EXPECT_EQ (24, cr.popfore ());
    EXPECT_EQ (32, cr.popfore ());
    EXPECT_EQ (41, cr.popfore ());
    EXPECT_EQ (42, cr.popfore ());
    EXPECT_EQ (43, cr.popfore ());
    EXPECT_EQ (44, cr.popfore ());

    EXPECT_EQ (120, cr.popfore ());

    EXPECT_EQ (201, cr.popfore ());
    EXPECT_EQ (202, cr.popfore ());
    EXPECT_EQ (203, cr.popfore ());
    EXPECT_EQ (204, cr.popfore ());
    EXPECT_EQ (220, cr.popfore ());
    EXPECT_EQ (221, cr.popfore ());
    EXPECT_EQ (222, cr.popfore ());
    EXPECT_EQ (223, cr.popfore ());
    EXPECT_EQ (224, cr.popfore ());
    EXPECT_EQ (232, cr.popfore ());
    EXPECT_EQ (241, cr.popfore ());
    EXPECT_EQ (242, cr.popfore ());
    EXPECT_EQ (243, cr.popfore ());
    EXPECT_EQ (244, cr.popfore ());

    EXPECT_EQ (320, cr.popfore ());

    EXPECT_EQ (401, cr.popfore ());
    EXPECT_EQ (402, cr.popfore ());
    EXPECT_EQ (403, cr.popfore ());
    EXPECT_EQ (404, cr.popfore ());
    EXPECT_EQ (420, cr.popfore ());
    EXPECT_EQ (421, cr.popfore ());
    EXPECT_EQ (422, cr.popfore ());
    EXPECT_EQ (423, cr.popfore ());
    EXPECT_EQ (424, cr.popfore ());
    EXPECT_EQ (432, cr.popfore ());
    EXPECT_EQ (441, cr.popfore ());
    EXPECT_EQ (442, cr.popfore ());
    EXPECT_EQ (443, cr.popfore ());
    EXPECT_EQ (444, cr.popfore ());
  }

  ac.Remove (1);  // will require cascading rotations

  EXPECT_EQ (43, ac.Count ());
  EXPECT_EQ (7, ac.Depth ());

  {
    ObCrawl<unt16> cr = ac.Crawl ();
    //    EXPECT_EQ (1, cr . popfore ());
    EXPECT_EQ (2, cr.popfore ());
    EXPECT_EQ (3, cr.popfore ());
    EXPECT_EQ (4, cr.popfore ());
    EXPECT_EQ (12, cr.popfore ());
    EXPECT_EQ (21, cr.popfore ());
    EXPECT_EQ (22, cr.popfore ());
    EXPECT_EQ (23, cr.popfore ());
    EXPECT_EQ (24, cr.popfore ());
    EXPECT_EQ (32, cr.popfore ());
    EXPECT_EQ (41, cr.popfore ());
    EXPECT_EQ (42, cr.popfore ());
    EXPECT_EQ (43, cr.popfore ());
    EXPECT_EQ (44, cr.popfore ());

    EXPECT_EQ (120, cr.popfore ());

    EXPECT_EQ (201, cr.popfore ());
    EXPECT_EQ (202, cr.popfore ());
    EXPECT_EQ (203, cr.popfore ());
    EXPECT_EQ (204, cr.popfore ());
    EXPECT_EQ (220, cr.popfore ());
    EXPECT_EQ (221, cr.popfore ());
    EXPECT_EQ (222, cr.popfore ());
    EXPECT_EQ (223, cr.popfore ());
    EXPECT_EQ (224, cr.popfore ());
    EXPECT_EQ (232, cr.popfore ());
    EXPECT_EQ (241, cr.popfore ());
    EXPECT_EQ (242, cr.popfore ());
    EXPECT_EQ (243, cr.popfore ());
    EXPECT_EQ (244, cr.popfore ());

    EXPECT_EQ (320, cr.popfore ());

    EXPECT_EQ (401, cr.popfore ());
    EXPECT_EQ (402, cr.popfore ());
    EXPECT_EQ (403, cr.popfore ());
    EXPECT_EQ (404, cr.popfore ());
    EXPECT_EQ (420, cr.popfore ());
    EXPECT_EQ (421, cr.popfore ());
    EXPECT_EQ (422, cr.popfore ());
    EXPECT_EQ (423, cr.popfore ());
    EXPECT_EQ (424, cr.popfore ());
    EXPECT_EQ (432, cr.popfore ());
    EXPECT_EQ (441, cr.popfore ());
    EXPECT_EQ (442, cr.popfore ());
    EXPECT_EQ (443, cr.popfore ());
    EXPECT_EQ (444, cr.popfore ());
  }
}


TEST (ObAcaia, Iterator)
{
  ObAcacia<Str> t;
  t.Append (Str ("blue"));
  t.Append (Str ("red"));
  t.Append (Str ("green"));
  t.Append (Str ("purple"));
  t.Append (Str ("brown"));
  t.Append (Str ("orange"));
  t.Append (Str ("fuchsia"));
  t.Append (Str ("maroon"));

  const char *const expected[] = {"blue",   "brown",  "fuchsia", "green",
                                  "maroon", "orange", "purple",  "red"};

  size_t i = 0;
  for (ObAcacia<Str>::const_iterator it = t.begin (); it != t.end (); ++it)
    EXPECT_STREQ (expected[i++], *it);

  EXPECT_EQ (size_t (8), i);
}


#include <libLoam/c/ob-rand.h>


TEST (ObAcacia, AcaciaFitnessAndTroveEquivalence)
{
  int32 *fill = (int32 *) malloc (100000 * sizeof (int32));
  int32 *pull = (int32 *) malloc (100000 * sizeof (int32));

  ObAcacia<int32> ac;
  ObCrawl<int32> cr;
  ObTrove<int32> tr (1.4);
  tr.EnsureRoomFor (110000);
  for (int32 w = 1; w < 1001; w *= 10)
    {
      int32 chunk = 10000 / w;
      int32 ennui = 100000 / chunk;
      fprintf (stderr,
               "essaying %d repetitions of %d-fold chunky randomness...\n",
               ennui, chunk);

      for (int32 q = ennui; q > 0; --q)
        {
          int32 *ff = fill + chunk;
          int32 *pp = pull + chunk;
          while (ff > fill)
            {
              *--ff = ob_rand_int32 (-chunk, chunk);
              *--pp = ob_rand_int32 (-chunk, chunk);
            }
          int64 u, cnt = 0;
          ff = fill + chunk;
          ac.Empty ();
          for (u = chunk; u > 0; --u)
            if (OB_OK == ac.Append (*--ff))
              cnt++;

          EXPECT_EQ (cnt, ac.Count ());

          cr = ac.Crawl ();
          tr.Empty ();
          while (!cr.isempty ())
            tr.Append (cr.popfore ());

          EXPECT_EQ (cnt, tr.Count ());

          u = cnt;
          cr.reload ();
          while (u > 0)
            EXPECT_EQ (tr.Nth (--u), cr.popaft ());
          EXPECT_TRUE (cr.isempty ());

          pp = pull + chunk;
          while (pp > pull)
            {
              if (OB_OK == ac.Remove (*--pp))
                cnt--;
              tr.Remove (*pp);
            }

          EXPECT_EQ (cnt, ac.Count ());
          EXPECT_EQ (cnt, tr.Count ());

          cr = ac.Crawl ();
          for (u = 0; u < cnt; ++u)
            EXPECT_EQ (tr.Nth (u), cr.popfore ());
          EXPECT_TRUE (cr.isempty ());
        }
    }

  free (fill);
  free (pull);
}


#include <libLoam/c++/FatherTime.h>
#include <libLoam/c++/ObUniqueTrove.h>


TEST (ObAcacia, DISABLED_Speed)
{
  int32 q, w, chunk, dups = 0;
  int32 *randies = (int32 *) malloc (1000000 * sizeof (int32));
  for (q = 999999; q >= 0; q--)
    randies[q] = (ob_rand_unt32 () % 2000001) - 1000000;
  int32 *searchies = (int32 *) malloc (1000000 * sizeof (int32));
  for (q = 999999; q >= 0; q--)
    searchies[q] = (ob_rand_unt32 () % 2000001) - 1000000;

  ObAcacia<int32> ac1;
  int32 *ss, *rr = &randies[999999];
  for (; rr >= randies; --rr)
    if (OB_ALREADY_PRESENT == ac1.Append (*rr))
      dups++;

  fprintf (stderr,
           "out of one million entries, <%d> duplicates "
           "dared show their ugly faces.\n",
           dups);

  FatherTime uhr, nuke_uhr;

  for (w = 1; w < 1000001; w *= 10)
    {
      uhr.ZeroTime ();
      chunk = 1000000 / w;
      float64 nuk = 0.0;
      for (q = w; q > 0; q--)
        {
          {
            ObAcacia<int32> ac;
            for (rr = &randies[chunk - 1]; rr >= randies; --rr)
              ac.Append (*rr);
            nuke_uhr.ZeroTime ();
          }
          nuk += nuke_uhr.CurTime ();
        }
      fprintf (stderr,
               "ACAC: [%d] iterations of [%d] "
               "insertions: %f seconds; tear-down in %f secs\n",
               w, chunk, uhr.CurTime () - nuk, nuk);
    }

  for (w = 1; w < 10001; w *= 10)
    {
      uhr.ZeroTime ();
      chunk = 10000 / w;
      float64 nuk = 0.0;
      for (q = w; q > 0; q--)
        {
          {
            ObUniqueTrove<int32> ut;
            for (rr = &randies[chunk - 1]; rr >= randies; --rr)
              ut.Append (*rr);
            nuke_uhr.ZeroTime ();
          }
          nuk += nuke_uhr.CurTime ();
        }
      fprintf (stderr,
               "UQTR: [%d] iterations of [%d] trove "
               "insertions: %f seconds; tear-down in %f secs\n",
               w, chunk, uhr.CurTime () - nuk, nuk);
    }


  for (w = 1; w < 1000001; w *= 10)
    {
      ObAcacia<int32> ac;
      uhr.ZeroTime ();
      chunk = 1000000 / w;
      float64 nuk = 0.0;
      for (q = w; q > 0; q--)
        {
          for (rr = &randies[chunk - 1]; rr >= randies; --rr)
            ac.Append (*rr);
          nuke_uhr.ZeroTime ();
          for (rr = &randies[chunk - 1]; rr >= randies; --rr)
            ac.Remove (*rr);
          nuk += nuke_uhr.CurTime ();
        }
      fprintf (stderr,
               "ACAC-rem: [%d] iterations of [%d] "
               "removals: %f seconds\n",
               w, chunk, nuk);
    }



  for (w = 1; w < 1000001; w *= 10)
    {
      chunk = 1000000 / w;
      ObAcacia<int32> ac;
      int32 fnd = 0;
      for (rr = &randies[chunk - 1]; rr >= randies; --rr)
        ac.Append (*rr);
      uhr.ZeroTime ();
      for (q = w; q > 0; q--)
        {
          for (ss = &searchies[chunk - 1]; ss >= searchies; --ss)
            fnd += (ac.Contains (*ss) ? 1 : 0);
        }
      fprintf (stderr, "ACAC: [%d] iterations of [%d] searches: %f seconds\n",
               w, chunk, uhr.CurTime ());
    }

  free (searchies);
  free (randies);
}

static int livecount = 0;
class Dorf : public AnkleObject
{
  PATELLA_SUBCLASS (Dorf, AnkleObject);

 public:
  Str s;
  Dorf (Str s_) : s (s_) { livecount++; }
  ~Dorf () { livecount--; }
};

TEST (ObAcaciaTest, RangedForLoopInts)
{
  ObAcacia<int64> intset;
  intset.Append (1);
  intset.Append (2);
  intset.Append (3);
  intset.Append (4);
  intset.Append (5);
  int sum = 0;
  for (auto i : intset)
    {
      static_assert (std::is_same<decltype (i), int64>::value,
                     "std::begin(ObAcacia<int64>) should return an iterator "
                     "with elements of type int64");
      sum += i;
    }
  EXPECT_EQ (5, intset.Count ());
  EXPECT_EQ (15, sum);
}

TEST (ObAcaciaTest, RangedForLoopObRefs)
{
  ObAcacia<Dorf *> dorves;
  ObAcacia<Str> strings;
  dorves.Append (new Dorf ("avert"));
  dorves.Append (new Dorf ("bituminous"));
  dorves.Append (new Dorf ("catastrophe"));

  for (auto dorf : dorves)
    {
      static_assert (std::is_same<decltype (dorf), Dorf *>::value,
                     "std::begin(ObAcacia<AnkleObject *>) should return an "
                     "iterator with elements of type AnkleObject*");
      strings.Append (dorf->s);
    }
  EXPECT_TRUE (strings.Contains ("avert"));
  EXPECT_TRUE (strings.Contains ("bituminous"));
  EXPECT_TRUE (strings.Contains ("catastrophe"));
}

TEST (ObAcaciaTest, SelfCopy)
{
  ObAcacia<AnkleObject *> dorf_set;
  dorf_set.Append (new Dorf ("one"));
  dorf_set.Append (new Dorf ("two"));
  dorf_set.Append (new Dorf ("three"));
  dorf_set.Append (new Dorf ("four"));
  dorf_set.Append (new Dorf ("five"));

  ASSERT_EQ (5, dorf_set.Count ());
  ASSERT_EQ (livecount, dorf_set.Count ());

  dorf_set =
    *&dorf_set;  // annotate with *& to tell clang we intend self-assignment
  ASSERT_EQ (5, dorf_set.Count ());
  ASSERT_EQ (livecount, dorf_set.Count ());
}

TEST (ObAcaciaTest, CopyCopy)
{
  ObAcacia<AnkleObject *> dorf_set;
  dorf_set.Append (new Dorf ("one"));
  dorf_set.Append (new Dorf ("two"));
  dorf_set.Append (new Dorf ("three"));
  dorf_set.Append (new Dorf ("four"));
  dorf_set.Append (new Dorf ("five"));

  ASSERT_EQ (5, dorf_set.Count ());
  ASSERT_EQ (livecount, dorf_set.Count ());

  ObAcacia<AnkleObject *> dorf_set_2;
  dorf_set_2 = dorf_set;
  ASSERT_EQ (5, dorf_set.Count ());
  ASSERT_EQ (livecount, dorf_set_2.Count ());
}
