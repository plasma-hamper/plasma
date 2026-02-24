
/* (c)  oblong industries */

// Tests a mixture of multi-await, single-await, next, and other operations.
// Not to be confused with multi_test.c, which is not part of the test suite.

#include "pool_cmd.h"
#include "libLoam/c/ob-rand.h"
#include "libLoam/c/ob-sys.h"
#include "libLoam/c/ob-pthread.h"
#include "libLoam/c/ob-util.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-time.h"
#include "libLoam/c/ob-file.h"
#include "libLoam/c/ob-vers.h"
#include "libPlasma/c/protein.h"
#include "libPlasma/c/slaw.h"

typedef enum
{
  TEST_MULTI,
  TEST_SINGLE,
  TEST_NEXT,
  TEST_NTH,
  TEST_NEWEST,
  TEST_OLDEST,
  TEST_649,
  TEST_MAX
} test_op;

static int socks[2];  // two ends of a socketpair

static void *thread_main (void *arg)
{
  char **pnames = (char **) arg;
  int nhoses, i;
  ob_rand_t *rstate = ob_rand_allocate_state (OB_RAND_COMPLETELY_RANDOM_PLEASE);

  for (nhoses = 0; pnames[nhoses] != NULL; nhoses++)
    ;  // empty loop (to count hoses)

  pool_hose *hoses = (pool_hose *) calloc (nhoses, sizeof (pool_hose));

  for (i = 0; i < nhoses; i++)
    OB_DIE_ON_ERROR (pool_participate (pnames[i], hoses + i, NULL));

  for (;;)
    {
      int hoseno;
      if (sizeof (hoseno) != read (socks[0], &hoseno, sizeof (hoseno)))
        OB_FATAL_ERROR_CODE (0x20412000, "read failed: %s\n", strerror (errno));

      if (hoseno < 0)
        break;  // -1 means terminate thread

      if (hoseno >= nhoses)
        OB_FATAL_ERROR_CODE (0x20412001, "hoseno was %d but nhoses was %d\n",
                             hoseno, nhoses);

      // random sleep between 0 and 10 milliseconds (to let awaiter wait)
      OB_DIE_ON_ERROR (ob_micro_sleep (ob_rand_state_int32 (0, 10000, rstate)));

      protein p =
        protein_from_ff (slaw_list_inline_c ("what", "ever", NULL),
                         slaw_map_inline_cf ("nothing", slaw_nil (), NULL));
      OB_DIE_ON_ERROR (pool_deposit (hoses[hoseno], p, NULL));
      protein_free (p);
    }

  for (i = 0; i < nhoses; i++)
    {
      OB_DIE_ON_ERROR (pool_withdraw (hoses[i]));
      OB_DIE_ON_ERROR (pool_dispose (pnames[i]));
    }

  free (hoses);
  ob_rand_free_state (rstate);
  return NULL;
}

static void stimulate (int hoseno)
{
  if (sizeof (hoseno) != write (socks[1], &hoseno, sizeof (hoseno)))
    OB_FATAL_ERROR_CODE (0x20412002, "didn't write: %s\n", strerror (errno));
}

static void do_op (test_op op, pool_hose ph, int hoseno, pool_gang gang,
                   ob_rand_t *rstate)
{
  pool_hose ret_ph = NULL;
  protein ret_prot = NULL;
  protein p = NULL;
  int64 ret_index = -1;
  int64 dep_index = -1;
  ob_retort tort, expected;
  int64 current_index, idx, new_index;
  const pool_timestamp timeout = 60 * 10;  // 10 minute timeout

  tort = pool_index (ph, &current_index);
  OB_DIE_ON_ERROR (tort);

  switch (op)
    {
      case TEST_MULTI:
        stimulate (hoseno);
        tort = pool_await_next_multi (gang, timeout, &ret_ph, &ret_prot, NULL,
                                      &ret_index);
        OB_DIE_ON_ERROR (tort);
        if (ret_ph != ph)
          error_exit ("unexpected hose: got '%s' but expected '%s'\n",
                      pool_name (ret_ph), pool_name (ph));
        if (ret_index != current_index)
          OB_FATAL_ERROR_CODE (0x20412004,
                               "%s: index was not what I expected: %" OB_FMT_64
                               "d != %" OB_FMT_64 "d\n",
                               pool_name (ph), ret_index, current_index);
        OB_DIE_ON_ERROR (pool_index (ph, &new_index));
        if (new_index != current_index + 1)
          OB_FATAL_ERROR_CODE (0x20412017,
                               "%s: new index was not what I expected: "
                               "%" OB_FMT_64 "d != %" OB_FMT_64 "d + 1\n",
                               pool_name (ph), new_index, current_index);
        protein_free (ret_prot);
        break;
      case TEST_SINGLE:
        stimulate (hoseno);
        tort = pool_await_next (ph, timeout, &ret_prot, NULL, &ret_index);
        OB_DIE_ON_ERROR (tort);
        if (ret_index != current_index)
          OB_FATAL_ERROR_CODE (0x20412005, "index was not what I expected\n");
        OB_DIE_ON_ERROR (pool_index (ph, &new_index));
        if (new_index != current_index + 1)
          OB_FATAL_ERROR_CODE (0x20412018,
                               "%s: new index was not what I expected: "
                               "%" OB_FMT_64 "d != %" OB_FMT_64 "d + 1\n",
                               pool_name (ph), new_index, current_index);
        protein_free (ret_prot);
        break;
      case TEST_NEXT:
        expected = POOL_NO_SUCH_PROTEIN;
        if (ob_rand_state_int32 (0, 2, rstate))
          {
            p = protein_from_ff (
              slaw_list_inline_c ("what", "ever", NULL),
              slaw_map_inline_cf (
                "random",
                slaw_float64 (ob_rand_state_float64 (0.0, 1.0, rstate)), NULL));
            tort = pool_deposit (ph, p, &dep_index);
            OB_DIE_ON_ERROR (tort);
            expected = OB_OK;
          }
        tort = pool_next (ph, &ret_prot, NULL, &ret_index);
        if (tort != expected)
          {
            if (OB_OK == tort)
              slaw_spew_overview_to_stderr (ret_prot), fputc ('\n', stderr);
            error_exit ("Expected %s but got %s at %" OB_FMT_64 "d\n",
                        ob_error_string (expected), ob_error_string (tort),
                        ret_index);
          }
        if (p && !slawx_equal (p, ret_prot))
          {
            slaw str1 = slaw_spew_overview_to_string (p);
            slaw str2 = slaw_spew_overview_to_string (ret_prot);
            error_exit ("proteins differ between %" OB_FMT_64
                        "d and %" OB_FMT_64 "d:\n%s\n%s\n",
                        dep_index, ret_index, slaw_string_emit (str1),
                        slaw_string_emit (str2));
          }
        if (p)
          {
            protein_free (ret_prot);
            protein_free (p);
            if (ret_index != dep_index)
              error_exit ("index mismatch: %" OB_FMT_64 "d, %" OB_FMT_64 "d\n",
                          ret_index, dep_index);
          }
        break;
      case TEST_NTH:
        idx = ob_rand_state_int32 (1, (int) (current_index * 1.5), rstate);
        expected = (idx < current_index) ? OB_OK : POOL_NO_SUCH_PROTEIN;
        tort = pool_nth_protein (ph, idx, &ret_prot, NULL);
        if (tort != expected)
          OB_FATAL_ERROR_CODE (0x20412009, "Expected %s but got %s\n",
                               ob_error_string (expected),
                               ob_error_string (tort));
        // XXX: would we like to check the contents of the protein?
        protein_free (ret_prot);
        break;
      case TEST_NEWEST:
        expected = (current_index == 0) ? POOL_NO_SUCH_PROTEIN : OB_OK;
        tort = pool_newest_index (ph, &ret_index);
        if (tort != expected)
          OB_FATAL_ERROR_CODE (0x2041200a, "Expected %s but got %s\n",
                               ob_error_string (expected),
                               ob_error_string (tort));
        if (current_index - 1 != ret_index)
          OB_FATAL_ERROR_CODE (
            0x2041200b, "Expected %" OB_FMT_64 "d but got %" OB_FMT_64 "d\n",
            current_index - 1, ret_index);
        break;
      case TEST_OLDEST:
        expected = (current_index == 0) ? POOL_NO_SUCH_PROTEIN : OB_OK;
        tort = pool_oldest_index (ph, &ret_index);
        if (tort != expected)
          OB_FATAL_ERROR_CODE (0x2041200c, "Expected %s but got %s\n",
                               ob_error_string (expected),
                               ob_error_string (tort));
        if (current_index <= ret_index)
          error_exit ("well that shouldn't happen: %" OB_FMT_64 "d, %" OB_FMT_64
                      "d\n",
                      current_index, ret_index);
        break;
      case TEST_649:
        stimulate (hoseno);
again:
        OB_DIE_ON_ERROR (pool_await_multi (gang, timeout));
        OB_DIE_ON_ERROR (pool_index (ph, &new_index));
        if (new_index != current_index)
          OB_FATAL_ERROR_CODE (0x20412019,
                               "%s: new index was not what I expected: "
                               "%" OB_FMT_64 "d != %" OB_FMT_64 "d\n",
                               pool_name (ph), new_index, current_index);
        switch (ob_rand_state_int32 (0, 3, rstate))
          {
            case 0:
              goto again;
            case 1:
              OB_DIE_ON_ERROR (pool_frwdby (ph, 1));
              break;
            default:
              OB_DIE_ON_ERROR (pool_next (ph, &ret_prot, NULL, &ret_index));
              if (ret_index != current_index)
                OB_FATAL_ERROR_CODE (0x2041201a,
                                     "%s: returned index was not "
                                     "what I expected: %" OB_FMT_64
                                     "d != %" OB_FMT_64 "d\n",
                                     pool_name (ph), new_index, current_index);
              protein_free (ret_prot);
              break;
          }
        break;
      default:
        OB_FATAL_ERROR_CODE (0x2041200e, "umm... no\n");
    }
}

static void usage (void)
{
  fprintf (stderr, "Usage: test-multi-await [-t <type>] [-s <size>] "
                   "[-i <toc cap>] <poolname> [<poolname> ...]\n");
  exit (EXIT_FAILURE);
}

static int mainish (int argc, char **argv)
{
  pool_cmd_info cmd;
  int c;

  memset (&cmd, 0, sizeof (cmd));
  while ((c = getopt (argc, argv, "s:t:i:")) != -1)
    {
      switch (c)
        {
          case 's':
            cmd.size = strtoll (optarg, NULL, 0);
            break;
          case 't':
            cmd.type = optarg;
            break;
          case 'i':
            cmd.toc_capacity = strtoll (optarg, NULL, 0);
            break;
          default:
            usage ();
        }
    }

  pool_cmd_setup_options (&cmd);
  int first_pool_arg = optind;

  // Must have at least one pool
  if (first_pool_arg == argc)
    usage ();

  pool_gang gang;
  ob_retort pret = pool_new_gang (&gang);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x2041200f, "New gang creation failed: %s\n",
                         ob_error_string (pret));

  const int num_pools = argc - first_pool_arg;
  pool_hose *hoses = (pool_hose *) calloc (num_pools, sizeof (pool_hose));

  // Iterate through the pools listed and add them to our await gang
  int i;
  for (i = first_pool_arg; i < argc; i++)
    {
      pool_hose new_ph;
      const char *poolName = argv[i];
      // ob_log (OBLV_DBUG, 0x20412010, "adding pool %s\n", poolName);
      pret = pool_participate_creatingly (poolName, cmd.type, &new_ph,
                                          cmd.create_options);
      if ((pret != OB_OK) && (pret != POOL_CREATED))
        {
          pool_disband_gang (gang, true);
          OB_FATAL_ERROR_CODE (0x20412011,
                               "Participate_creatingly in pool %s failed: %s\n",
                               poolName, ob_error_string (pret));
        }
      pret = pool_join_gang (gang, new_ph);
      if (pret != OB_OK)
        {
          pool_disband_gang (gang, true);
          OB_FATAL_ERROR_CODE (0x20412012, "Pool %s failed to join gang: %s\n",
                               poolName, ob_error_string (pret));
        }
      hoses[i - first_pool_arg] = new_ph;
    }

  if ((pret = ob_pipe (socks)) < OB_OK)
    OB_FATAL_ERROR_CODE (0x20412013, "ob_pipe: %s\n", ob_error_string (pret));

  pthread_t thr;
  int erryes = pthread_create (&thr, NULL, thread_main, argv + first_pool_arg);
  if (erryes)
    OB_FATAL_ERROR_CODE (0x20412014, "pthread_create: %s\n", strerror (erryes));

  int hose1, hose2;
  test_op op1, op2;

  ob_rand_t *rstate = ob_rand_allocate_state (OB_RAND_COMPLETELY_RANDOM_PLEASE);

  for (hose1 = 0; hose1 < num_pools; hose1++)
    for (hose2 = 0; hose2 < num_pools; hose2++)
      for (op1 = TEST_MULTI; op1 < TEST_MAX; op1 = (test_op) (1 + (int) op1))
        for (op2 = TEST_MULTI; op2 < TEST_MAX; op2 = (test_op) (1 + (int) op2))
          {
            do_op (op1, hoses[hose1], hose1, gang, rstate);
            do_op (op2, hoses[hose2], hose2, gang, rstate);
          }

  free (hoses);
  OB_DIE_ON_ERROR (pool_disband_gang (gang, true));
  pool_cmd_free_options (&cmd);

  stimulate (-1);  // cause thread to exit
  erryes = pthread_join (thr, NULL);
  if (erryes)
    OB_FATAL_ERROR_CODE (0x20412015, "pthread_join: %s\n", strerror (erryes));

  if (0 != close (socks[0]) || 0 != close (socks[1]))
    OB_FATAL_ERROR_CODE (0x20412016, "close: %s\n", strerror (errno));

  ob_rand_free_state (rstate);

  return EXIT_SUCCESS;
}

int main (int argc, char **argv)
{
  OB_DIE_ON_ERROR (OB_CHECK_ABI ());

  return check_for_leaked_file_descriptors_scoped (mainish, argc, argv);
}
