
/* (c)  oblong industries */

// This is a gtest-based (and thus C++) test that serves as a catch-all
// for a bunch of small tests that test things that haven't otherwise
// been tested.  (In other words, I'm trying to get the coverage up.)
// It seemed overkill to make each of these tests a separate program,
// and gtest provides a nice way to mush them into one file while
// still keeping them logically separate.

#include "plasma_config.h"
#include "plasma-gtest-helpers.h"
#include <gtest/gtest.h>
#include <tests/ob-test-helpers.h>
#include "libLoam/c/ob-pthread.h"
#include "libLoam/c/ob-rand.h"
#include "libLoam/c/ob-util.h"
#include "libLoam/c/ob-time.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-file.h"
#include "libPlasma/c/pool_cmd.h"
#include "libPlasma/c/slaw-string.h"
#include "libPlasma/c/slaw-path.h"
#include "libPlasma/c/private/plasma-testing.h"
#include "libPlasma/c/protein.h"
#include "libPlasma/c/slaw.h"
#include "libPlasma/c/plasma-retorts.h"

#include <set>
#include <vector>
#include <algorithm>

#include <random>

// Although pool_participate can take "participate options", we've never
// tried giving it anything other than NULL.  This is understandable, since
// we currently don't support any participate options, but we should at
// least exercise the code path by passing in a made-up option.
TEST (MiscPoolTest, ParticipateOptions)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));
  protein popt =
    protein_from_ff (NULL,
                     slaw_map_inline_cc ("my favorite nonsense phrase",
                                         "oblolate the syntax information",
                                         NULL));
  EXPECT_TORTEQ (OB_OK, pool_participate (cmd.pool_name, &cmd.ph, popt));
  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  protein_free (popt);
  pool_cmd_free_options (&cmd);
}

// tests passing options as a map instead of a protein
TEST (MiscPoolTest, ParticipateOptions2)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));
  slaw popt = slaw_map_inline_cc ("my favorite nonsense phrase",
                                  "oblolate the syntax information", NULL);
  EXPECT_TORTEQ (OB_OK, pool_participate (cmd.pool_name, &cmd.ph, popt));
  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  slaw_free (popt);
  pool_cmd_free_options (&cmd);
}

// We only allow proteins to be deposited into a pool, and pool_deposit()
// returns POOL_NOT_A_PROTEIN if we deposit some other sort of slaw into
// the pool.  But it looks like our existing test suite never exercised
// this error case.
TEST (MiscPoolTest, NotAProtein)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));
  POOL_CMD_OPEN_POOL (&cmd);
  slaw s = slaw_string ("I am not a protein");
  EXPECT_EQ (POOL_NOT_A_PROTEIN, pool_deposit (cmd.ph, s, NULL));
  slaw_free (s);
  s = slaw_nil ();
  EXPECT_EQ (POOL_NOT_A_PROTEIN, pool_deposit (cmd.ph, s, NULL));
  slaw_free (s);
  s = slaw_map_inline_cc ("my favorite nonsense phrase",
                          "oblolate the syntax information", NULL);
  EXPECT_EQ (POOL_NOT_A_PROTEIN, pool_deposit (cmd.ph, s, NULL));
  slaw_free (s);
  s = slaw_unt32 (0x4ffe874);
  EXPECT_EQ (POOL_NOT_A_PROTEIN, pool_deposit (cmd.ph, s, NULL));
  slaw_free (s);
  s = protein_from (NULL, NULL);
  EXPECT_TORTEQ (OB_OK, pool_deposit (cmd.ph, s, NULL));
  slaw_free (s);
  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  pool_cmd_free_options (&cmd);
}

// Test pool_seekby(), which the test suite never called.
TEST (MiscPoolTest, Seekby)
{
  pool_cmd_info cmd;
  int64 idx;
  pool_cmd_options_from_env (&cmd);
  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));
  POOL_CMD_OPEN_POOL (&cmd);
  EXPECT_TORTEQ (OB_OK, pool_seekby (cmd.ph, -1));
  EXPECT_TORTEQ (OB_OK, pool_index (cmd.ph, &idx));
  EXPECT_EQ (-1, idx);
  EXPECT_TORTEQ (OB_OK, pool_seekby (cmd.ph, -2));
  EXPECT_TORTEQ (OB_OK, pool_index (cmd.ph, &idx));
  EXPECT_EQ (-3, idx);
  EXPECT_TORTEQ (OB_OK, pool_seekby (cmd.ph, 100));
  EXPECT_TORTEQ (OB_OK, pool_index (cmd.ph, &idx));
  EXPECT_EQ (97, idx);
  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  pool_cmd_free_options (&cmd);
}

TEST (MiscPoolTest, AwaitNextDiscarded)
{
  pool_cmd_info cmd;
  int64 idx = -1;
  pool_cmd_options_from_env (&cmd);
  shrink_size_for_valgrind (&cmd);
  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));
  pool_cmd_fill_pool (&cmd);
  POOL_CMD_OPEN_POOL (&cmd);
  EXPECT_TORTEQ (OB_OK, pool_seekto (cmd.ph, 0));
  protein p;
  EXPECT_TORTEQ (OB_OK, pool_await_next (cmd.ph, 923, &p, NULL, &idx));
  EXPECT_EQ (1, idx);
  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  protein_free (p);
  pool_cmd_free_options (&cmd);
}

TEST (MiscPoolTest, ProbeFrwdNotFound)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  shrink_size_for_valgrind (&cmd);
  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));
  pool_cmd_fill_pool (&cmd);
  POOL_CMD_OPEN_POOL (&cmd);
  EXPECT_TORTEQ (OB_OK, pool_rewind (cmd.ph));
  int64 actual, expected;
  EXPECT_TORTEQ (OB_OK, pool_index (cmd.ph, &expected));
  protein p = NULL;
  int64 idx = -1;
  slaw srch = slaw_string ("bananagram");
  EXPECT_EQ (POOL_NO_SUCH_PROTEIN,
             pool_probe_frwd (cmd.ph, srch, &p, NULL, &idx))
    << "At index " << idx << ", the protein was:" << std::endl
    << p;
  EXPECT_TORTEQ (OB_OK, pool_index (cmd.ph, &actual));
  // expect index to be unchanged by failed pool_probe_frwd (bug 547 comment 1)
  EXPECT_EQ (expected, actual);
  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  slaw_free (srch);
  pool_cmd_free_options (&cmd);
}

TEST (MiscPoolTest, ZeroAwaitProbeFrwdNotFound)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  shrink_size_for_valgrind (&cmd);
  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));
  pool_cmd_fill_pool (&cmd);
  POOL_CMD_OPEN_POOL (&cmd);
  EXPECT_TORTEQ (OB_OK, pool_rewind (cmd.ph));
  int64 actual, expected;
  EXPECT_TORTEQ (OB_OK, pool_index (cmd.ph, &expected));
  protein p;
  slaw srch = slaw_string ("bananagram");
  EXPECT_TORTEQ (POOL_AWAIT_TIMEDOUT,
                 pool_await_probe_frwd (cmd.ph, srch, POOL_NO_WAIT, &p, NULL,
                                        NULL));
  EXPECT_TORTEQ (OB_OK, pool_index (cmd.ph, &actual));
  // expect index to be unchanged by failed pool_probe_frwd (bug 547 comment 1)
  EXPECT_EQ (expected, actual);
  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  slaw_free (srch);
  pool_cmd_free_options (&cmd);
}

TEST (MiscPoolTest, NonzeroAwaitProbeFrwdNotFound)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  shrink_size_for_valgrind (&cmd);
  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));
  pool_cmd_fill_pool (&cmd);
  POOL_CMD_OPEN_POOL (&cmd);
  EXPECT_TORTEQ (OB_OK, pool_rewind (cmd.ph));
  int64 actual, expected;
  EXPECT_TORTEQ (OB_OK, pool_index (cmd.ph, &expected));
  protein p;
  slaw srch = slaw_string ("bananagram");
  EXPECT_EQ (POOL_AWAIT_TIMEDOUT,
             pool_await_probe_frwd (cmd.ph, srch, 0.0000000001, &p, NULL,
                                    NULL));
  EXPECT_TORTEQ (OB_OK, pool_index (cmd.ph, &actual));
  // expect index to be unchanged by failed pool_probe_frwd (bug 547 comment 1)
  EXPECT_EQ (expected, actual);
  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  slaw_free (srch);
  pool_cmd_free_options (&cmd);
}

TEST (MiscPoolTest, PrevEmpty)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));
  POOL_CMD_OPEN_POOL (&cmd);
  protein p;
  EXPECT_EQ (POOL_NO_SUCH_PROTEIN, pool_prev (cmd.ph, &p, NULL, NULL));
  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  pool_cmd_free_options (&cmd);
}

TEST (MiscPoolTest, PrevBeyondEnd)
{
  pool_cmd_info cmd;
  int64 idx, newest;
  pool_cmd_options_from_env (&cmd);
  shrink_size_for_valgrind (&cmd);
  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));
  pool_cmd_fill_pool (&cmd);
  POOL_CMD_OPEN_POOL (&cmd);
  EXPECT_TORTEQ (OB_OK, pool_newest_index (cmd.ph, &newest));
  EXPECT_TORTEQ (OB_OK, pool_seekto (cmd.ph, 100 * newest));
  protein p = NULL;
  EXPECT_TORTEQ (OB_OK, pool_prev (cmd.ph, &p, NULL, &idx));
  EXPECT_EQ (newest, idx);
  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  protein_free (p);
  pool_cmd_free_options (&cmd);
}

TEST (MiscPoolTest, ProbeBackNotFound)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  shrink_size_for_valgrind (&cmd);
  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));
  pool_cmd_fill_pool (&cmd);
  POOL_CMD_OPEN_POOL (&cmd);
  int64 actual, expected;
  EXPECT_TORTEQ (OB_OK, pool_index (cmd.ph, &expected));
  protein p;
  slaw srch = slaw_string ("bananagram");
  EXPECT_EQ (POOL_NO_SUCH_PROTEIN,
             pool_probe_back (cmd.ph, srch, &p, NULL, NULL));
  EXPECT_TORTEQ (OB_OK, pool_index (cmd.ph, &actual));
  // expect index to be unchanged by failed pool_probe_back (bug 547 comment 1)
  EXPECT_EQ (expected, actual);
  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  slaw_free (srch);
  pool_cmd_free_options (&cmd);
}

typedef std::vector<slaw> SlawVector;
typedef std::vector<pool_hose> HoseVector;

// It looks like some of the code paths in the linked list manipulation
// for adding and removing gang members weren't getting covered.  This attempts
// to remedy that by doing some semi-random adding and removing of gang
// members.
TEST (MiscPoolTest, GangManipulation)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  const char *jets[] = {"Tony", "Riff", "Action", "Diesel", "Snowboy", NULL};
  const char *sharks[] = {"Bernardo", "Chino", "Pepe", NULL};
  SlawVector jets_slaw, sharks_slaw;
  HoseVector jets_hoses, sharks_hoses;
  pool_gang gang;
  EXPECT_TORTEQ (OB_OK, pool_new_gang (&gang));
  for (int i = 0; jets[i] != NULL; i++)
    {
      slaw pool_name = slaw_string_format ("%s.%s", cmd.pool_name, jets[i]);
      EXPECT_TORTEQ (OB_OK, pool_create (slaw_string_emit (pool_name), cmd.type,
                                         cmd.create_options));
      jets_slaw.push_back (pool_name);
      pool_hose ph;
      EXPECT_TORTEQ (OB_OK, pool_participate (slaw_string_emit (pool_name), &ph,
                                              NULL));
      jets_hoses.push_back (ph);
      EXPECT_TORTEQ (OB_OK, pool_join_gang (gang, ph));
    }
  EXPECT_EQ (5, pool_gang_count (gang));
  int64 n = 0;
  for (HoseVector::reverse_iterator it = jets_hoses.rbegin ();
       it != jets_hoses.rend (); it++, n++)
    EXPECT_EQ (*it, pool_gang_nth (gang, n));
  EXPECT_TRUE (NULL == pool_gang_nth (gang, n));
  EXPECT_TRUE (NULL == pool_gang_nth (gang, -1));
  for (int i = 0; sharks[i] != NULL; i++)
    {
      slaw pool_name = slaw_string_format ("%s.%s", cmd.pool_name, sharks[i]);
      EXPECT_TORTEQ (OB_OK, pool_create (slaw_string_emit (pool_name), cmd.type,
                                         cmd.create_options));
      sharks_slaw.push_back (pool_name);
    }
#ifdef HAVE_STD_SHUFFLE
   std::shuffle(jets_hoses.begin(), jets_hoses.end(), std::mt19937{std::random_device{}()});
#else
   std::random_shuffle (jets_hoses.begin (), jets_hoses.end ());
#endif


  for (int i = 0; i < 3; i++)
    {
      pool_hose ph = jets_hoses.back ();
      jets_hoses.pop_back ();
      EXPECT_TORTEQ (OB_OK, pool_leave_gang (gang, ph));
      EXPECT_TORTEQ (OB_OK, pool_withdraw (ph));
    }
  EXPECT_EQ (2, pool_gang_count (gang));
  for (SlawVector::iterator it = sharks_slaw.begin (); it != sharks_slaw.end ();
       it++)
    {
      const char *pool_name = slaw_string_emit (*it);
      pool_hose ph;
      EXPECT_TORTEQ (OB_OK, pool_participate (pool_name, &ph, NULL));
      sharks_hoses.push_back (ph);
      EXPECT_TORTEQ (OB_OK, pool_join_gang (gang, ph));
    }
  EXPECT_EQ (5, pool_gang_count (gang));
#ifdef HAVE_STD_SHUFFLE
  std::shuffle(sharks_hoses.begin(), sharks_hoses.end(), std::mt19937{std::random_device{}()});
#else
  std::random_shuffle (sharks_hoses.begin (), sharks_hoses.end ());
#endif

  for (HoseVector::iterator it = sharks_hoses.begin ();
       it != sharks_hoses.end (); it++)
    {
      pool_hose ph = *it;
      EXPECT_TORTEQ (OB_OK, pool_leave_gang (gang, ph));
      EXPECT_TORTEQ (OB_OK, pool_withdraw (ph));
    }
  EXPECT_EQ (2, pool_gang_count (gang));
  for (HoseVector::iterator it = jets_hoses.begin (); it != jets_hoses.end ();
       it++)
    {
      pool_hose ph = *it;
      EXPECT_TORTEQ (OB_OK, pool_leave_gang (gang, ph));
      EXPECT_TORTEQ (OB_OK, pool_withdraw (ph));
    }
  EXPECT_EQ (0, pool_gang_count (gang));
  for (SlawVector::iterator it = sharks_slaw.begin (); it != sharks_slaw.end ();
       it++)
    {
      const char *pool_name = slaw_string_emit (*it);
      EXPECT_TORTEQ (OB_OK, pool_dispose (pool_name));
      slaw_free (*it);
    }
  for (SlawVector::iterator it = jets_slaw.begin (); it != jets_slaw.end ();
       it++)
    {
      const char *pool_name = slaw_string_emit (*it);
      EXPECT_TORTEQ (OB_OK, pool_dispose (pool_name));
      slaw_free (*it);
    }
  EXPECT_TORTEQ (OB_OK, pool_disband_gang (gang, false));
  pool_cmd_free_options (&cmd);
}

TEST (MiscPoolTest, GangAccessorErrors)
{
  EXPECT_EQ (-1, pool_gang_count (NULL));
  EXPECT_TRUE (NULL == pool_gang_nth (NULL, 0));
}

typedef struct
{
  const char *pool_name;
  int64 last_duck;
  int64 last_goose;
} fowl_t;

static const int64 FOWL_ITERATIONS = 100;

static void *fowl (void *v)
{
  fowl_t *f = (fowl_t *) v;

  pool_hose ph;
  EXPECT_TORTEQ (OB_OK, pool_participate (f->pool_name, &ph, NULL));
  f->last_duck = -1;
  f->last_goose = -1;

  ob_rand_t *r = ob_rand_allocate_state ();

  for (int64 i = 0; i < FOWL_ITERATIONS; i++)
    {
      bool is_goose = (ob_rand_state_float64 (0.0, 1.0, r) > 0.9);
      protein p =
        protein_from_ff (slaw_list_inline_c (is_goose ? "goose" : "duck", NULL),
                         slaw_map_inline_cf ("last-duck",
                                             slaw_int64 (f->last_duck),
                                             "last-goose",
                                             slaw_int64 (f->last_goose), NULL));
      int64 idx = -1;
      EXPECT_TORTEQ (OB_OK, pool_deposit (ph, p, &idx));
      EXPECT_EQ (i, idx);
      if (is_goose)
        f->last_goose = i;
      else
        f->last_duck = i;
      protein_free (p);
      EXPECT_TORTEQ (OB_OK, ob_micro_sleep (10000));
    }

  EXPECT_TORTEQ (OB_OK, pool_withdraw (ph));

  ob_rand_free_state (r);

  return NULL;
}

/* This is ugly, and I don't really know why, but for some reason I
 * don't understand, under valgrind, select() actually takes slightly
 * less time than expected:
 *   MiscPoolTest.C:368: Failure
 *   Expected: (duration) >= (tout), actual: 0.099632 vs 0.1
 * Rather than trying to figure out why, I'm just going to accept it
 * for now, and only care about cases where the timely is grossly
 * less than what was expected.
 *
 * XXX: Okay, this doesn't just happen under valgrind... I saw it just
 * with a plain old "make check", too.  And apparently 95% wasn't good
 * enough, so I'll have to reduce it to 94%.  Will that be enough?
 *   MiscPoolTest.C:396: Failure
 *   Expected: (duration) >= (tout * FUDGE_FACTOR), actual: 0.094878 vs 0.095
 *
 * No, not enough; saw 93.75% on Windows!
 * YYY: Cranking all the way down to 80% because of bug 1179.
 */
static const float64 FUDGE_FACTOR = 0.80;

TEST (MiscPoolTest, AwaitProbeFrwd)
{
  pool_cmd_info cmd;
  fowl_t f;
  pthread_t thr;
  OB_CLEAR (f);
  pool_cmd_options_from_env (&cmd);
  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));
  f.pool_name = cmd.pool_name;
  EXPECT_EQ (0, pthread_create (&thr, NULL, fowl, &f));
  POOL_CMD_OPEN_POOL (&cmd);
  ob_rand_t *r = ob_rand_allocate_state ();
  // int64 last_duck = -1;
  // int64 last_goose = -1;
  int64 idx;

  for (;;)
    {
      bool want_goose = (ob_rand_state_float64 (0.0, 1.0, r) > 0.9);
      slaw srch = slaw_string (want_goose ? "goose" : "duck");
      pool_timestamp tout = 0.1;
      float64 before = ob_current_time ();
      protein p = NULL;
      ob_retort tort =
        pool_await_probe_frwd (cmd.ph, srch, tout, &p, NULL, &idx);
      float64 duration = ob_current_time () - before;
      if (tort == POOL_AWAIT_TIMEDOUT)
        {
          slaw_free (srch);
          EXPECT_GE (duration, tout * FUDGE_FACTOR);
          tort = pool_newest_index (cmd.ph, &idx);
          if (tort == POOL_NO_SUCH_PROTEIN)  // haven't had first deposit yet
            continue;
          EXPECT_TORTEQ (OB_OK, tort);
          EXPECT_LT (idx, FOWL_ITERATIONS);
          if (idx >= FOWL_ITERATIONS - 1)
            break;  // this means thread has finished depositing
        }
      else
        {
          EXPECT_TORTEQ (OB_OK, tort);
          EXPECT_LT (idx, FOWL_ITERATIONS);
          bslaw species = slaw_list_emit_first (protein_descrips (p));
          EXPECT_TRUE (slawx_equal (species, srch));
          // TODO: some clever bookkeeping about when the
          // last duck and goose were.
          protein_free (p);
          slaw_free (srch);
        }
    }

  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_EQ (0, pthread_join (thr, NULL));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  pool_cmd_free_options (&cmd);
  ob_rand_free_state (r);
}

static void deposit_100_proteins (pool_hose ph)
{
  int64 i;
  slaw descrips = slaw_list_inline_c ("foo!", NULL);

  for (i = 0; i < 100; i++)
    {
      protein p =
        protein_from_lf (descrips,
                         slaw_map_inline_cf ("n", slaw_int64 (i), NULL));
      EXPECT_TORTEQ (OB_OK, pool_deposit (ph, p, NULL));
      protein_free (p);
    }

  slaw_free (descrips);
}

static void test_next_behavior (ob_retort (*func) (
  pool_hose ph, protein *ret_prot, pool_timestamp *ret_ts, int64 *ret_index))
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));
  POOL_CMD_OPEN_POOL (&cmd);
  deposit_100_proteins (cmd.ph);
  EXPECT_TORTEQ (OB_OK, pool_seekto (cmd.ph, 50));
  int64 idx = -1;
  protein p = NULL;
  ob_retort tort = func (cmd.ph, &p, NULL, &idx);

  EXPECT_TORTEQ (OB_OK, tort);
  EXPECT_EQ (50, idx);
  EXPECT_EQ (50, slaw_path_get_int64 (p, "n", -1));

  if (tort >= OB_OK)
    protein_free (p);

  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  pool_cmd_free_options (&cmd);
}

// The next three functions are adapters to make various other
// functions look like pool_next().
static ob_retort func_await_next (pool_hose ph, protein *ret_prot,
                                  pool_timestamp *ret_ts, int64 *ret_index)
{
  const float64 long_time = 600;  // 10 minutes
  float64 start = ob_current_time ();
  ob_retort tort = pool_await_next (ph, long_time, ret_prot, ret_ts, ret_index);
  float64 delta = ob_current_time () - start;
  // If it succeeeds but takes ~10 minutes, that's bad
  EXPECT_GT (long_time * FUDGE_FACTOR, delta);
  return tort;
}

static ob_retort func_next_multi (pool_hose ph, protein *ret_prot,
                                  pool_timestamp *ret_ts, int64 *ret_index)
{
  pool_gang g;
  EXPECT_TORTEQ (OB_OK, pool_new_gang (&g));
  EXPECT_TORTEQ (OB_OK, pool_join_gang (g, ph));
  pool_hose h;
  ob_retort tort = pool_next_multi (g, &h, ret_prot, ret_ts, ret_index);
  if (tort >= OB_OK)
    {
      EXPECT_EQ (ph, h);
    }
  EXPECT_TORTEQ (OB_OK, pool_disband_gang (g, false));
  return tort;
}

static ob_retort func_await_next_multi (pool_hose ph, protein *ret_prot,
                                        pool_timestamp *ret_ts,
                                        int64 *ret_index)
{
  const float64 long_time = 600;  // 10 minutes
  float64 start = ob_current_time ();
  pool_gang g;
  EXPECT_TORTEQ (OB_OK, pool_new_gang (&g));
  EXPECT_TORTEQ (OB_OK, pool_join_gang (g, ph));
  pool_hose h;
  ob_retort tort =
    pool_await_next_multi (g, long_time, &h, ret_prot, ret_ts, ret_index);
  if (tort >= OB_OK)
    {
      EXPECT_EQ (ph, h);
    }
  EXPECT_TORTEQ (OB_OK, pool_disband_gang (g, false));
  float64 delta = ob_current_time () - start;
  // If it succeeeds but takes ~10 minutes, that's bad
  EXPECT_GT (long_time * FUDGE_FACTOR, delta);
  return tort;
}

TEST (MiscPoolTest, MiddleNext)
{
  test_next_behavior (pool_next);
}

TEST (MiscPoolTest, MiddleAwaitNext)
{
  test_next_behavior (func_await_next);
}

TEST (MiscPoolTest, MiddleNextMulti)
{
  test_next_behavior (func_next_multi);
}

TEST (MiscPoolTest, MiddleAwaitNextMulti)
{
  test_next_behavior (func_await_next_multi);
}

static void *deposit_one_protein (void *arg)
{
  const char *name = (const char *) arg;
  pool_hose ph = NULL;
  protein p = NULL;
  EXPECT_TORTEQ (OB_OK, pool_participate (name, &ph, NULL));
  sleep (1);
  EXPECT_TORTEQ (OB_OK, pool_cmd_add_test_protein (ph, "gravity", &p, NULL));
  protein_free (p);
  EXPECT_TORTEQ (OB_OK, pool_withdraw (ph));
  return NULL;
}

TEST (MiscPoolTest, RepositionAndAwait)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  slaw s_pool_b = slaw_string_format ("%s-b", cmd.pool_name);
  const char *pool_b = slaw_string_emit (s_pool_b);
  pool_hose phb = NULL;
  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));
  ob_retort tort =
    pool_participate_creatingly (pool_b, cmd.type, &phb, cmd.create_options);
  EXPECT_TORTEQ (POOL_CREATED, tort);
  POOL_CMD_OPEN_POOL (&cmd);
  deposit_100_proteins (cmd.ph);

  int64 idx = -1;
  EXPECT_TORTEQ (OB_OK, pool_index (cmd.ph, &idx));
  EXPECT_EQ (0, idx);

  EXPECT_TORTEQ (OB_OK, pool_runout (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_index (cmd.ph, &idx));
  EXPECT_EQ (100, idx);

  pool_gang g = NULL;
  EXPECT_TORTEQ (OB_OK, pool_new_gang (&g));
  EXPECT_TORTEQ (OB_OK, pool_join_gang (g, cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_join_gang (g, phb));

  pthread_t thr;
  EXPECT_EQ (0,
             pthread_create (&thr, NULL, deposit_one_protein, (void *) pool_b));

  const float64 long_time = 600;  // 10 minutes
  pool_hose ho = NULL;
  protein pro = NULL;
  float64 start = ob_current_time ();
  idx = -1;
  EXPECT_TORTEQ (OB_OK,
                 pool_await_next_multi (g, long_time, &ho, &pro, NULL, &idx));
  EXPECT_EQ (phb, ho);
  EXPECT_EQ (0, idx);
  int64 xdi = -1;
  EXPECT_TORTEQ (OB_OK, pool_index (phb, &xdi));
  EXPECT_EQ (1, xdi);
  protein_free (pro);
  float64 delta = ob_current_time () - start;
  // If it succeeeds but takes ~10 minutes, that's bad
  EXPECT_GT (long_time * FUDGE_FACTOR, delta);

  EXPECT_TORTEQ (OB_OK, pool_seekto (cmd.ph, 50));
  start = ob_current_time ();
  EXPECT_TORTEQ (OB_OK,
                 pool_await_next_multi (g, long_time, &ho, &pro, NULL, &idx));
  EXPECT_EQ (cmd.ph, ho);
  EXPECT_EQ (50, idx);
  EXPECT_TORTEQ (OB_OK, pool_index (cmd.ph, &xdi));
  EXPECT_EQ (51, xdi);
  protein_free (pro);
  delta = ob_current_time () - start;
  // If it succeeeds but takes ~10 minutes, that's bad
  EXPECT_GT (long_time * FUDGE_FACTOR, delta);

  EXPECT_EQ (0, pthread_join (thr, NULL));
  EXPECT_TORTEQ (OB_OK, pool_leave_gang (g, cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_leave_gang (g, phb));
  EXPECT_TORTEQ (OB_OK, pool_disband_gang (g, false));
  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  EXPECT_TORTEQ (OB_OK, pool_withdraw (phb));
  EXPECT_TORTEQ (OB_OK, pool_dispose (pool_b));
  pool_cmd_free_options (&cmd);
  slaw_free (s_pool_b);
}

static void test_runout_and_await (ob_retort (*func) (
  pool_hose ph, protein *ret_prot, pool_timestamp *ret_ts, int64 *ret_index))
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));
  POOL_CMD_OPEN_POOL (&cmd);
  deposit_100_proteins (cmd.ph);
  EXPECT_TORTEQ (OB_OK, pool_runout (cmd.ph));

  pthread_t thr;
  EXPECT_EQ (0, pthread_create (&thr, NULL, deposit_one_protein,
                                (void *) cmd.pool_name));

  int64 idx = -1;
  protein p = NULL;
  ob_retort tort = func (cmd.ph, &p, NULL, &idx);

  EXPECT_TORTEQ (OB_OK, tort);
  EXPECT_EQ (100, idx);
  EXPECT_STREQ ("value_gravity", slaw_path_get_string (p, "key_gravity",
                                                       "rudely flatten boxes"));

  if (tort >= OB_OK)
    protein_free (p);

  EXPECT_EQ (0, pthread_join (thr, NULL));
  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  pool_cmd_free_options (&cmd);
}

TEST (MiscPoolTest, RunoutAndAwaitSingle)
{
  test_runout_and_await (func_await_next);
}

TEST (MiscPoolTest, RunoutAndAwaitMulti)
{
  test_runout_and_await (func_await_next_multi);
}

static ob_retort degenerate_multi (pool_hose ph, pool_timestamp timeout,
                                   protein *ret_prot, pool_timestamp *ret_ts,
                                   int64 *ret_index)
{
  pool_gang g = NULL;
  EXPECT_TORTEQ (OB_OK, pool_new_gang (&g));
  EXPECT_TORTEQ (OB_OK, pool_join_gang (g, ph));
  pool_hose ho = NULL;
  ob_retort tort =
    pool_await_next_multi (g, timeout, &ho, ret_prot, ret_ts, ret_index);
  EXPECT_TORTEQ (OB_OK, pool_disband_gang (g, false));
  return tort;
}

static void my_await_test (
  bool empty,
  ob_retort (*func) (pool_hose ph, pool_timestamp timeout, protein *ret_prot,
                     pool_timestamp *ret_ts, int64 *ret_index),
  pool_timestamp timeout, ob_retort expected)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));
  POOL_CMD_OPEN_POOL (&cmd);

  if (!empty)
    {
      deposit_100_proteins (cmd.ph);
      EXPECT_TORTEQ (OB_OK, pool_runout (cmd.ph));
    }

  protein p;
  ob_retort tort = func (cmd.ph, timeout, &p, NULL, NULL);
  EXPECT_TORTEQ (expected, tort);

  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  pool_cmd_free_options (&cmd);
}

TEST (MiscPoolTest, ZeroAwaitEmpty)
{
  my_await_test (true, pool_await_next, POOL_NO_WAIT, POOL_AWAIT_TIMEDOUT);
}

TEST (MiscPoolTest, NonzeroAwaitEmpty)
{
  my_await_test (true, pool_await_next, 1e-9, POOL_AWAIT_TIMEDOUT);
}

TEST (MiscPoolTest, ZeroMultiAwaitEmpty)
{
  my_await_test (true, degenerate_multi, POOL_NO_WAIT, POOL_AWAIT_TIMEDOUT);
}

TEST (MiscPoolTest, NonzeroMultiAwaitEmpty)
{
  my_await_test (true, degenerate_multi, 1e-9, POOL_AWAIT_TIMEDOUT);
}

TEST (MiscPoolTest, ZeroAwaitRunout)
{
  my_await_test (false, pool_await_next, POOL_NO_WAIT, POOL_AWAIT_TIMEDOUT);
}

TEST (MiscPoolTest, NonzeroAwaitRunout)
{
  my_await_test (false, pool_await_next, 1e-9, POOL_AWAIT_TIMEDOUT);
}

TEST (MiscPoolTest, ZeroMultiAwaitRunout)
{
  my_await_test (false, degenerate_multi, POOL_NO_WAIT, POOL_AWAIT_TIMEDOUT);
}

TEST (MiscPoolTest, NonzeroMultiAwaitRunout)
{
  my_await_test (false, degenerate_multi, 1e-9, POOL_AWAIT_TIMEDOUT);
}

// A test for bug 1286
TEST (MiscPoolTest, NegativeNext)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));
  POOL_CMD_OPEN_POOL (&cmd);
  deposit_100_proteins (cmd.ph);
  EXPECT_TORTEQ (OB_OK, pool_seekto (cmd.ph, -1));
  protein p = NULL;
  int64 idx = 666;
  ob_retort tort = pool_next (cmd.ph, &p, NULL, &idx);
  EXPECT_TORTEQ (OB_OK, tort);
  EXPECT_EQ (0, idx);
  EXPECT_EQ (0, slaw_path_get_int64 (p, "n", 456));
  protein_free (p);
  EXPECT_TORTEQ (OB_OK, pool_index (cmd.ph, &idx));
  EXPECT_EQ (1, idx);
  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  pool_cmd_free_options (&cmd);
}

TEST (MiscPoolTest, NegativeNth)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));
  POOL_CMD_OPEN_POOL (&cmd);
  deposit_100_proteins (cmd.ph);
  protein p = NULL;
  int64 idx = 666;
  ob_retort tort = pool_nth_protein (cmd.ph, -1, &p, NULL);
  EXPECT_TORTEQ (OB_OK, tort);
  EXPECT_EQ (99, slaw_path_get_int64 (p, "n", 456));
  protein_free (p);
  EXPECT_TORTEQ (OB_OK, pool_index (cmd.ph, &idx));
  EXPECT_EQ (0, idx);
  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  pool_cmd_free_options (&cmd);
}

TEST (MiscPoolTest, PoolFetch)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));
  POOL_CMD_OPEN_POOL (&cmd);
  deposit_100_proteins (cmd.ph);

  unt8 bites[256];
  for (size_t i = 0; i < 256; i++)
    bites[i] = (unt8) i;

  protein foo =
    protein_from_ffr (slaw_string ("foo"), slaw_string ("bar"), bites, 256);
  float64 foots;
  EXPECT_TORTEQ (OB_OK, pool_deposit_ex (cmd.ph, foo, NULL, &foots));

  int64 idx_new, idx_old;
  pool_fetch_op ops[8];
  OB_CLEAR (ops);
  ops[0].idx = 0;
  ops[0].want_descrips = true;
  ops[0].want_ingests = false;
  ops[0].rude_offset = -1;
  ops[1].idx = 1;
  ops[1].want_descrips = false;
  ops[1].want_ingests = true;
  ops[1].rude_offset = -1;
  ops[2].idx = 2;
  ops[2].want_descrips = true;
  ops[2].want_ingests = true;
  ops[2].rude_offset = -1;
  ops[3].idx = 123;
  ops[3].want_descrips = true;
  ops[3].want_ingests = true;
  ops[3].rude_offset = -1;
  ops[4].idx = 100;
  ops[4].want_descrips = true;
  ops[4].want_ingests = true;
  ops[4].rude_offset = -1;
  ops[5].idx = 100;
  ops[5].want_descrips = true;
  ops[5].want_ingests = true;
  ops[5].rude_offset = 0;
  ops[5].rude_length = -1;
  ops[6].idx = 100;
  ops[6].want_descrips = true;
  ops[6].want_ingests = true;
  ops[6].rude_offset = 128;
  ops[6].rude_length = -1;
  ops[7].idx = 100;
  ops[7].want_descrips = true;
  ops[7].want_ingests = true;
  ops[7].rude_offset = 64;
  ops[7].rude_length = 64;
  pool_fetch (cmd.ph, ops, 8, &idx_old, &idx_new);
  EXPECT_EQ (0, idx_old);
  EXPECT_EQ (100, idx_new);

  EXPECT_EQ (1, slaw_list_count (protein_descrips (ops[0].p)));
  EXPECT_EQ (-1, slaw_list_count (protein_ingests (ops[0].p)));
  EXPECT_EQ (-1, slaw_list_count (protein_descrips (ops[1].p)));
  EXPECT_EQ (1, slaw_list_count (protein_ingests (ops[1].p)));
  EXPECT_EQ (1, slaw_list_count (protein_descrips (ops[2].p)));
  EXPECT_EQ (1, slaw_list_count (protein_ingests (ops[2].p)));

  for (int i = 0; i < 3; i++)
    {
      EXPECT_TORTEQ (OB_OK, ops[i].tort);
      EXPECT_EQ (i, ops[i].idx);
      EXPECT_EQ (1, ops[i].num_descrips);
      EXPECT_EQ (1, ops[i].num_ingests);
      EXPECT_EQ (16, ops[i].descrip_bytes);
      EXPECT_EQ (40, ops[i].ingest_bytes);
      EXPECT_EQ (0, ops[i].rude_bytes);
      EXPECT_EQ (72, ops[i].total_bytes);
      EXPECT_LT (1262304000.0, ops[i].ts);   // at least 2010
      EXPECT_GT (32819904000.0, ops[i].ts);  // but less than 3010
      protein_free (ops[i].p);
    }

  EXPECT_TORTEQ (POOL_NO_SUCH_PROTEIN, ops[3].tort);

  for (int i = 4; i < 8; i++)
    {
      EXPECT_TORTEQ (OB_OK, ops[i].tort);
      EXPECT_EQ (100, ops[i].idx);
      EXPECT_EQ (-1, ops[i].num_descrips);
      EXPECT_EQ (-1, ops[i].num_ingests);
      EXPECT_EQ (256, ops[i].rude_bytes);
      EXPECT_EQ (foots, ops[i].ts);
    }

  int64 len4, len5, len6, len7;
  protein_rude (ops[4].p, &len4);
  protein_rude (ops[5].p, &len5);
  const unt8 *rude6 = (unt8 *) protein_rude (ops[6].p, &len6);
  const unt8 *rude7 = (unt8 *) protein_rude (ops[7].p, &len7);

  EXPECT_EQ (0, len4);
  EXPECT_EQ (256, len5);
  EXPECT_EQ (128, len6);
  EXPECT_EQ (64, len7);

  EXPECT_TRUE (proteins_equal (ops[5].p, foo));

  for (size_t i = 0; i < 128; i++)
    EXPECT_EQ (i + 128, rude6[i]);

  for (size_t i = 0; i < 64; i++)
    EXPECT_EQ (i + 64, rude7[i]);

  for (int i = 4; i < 8; i++)
    protein_free (ops[i].p);

  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  pool_cmd_free_options (&cmd);
  protein_free (foo);
}

TEST (MiscPoolTest, PoolFetchExNewest)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));
  POOL_CMD_OPEN_POOL (&cmd);
  deposit_100_proteins (cmd.ph);

  int64 idx_new, idx_old;
  pool_fetch_op ops[3];
  OB_CLEAR (ops);
  ops[0].idx = 0;
  ops[0].want_descrips = true;
  ops[0].want_ingests = false;
  ops[0].rude_offset = -1;
  ops[1].idx = 123;
  ops[1].want_descrips = true;
  ops[1].want_ingests = true;
  ops[1].rude_offset = -1;
  ops[2].idx = 2;
  ops[2].want_descrips = true;
  ops[2].want_ingests = true;
  ops[2].rude_offset = -1;
  pool_fetch_ex (cmd.ph, ops, 3, &idx_old, &idx_new, true);
  EXPECT_EQ (0, idx_old);
  EXPECT_EQ (99, idx_new);

  EXPECT_EQ (1, slaw_list_count (protein_descrips (ops[0].p)));
  EXPECT_EQ (-1, slaw_list_count (protein_ingests (ops[0].p)));
  EXPECT_EQ (1, slaw_list_count (protein_descrips (ops[1].p)));
  EXPECT_EQ (1, slaw_list_count (protein_ingests (ops[1].p)));
  EXPECT_EQ (1, slaw_list_count (protein_descrips (ops[2].p)));
  EXPECT_EQ (1, slaw_list_count (protein_ingests (ops[2].p)));

  EXPECT_EQ (0, ops[0].idx);
  EXPECT_EQ (99, ops[1].idx);
  EXPECT_EQ (2, ops[2].idx);

  EXPECT_EQ (99, slaw_path_get_int64 (ops[1].p, "n", -1));
  EXPECT_EQ (2, slaw_path_get_int64 (ops[2].p, "n", -1));

  for (int i = 0; i < 3; i++)
    {
      EXPECT_TORTEQ (OB_OK, ops[i].tort);
      EXPECT_EQ (1, ops[i].num_descrips);
      EXPECT_EQ (1, ops[i].num_ingests);
      EXPECT_EQ (16, ops[i].descrip_bytes);
      EXPECT_EQ (40, ops[i].ingest_bytes);
      EXPECT_EQ (0, ops[i].rude_bytes);
      EXPECT_EQ (72, ops[i].total_bytes);
      EXPECT_LT (1262304000.0, ops[i].ts);   // at least 2010
      EXPECT_GT (32819904000.0, ops[i].ts);  // but less than 3010
      protein_free (ops[i].p);
    }

  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  pool_cmd_free_options (&cmd);
}

TEST (MiscPoolTest, PoolFetchExOldest)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));
  pool_cmd_fill_pool (&cmd);
  POOL_CMD_OPEN_POOL (&cmd);

  int64 idx_new, idx_old;
  pool_fetch_op ops[3];
  OB_CLEAR (ops);
  ops[0].idx = 0;
  ops[0].want_descrips = true;
  ops[0].want_ingests = true;
  ops[0].rude_offset = -1;
  ops[1].idx = 1;
  ops[1].want_descrips = true;
  ops[1].want_ingests = true;
  ops[1].rude_offset = -1;
  ops[2].idx = 2;
  ops[2].want_descrips = true;
  ops[2].want_ingests = true;
  ops[2].rude_offset = -1;
  pool_fetch_ex (cmd.ph, ops, 3, &idx_old, &idx_new, true);
  EXPECT_EQ (1, idx_old);

  EXPECT_EQ (1, slaw_list_count (protein_descrips (ops[0].p)));
  EXPECT_EQ (1, slaw_list_count (protein_ingests (ops[0].p)));
  EXPECT_EQ (1, slaw_list_count (protein_descrips (ops[1].p)));
  EXPECT_EQ (1, slaw_list_count (protein_ingests (ops[1].p)));
  EXPECT_EQ (1, slaw_list_count (protein_descrips (ops[2].p)));
  EXPECT_EQ (1, slaw_list_count (protein_ingests (ops[2].p)));

  EXPECT_EQ (1, ops[0].idx);
  EXPECT_EQ (1, ops[1].idx);
  EXPECT_EQ (2, ops[2].idx);

  for (int i = 0; i < 3; i++)
    {
      EXPECT_TORTEQ (OB_OK, ops[i].tort);
      EXPECT_EQ (1, ops[i].num_descrips);
      EXPECT_EQ (1, ops[i].num_ingests);
      EXPECT_EQ (0, ops[i].rude_bytes);
      EXPECT_LT (1262304000.0, ops[i].ts);   // at least 2010
      EXPECT_GT (32819904000.0, ops[i].ts);  // but less than 3010
      protein_free (ops[i].p);
    }

  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  pool_cmd_free_options (&cmd);
}

static void populate_rude_data (unt8 *data, unt32 len)
{
  for (unt32 i = 0; i < len; i++)
    data[i] = i * 1993;
}

static protein make_rude_protein (unt32 len)
{
  unt8 *rude = (unt8 *) malloc (len);
  populate_rude_data (rude, len);
  protein p = protein_from_ffr (slaw_list_inline_c ("a", "b", "c", "d", NULL),
                                slaw_unt32 (len), rude, len);
  free (rude);
  return p;
}

static unt64 fnv_rude (bprotein p)
{
  int64 len;
  const unt8 *rude = (const unt8 *) protein_rude (p, &len);
  // compute 64-bit FNV hash
  unt64 h = OB_CONST_U64 (14695981039346656037);
  for (int64 i = 0; i < len; i++)
    h = (h * OB_CONST_U64 (1099511628211)) ^ rude[i];
  return h;
}

TEST (MiscPoolTest, PoolFetchRude)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));
  POOL_CMD_OPEN_POOL (&cmd);
  for (int i = 0; i < 3; i++)
    {
      protein p = make_rude_protein (50 + i * 100);
      EXPECT_TORTEQ (OB_OK, pool_deposit (cmd.ph, p, NULL));
      protein_free (p);
    }

  int64 idx_new, idx_old;
  pool_fetch_op ops[4];
  OB_CLEAR (ops);
  ops[0].idx = 0;
  ops[0].want_descrips = true;
  ops[0].want_ingests = false;
  ops[0].rude_offset = -1;
  ops[1].idx = 1;
  ops[1].want_descrips = false;
  ops[1].want_ingests = true;
  ops[1].rude_offset = 0;
  ops[1].rude_length = 100;
  ops[2].idx = 2;
  ops[2].want_descrips = true;
  ops[2].want_ingests = true;
  ops[2].rude_offset = 100;
  ops[2].rude_length = -1;
  ops[3].idx = 3;
  ops[3].want_descrips = true;
  ops[3].want_ingests = true;
  ops[3].rude_offset = 100;
  ops[3].rude_length = -1;
  pool_fetch (cmd.ph, ops, 4, &idx_old, &idx_new);
  EXPECT_EQ (0, idx_old);
  EXPECT_EQ (2, idx_new);

  EXPECT_EQ (4, slaw_list_count (protein_descrips (ops[0].p)));
  EXPECT_TRUE (NULL == protein_ingests (ops[0].p));
  EXPECT_TRUE (NULL == protein_descrips (ops[1].p));
  EXPECT_TRUE (slaw_is_unt32 (protein_ingests (ops[1].p)));
  EXPECT_EQ (4, slaw_list_count (protein_descrips (ops[2].p)));
  EXPECT_TRUE (slaw_is_unt32 (protein_ingests (ops[2].p)));

  int64 l1, l2, l3;
  protein_rude (ops[0].p, &l1);
  protein_rude (ops[1].p, &l2);
  protein_rude (ops[2].p, &l3);
  EXPECT_EQ (0, l1);
  EXPECT_EQ (100, l2);
  EXPECT_EQ (150, l3);

  EXPECT_EQ (OB_CONST_U64 (17173231656300957313), fnv_rude (ops[1].p));
  EXPECT_EQ (OB_CONST_U64 (1825855873001453224), fnv_rude (ops[2].p));

  for (int i = 0; i < 3; i++)
    {
      int64 x;
      EXPECT_TORTEQ (OB_OK, ops[i].tort);
      EXPECT_EQ (i, ops[i].idx);
      EXPECT_EQ (4, ops[i].num_descrips);
      EXPECT_EQ (-1, ops[i].num_ingests);
      EXPECT_EQ (40, ops[i].descrip_bytes);
      EXPECT_EQ (8, ops[i].ingest_bytes);
      EXPECT_EQ ((x = 50 + i * 100), ops[i].rude_bytes);
      EXPECT_EQ (64 + ((x + 7) & ~7), ops[i].total_bytes);
      EXPECT_LT (1262304000.0, ops[i].ts);   // at least 2010
      EXPECT_GT (32819904000.0, ops[i].ts);  // but less than 3010
      protein_free (ops[i].p);
    }

  EXPECT_EQ (POOL_NO_SUCH_PROTEIN, ops[3].tort);

  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  pool_cmd_free_options (&cmd);
}

TEST (MiscPoolTest, AwaitWithoutNext)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  slaw s_pool_b = slaw_string_format ("%s-b", cmd.pool_name);
  const char *pool_b = slaw_string_emit (s_pool_b);
  pool_hose phb = NULL;
  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));
  ob_retort tort =
    pool_participate_creatingly (pool_b, cmd.type, &phb, cmd.create_options);
  EXPECT_TORTEQ (POOL_CREATED, tort);
  POOL_CMD_OPEN_POOL (&cmd);
  deposit_100_proteins (cmd.ph);

  int64 idx = -1;
  EXPECT_TORTEQ (OB_OK, pool_index (cmd.ph, &idx));
  EXPECT_EQ (0, idx);

  EXPECT_TORTEQ (OB_OK, pool_runout (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_index (cmd.ph, &idx));
  EXPECT_EQ (100, idx);

  pool_gang g = NULL;
  EXPECT_TORTEQ (OB_OK, pool_new_gang (&g));
  EXPECT_TORTEQ (OB_OK, pool_join_gang (g, cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_join_gang (g, phb));

  pthread_t thr;
  EXPECT_EQ (0,
             pthread_create (&thr, NULL, deposit_one_protein, (void *) pool_b));

  const float64 long_time = 600;  // 10 minutes
  float64 start = ob_current_time ();
  EXPECT_TORTEQ (OB_OK, pool_await_multi (g, long_time));
  int64 xdi = -1;
  EXPECT_TORTEQ (OB_OK, pool_index (phb, &xdi));
  EXPECT_EQ (0, xdi);
  float64 delta = ob_current_time () - start;
  // If it succeeeds but takes ~10 minutes, that's bad
  EXPECT_GT (long_time * FUDGE_FACTOR, delta);

  EXPECT_TORTEQ (OB_OK, pool_seekto (cmd.ph, 50));
  start = ob_current_time ();
  EXPECT_TORTEQ (OB_OK, pool_await_multi (g, long_time));
  EXPECT_TORTEQ (OB_OK, pool_index (cmd.ph, &xdi));
  EXPECT_EQ (50, xdi);
  delta = ob_current_time () - start;
  // If it succeeeds but takes ~10 minutes, that's bad
  EXPECT_GT (long_time * FUDGE_FACTOR, delta);

  EXPECT_EQ (0, pthread_join (thr, NULL));
  EXPECT_TORTEQ (OB_OK, pool_leave_gang (g, cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_leave_gang (g, phb));
  EXPECT_TORTEQ (OB_OK, pool_disband_gang (g, false));
  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  EXPECT_TORTEQ (OB_OK, pool_withdraw (phb));
  EXPECT_TORTEQ (OB_OK, pool_dispose (pool_b));
  pool_cmd_free_options (&cmd);
  slaw_free (s_pool_b);
}

// copied from previous test
TEST (MiscPoolTest, LocalUriTest)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  char *tmp_dir = NULL;
  ASSERT_EQ (OB_OK, ob_mkdtemp ("test-", &tmp_dir));
  char *p;
  for (p = tmp_dir; *p; p++)
    if (*p == '\\')
      *p = '/';
  slaw s_pool_b = slaw_string_format ("local:%s/pool-b", tmp_dir);
  const char *pool_b = slaw_string_emit (s_pool_b);
  pool_hose phb = NULL;
  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));
  ob_retort tort =
    pool_participate_creatingly (pool_b, cmd.type, &phb, cmd.create_options);
  EXPECT_TORTEQ (POOL_CREATED, tort);
  POOL_CMD_OPEN_POOL (&cmd);
  deposit_100_proteins (cmd.ph);

  int64 idx = -1;
  EXPECT_TORTEQ (OB_OK, pool_index (cmd.ph, &idx));
  EXPECT_EQ (0, idx);

  EXPECT_TORTEQ (OB_OK, pool_runout (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_index (cmd.ph, &idx));
  EXPECT_EQ (100, idx);

  pool_gang g = NULL;
  EXPECT_TORTEQ (OB_OK, pool_new_gang (&g));
  EXPECT_TORTEQ (OB_OK, pool_join_gang (g, cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_join_gang (g, phb));

  pthread_t thr;
  EXPECT_EQ (0,
             pthread_create (&thr, NULL, deposit_one_protein, (void *) pool_b));

  const float64 long_time = 600;  // 10 minutes
  float64 start = ob_current_time ();
  EXPECT_TORTEQ (OB_OK, pool_await_multi (g, long_time));
  int64 xdi = -1;
  EXPECT_TORTEQ (OB_OK, pool_index (phb, &xdi));
  EXPECT_EQ (0, xdi);
  float64 delta = ob_current_time () - start;
  // If it succeeeds but takes ~10 minutes, that's bad
  EXPECT_GT (long_time * FUDGE_FACTOR, delta);

  EXPECT_TORTEQ (OB_OK, pool_seekto (cmd.ph, 50));
  start = ob_current_time ();
  EXPECT_TORTEQ (OB_OK, pool_await_multi (g, long_time));
  EXPECT_TORTEQ (OB_OK, pool_index (cmd.ph, &xdi));
  EXPECT_EQ (50, xdi);
  delta = ob_current_time () - start;
  // If it succeeeds but takes ~10 minutes, that's bad
  EXPECT_GT (long_time * FUDGE_FACTOR, delta);

  EXPECT_EQ (0, pthread_join (thr, NULL));
  EXPECT_TORTEQ (OB_OK, pool_leave_gang (g, cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_leave_gang (g, phb));
  EXPECT_TORTEQ (OB_OK, pool_disband_gang (g, false));
  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  EXPECT_TORTEQ (OB_OK, pool_withdraw (phb));

  // now we are going to test listing the local pool
  slaw the_list = NULL;
  slaw uri = slaw_string_format ("local:%s", tmp_dir);
  EXPECT_TORTEQ (OB_OK, pool_list_ex (slaw_string_emit (uri), &the_list));
  EXPECT_EQ (1, slaw_list_count (the_list));
  EXPECT_STREQ ("pool-b", slaw_string_emit (slaw_list_emit_first (the_list)));
  slaw_free (the_list);
  slaw_free (uri);

  EXPECT_TORTEQ (OB_OK, pool_dispose (pool_b));
  pool_cmd_free_options (&cmd);
  slaw_free (s_pool_b);
  rmdir (tmp_dir);
  free (tmp_dir);
}

typedef struct
{
  const char *name;
  pool_timestamp donetime;
} temp_empty_struct;

static void *temp_empty_main (void *v)
{
  temp_empty_struct *tes = (temp_empty_struct *) v;
  const char *name = tes->name;

  pool_hose ph;
  EXPECT_TORTEQ (OB_OK, pool_participate (name, &ph, NULL));

  while (ob_current_time () < tes->donetime)
    {
      int64 idx;
      EXPECT_TORTEQ (OB_OK, pool_newest_index (ph, &idx));
      EXPECT_GE (idx, 0);
      EXPECT_TORTEQ (OB_OK, pool_oldest_index (ph, &idx));
      EXPECT_GE (idx, 0);
    }

  EXPECT_TORTEQ (OB_OK, pool_withdraw (ph));
  return NULL;
}

// Tests the cases in pool_mmap.c where it busy-waits because a
// pool is "temporarily empty" (e. g. log messages 0x20104029
// and 0x2010402a)
TEST (MiscPoolTest, TemporarilyEmpty)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));
  POOL_CMD_OPEN_POOL (&cmd);
  ob_retort tort;
  int64 idx;
  pool_timestamp donetime;
  unt64 size = cmd.size;
  protein p = NULL;

  if (ob_running_under_valgrind ())
    // Valgrind is really slow, so we'll cheat a little in finding the best
    // size, to avoid this test taking forever.
    size -= 920;

  do
    {
      EXPECT_GT (size, 0U);
      Free_Protein (p);
      size -= 8;
      slaw big = slaw_unt8_array_filled (size, 'P');
      p = protein_from_ff (NULL, big);
      tort = pool_deposit_ex (cmd.ph, p, &idx, &donetime);
    }
  while (tort == POOL_PROTEIN_BIGGER_THAN_POOL);
  EXPECT_TORTEQ (OB_OK, tort);
  EXPECT_EQ (0, idx);
  EXPECT_LT (1262304000.0, donetime);   // at least 2010
  EXPECT_GT (32819904000.0, donetime);  // but less than 3010
  donetime += 10;                       // Let's run this test for 10 seconds

  pthread_t thr;
  temp_empty_struct tes;
  tes.name = cmd.pool_name;
  tes.donetime = donetime;
  EXPECT_EQ (0, pthread_create (&thr, NULL, temp_empty_main, &tes));

  pool_timestamp mytime;
  do
    {
      tort = pool_deposit_ex (cmd.ph, p, &idx, &mytime);
      EXPECT_TORTEQ (OB_OK, tort);
      EXPECT_GT (idx, 0);
    }
  while (tort >= OB_OK && mytime < donetime);

  Free_Protein (p);

  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_EQ (0, pthread_join (thr, NULL));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  pool_cmd_free_options (&cmd);
}

/* This is similar to RecentServerOnly.RenameSourceDoesNotExist, except that
 * in RecentServerOnly we know rename is supported, so we expect it to
 * return POOL_NO_SUCH_POOL.  But here in MiscPoolTest, we might be running
 * a compatibility test against an old server that doesn't support rename.
 * So we have to expect either POOL_NO_SUCH_POOL or POOL_UNSUPPORTED_OPERATION.
 * But this lets us check that the POOL_UNSUPPORTED_OPERATION retort is
 * returned correctly, without leaking memory, etc. */
TEST (MiscPoolTest, MinimalRenameTest)
{
  const char *s1 = "it's astounding";
  const char *s2 = "time is fleeting";

  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  slaw pool1_s = slaw_string_format ("%s.dir/%s", cmd.pool_name, s1);
  slaw pool2_s = slaw_string_format ("%s.dir/%s", cmd.pool_name, s2);
  const char *pool1 = slaw_string_emit (pool1_s);
  const char *pool2 = slaw_string_emit (pool2_s);

  const ob_retort actual = pool_rename (pool1, pool2);
  if (POOL_UNSUPPORTED_OPERATION != actual)
    {
      const ob_retort expected = POOL_NO_SUCH_POOL;
      EXPECT_TORTEQ (expected, actual);
    }

  slaw_free (pool1_s);
  slaw_free (pool2_s);
  pool_cmd_free_options (&cmd);
}

// These next four (and a half) tests are a little weird because they use
// hardcoded names, and don't depend on whether we're supposed to be testing
// local or remote pools.
TEST (MiscPoolTest, MismatchedRenameTest)
{
  EXPECT_EQ (POOL_IMPOSSIBLE_RENAME,
             pool_rename ("foo", "tcp://example.com/bar"));
}

TEST (MiscPoolTest, MalformedRenameTest)
{
  const ob_retort expected = POOL_POOLNAME_BADTH;
  const ob_retort actual =
    pool_rename ("tcp://malformed.com", "tcp://malformed.com");
  EXPECT_TORTEQ (expected, actual);
}

TEST (MiscPoolTest, ImpossiblePortTest)
{
  const char *name = "tcp://"
                     "google.com:"
                     "100000000000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000/foo";
  const ob_retort expected = POOL_POOLNAME_BADTH;
  const ob_retort actual = pool_rename (name, name);
  EXPECT_TORTEQ (expected, actual);
}

TEST (MiscPoolTest, ImpossiblePortTest2)
{
  const char *name = "tcp://google.com:crap/foo";
  const ob_retort expected = POOL_POOLNAME_BADTH;
  const ob_retort actual = pool_rename (name, name);
  EXPECT_TORTEQ (expected, actual);
}

TEST (MiscPoolTest, CrossServerRenameTest)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);

  // suppress the message:
  // error: <2010801a> Can't rename from localhost:65455 to example.com:65456
  EXPECT_TORTEQ (OB_OK, ob_suppress_message (OBLV_ERROR, 0x2010801a));

  const ob_retort expected = POOL_IMPOSSIBLE_RENAME;
  const ob_retort actual = pool_rename (cmd.pool_name, "tcp://example.com/bar");
  EXPECT_TORTEQ (expected, actual);

  pool_cmd_free_options (&cmd);
}

// Mostly focus on testing invalid pool names.  Valid pool names are
// tested more thoroughly in RecentServerOnly.WeirdPoolNames
TEST (MiscPoolTest, PoolValidateName)
{
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name (NULL));
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name (""));
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name ("foo:bar"));
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name ("foo\\bar"));
  EXPECT_TORTEQ (OB_OK, pool_validate_name ("foo..bar"));
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name ("foo//bar"));
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name (".foobar"));
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name ("/foobar"));
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name ("foobar/"));
  EXPECT_TORTEQ (OB_OK, pool_validate_name ("foobar"));
  EXPECT_TORTEQ (OB_OK, pool_validate_name ("foo/bar"));
  EXPECT_TORTEQ (OB_OK, pool_validate_name ("foo.bar"));
  EXPECT_TORTEQ (OB_OK, pool_validate_name ("Something with spaces in it!"));
  EXPECT_TORTEQ (OB_OK, pool_validate_name ("^^^ I like hats ^^^"));
  EXPECT_EQ (POOL_POOLNAME_BADTH,
             pool_validate_name ("*** It's full of stars! ***"));
  EXPECT_TORTEQ (OB_OK, pool_validate_name ("f"));
  // Well, it used to be, but now it's not :)
  EXPECT_EQ (POOL_POOLNAME_BADTH,
             pool_validate_name ("Perhaps it shouldn't be legal\n"
                                 "to have newlines in your pool\n"
                                 "name, but currently it is!\n"));
  EXPECT_TORTEQ (OB_OK, pool_validate_name ("I am a legal pool name ;)"));
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name ("But I am not :("));
  // Because it's for validating local pool names, not pool URIs
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name ("tcp://example.com/fb"));
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name ("."));
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name (".."));
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name ("i/am/./evil"));
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name ("i/am/../evil"));
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name ("i/am//evil"));
  EXPECT_EQ (POOL_POOLNAME_BADTH,
             pool_validate_name ("We don't allow pool names to end with a ."));
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name ("the final frontier "));
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name ("CONOUT$"));
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name ("I/like/.emacs"));
  // We don't allow any Unicode for now (bug 1592)
  EXPECT_EQ (POOL_POOLNAME_BADTH,
             pool_validate_name ("\302\277Qui\303\251n quiere "
                                 "ser millonario?"));
  EXPECT_EQ (POOL_POOLNAME_BADTH,
             pool_validate_name ("\330\254\331\205\331\207\331\210\330\261"
                                 "\331\212\330\251 "
                                 "\331\205\330\265\330\261 "
                                 "\330\247\331\204\330\271\330\261\330\250"
                                 "\331\212"));
  EXPECT_EQ (POOL_POOLNAME_BADTH,
             pool_validate_name ("I \342\231\245 toxic waste"));
  EXPECT_EQ (POOL_POOLNAME_BADTH,
             pool_validate_name ("I wouldn't s\360\235\224\270y "
                                 "J\360\235\224\270mie's "
                                 "\360\235\224\270n evil genius"));
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name ("a name with \" in it"));
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name ("a name with < in it"));
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name ("a name with > in it"));
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name ("a name with : in it"));
  // This is only illegal because "a name with " ends in a space.
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name ("a name with / in it"));
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name ("a name with \\ in it"));
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name ("a name with | in it"));
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name ("a name with ? in it"));
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name ("a name with * in it"));
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name ("LPT1"));
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name ("LPT9"));
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name ("COM1"));
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name ("COM9"));
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name ("CON"));
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name ("AUX"));
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name ("NuL"));
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name ("NUL.txt"));
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name ("foo/lpt5/bar"));
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name ("lost+found"));
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name ("LOST+FOUND"));
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name ("lost+found.txt"));
  EXPECT_EQ (POOL_POOLNAME_BADTH, pool_validate_name ("lost+found/foo"));
}

TEST (MiscPoolTest, IllegalOptions)
{
  // suppress the message:
  // error: <20101021> For pool 'test_pool', options is not a map or a protein
  EXPECT_TORTEQ (OB_OK, ob_suppress_message (OBLV_ERROR, 0x20101021));

  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  slaw badness = slaw_string ("Zygohistomorphic prepromorphisms");
  EXPECT_EQ (POOL_NOT_A_PROTEIN_OR_MAP,
             pool_create (cmd.pool_name, cmd.type, badness));
  EXPECT_EQ (POOL_NO_SUCH_POOL, pool_dispose (cmd.pool_name));
  slaw_free (badness);
  pool_cmd_free_options (&cmd);
}

TEST (MiscPoolTest, IllegalOptions2)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));
  slaw popt = slaw_float64 (3.14);
  EXPECT_TORTEQ (OB_OK, pool_participate (cmd.pool_name, &cmd.ph, popt));
  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  slaw_free (popt);
  pool_cmd_free_options (&cmd);
}

#ifndef _MSC_VER  // CLOEXEC not relevant on Windows
TEST (MiscPoolTest, DISABLED_CloseOnExec)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));
  POOL_CMD_OPEN_POOL (&cmd);
  EXPECT_TORTEQ (OB_OK, pool_hose_enable_wakeup (cmd.ph));
  fd_and_description fd[5];
  const int nfd = sizeof (fd) / sizeof (fd[0]);
  private_get_file_descriptors (cmd.ph, fd, nfd);
  refute_inheritance (fd, nfd);
  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  pool_cmd_free_options (&cmd);
}
#endif  // _MSC_VER

TEST (MiscPoolTest, PoolExists)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));
  EXPECT_TORTEQ (OB_YES, pool_exists (cmd.pool_name));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  EXPECT_TORTEQ (OB_NO, pool_exists (cmd.pool_name));
  char *bad = strdup (cmd.pool_name);
  /* I believe a name ending in slash is illegal in all versions
   * of plasma, old and new. */
  bad[strlen (bad) - 1] = '/';
  EXPECT_TORTEQ (POOL_POOLNAME_BADTH, pool_exists (bad));
  free (bad);
  pool_cmd_free_options (&cmd);
}

TEST (MiscPoolTest, ContextAllocFree)
{
  pool_context ctx = NULL;
  EXPECT_TORTEQ (OB_ARGUMENT_WAS_NULL, pool_new_context (NULL));
  pool_free_context (NULL);
  EXPECT_TORTEQ (OB_OK, pool_new_context (&ctx));
  pool_free_context (ctx);
}

TEST (MiscPoolTest, ClientCertificateInContext)
{
  pool_context ctx = NULL;
  pool_cmd_info cmd;

  /* Use same cert as would be found by normal mechanism */
  /* FIXME: also verify that using a bogus cert here fails */
  char *my_certificate = ob_read_file_srcdir (
    "bld/cmake/fixtures/tcps/client-certificate-chain.pem");
  char *my_private_key =
    ob_read_file_srcdir ("bld/cmake/fixtures/tcps/client-private-key.pem");

  slaw options = slaw_map_inline_cc ("certificate", my_certificate,
                                     "private-key", my_private_key, NULL);

  EXPECT_TORTEQ (OB_OK, pool_new_context (&ctx));
  EXPECT_TORTEQ (OB_OK, pool_ctx_set_options (ctx, options));
  pool_cmd_options_from_env (&cmd);
  EXPECT_TORTEQ (OB_OK, pool_create_ctx (cmd.pool_name, cmd.type,
                                         cmd.create_options, ctx));
  EXPECT_TORTEQ (OB_YES, pool_exists_ctx (cmd.pool_name, ctx));
  EXPECT_TORTEQ (OB_OK, pool_dispose_ctx (cmd.pool_name, ctx));
  EXPECT_TORTEQ (OB_NO, pool_exists_ctx (cmd.pool_name, ctx));
  pool_cmd_free_options (&cmd);
  pool_free_context (ctx);
  slaw_free (options);
  free (my_private_key);
  free (my_certificate);
}

TEST (MiscPoolTest, PoolParticipateCreatinglyRejectsNullType)
{
  pool_cmd_info cmd;
  pool_hose phb = NULL;

  pool_cmd_options_from_env (&cmd);
  EXPECT_TORTEQ (POOL_TYPE_BADTH,
                 pool_participate_creatingly (cmd.pool_name, NULL, &phb,
                                              cmd.create_options));
  EXPECT_EQ (NULL, phb);

  pool_cmd_free_options (&cmd);
}
