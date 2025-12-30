
/* (c)  oblong industries */

#include <gtest/gtest.h>
#include <algorithm>
#include "plasma_config.h"
#include "libLoam/c/ob-rand.h"
#include "libLoam/c/ob-time.h"
#include "libLoam/c/ob-util.h"
#include "libLoam/c++/FatherTime.h"
#include "libLoam/c++/ObTrove.h"

#include <random>

using namespace oblong::loam;

class FatherTimeTest : public ::testing::Test
{
 public:
  FatherTime father;
  FatherTime *const son;

  FatherTimeTest () : son (new FatherTime) {}
  ~FatherTimeTest () override { son->Delete (); }
};

enum
{
  ENOUGH = 3000
};

static int cmp (const int32 &a, const int32 &b)
{
  if (a < b)
    return -1;
  else if (a > b)
    return 1;
  else
    return 0;
}

/**
 * Do something to take up time.  Sorting a 3000-element
 * list with an n-squared sorting algorithm should do.
 * On my machine, this takes about 40 milliseconds without
 * valgrind, or about 1 second with valgrind.
 */
static void waste_time ()
{
  int32 vals[ENOUGH];

  const int enuf = (ob_running_under_valgrind () ? ENOUGH / 4 : ENOUGH);

  for (int i = 0; i < enuf; i++)
    vals[i] = i;

#ifdef HAVE_STD_SHUFFLE
  std::shuffle (vals + 0, vals + enuf, std::mt19937{std::random_device{}()});
#else
  std::random_shuffle (vals + 0, vals + enuf);
#endif

  ObTrove<int32> trov (vals, enuf);
  trov.Sort (cmp);

  for (int i = 0; i < enuf; i++)
    EXPECT_EQ (i, trov.Nth (i));
}

#define FLOAT64_EXPECT_LE(x, y) EXPECT_PRED_FORMAT2 (::testing::DoubleLE, x, y)

#define FLOAT64_EXPECT_GE(x, y) EXPECT_PRED_FORMAT2 (::testing::DoubleLE, y, x)

// This test runs on a multitasking system which might
// not give us anything near 100% of a cpu core.
// When writing a check that verifies current time
// is less than a computed value, allow it to be
// over by this much to avoid spurious test failures.
// Adjust upwards until no spurious test failures are
// observed on busy systems running the tests.
#define SLOP 5

// How long a few lines of code should take to run
#define MOMENT 0.000001

TEST_F (FatherTimeTest, Zeroish)
{
  // Check that new FatherTimes are not paused ...
  EXPECT_FALSE (father.IsTimePaused ());
  EXPECT_FALSE (son->IsTimePaused ());
  // ... and are running at one second per second
  EXPECT_EQ (1.0, father.SecondsPerSecond ());
  EXPECT_EQ (1.0, son->SecondsPerSecond ());
  // They're also supposed to start at zero.  They won't be
  // zero since they're already running, but they should be close.
  FLOAT64_EXPECT_GE (father.CurTime (), 0);
  EXPECT_LT (father.CurTime (), MOMENT + SLOP);
  EXPECT_LT (son->CurTime (), MOMENT + SLOP);
  FLOAT64_EXPECT_GE (son->CurTime (), 0);
  // Father should have been constructed before son
  float64 t_son = son->CurTime ();
  float64 t_father = father.CurTime ();
  FLOAT64_EXPECT_GE (t_father, t_son);
}

TEST_F (FatherTimeTest, CopyConstructor)
{
  waste_time ();
  FatherTime holy_spirit (father);
  waste_time ();
  float64 t_son = son->CurTime ();
  float64 t_holy_spirit = holy_spirit.CurTime ();
  float64 t_father = father.CurTime ();
  FLOAT64_EXPECT_LE (t_son, t_holy_spirit);
  FLOAT64_EXPECT_LE (t_holy_spirit, t_father);
}

TEST_F (FatherTimeTest, CopyFrom)
{
  waste_time ();
  father.PauseTime ();
  waste_time ();
  EXPECT_TRUE (father.IsTimePaused ());
  EXPECT_FALSE (son->IsTimePaused ());
  son->CopyFrom (father);
  EXPECT_TRUE (father.IsTimePaused ());
  EXPECT_TRUE (son->IsTimePaused ());
  EXPECT_EQ (father.CurTime (), son->CurTime ());
}

TEST_F (FatherTimeTest, AbsoluteTime)
{
  // If time flies, does absolute time fly absolutely?
  float64 t1 = FatherTime::AbsoluteTime ();
  float64 t2 = ob_current_time ();
  float64 t3 = FatherTime::AbsoluteTime ();
  FLOAT64_EXPECT_LE (t1, t2);
  FLOAT64_EXPECT_LE (t2, t3);
}

TEST_F (FatherTimeTest, SetSecondsPerSecond)
{
  father.SetSecondsPerSecond (2.0);
  father.ZeroTime ();
  son->ZeroTime ();
  EXPECT_EQ (2.0, father.SecondsPerSecond ());
  EXPECT_EQ (1.0, son->SecondsPerSecond ());
  waste_time ();
  float64 t_son = son->CurTime ();
  float64 t_father = father.CurTime ();
  FLOAT64_EXPECT_LE (t_son * 2, t_father);
}

TEST_F (FatherTimeTest, ZeroTime)
{
  father.ZeroTime ();
  float64 t_father = father.CurTime ();
  float64 t_son = son->CurTime ();
  FLOAT64_EXPECT_GE (t_son, t_father);
}

TEST_F (FatherTimeTest, SetTime)
{
  float64 amount = ob_rand_float64 (100.0, 1000.0);
  float64 ret = son->SetTime (amount);
  EXPECT_EQ (amount, ret);
  father.ZeroTime ();
  waste_time ();
  float64 t_father = father.CurTime ();
  float64 t_son = son->CurTime ();
  FLOAT64_EXPECT_GE (t_son, t_father + amount);
}

TEST_F (FatherTimeTest, AdjustTime)
{
  float64 amount = ob_rand_float64 (100.0, 1000.0);
  float64 ret = son->AdjustTime (amount);
  FLOAT64_EXPECT_GE (ret, 100);
  waste_time ();
  float64 t_son = son->CurTime ();
  float64 t_father = father.CurTime ();
  FLOAT64_EXPECT_GE (t_father + amount, t_son);
  FLOAT64_EXPECT_GE (t_son, ret);
}

TEST_F (FatherTimeTest, DeltaTime1)
{
  waste_time ();
  float64 d1 = son->DeltaTime ();
  waste_time ();
  float64 d2 = son->DeltaTime ();
  float64 t = father.CurTime ();
  FLOAT64_EXPECT_LE (d1 + d2, t);
}

TEST_F (FatherTimeTest, DeltaTime2)
{
  waste_time ();
  float64 d1 = son->CurTime ();
  waste_time ();
  float64 d2 = son->DeltaTime ();
  float64 t = father.DeltaTime ();
  FLOAT64_EXPECT_LE (d1 + d2, t);
}

TEST_F (FatherTimeTest, PauseTime)
{
  EXPECT_FALSE (father.IsTimePaused ());
  father.PauseTime ();
  EXPECT_TRUE (father.IsTimePaused ());
  float64 t1 = father.CurTime ();
  waste_time ();
  float64 t2 = father.CurTime ();
  EXPECT_EQ (t1, t2);
  EXPECT_TRUE (father.IsTimePaused ());
  father.UnPauseTime ();
  EXPECT_FALSE (father.IsTimePaused ());
}

TEST_F (FatherTimeTest, TogglePauseTime)
{
  EXPECT_FALSE (son->IsTimePaused ());
  son->TogglePauseTime ();
  EXPECT_TRUE (son->IsTimePaused ());
  son->TogglePauseTime ();
  EXPECT_FALSE (son->IsTimePaused ());
}


TEST_F (FatherTimeTest, DeltaTimePeek)
{
  father.ZeroTime ();
  ob_micro_sleep ((unt32) 75000);
  float64 t = father.DeltaTime ();
  FLOAT64_EXPECT_GE (t, 0.06);
  t = father.DeltaTime ();
  FLOAT64_EXPECT_LE (t, MOMENT + SLOP);

  ob_micro_sleep ((unt32) 30000);
  t = father.DeltaTimePeek ();
  //  t = father . DeltaTime ();  // nope
  FLOAT64_EXPECT_GE (t, 0.02);
  ob_micro_sleep ((unt32) 30000);
  t = father.DeltaTimePeek ();
  //  t = father . DeltaTime ();  // supernope
  FLOAT64_EXPECT_GE (t, 0.04);
  ob_micro_sleep ((unt32) 30000);
  t = father.DeltaTime ();
  FLOAT64_EXPECT_GE (t, 0.06);
  t = father.DeltaTimePeek ();
  FLOAT64_EXPECT_LE (t, MOMENT + SLOP);
}

TEST_F (FatherTimeTest, CurTimePeek)
{
  father.ZeroTime ();
  ob_micro_sleep ((unt32) 75000);
  float64 t = father.CurTime ();
  FLOAT64_EXPECT_GE (t, 0.06);
  t = father.DeltaTimePeek ();
  FLOAT64_EXPECT_LE (t, MOMENT + SLOP);

  ob_micro_sleep ((unt32) 30000);
  float64 ct = father.CurTimePeek ();
  //  float64 ct = father . CurTime ();  // failure
  FLOAT64_EXPECT_GE (ct, 0.09);
  t = father.DeltaTimePeek ();
  FLOAT64_EXPECT_GE (t, 0.02);
  ob_micro_sleep ((unt32) 30000);
  ct = father.CurTimePeek ();
  //  ct = father . CurTime ();  // more failure
  FLOAT64_EXPECT_GE (ct, 0.12);
  t = father.DeltaTimePeek ();
  FLOAT64_EXPECT_GE (t, 0.04);
  ob_micro_sleep ((unt32) 30000);
  ct = father.CurTime ();
  FLOAT64_EXPECT_GE (ct, 0.15);
  t = father.DeltaTimePeek ();
  FLOAT64_EXPECT_LE (t, MOMENT + SLOP);
}
