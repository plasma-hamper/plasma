
/* (c)  oblong industries */

// This is similar to MiscPoolTest, but is for tests which won't work
// properly with an old pool server.  (In other words, these tests get
// run for mmap pools and with a pool server from the same checkout, but
// not for the backwards compatibility tests.)

#include "plasma-gtest-helpers.h"
#include <gtest/gtest.h>
#include "libLoam/c/ob-pthread.h"
#include "libLoam/c/ob-rand.h"
#include "libLoam/c/ob-util.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-time.h"
#include "libPlasma/c/pool_cmd.h"
#include "libPlasma/c/pool_options.h"
#include "libPlasma/c/slaw-string.h"
#include "libPlasma/c/slaw-path.h"
#include <algorithm>

static void require_resizable (pool_cmd_info *cmd)
{
  slaw re = slaw_map_inline_cf ("resizable", slaw_boolean (true), "single-file",
                                slaw_boolean (true), NULL);
  protein p = protein_from_ff (
    NULL, slaw_maps_merge (protein_ingests (cmd->create_options), re, NULL));
  Free_Protein (cmd->create_options);
  cmd->create_options = p;
  slaw_free (re);
}

#ifndef _MSC_VER  // permissions not yet supported on Windows
TEST (RecentServerOnly, FailedPermissionize)
{
  if (0 == getuid ())
    OB_LOG_WARNING ("Skipping this test because we are root (bug 1403)");
  else
    {
      // this assumes we are not running as root!
      pool_cmd_info cmd;
      slaw perm = slaw_map_inline_cc ("owner", "root", NULL);
      pool_cmd_options_from_env (&cmd);
      protein opts = protein_from_ff (
        NULL,
        slaw_maps_merge (protein_ingests (cmd.create_options), perm, NULL));
      const ob_retort expected = ob_errno_to_retort (EPERM);
      const ob_retort actual = pool_create (cmd.pool_name, cmd.type, opts);
      EXPECT_TORTEQ (expected, actual);
      if (actual >= OB_OK)
        {
          // Prevent cascading failure because pool exists (bug 1404)
          EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
        }
      protein_free (opts);
      slaw_free (perm);
      pool_cmd_free_options (&cmd);
    }
}

// tests passing options as a map instead of a protein
TEST (RecentServerOnly, FailedPermissionize2)
{
  if (0 == getuid ())
    OB_LOG_WARNING ("Skipping this test because we are root (bug 1403)");
  else
    {
      // this assumes we are not running as root!
      pool_cmd_info cmd;
      slaw perm = slaw_map_inline_cc ("owner", "root", NULL);
      pool_cmd_options_from_env (&cmd);
      slaw opts =
        slaw_maps_merge (protein_ingests (cmd.create_options), perm, NULL);
      const ob_retort expected = ob_errno_to_retort (EPERM);
      const ob_retort actual = pool_create (cmd.pool_name, cmd.type, opts);
      EXPECT_TORTEQ (expected, actual);
      if (actual >= OB_OK)
        {
          // Prevent cascading failure because pool exists (bug 1404)
          EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
        }
      slaw_free (opts);
      slaw_free (perm);
      pool_cmd_free_options (&cmd);
    }
}
#endif

TEST (RecentServerOnly, SizeUsed)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  const ob_retort expected = OB_OK;
  const ob_retort actual =
    pool_create (cmd.pool_name, cmd.type, cmd.create_options);
  EXPECT_TORTEQ (expected, actual);
  POOL_CMD_OPEN_POOL (&cmd);
  const char *name = "InternalFrameInternalFrameTitlePaneInternalFrameTitlePane"
                     "MaximizeButtonWindowNotFocusedState";

  // zero proteins in pool
  protein p = NULL;
  ob_retort tort = pool_get_info (cmd.ph, -1, &p);
  EXPECT_TORTEQ (OB_OK, tort);
  int64 size_used = slaw_path_get_int64 (p, "size-used", -1);
  EXPECT_EQ (0, size_used);
  Free_Protein (p);

  // one protein in pool
  tort = pool_cmd_add_test_protein (cmd.ph, name, &p, NULL);
  EXPECT_TORTEQ (OB_OK, tort);
  Free_Protein (p);
  int64 at_least = 3 * strlen (name);
  tort = pool_get_info (cmd.ph, -1, &p);
  EXPECT_TORTEQ (OB_OK, tort);
  size_used = slaw_path_get_int64 (p, "size-used", -1);
  EXPECT_GT (size_used, at_least);
  Free_Protein (p);

  // two proteins in pool
  int64 first_size = size_used;
  tort = pool_cmd_add_test_protein (cmd.ph, name, &p, NULL);
  EXPECT_TORTEQ (OB_OK, tort);
  Free_Protein (p);
  at_least *= 2;
  tort = pool_get_info (cmd.ph, -1, &p);
  EXPECT_TORTEQ (OB_OK, tort);
  size_used = slaw_path_get_int64 (p, "size-used", -1);
  EXPECT_GT (size_used, at_least);
  EXPECT_EQ (size_used, 2 * first_size);
  Free_Protein (p);

  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  pool_cmd_free_options (&cmd);
}

typedef void (*swap_func) (slaw &a, slaw &b);

static void no_nested_pool (swap_func maybe_swap)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  slaw pool1_s = slaw_string_format ("%s.dir/qm", cmd.pool_name);
  slaw pool2_s = slaw_string_format ("%s.dir/qm/foo", cmd.pool_name);
  maybe_swap (pool1_s, pool2_s);
  const char *pool1 = slaw_string_emit (pool1_s);
  const char *pool2 = slaw_string_emit (pool2_s);

  ob_retort expected = OB_OK;
  ob_retort actual = pool_create (pool1, cmd.type, cmd.create_options);
  EXPECT_TORTEQ (expected, actual);

  expected = POOL_ILLEGAL_NESTING;
  actual = pool_create (pool2, cmd.type, cmd.create_options);
  EXPECT_TORTEQ (expected, actual);

  expected = OB_OK;
  actual = pool_dispose (pool1);
  EXPECT_TORTEQ (expected, actual);

  slaw_free (pool1_s);
  slaw_free (pool2_s);
  pool_cmd_free_options (&cmd);
}

static void dont_swap (slaw &a, slaw &b) {}

static void do_swap (slaw &a, slaw &b)
{
  std::swap (a, b);
}

TEST (RecentServerOnly, DISABLED_NoNestedPool1)
{
  no_nested_pool (dont_swap);
}

TEST (RecentServerOnly, DISABLED_NoNestedPool2)
{
  no_nested_pool (do_swap);
}

static void test_rename (const char *s1, const char *s2, bool whileOpen = false)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  slaw pool1_s = slaw_string_format ("%s.dir/%s", cmd.pool_name, s1);
  slaw pool2_s = slaw_string_format ("%s.dir/%s", cmd.pool_name, s2);
  const char *pool1 = slaw_string_emit (pool1_s);
  const char *pool2 = slaw_string_emit (pool2_s);

  ob_retort expected = OB_OK;
  ob_retort actual = pool_create (pool1, cmd.type, cmd.create_options);
  EXPECT_TORTEQ (expected, actual);

  pool_hose ph;
  EXPECT_TORTEQ (OB_OK, pool_participate (pool1, &ph, NULL));
  protein p = protein_from_ff (NULL, slaw_string ("madness takes its toll"));
  EXPECT_TORTEQ (OB_OK, pool_deposit (ph, p, NULL));

  if (whileOpen)
    {
      expected = POOL_IN_USE;
      actual = pool_rename (pool1, pool2);
      EXPECT_TORTEQ (expected, actual);
    }

  protein r = protein_from_ff (NULL, slaw_string ("I've got to keep control"));
  EXPECT_TORTEQ (OB_OK, pool_deposit (ph, r, NULL));

  EXPECT_TORTEQ (OB_OK, pool_withdraw (ph));
  ph = NULL;

  expected = OB_OK;
  actual = pool_rename (pool1, pool2);
  EXPECT_TORTEQ (expected, actual);

  EXPECT_TORTEQ (OB_OK, pool_participate (pool2, &ph, NULL));
  EXPECT_TORTEQ (OB_OK, pool_rewind (ph));

  protein q = NULL;
  expected = OB_OK;
  actual = pool_next (ph, &q, NULL, NULL);
  EXPECT_TORTEQ (expected, actual);

  EXPECT_TRUE (slawx_equal (p, q));
  Free_Protein (q);

  expected = OB_OK;
  actual = pool_next (ph, &q, NULL, NULL);
  EXPECT_TORTEQ (expected, actual);

  EXPECT_TRUE (slawx_equal (r, q));
  Free_Protein (q);

  if (whileOpen)
    {
      EXPECT_EQ (POOL_IN_USE, pool_dispose (pool2));
    }

  EXPECT_TORTEQ (OB_OK, pool_withdraw (ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (pool2));
  protein_free (p);
  protein_free (r);
  slaw_free (pool1_s);
  slaw_free (pool2_s);
  pool_cmd_free_options (&cmd);
}

TEST (RecentServerOnly, Rename)
{
  test_rename ("it's astounding", "time is fleeting");
}

TEST (RecentServerOnly, RenameAcrossDirectories1)
{
  test_rename ("it's/astounding", "time/is/fleeting");
}

TEST (RecentServerOnly, RenameAcrossDirectories2)
{
  test_rename ("madness/takes/its/toll", "stop/pay/toll");
}

TEST (RecentServerOnly, RenameWhileOpen)
{
  test_rename ("it's astounding", "time is fleeting", true);
}

TEST (RecentServerOnly, SelfRename)
{
  test_rename ("it's astounding", "it's astounding");
}

TEST (RecentServerOnly, SelfRenameWhileOpen)
{
  test_rename ("it's astounding", "it's astounding", true);
}

TEST (RecentServerOnly, RenameDestinationExists)
{
  const char *s1 = "it's astounding";
  const char *s2 = "time is fleeting";

  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  slaw pool1_s = slaw_string_format ("%s.dir/%s", cmd.pool_name, s1);
  slaw pool2_s = slaw_string_format ("%s.dir/%s", cmd.pool_name, s2);
  const char *pool1 = slaw_string_emit (pool1_s);
  const char *pool2 = slaw_string_emit (pool2_s);

  ob_retort expected = OB_OK;
  ob_retort actual = pool_create (pool1, cmd.type, cmd.create_options);
  EXPECT_TORTEQ (expected, actual);

  expected = OB_OK;
  actual = pool_create (pool2, cmd.type, cmd.create_options);
  EXPECT_TORTEQ (expected, actual);

  pool_hose ph;
  EXPECT_TORTEQ (OB_OK, pool_participate (pool1, &ph, NULL));
  protein p = protein_from_ff (NULL, slaw_string ("madness takes its toll"));
  EXPECT_TORTEQ (OB_OK, pool_deposit (ph, p, NULL));

  protein r = protein_from_ff (NULL, slaw_string ("I've got to keep control"));
  EXPECT_TORTEQ (OB_OK, pool_deposit (ph, r, NULL));

  EXPECT_TORTEQ (OB_OK, pool_withdraw (ph));
  ph = NULL;

  expected = POOL_EXISTS;
  actual = pool_rename (pool1, pool2);
  EXPECT_TORTEQ (expected, actual);

  expected = OB_OK;
  actual = pool_participate (pool1, &ph, NULL);
  EXPECT_TORTEQ (expected, actual);

  EXPECT_TORTEQ (OB_OK, pool_rewind (ph));

  protein q = NULL;
  expected = OB_OK;
  actual = pool_next (ph, &q, NULL, NULL);
  EXPECT_TORTEQ (expected, actual);

  EXPECT_TRUE (slawx_equal (p, q));
  Free_Protein (q);

  expected = OB_OK;
  actual = pool_next (ph, &q, NULL, NULL);
  EXPECT_TORTEQ (expected, actual);

  EXPECT_TRUE (slawx_equal (r, q));
  Free_Protein (q);

  EXPECT_TORTEQ (OB_OK, pool_withdraw (ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (pool1));
  EXPECT_TORTEQ (OB_OK, pool_dispose (pool2));
  protein_free (p);
  protein_free (r);
  slaw_free (pool1_s);
  slaw_free (pool2_s);
  pool_cmd_free_options (&cmd);
}

static void rename_nonexistent (pool_cmd_info &cmd)
{
  const char *s1 = "it's astounding";
  const char *s2 = "time is fleeting";

  ob_retort expected, actual;
  slaw pool1_s = slaw_string_format ("%s.dir/%s", cmd.pool_name, s1);
  slaw pool2_s = slaw_string_format ("%s.dir/%s", cmd.pool_name, s2);
  const char *pool1 = slaw_string_emit (pool1_s);
  const char *pool2 = slaw_string_emit (pool2_s);

  expected = POOL_NO_SUCH_POOL;
  actual = pool_rename (pool1, pool2);
  EXPECT_TORTEQ (expected, actual);

  slaw_free (pool1_s);
  slaw_free (pool2_s);
}

TEST (RecentServerOnly, RenameSourceDoesNotExist)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  rename_nonexistent (cmd);
  pool_cmd_free_options (&cmd);
}

TEST (RecentServerOnly, RenameSourceDoesNotExistButSomethingElseDoes)
{
  // I learned the hard way that entirely different code paths are
  // taken when no pools exist at all, versus when some pools exist
  // but not the one you want.
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  const char *s3 = "madness takes its toll";
  slaw pool3_s = slaw_string_format ("%s.dir/%s", cmd.pool_name, s3);
  const char *pool3 = slaw_string_emit (pool3_s);

  ob_retort expected = OB_OK;
  ob_retort actual = pool_create (pool3, cmd.type, cmd.create_options);
  EXPECT_TORTEQ (expected, actual);

  rename_nonexistent (cmd);

  EXPECT_TORTEQ (OB_OK, pool_dispose (pool3));
  slaw_free (pool3_s);
  pool_cmd_free_options (&cmd);
}

TEST (RecentServerOnly, AdvanceOldest)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  shrink_size_for_valgrind (&cmd);
  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));
  pool_cmd_fill_pool (&cmd);
  POOL_CMD_OPEN_POOL (&cmd);

  int64 newest, oldest, newnewest;
  EXPECT_TORTEQ (OB_OK, pool_newest_index (cmd.ph, &newest));
  EXPECT_TORTEQ (OB_OK, pool_oldest_index (cmd.ph, &oldest));
  const int64 middlest = (newest + oldest) / 2;
  EXPECT_TORTEQ (OB_OK, pool_advance_oldest (cmd.ph, middlest));
  EXPECT_TORTEQ (OB_OK, pool_newest_index (cmd.ph, &newnewest));
  EXPECT_TORTEQ (OB_OK, pool_oldest_index (cmd.ph, &oldest));
  EXPECT_EQ (newest, newnewest);
  EXPECT_EQ (oldest, middlest);

  protein p = NULL;
  EXPECT_TORTEQ (OB_OK, pool_nth_protein (cmd.ph, middlest, &p, NULL));
  EXPECT_TRUE (slaw_is_protein (p));
  Free_Protein (p);

  EXPECT_EQ (POOL_NO_SUCH_PROTEIN,
             pool_nth_protein (cmd.ph, middlest - 1, &p, NULL));

  protein q = protein_from_ff (NULL, slaw_string ("madness takes its toll"));
  EXPECT_TORTEQ (OB_OK, pool_deposit (cmd.ph, q, NULL));

  EXPECT_TORTEQ (OB_OK, pool_newest_index (cmd.ph, &newnewest));
  EXPECT_TORTEQ (OB_OK, pool_oldest_index (cmd.ph, &oldest));
  EXPECT_EQ (newest + 1, newnewest);
  EXPECT_EQ (oldest, middlest);

  EXPECT_TORTEQ (OB_OK, pool_nth_protein (cmd.ph, newnewest, &p, NULL));
  EXPECT_TRUE (proteins_equal (p, q));
  Free_Protein (p);

  Free_Protein (q);
  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  pool_cmd_free_options (&cmd);
}

TEST (RecentServerOnly, AdvanceOldestBackward)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  shrink_size_for_valgrind (&cmd);
  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));
  pool_cmd_fill_pool (&cmd);
  POOL_CMD_OPEN_POOL (&cmd);

  protein q = protein_from_ff (NULL, slaw_string ("madness takes its toll"));
  for (int i = 0; i < 3; i++)
    EXPECT_TORTEQ (OB_OK, pool_deposit (cmd.ph, q, NULL));
  Free_Protein (q);

  int64 newest, oldest, newnewest, newoldest;
  EXPECT_TORTEQ (OB_OK, pool_newest_index (cmd.ph, &newest));
  EXPECT_TORTEQ (OB_OK, pool_oldest_index (cmd.ph, &oldest));
  EXPECT_EQ (OB_NOTHING_TO_DO, pool_advance_oldest (cmd.ph, oldest - 1));
  EXPECT_TORTEQ (OB_OK, pool_newest_index (cmd.ph, &newnewest));
  EXPECT_TORTEQ (OB_OK, pool_oldest_index (cmd.ph, &newoldest));
  EXPECT_EQ (newest, newnewest);
  EXPECT_EQ (oldest, newoldest);

  protein p = NULL;
  EXPECT_TORTEQ (OB_OK, pool_nth_protein (cmd.ph, oldest, &p, NULL));
  EXPECT_TRUE (slaw_is_protein (p));
  Free_Protein (p);

  EXPECT_EQ (POOL_NO_SUCH_PROTEIN,
             pool_nth_protein (cmd.ph, oldest - 1, &p, NULL));

  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  pool_cmd_free_options (&cmd);
}

TEST (RecentServerOnly, AdvanceOldestUnchanged)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  shrink_size_for_valgrind (&cmd);
  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));
  pool_cmd_fill_pool (&cmd);
  POOL_CMD_OPEN_POOL (&cmd);

  int64 newest, oldest, newnewest, newoldest;
  EXPECT_TORTEQ (OB_OK, pool_newest_index (cmd.ph, &newest));
  EXPECT_TORTEQ (OB_OK, pool_oldest_index (cmd.ph, &oldest));
  EXPECT_EQ (OB_NOTHING_TO_DO, pool_advance_oldest (cmd.ph, oldest));
  EXPECT_TORTEQ (OB_OK, pool_newest_index (cmd.ph, &newnewest));
  EXPECT_TORTEQ (OB_OK, pool_oldest_index (cmd.ph, &newoldest));
  EXPECT_EQ (newest, newnewest);
  EXPECT_EQ (oldest, newoldest);

  protein p = NULL;
  EXPECT_TORTEQ (OB_OK, pool_nth_protein (cmd.ph, oldest, &p, NULL));
  EXPECT_TRUE (slaw_is_protein (p));
  Free_Protein (p);

  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  pool_cmd_free_options (&cmd);
}

TEST (RecentServerOnly, AdvanceOldestToNewest)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  shrink_size_for_valgrind (&cmd);
  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));
  pool_cmd_fill_pool (&cmd);
  POOL_CMD_OPEN_POOL (&cmd);

  int64 newest, oldest, newnewest;
  EXPECT_TORTEQ (OB_OK, pool_newest_index (cmd.ph, &newest));
  EXPECT_TORTEQ (OB_OK, pool_oldest_index (cmd.ph, &oldest));
  EXPECT_TORTEQ (OB_OK, pool_advance_oldest (cmd.ph, newest));
  EXPECT_TORTEQ (OB_OK, pool_newest_index (cmd.ph, &newnewest));
  EXPECT_TORTEQ (OB_OK, pool_oldest_index (cmd.ph, &oldest));
  EXPECT_EQ (newest, newnewest);
  EXPECT_EQ (oldest, newest);

  protein p = NULL;
  EXPECT_TORTEQ (OB_OK, pool_nth_protein (cmd.ph, newest, &p, NULL));
  EXPECT_TRUE (slaw_is_protein (p));
  Free_Protein (p);

  EXPECT_EQ (POOL_NO_SUCH_PROTEIN,
             pool_nth_protein (cmd.ph, newest - 1, &p, NULL));
  EXPECT_EQ (POOL_NO_SUCH_PROTEIN,
             pool_nth_protein (cmd.ph, newest + 1, &p, NULL));

  protein q = protein_from_ff (NULL, slaw_string ("madness takes its toll"));
  EXPECT_TORTEQ (OB_OK, pool_deposit (cmd.ph, q, NULL));

  EXPECT_TORTEQ (OB_OK, pool_newest_index (cmd.ph, &newnewest));
  EXPECT_TORTEQ (OB_OK, pool_oldest_index (cmd.ph, &oldest));
  EXPECT_EQ (newest + 1, newnewest);
  EXPECT_EQ (oldest, newest);

  EXPECT_TORTEQ (OB_OK, pool_nth_protein (cmd.ph, newnewest, &p, NULL));
  EXPECT_TRUE (proteins_equal (p, q));
  Free_Protein (p);

  Free_Protein (q);
  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  pool_cmd_free_options (&cmd);
}

TEST (RecentServerOnly, AdvanceOldestBeyondNewest)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  require_resizable (&cmd);
  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));
  pool_cmd_fill_pool (&cmd);
  POOL_CMD_OPEN_POOL (&cmd);

  int64 newest, oldest, newnewest;
  EXPECT_TORTEQ (OB_OK, pool_newest_index (cmd.ph, &newest));
  EXPECT_TORTEQ (OB_OK, pool_oldest_index (cmd.ph, &oldest));
  EXPECT_TORTEQ (OB_OK, pool_advance_oldest (cmd.ph, newest + 1));
  EXPECT_TORTEQ (POOL_NO_SUCH_PROTEIN, pool_newest_index (cmd.ph, &newnewest));
  EXPECT_TORTEQ (POOL_NO_SUCH_PROTEIN, pool_oldest_index (cmd.ph, &oldest));

  protein p = NULL;
  EXPECT_TORTEQ (POOL_NO_SUCH_PROTEIN,
                 pool_nth_protein (cmd.ph, newest, &p, NULL));
  EXPECT_TORTEQ (POOL_NO_SUCH_PROTEIN,
                 pool_nth_protein (cmd.ph, newest - 1, &p, NULL));
  EXPECT_TORTEQ (POOL_NO_SUCH_PROTEIN,
                 pool_nth_protein (cmd.ph, newest + 1, &p, NULL));

  protein q = protein_from_ff (NULL, slaw_string ("madness takes its toll"));
  EXPECT_TORTEQ (OB_OK, pool_deposit (cmd.ph, q, NULL));

  EXPECT_TORTEQ (OB_OK, pool_newest_index (cmd.ph, &newnewest));
  EXPECT_TORTEQ (OB_OK, pool_oldest_index (cmd.ph, &oldest));
  EXPECT_EQ (newest + 1, newnewest);
  EXPECT_EQ (newest + 1, oldest);

  EXPECT_TORTEQ (OB_OK, pool_nth_protein (cmd.ph, newnewest, &p, NULL));
  EXPECT_TRUE (proteins_equal (p, q));
  Free_Protein (p);

  Free_Protein (q);
  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  pool_cmd_free_options (&cmd);
}

typedef std::vector<slaw> sv;

static const char *spewnsave (sv &v, bslaw s)
{
  slaw o = slaw_spew_overview_to_string (s);
  v.push_back (o);
  return slaw_string_emit (o);
}

TEST (RecentServerOnly, AdvanceOldestFurtherBeyondNewest)
{
  sv f;
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  shrink_size_for_valgrind (&cmd);
  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));
  pool_cmd_fill_pool (&cmd);
  POOL_CMD_OPEN_POOL (&cmd);

  int64 newest, oldest, newnewest, newoldest;
  EXPECT_TORTEQ (OB_OK, pool_newest_index (cmd.ph, &newest));
  EXPECT_TORTEQ (OB_OK, pool_oldest_index (cmd.ph, &oldest));
  EXPECT_EQ (POOL_NO_SUCH_PROTEIN, pool_advance_oldest (cmd.ph, newest + 2));
  EXPECT_TORTEQ (OB_OK, pool_newest_index (cmd.ph, &newnewest));
  EXPECT_TORTEQ (OB_OK, pool_oldest_index (cmd.ph, &newoldest));
  EXPECT_EQ (newest, newnewest);
  EXPECT_EQ (oldest, newoldest);

  protein p = NULL;
  EXPECT_TORTEQ (OB_OK, pool_nth_protein (cmd.ph, newest, &p, NULL));
  EXPECT_TRUE (slaw_is_protein (p));

  protein q = NULL;
  EXPECT_TORTEQ (OB_OK, pool_nth_protein (cmd.ph, oldest, &q, NULL));
  EXPECT_TRUE (proteins_equal (p, q)) << "p =" << std::endl
                                      << spewnsave (f, p) << std::endl
                                      << "q =" << std::endl
                                      << spewnsave (f, q) << std::endl;
  Free_Protein (p);
  Free_Protein (q);

  EXPECT_EQ (POOL_NO_SUCH_PROTEIN,
             pool_nth_protein (cmd.ph, newest + 1, &p, NULL));

  q = protein_from_ff (NULL, slaw_string ("madness takes its toll"));
  EXPECT_TORTEQ (OB_OK, pool_deposit (cmd.ph, q, NULL));

  EXPECT_TORTEQ (OB_OK, pool_newest_index (cmd.ph, &newnewest));
  EXPECT_TORTEQ (OB_OK, pool_oldest_index (cmd.ph, &newoldest));
  EXPECT_EQ (newest + 1, newnewest);
  // XXX: this is making certain assumptions about protein sizes
  EXPECT_EQ (oldest + 1, newoldest);

  EXPECT_TORTEQ (OB_OK, pool_nth_protein (cmd.ph, newnewest, &p, NULL));
  EXPECT_TRUE (proteins_equal (p, q));
  Free_Protein (p);

  Free_Protein (q);
  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  pool_cmd_free_options (&cmd);

  for (sv::iterator it = f.begin (); it != f.end (); it++)
    slaw_free (*it);
}

typedef struct
{
  pool_hose ph;
  int64 newest;
  int64 oldest;
} stress_struct;

static void *stress_thread (void *v)
{
  stress_struct *s = (stress_struct *) v;
  for (int64 i = s->newest; i > s->oldest; i--)
    {
      protein p = NULL;
      ob_retort tort = pool_nth_protein (s->ph, i, &p, NULL);
      EXPECT_TRUE (tort == OB_OK || tort == POOL_NO_SUCH_PROTEIN);
      Free_Protein (p);
    }
  return NULL;
}

TEST (RecentServerOnly, AdvanceOldestStress)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  shrink_size_for_valgrind (&cmd);
  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));
  POOL_CMD_OPEN_POOL (&cmd);

  // Fill pool almost full, but not quite
  // (So can't call pool_cmd_fill_pool(), which will wrap)
  const int goal = (ob_running_under_valgrind () ? 435 : 7700);
  protein p = pool_cmd_create_test_protein ("first");
  for (int i = 0; i < goal; i++)
    EXPECT_TORTEQ (OB_OK, pool_deposit (cmd.ph, p, NULL));
  Free_Protein (p);

  stress_struct s;
  OB_CLEAR (s);
  EXPECT_TORTEQ (OB_OK, pool_newest_index (cmd.ph, &s.newest));
  EXPECT_TORTEQ (OB_OK, pool_oldest_index (cmd.ph, &s.oldest));

  pthread_t thr;
  EXPECT_TORTEQ (OB_OK, pool_hose_clone (cmd.ph, &s.ph));
  EXPECT_EQ (0, pthread_create (&thr, NULL, stress_thread, &s));

  ob_retort tort;
  for (int64 i = s.oldest + 1; i < s.newest; i++)
    EXPECT_TORTEQ (OB_OK, (tort = pool_advance_oldest (cmd.ph, i)))
      << "tort = " << ob_error_string (tort) << ", i = " << i;

  EXPECT_EQ (0, pthread_join (thr, NULL));
  EXPECT_TORTEQ (OB_OK, pool_withdraw (s.ph));
  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  pool_cmd_free_options (&cmd);
}

static void pool_in_use_test (ob_retort (*func) (const char *name),
                              ob_retort func_expectation)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  ob_retort expected = OB_OK;
  ob_retort actual = pool_create (cmd.pool_name, cmd.type, cmd.create_options);
  EXPECT_TORTEQ (expected, actual);
  POOL_CMD_OPEN_POOL (&cmd);
  const char *name = "InternalFrameInternalFrameTitlePaneInternalFrameTitlePane"
                     "MaximizeButtonWindowNotFocusedState";

  // one protein in pool
  protein p;
  ob_retort tort = pool_cmd_add_test_protein (cmd.ph, name, &p, NULL);
  EXPECT_TORTEQ (OB_OK, tort);
  Free_Protein (p);

  EXPECT_EQ (func_expectation, func (cmd.pool_name));

  // two proteins in pool
  tort = pool_cmd_add_test_protein (cmd.ph, name, &p, NULL);
  EXPECT_TORTEQ (OB_OK, tort);

  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_TORTEQ (OB_OK, func (cmd.pool_name));

  POOL_CMD_OPEN_POOL (&cmd);
  EXPECT_TORTEQ (OB_OK, pool_rewind (cmd.ph));

  for (int i = 0; i < 2; i++)
    {
      protein q = NULL;
      expected = OB_OK;
      actual = pool_next (cmd.ph, &q, NULL, NULL);
      EXPECT_TORTEQ (expected, actual);

      EXPECT_TRUE (slawx_equal (p, q));
      Free_Protein (q);
    }

  Free_Protein (p);

  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  pool_cmd_free_options (&cmd);
}

TEST (RecentServerOnly, PoolSleep)
{
#ifdef _MSC_VER
  // pool_sleep() is a nop on Windows, so it always says OB_OK
  pool_in_use_test (pool_sleep, OB_OK);
#else
  pool_in_use_test (pool_sleep, POOL_IN_USE);
#endif
}

TEST (RecentServerOnly, PoolInUse)
{
  pool_in_use_test (pool_check_in_use, POOL_IN_USE);
}

static void *deposit_one_protein (void *arg)
{
  const char *name = (const char *) arg;
  pool_hose ph = NULL;
  protein p = NULL;
  EXPECT_TORTEQ (OB_OK, pool_participate (name, &ph, NULL));
  ob_micro_sleep (20000);
  EXPECT_TORTEQ (OB_OK, pool_cmd_add_test_protein (ph, name, &p, NULL));
  protein_free (p);
  EXPECT_TORTEQ (OB_OK, pool_withdraw (ph));
  return NULL;
}

static const char *const weird_names[] = {
  "foobar",
  "foo/bar",
  "foo.bar",
  "Something with spaces in it!",
  "^^^ I like hats ^^^",
  "f",
  "I am a legal pool name ;)",
  "This used to not be legal... but now it is!",
  "$100",
  "commercial",
  "conscience",
  "null",
  "LPT1 is not legal as a pool name, but it's okay as part of a longer name",
  "a name with   in it",
  "a name with ! in it",
  "a name with # in it",
  "a name with $ in it",
  "a name with % in it",
  "a name with & in it",
  "a name with ' in it",
  "a name with ( in it",
  "a name with ) in it",
  "a name with + in it",
  "a name with , in it",
  "a name with - in it",
  "a name with . in it",
  "a name with ; in it",
  "a name with = in it",
  "a name with @ in it",
  "a name with [ in it",
  "a name with ] in it",
  "a name with ^ in it",
  "a name with _ in it",
  "a name with ` in it",
  "a name with { in it",
  "a name with } in it",
  "a name with ~ in it",
  " the final frontier",
  "CONOUT",
  "foo/COM10/bar",
  "AUX1",
  "COM0",
  "LPT0",
  "Trey thinks no one will put %s/or %f/or worse, %n/in a pool name"};

TEST (RecentServerOnly, WeirdPoolNames)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  char *server;
  if (strstr (cmd.pool_name, "://"))
    {
      server = strdup (cmd.pool_name);
      char *slash = strrchr (server, '/');
      if (slash)
        slash[1] = 0;
    }
  else
    server = NULL;

  const size_t inc = (ob_running_under_valgrind () ? 7 : 1);

  for (size_t i = 0; i < (sizeof (weird_names) / sizeof (weird_names[0]));
       i += inc)
    {
      EXPECT_TORTEQ (OB_OK, pool_validate_name (weird_names[0]));

      slaw s;
      if (server)
        s = slaw_string_format ("%s%s", server, weird_names[i]);
      else
        s = slaw_string (weird_names[i]);
      const char *name = slaw_string_emit (s);

      ob_retort expected = OB_OK;
      ob_retort actual = pool_create (name, cmd.type, cmd.create_options);
      EXPECT_TORTEQ (expected, actual);

      pool_hose ph = NULL;
      expected = OB_OK;
      actual = pool_participate (name, &ph, NULL);
      EXPECT_TORTEQ (expected, actual);

      if (OB_OK == actual)
        {
          protein p = NULL;
          pthread_t thr;
          EXPECT_EQ (
            0, pthread_create (&thr, NULL, deposit_one_protein, (void *) name));
          EXPECT_TORTEQ (OB_OK, pool_await_next (ph, 600, &p, NULL, NULL));
          EXPECT_EQ (0, pthread_join (thr, NULL));
          Free_Protein (p);
          EXPECT_TORTEQ (OB_OK, pool_withdraw (ph));
        }

      slaw lst = NULL;
      EXPECT_TORTEQ (OB_OK, pool_list_remote (server, &lst));
      const int64 n = slaw_list_count (lst);
      EXPECT_EQ (1, n) << lst;
      if (1 == n)
        {
          const char *found = slaw_string_emit (slaw_list_emit_first (lst));
          EXPECT_STREQ (weird_names[i], found);
        }

      EXPECT_TORTEQ (OB_OK, pool_dispose (name));

      slaw_free (s);
      slaw_free (lst);
    }

  pool_cmd_free_options (&cmd);
  free (server);
}

static int64 get_size (pool_hose ph)
{
  protein p = NULL;
  EXPECT_TORTEQ (OB_OK, pool_get_info (ph, -1, &p));
  int64 size_used = slaw_path_get_int64 (p, "size-used", -1);
  protein_free (p);
  return size_used;
}

static ob_retort set_size (pool_hose ph, unt64 sz)
{
  slaw opts = slaw_map_inline_cf ("size", slaw_unt64 (sz), NULL);
  ob_retort tort = pool_change_options (ph, opts);
  slaw_free (opts);
  return tort;
}

TEST (RecentServerOnly, Resize)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  require_resizable (&cmd);

  ob_retort expected = OB_OK;
  ob_retort actual = pool_create (cmd.pool_name, cmd.type, cmd.create_options);
  EXPECT_TORTEQ (expected, actual);

  pool_hose ph;
  EXPECT_TORTEQ (OB_OK, pool_participate (cmd.pool_name, &ph, NULL));
  protein p = protein_from_ff (NULL, slaw_string ("madness takes its toll"));
  EXPECT_TORTEQ (OB_OK, pool_deposit (ph, p, NULL));

  const int64 size1 = get_size (ph);

  expected = OB_OK;
  actual = set_size (ph, 2 * MEGABYTE);
  EXPECT_TORTEQ (expected, actual);

  const int64 size2 = get_size (ph);
  EXPECT_EQ (size1, size2);

  protein r = protein_from_ff (NULL, slaw_string ("I've got to keep control"));
  EXPECT_TORTEQ (OB_OK, pool_deposit (ph, r, NULL));

  const int64 size3 = get_size (ph);
  EXPECT_GT (size3, size2);

  EXPECT_TORTEQ (OB_OK, pool_rewind (ph));

  protein q = NULL;
  expected = OB_OK;
  actual = pool_next (ph, &q, NULL, NULL);
  EXPECT_TORTEQ (expected, actual);

  EXPECT_TRUE (slawx_equal (p, q));
  Free_Protein (q);

  expected = OB_OK;
  actual = pool_next (ph, &q, NULL, NULL);
  EXPECT_TORTEQ (expected, actual);

  EXPECT_TRUE (slawx_equal (r, q));
  Free_Protein (q);

  EXPECT_TORTEQ (OB_OK, pool_withdraw (ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  protein_free (p);
  protein_free (r);
  pool_cmd_free_options (&cmd);
}

TEST (RecentServerOnly, ResizeWrapped)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  require_resizable (&cmd);

  ob_retort expected = OB_OK;
  ob_retort actual = pool_create (cmd.pool_name, cmd.type, cmd.create_options);
  EXPECT_TORTEQ (expected, actual);

  pool_cmd_fill_pool (&cmd);

  int64 idx;
  pool_hose ph;
  EXPECT_TORTEQ (OB_OK, pool_participate (cmd.pool_name, &ph, NULL));
  protein p = protein_from_ff (NULL, slaw_string ("madness takes its toll"));
  EXPECT_TORTEQ (OB_OK, pool_deposit (ph, p, &idx));

  expected = OB_OK;
  actual = set_size (ph, 512 * 1024);
  EXPECT_TORTEQ (expected, actual);

  protein r = protein_from_ff (NULL, slaw_string ("I've got to keep control"));
  EXPECT_TORTEQ (OB_OK, pool_deposit (ph, r, NULL));

  EXPECT_TORTEQ (OB_OK, pool_seekto (ph, idx));

  protein q = NULL;
  expected = OB_OK;
  actual = pool_next (ph, &q, NULL, NULL);
  EXPECT_TORTEQ (expected, actual);

  EXPECT_TRUE (slawx_equal (p, q)) << "p =" << std::endl
                                   << p << "q =" << std::endl
                                   << q;
  Free_Protein (q);

  expected = OB_OK;
  actual = pool_next (ph, &q, NULL, NULL);
  EXPECT_TORTEQ (expected, actual);

  EXPECT_TRUE (slawx_equal (r, q)) << "r =" << std::endl
                                   << r << "q =" << std::endl
                                   << q;
  Free_Protein (q);

  expected = OB_OK;
  actual = pool_withdraw (ph);
  EXPECT_TORTEQ (expected, actual);

  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  protein_free (p);
  protein_free (r);
  pool_cmd_free_options (&cmd);
}

TEST (RecentServerOnly, ResizeToBecomeEmpty)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  require_resizable (&cmd);

  ob_retort expected = OB_OK;
  ob_retort actual = pool_create (cmd.pool_name, cmd.type, cmd.create_options);
  EXPECT_TORTEQ (expected, actual);

  int64 idx, i1, i2;
  pool_hose ph;
  EXPECT_TORTEQ (OB_OK, pool_participate (cmd.pool_name, &ph, NULL));
  protein p = protein_from_ff (NULL, slaw_unt8_array_empty (64 * 1024));

  EXPECT_EQ (POOL_NO_SUCH_PROTEIN, pool_oldest_index (ph, &i1));
  EXPECT_EQ (POOL_NO_SUCH_PROTEIN, pool_newest_index (ph, &i2));

  EXPECT_TORTEQ (OB_OK, pool_deposit (ph, p, NULL));

  EXPECT_TORTEQ (OB_OK, pool_oldest_index (ph, &i1));
  EXPECT_TORTEQ (OB_OK, pool_newest_index (ph, &i2));
  EXPECT_EQ (0, i1);
  EXPECT_EQ (0, i2);

  expected = OB_OK;
  actual = set_size (ph, 64 * 1024);
  EXPECT_TORTEQ (expected, actual);

  EXPECT_TORTEQ (POOL_NO_SUCH_PROTEIN, pool_oldest_index (ph, &i1));
  EXPECT_TORTEQ (POOL_NO_SUCH_PROTEIN, pool_newest_index (ph, &i2));

  protein r = protein_from_ff (NULL, slaw_string ("I've got to keep control"));
  EXPECT_TORTEQ (OB_OK, pool_deposit (ph, r, &idx));

  EXPECT_TORTEQ (OB_OK, pool_oldest_index (ph, &i1));
  EXPECT_TORTEQ (OB_OK, pool_newest_index (ph, &i2));
  EXPECT_EQ (1, i1);
  EXPECT_EQ (1, i2);

  EXPECT_TORTEQ (OB_OK, pool_seekto (ph, idx));
  protein q = NULL;

  expected = OB_OK;
  actual = pool_next (ph, &q, NULL, NULL);
  EXPECT_TORTEQ (expected, actual);

  EXPECT_TRUE (slawx_equal (r, q)) << "r =" << std::endl
                                   << r << "q =" << std::endl
                                   << q;
  Free_Protein (q);

  expected = OB_OK;
  actual = pool_withdraw (ph);
  EXPECT_TORTEQ (expected, actual);

  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  protein_free (p);
  protein_free (r);
  pool_cmd_free_options (&cmd);
}

TEST (RecentServerOnly, ResizeWhenEmpty)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  require_resizable (&cmd);

  ob_retort expected = OB_OK;
  ob_retort actual = pool_create (cmd.pool_name, cmd.type, cmd.create_options);
  EXPECT_TORTEQ (expected, actual);

  int64 idx, i1, i2;
  pool_hose ph;
  EXPECT_TORTEQ (OB_OK, pool_participate (cmd.pool_name, &ph, NULL));

  EXPECT_EQ (POOL_NO_SUCH_PROTEIN, pool_oldest_index (ph, &i1));
  EXPECT_EQ (POOL_NO_SUCH_PROTEIN, pool_newest_index (ph, &i2));

  expected = OB_OK;
  actual = set_size (ph, 64 * 1024);
  EXPECT_TORTEQ (expected, actual);

  EXPECT_EQ (POOL_NO_SUCH_PROTEIN, pool_oldest_index (ph, &i1));
  EXPECT_EQ (POOL_NO_SUCH_PROTEIN, pool_newest_index (ph, &i2));

  protein r = protein_from_ff (NULL, slaw_string ("I've got to keep control"));
  EXPECT_TORTEQ (OB_OK, pool_deposit (ph, r, &idx));

  EXPECT_TORTEQ (OB_OK, pool_oldest_index (ph, &i1));
  EXPECT_TORTEQ (OB_OK, pool_newest_index (ph, &i2));
  EXPECT_EQ (0, i1);
  EXPECT_EQ (0, i2);

  EXPECT_TORTEQ (OB_OK, pool_seekto (ph, idx));
  protein q = NULL;

  expected = OB_OK;
  actual = pool_next (ph, &q, NULL, NULL);
  EXPECT_TORTEQ (expected, actual);

  EXPECT_TRUE (slawx_equal (r, q)) << "r =" << std::endl
                                   << r << "q =" << std::endl
                                   << q;
  Free_Protein (q);

  expected = OB_OK;
  actual = pool_withdraw (ph);
  EXPECT_TORTEQ (expected, actual);

  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  protein_free (r);
  pool_cmd_free_options (&cmd);
}

static protein toupee (void)
{
  return protein_from_ff (
    slaw_list_inline_c ("limerick", NULL),
    slaw_map_inline_cf (
      "author", slaw_string ("jh"), "president",
      slaw_string ("Leslie Lynch King, Jr."), "bug", slaw_unt16 (2072), "text",
      slaw_list_inline_c (
        "ford kept reporters at bay", "through use of a robo-toupee:",
        "when questioned, he'd shrug", "and say \"talk to the rug\",",
        "which then said \"no comment "
        "today\".",
        NULL),
      NULL));
}

TEST (RecentServerOnly, StopWhenFull)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  require_resizable (&cmd);

  slaw stop = slaw_map_inline_cf ("stop-when-full", slaw_boolean (true), NULL);
  protein opts = protein_from_ff (
    NULL, slaw_maps_merge (protein_ingests (cmd.create_options), stop, NULL));
  Free_Slaw (stop);

  EXPECT_TORTEQ (OB_OK, pool_create (cmd.pool_name, cmd.type, opts));

  POOL_CMD_OPEN_POOL (&cmd);
  protein p = toupee ();

  int64 idx;
  EXPECT_TORTEQ (OB_OK, pool_deposit (cmd.ph, p, &idx));
  EXPECT_EQ (0, idx);

  int64 oldest;
  ob_retort tort;

  do
    {
      EXPECT_TORTEQ (OB_OK, pool_oldest_index (cmd.ph, &oldest));
      EXPECT_EQ (0, oldest);
      const int64 expected = 1 + idx;
      tort = pool_deposit (cmd.ph, p, &idx);
      EXPECT_EQ (expected, idx);
    }
  while (tort == OB_OK && oldest == 0);

  EXPECT_TORTEQ (POOL_FULL, tort);
  EXPECT_TORTEQ (OB_OK, pool_oldest_index (cmd.ph, &oldest));
  EXPECT_EQ (0, oldest);

  stop = slaw_map_inline_cf ("stop-when-full", slaw_boolean (false), NULL);
  EXPECT_TORTEQ (OB_OK, pool_change_options (cmd.ph, stop));
  Free_Slaw (stop);

  EXPECT_TORTEQ (OB_OK, pool_deposit (cmd.ph, p, NULL));
  EXPECT_TORTEQ (OB_OK, pool_oldest_index (cmd.ph, &oldest));
  EXPECT_EQ (1, oldest);

  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  protein_free (p);
  protein_free (opts);
  pool_cmd_free_options (&cmd);
}

TEST (RecentServerOnly, Frozen)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  require_resizable (&cmd);

  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));

  POOL_CMD_OPEN_POOL (&cmd);
  protein p = toupee ();

  int64 idx;
  EXPECT_TORTEQ (OB_OK, pool_deposit (cmd.ph, p, &idx));
  EXPECT_EQ (0, idx);

  slaw freeze = slaw_map_inline_cf ("frozen", slaw_boolean (true), NULL);
  EXPECT_TORTEQ (OB_OK, pool_change_options (cmd.ph, freeze));
  Free_Slaw (freeze);

  EXPECT_TORTEQ (POOL_FROZEN, pool_deposit (cmd.ph, p, &idx));

  freeze = slaw_map_inline_cf ("frozen", slaw_boolean (false), NULL);
  EXPECT_TORTEQ (OB_OK, pool_change_options (cmd.ph, freeze));
  Free_Slaw (freeze);

  EXPECT_TORTEQ (OB_OK, pool_deposit (cmd.ph, p, &idx));
  EXPECT_EQ (1, idx);

  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  protein_free (p);
  pool_cmd_free_options (&cmd);
}

// simplest case of auto-dispose
TEST (RecentServerOnly, AutoDispose)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  require_resizable (&cmd);

  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));

  POOL_CMD_OPEN_POOL (&cmd);
  protein p = toupee ();

  slaw disp = slaw_map_inline_cf ("auto-dispose", slaw_boolean (true), NULL);
  EXPECT_TORTEQ (OB_OK, pool_change_options (cmd.ph, disp));
  Free_Slaw (disp);

  int64 idx;
  EXPECT_TORTEQ (OB_OK, pool_deposit (cmd.ph, p, &idx));
  EXPECT_EQ (0, idx);

  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  // should already have been disposed; disposing again should fail
  EXPECT_TORTEQ (POOL_NO_SUCH_POOL, pool_dispose (cmd.pool_name));
  protein_free (p);
  pool_cmd_free_options (&cmd);
}

// uses two hoses; make sure auto-dispose happens on the last withdraw
TEST (RecentServerOnly, AutoDispose2)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  require_resizable (&cmd);

  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));

  POOL_CMD_OPEN_POOL (&cmd);
  protein p = toupee ();

  slaw disp = slaw_map_inline_cf ("auto-dispose", slaw_boolean (true), NULL);
  EXPECT_TORTEQ (OB_OK, pool_change_options (cmd.ph, disp));
  Free_Slaw (disp);

  int64 idx;
  EXPECT_TORTEQ (OB_OK, pool_deposit (cmd.ph, p, &idx));
  EXPECT_EQ (0, idx);

  pool_hose ph_balanced;
  EXPECT_TORTEQ (OB_OK, pool_participate (cmd.pool_name, &ph_balanced, NULL));

  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));

  protein q = NULL;
  EXPECT_TORTEQ (OB_OK, pool_nth_protein (ph_balanced, 0, &q, NULL));
  EXPECT_TRUE (slawx_equal (p, q));
  EXPECT_TORTEQ (OB_OK, pool_withdraw (ph_balanced));

  // should already have been disposed; disposing again should fail
  EXPECT_TORTEQ (POOL_NO_SUCH_POOL, pool_dispose (cmd.pool_name));
  protein_free (p);
  protein_free (q);
  pool_cmd_free_options (&cmd);
}

static bool get_sync (pool_hose ph)
{
  bool result = false;
  protein p = NULL;
  EXPECT_TORTEQ (OB_OK, pool_get_info (ph, -1, &p));
  bslaw ing = protein_ingests (p);
  EXPECT_TRUE (ing != NULL);
  bslaw booboo = slaw_map_find_c (ing, "sync");
  EXPECT_TRUE (booboo != NULL);
  const bool *boop = slaw_boolean_emit (booboo);
  EXPECT_TRUE (boop != NULL);
  if (boop)
    result = *boop;
  protein_free (p);
  return result;
}

static void set_sync (pool_hose ph, bool sy)
{
  slaw opts = slaw_map_inline_cf ("sync", slaw_boolean (sy), NULL);
  EXPECT_TORTEQ (OB_OK, pool_change_options (ph, opts));
  slaw_free (opts);
}

TEST (RecentServerOnly, Sync)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  require_resizable (&cmd);

  EXPECT_TORTEQ (OB_OK,
                 pool_create (cmd.pool_name, cmd.type, cmd.create_options));

  POOL_CMD_OPEN_POOL (&cmd);
  protein p = toupee ();

  EXPECT_FALSE (get_sync (cmd.ph));
  set_sync (cmd.ph, true);
  EXPECT_TRUE (get_sync (cmd.ph));

  int64 idx;
  EXPECT_TORTEQ (OB_OK, pool_deposit (cmd.ph, p, &idx));
  EXPECT_EQ (0, idx);

  pool_hose ph_balanced;
  EXPECT_TORTEQ (OB_OK, pool_participate (cmd.pool_name, &ph_balanced, NULL));

  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));

  EXPECT_TRUE (get_sync (ph_balanced));
  set_sync (ph_balanced, false);
  EXPECT_FALSE (get_sync (ph_balanced));

  protein q = NULL;
  EXPECT_TORTEQ (OB_OK, pool_nth_protein (ph_balanced, 0, &q, NULL));
  EXPECT_TRUE (slawx_equal (p, q));
  EXPECT_TORTEQ (OB_OK, pool_withdraw (ph_balanced));

  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  protein_free (p);
  protein_free (q);
  pool_cmd_free_options (&cmd);
}

TEST (RecentServerOnly, Bug3599NotificationDirectoryDeletion)
{
  pool_cmd_info cmd;
  pool_cmd_options_from_env (&cmd);
  ob_retort expected = OB_OK;
  ob_retort actual = pool_create (cmd.pool_name, cmd.type, cmd.create_options);
  EXPECT_TORTEQ (expected, actual);
  POOL_CMD_OPEN_POOL (&cmd);

  EXPECT_TORTEQ (POOL_IN_USE, pool_dispose (cmd.pool_name));

  pool_gang g = NULL;
  pool_hose ph = NULL;
  protein ret = NULL;
  EXPECT_TORTEQ (OB_OK, pool_new_gang (&g));
  EXPECT_TORTEQ (OB_OK, pool_join_gang (g, cmd.ph));
  EXPECT_TORTEQ (POOL_AWAIT_TIMEDOUT,
                 pool_await_next_multi (g, 0.5, &ph, &ret, NULL, NULL));
  EXPECT_TORTEQ (OB_OK, pool_leave_gang (g, cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_disband_gang (g, false));

  EXPECT_TORTEQ (OB_OK, pool_withdraw (cmd.ph));
  EXPECT_TORTEQ (OB_OK, pool_dispose (cmd.pool_name));
  pool_cmd_free_options (&cmd);
}
