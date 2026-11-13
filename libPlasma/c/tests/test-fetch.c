
/* (c)  ANIMIST contributors */

#include "pool_cmd.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-util.h"
#include "libPlasma/c/c-plasma.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUM_PROTEINS 5

static int exit_code = EXIT_SUCCESS;

static const char spaces[] =
  "                                                                ";

static const char *padding (int n)
{
  int i = strlen (spaces);

  i -= (n * 4);
  if (i < 0)
    i = 0;

  return spaces + i;
}

static void check_eq_i64 (int         line,
                          const char *pfx,
                          int64       expected,
                          int64       actual)
{
  if (expected != actual)
    {
      printf ("%s#%d: FAIL: %" OB_FMT_64 "d != %" OB_FMT_64 "d\n",
              pfx, line, expected, actual);
      exit_code = EXIT_FAILURE;
    }
}

static void check_eq_tort (int         line,
                           const char *pfx,
                           ob_retort   expected,
                           ob_retort   actual)
{
  if (expected != actual)
    {
      printf ("%s#%d: FAIL: '%s' != '%s'\n",
              pfx, line,
              ob_error_string(expected),
              ob_error_string(actual));
      exit_code = EXIT_FAILURE;
    }
}

#define CHECK_EQ_I64(qwzx1, qwzx2) \
  check_eq_i64 (__LINE__, prefix, (qwzx1), (qwzx2))

#define CHECK_EQ_TORT(qwzx1, qwzx2) \
  check_eq_tort (__LINE__, prefix, (qwzx1), (qwzx2))

static void check_op(int                  indent,
                     const pool_fetch_op *op,
                     int64                loIdx,
                     int64                hiIdx,
                     int64                idx,
                     bool                 clamp,
                     int                  pos)
{
  char prefix[80];
  snprintf (prefix, sizeof(prefix), "%sidx[%d]: ", padding(indent), pos);

  int64 expect_idx  = idx;
  int64 expect_tort = OB_OK;

  if (expect_idx < loIdx)
    {
      if (clamp)
        expect_idx = loIdx;
      else
        expect_tort = POOL_NO_SUCH_PROTEIN;
    }
  else if (expect_idx > hiIdx)
    {
      if (clamp)
        expect_idx = hiIdx;
      else
        expect_tort = POOL_NO_SUCH_PROTEIN;
    }

  CHECK_EQ_TORT(expect_tort, op->tort);

  if (expect_tort == OB_OK && op->tort == OB_OK)
    {
      CHECK_EQ_I64(expect_idx, op->idx);
    }

  if (op->tort == OB_OK)
    protein_free(op->p);
}

static void do_test_leaf (int       indent,
                          pool_hose ph,
                          int64     loIdx,
                          int64     hiIdx,
                          int64     idx0,
                          int64     idx1,
                          bool      clamp,
                          bool      whole)
{
  printf ("%swhole = %s\n", padding(indent), whole ? "true" : "false");

  pool_fetch_op ops[2];
  int64 oldest_idx = -1;
  int64 newest_idx = -1;
  int   j;

  OB_CLEAR(ops);
  ops[0].idx = idx0;
  ops[1].idx = idx1;

  for (j = 0; j < 2; j++)
    {
      if (whole)
        {
          ops[j].want_descrips = true;
          ops[j].want_ingests  = true;
          ops[j].rude_offset   = 0;
          ops[j].rude_length   = -1;
        }
      else
        {
          ops[j].want_descrips = false;
          ops[j].want_ingests  = false;
          ops[j].rude_offset   = -1;
          ops[j].rude_length   = 0;
        }
    }

  pool_fetch_ex (ph,
                 ops,
                 2,
                 &oldest_idx,
                 &newest_idx,
                 clamp);

  const char *prefix = padding(indent + 1);

  CHECK_EQ_I64(loIdx, oldest_idx);
  CHECK_EQ_I64(hiIdx, newest_idx);

  check_op(indent + 1, &ops[0], loIdx, hiIdx, idx0, clamp, 0);
  check_op(indent + 1, &ops[1], loIdx, hiIdx, idx1, clamp, 1);
}

static void do_test_5 (int       indent,
                       pool_hose ph,
                       int64     loIdx,
                       int64     hiIdx,
                       int64     idx0,
                       int64     idx1,
                       bool      clamp)
{
  printf ("%sclamp = %s\n", padding(indent), clamp ? "true" : "false");

  do_test_leaf (indent + 1, ph, loIdx, hiIdx, idx0, idx1, clamp, true);
  do_test_leaf (indent + 1, ph, loIdx, hiIdx, idx0, idx1, clamp, false);
}

static void do_test_4 (int       indent,
                       pool_hose ph,
                       int64     loIdx,
                       int64     hiIdx,
                       int64     idx0,
                       int64     idx1)
{
  printf ("%sfetch indices = %" OB_FMT_64 "d,%" OB_FMT_64 "d\n",
          padding(indent),
          idx0,
          idx1);

  do_test_5 (indent + 1, ph, loIdx, hiIdx, idx0, idx1, false);
  do_test_5 (indent + 1, ph, loIdx, hiIdx, idx0, idx1, true);
}

static void do_test_3 (int indent, pool_hose ph, int64 loIdx, int64 hiIdx)
{
  printf ("%spool proteins = %" OB_FMT_64 "d..%" OB_FMT_64 "d\n",
          padding(indent),
          loIdx,
          hiIdx);

  do_test_4 (indent + 1, ph, loIdx, hiIdx, 1, 2);
  do_test_4 (indent + 1, ph, loIdx, hiIdx, 0, OB_INT64_MAX);
}

static void add_proteins (pool_hose ph, int n)
{
  int  i;
  char buf[2];

  for (i = 0; i < n; i++)
    {
      protein p   = NULL;
      int64   idx = -1;

      buf[0] = '0' + i;
      buf[1] = 0;

      OB_DIE_ON_ERROR(pool_cmd_add_test_protein(ph, buf, &p, &idx));
      protein_free (p);
    }
}

static pool_hose init_pool (const char *name,
                            const char *typ,
                            bslaw       opts,
                            bool        chksum)
{
  if (slaw_is_protein(opts))
    opts = protein_ingests(opts);

  slaw opts0 = slaw_map_inline_cf("checksum", slaw_boolean(chksum), NULL);
  slaw opts1 = slaw_maps_merge(opts, opts0, NULL);

  OB_DIE_ON_ERROR(pool_create(name, typ, opts1));

  pool_hose ph = NULL;
  OB_DIE_ON_ERROR(pool_participate(name, &ph, NULL));

  add_proteins(ph, NUM_PROTEINS);

  slaw_free(opts0);
  slaw_free(opts1);

  return ph;
}

static void do_test_2 (int indent, const pool_cmd_info *cmd, bool chksum)
{
  printf ("%schksum = %s\n", padding (indent), chksum ? "true" : "false");

  pool_hose ph = init_pool (cmd->pool_name,
                            cmd->type,
                            cmd->create_options,
                            chksum);

  do_test_3 (indent + 1, ph, 0, NUM_PROTEINS - 1);
  OB_DIE_ON_ERROR(pool_advance_oldest (ph, 1));
  do_test_3 (indent + 1, ph, 1, NUM_PROTEINS - 1);

  OB_DIE_ON_ERROR(pool_withdraw (ph));
  OB_DIE_ON_ERROR(pool_dispose (cmd->pool_name));
}

static void do_test_1 (int indent, const pool_cmd_info *cmd)
{
  do_test_2 (indent, cmd, false);
  do_test_2 (indent, cmd, true);
}

static void usage (void)
{
  fprintf (stderr, "Usage: test-fetch [-t <type>] [-s <size>] "
                   "[-i <toc cap>] <pool_name>\n"
                   "\t<type> defaults to \"mmap\"\n"
                   "\t<size> defaults to %" OB_FMT_64 "u bytes\n",
           (unt64) POOL_MMAP_DEFAULT_SIZE);
  exit (EXIT_FAILURE);
}

static int mainish (int argc, char **argv)
{
  pool_cmd_info cmd;
  int c;

  OB_CLEAR(cmd);

  while ((c = getopt (argc, argv, "i:s:t:v")) != -1)
    {
      switch (c)
        {
          case 'i':
            cmd.toc_capacity = strtoll (optarg, NULL, 0);
            break;
          case 's':
            cmd.size = strtoll (optarg, NULL, 0);
            break;
          case 't':
            cmd.type = optarg;
            break;
          case 'v':
            cmd.verbose = 1;
            break;
          default:
            usage ();
        }
    }

  pool_cmd_setup_options (&cmd);
  if (pool_cmd_get_poolname (&cmd, argc, argv, optind))
    usage ();

  do_test_1 (0, &cmd);

  pool_cmd_free_options (&cmd);

  return exit_code;
}

int main (int argc, char **argv)
{
  return check_for_leaked_file_descriptors_scoped (mainish, argc, argv);
}
