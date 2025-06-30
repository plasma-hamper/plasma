
/* (c)  oblong industries */

#include "plasma_config.h"
#include "libLoam/c/ob-rand.h"
#include "libLoam/c/ob-sys.h"
#include "libLoam/c/ob-util.h"

#include <libLoam/c++/LoamStreams.h>
#include <libLoam/c++/ObAcacia.h>
#include <libLoam/c++/ObRetort.h>
#include <libLoam/c++/Str.h>

#include <libLoam/c/ob-pthread.h>

#include <gtest/gtest.h>

#include <errno.h>

#include <algorithm>
#include <unordered_set>

#include <random>


using namespace oblong::loam;


namespace {


ob_retort codes[] = {OB_OK, OB_BAD_INDEX, OB_NO_MEM, 123, OB_ARGUMENT_WAS_NULL};

const int CSIZE (sizeof (codes) / sizeof (codes[0]));

void builtin_tests ()
{
  for (int i = 0; i < CSIZE; ++i)
    {
      ObRetort r (codes[i]);
      EXPECT_EQ (codes[i], r.Code ());
      EXPECT_EQ (codes[i] < 0, r.IsError ());
      if (codes[i] < 0)
        EXPECT_TRUE (r.IsError ());
      else
        EXPECT_FALSE (r.IsError ());
      EXPECT_STREQ (ob_error_string (codes[i]), r.Description ());
    }
}

TEST (ObRetortTest, EmptyConstructor)
{
  EXPECT_FALSE (ObRetort ().IsError ());
  EXPECT_EQ (OB_OK, ObRetort ().Code ());
  EXPECT_STREQ (ob_error_string (OB_OK), ObRetort ().Description ());
}

TEST (ObRetortTest, BuiltinCodes)
{
  builtin_tests ();
}

TEST (ObRetortTest, Register)
{
  for (int i = 0; i < CSIZE; ++i)
    EXPECT_FALSE (ObRetort::Register (codes[i], "foo"));
  builtin_tests ();

  ob_retort nr (OB_OK);
  while (ob_retort_exists (++nr))
    {
    }

  const Str desc ("this is a new code");

  EXPECT_TRUE (ObRetort::Register (nr, desc));
  EXPECT_FALSE (ObRetort::Register (nr, desc));
  ObRetort r (nr);
  EXPECT_EQ (nr, r.Code ());
  EXPECT_FALSE (r.IsError ());
  EXPECT_EQ (desc, r.Description ());

  nr = 0;
  while (ob_retort_exists (--nr))
    {
    }

  EXPECT_TRUE (ObRetort::Register (nr, desc));
  ObRetort r2 (nr);
  EXPECT_EQ (nr, r2.Code ());
  EXPECT_TRUE (r2.IsError ());
  EXPECT_EQ (desc, r2.Description ());
  EXPECT_FALSE (ObRetort::Register (nr, desc));
}

TEST (ObRetortTest, Bidirectional)
{
  EXPECT_TRUE (ObRetort::Register (-99, "OB_DEAD_KITTEN"));
  EXPECT_STREQ ("OB_DEAD_KITTEN", ob_error_string (-99));
}

const int64 fudge_factor = (ob_running_under_valgrind () ? 7 : 1);

static void *my_thread (void *)
{
  const int64 enuf = 10000 / fudge_factor;
  for (ob_retort i = -enuf; i < enuf; i++)
    {
      ObRetort r = i;
      Str desc = r.Description ();
      EXPECT_NE (0, desc.Length ());
    }

  return NULL;
}

TEST (ObRetortTest, Threaded)
{
  const int NUM_THREADS = 2;

  pthread_t thr[NUM_THREADS];

  for (int k = 0; k < NUM_THREADS; k++)
    {
      EXPECT_EQ (0, pthread_create (&(thr[k]), NULL, my_thread, NULL));
    }
  for (ob_retort i = 1; i < 500; i += fudge_factor)
    for (ob_retort j = -i; j < i; j++)
      {
        ObRetort r = j;
        Str Desc = r.Description ();
        EXPECT_STREQ (ob_error_string (j), Desc);
      }

  for (int k = 0; k < NUM_THREADS; k++)
    {
      EXPECT_EQ (0, pthread_join (thr[k], NULL));
    }
}

TEST (ObRetortTest, Errno)
{
  ObRetort r = ob_errno_to_retort (EPERM);
  EXPECT_STREQ ("Operation not permitted", r.Description ());
}


/* Yes, try searching the Wiki for "booger" to see what Oblong has
 * thought of "const" in the past. */
TEST (ObRetortTest, ConstIsNotABooger)
{
  const ObRetort okay (OB_OK);
  const ObRetort not_okay (OB_UNKNOWN_ERR);

  EXPECT_EQ (ObRetort (OB_OK), okay);
  EXPECT_EQ (okay, ObRetort (OB_OK));
  EXPECT_EQ (OB_OK, okay);
  EXPECT_EQ (okay, OB_OK);

  EXPECT_NE (okay, not_okay);
  EXPECT_LT (not_okay, okay);
  EXPECT_LE (not_okay, okay);
  EXPECT_GT (okay, not_okay);
  EXPECT_GE (okay, not_okay);

  EXPECT_NE (OB_OK, not_okay);
  EXPECT_LT (not_okay, OB_OK);
  EXPECT_LE (not_okay, OB_OK);
  EXPECT_GT (OB_OK, not_okay);
  EXPECT_GE (OB_OK, not_okay);
}


#ifdef _MSC_VER
TEST (ObRetortTest, Win32Err)
{
  // Generate a Win32 API error (ERROR_FILE_NOT_FOUND)
  HANDLE h =
    OpenMutex (SYNCHRONIZE, false, "Malevolently flatten boxes left here");
  EXPECT_EQ (0, h);
  ObRetort r = ob_win32err_to_retort (GetLastError ());
  EXPECT_STREQ ("The system cannot find the file specified.", r.Description ());

  ObRetort q = ob_win32err_to_retort (0xfeedbeef);
  EXPECT_STREQ ("win32 API error 0xfeedbeef", q.Description ());
}
#endif


TEST (ObRetortTest, Comparison1)
{
  ObRetort okay (OB_OK);
  ObRetort ok (OB_OK);

  bool awesome = (okay == ok);
  EXPECT_TRUE (awesome);
}


TEST (ObRetortTest, Comparison2)
{
  ObRetort okay (OB_OK);
  ObRetort ok (OB_OK);
  ObRetort not_okay (OB_UNKNOWN_ERR);

  EXPECT_EQ (ok, okay);
  EXPECT_NE (okay, not_okay);
  EXPECT_LT (not_okay, okay);
  EXPECT_LE (not_okay, okay);
  EXPECT_LE (ok, okay);
  EXPECT_GT (okay, not_okay);
  EXPECT_GE (okay, not_okay);
  EXPECT_GE (okay, ok);
}



class TylopodaPod : public ObRetortPod
{
};
class VicunaPod : public TylopodaPod
{
};
class AlpacaPod : public TylopodaPod
{
};
class LlamaPod : public TylopodaPod
{
};
class GuanacoPod : public TylopodaPod
{
};


TEST (ObRetortTest, RetortPods)
{
  ObRetort tort;

  TylopodaPod *tp = new TylopodaPod;
  VicunaPod *vp = new VicunaPod;
  AlpacaPod *ap = new AlpacaPod;
  LlamaPod *lp = new LlamaPod;

  tort.SetRetortPod (lp);
  EXPECT_EQ (lp, tort.RetortPod ());
  EXPECT_EQ (lp, tort.FirstRetortPodOfClass<TylopodaPod> ());
  EXPECT_EQ (lp, tort.FirstRetortPodOfClass<LlamaPod> ());
  EXPECT_EQ (NULL, tort.FirstRetortPodOfClass<VicunaPod> ());

  tort.AccreteRetortPod (ap);
  EXPECT_EQ (ap, tort.RetortPod ());
  EXPECT_EQ (ap, tort.FirstRetortPodOfClass<TylopodaPod> ());
  EXPECT_EQ (ap, tort.FirstRetortPodOfClass<AlpacaPod> ());
  EXPECT_EQ (NULL, tort.FirstRetortPodOfClass<VicunaPod> ());

  tort.AccreteRetortPod (vp);
  EXPECT_EQ (vp, tort.RetortPod ());
  EXPECT_EQ (vp, tort.FirstRetortPodOfClass<TylopodaPod> ());
  EXPECT_EQ (vp, tort.FirstRetortPodOfClass<VicunaPod> ());
  EXPECT_EQ (NULL, tort.FirstRetortPodOfClass<GuanacoPod> ());

  tort.AccreteRetortPod (tp);
  EXPECT_EQ (tp, tort.RetortPod ());
  EXPECT_EQ (tp, tort.FirstRetortPodOfClass<TylopodaPod> ());


  EXPECT_EQ (vp, tp->NextRetortPodOfClass<TylopodaPod> ());
  EXPECT_EQ (ap, vp->NextRetortPodOfClass<TylopodaPod> ());
  EXPECT_EQ (lp, ap->NextRetortPodOfClass<TylopodaPod> ());
  EXPECT_EQ (NULL, lp->NextRetortPodOfClass<TylopodaPod> ());

  ObRetortPod *p = tort.RetortPod ();
  EXPECT_EQ (tp, p);
  p = p->Antecedent ();
  EXPECT_EQ (vp, p);
  p = p->Antecedent ();
  EXPECT_EQ (ap, p);
  p = p->Antecedent ();
  EXPECT_EQ (lp, p);
  p = p->Antecedent ();
  EXPECT_EQ (NULL, p);

  GuanacoPod *gp = new GuanacoPod;
  tort += gp;
  EXPECT_EQ (gp, tort.RetortPod ());
}


typedef ObAcacia<ObRetort> TortAcacia;
typedef std::unordered_set<ObRetort> TortSet;

static void compare_contents (const TortAcacia &ac, const TortSet &st,
                              const char *where)
{
  SCOPED_TRACE (where);

  EXPECT_EQ (int64 (st.size ()), ac.Count ());

  for (TortSet::const_iterator it = st.begin (); it != st.end (); it++)
    EXPECT_TRUE (ac.Find (*it));
}

TEST (ObRetortTest, RetortsInASet)
{
  /* Use a constant seed, for reproducibility. */
  ob_rand_t *r = ob_rand_allocate_state (37619);

  TortAcacia ac;
  TortSet st;

  /* Insert 2000 numbers between -1000 and 999.
   * There will be some duplicates, and some missed. */

  for (int j = 0; j < 2000; j++)
    {
      ObRetort n ((ob_retort) ob_rand_state_int32 (-1000, 1000, r));
      ac.Append (n);
      st.insert (n);
    }

  compare_contents (ac, st, "added some");

  /* Remove 2000 numbers between -1000 and 999.
   * This should remove many, but most likely not all,
   * of the numbers we inserted before. */

  for (int j = 0; j < 2000; j++)
    {
      ObRetort n ((ob_retort) ob_rand_state_int32 (-1000, 1000, r));
      ac.Remove (n);
      st.erase (n);
    }

  compare_contents (ac, st, "removed some");

  /* Now remove all the remaining elements, in a random order. */

  std::vector<ObRetort> vec;
  vec.resize (st.size ());
  std::copy (st.begin (), st.end (), vec.begin ());
#ifdef HAVE_STD_SHUFFLE
  std::shuffle(vec.begin(), vec.end(), std::mt19937{std::random_device{}()});
#else
  std::random_shuffle (vec.begin (), vec.end ());
#endif


  for (std::vector<ObRetort>::iterator it = vec.begin (); it != vec.end ();
       it++)
    {
      ac.Remove (*it);
      st.erase (*it);
    }

  compare_contents (ac, st, "should be empty now");

  ob_rand_free_state (r);
}


}  // namespace
