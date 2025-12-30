
/* (c)  oblong industries */

#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-rand.h"
#include "libLoam/c/ob-time.h"
#include "libLoam/c/ob-vers.h"
#include "libPlasma/c/pool.h"
#include "libPlasma/c/pool_cmd.h"
#include "libPlasma/c/slaw-path.h"
#include "libPlasma/c/protein.h"
#include "libPlasma/c/slaw.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libLoam/c/ob-pthread.h>

static void usage (void)
{
  fprintf (stderr, "Usage: random-access-test [-s size] [-i <toc cap>] "
                   "[-t type] [-S] <pool_name>\n");
  fprintf (stderr, "        -S print statistics\n");
  exit (EXIT_FAILURE);
}

static pool_hose hose;
static volatile bool done = false;
static volatile int64 proteins_written = 0;

static void *thread_main (void *ignored)
{
  int64 oldest, idx;
  int64 i = 0;
  do
    {
      slaw ingests = slaw_map_inline_cf ("bad wolf", slaw_int64 (i), NULL);
      protein p = protein_from_ff (NULL, ingests);
      OB_DIE_ON_ERROR (pool_deposit (hose, p, &idx));
      if (idx != i)
        OB_FATAL_ERROR_CODE (
          0x2040c000, "idx = %" OB_FMT_64 "d, i = %" OB_FMT_64 "d\n", idx, i);
      OB_DIE_ON_ERROR (pool_oldest_index (hose, &oldest));
      if (oldest > i)
        OB_FATAL_ERROR_CODE (0x2040c001,
                             "oldest = %" OB_FMT_64 "d, i = %" OB_FMT_64 "d\n",
                             oldest, i);
      proteins_written = ++i;
      protein_free (p);
    }
  while (oldest < 3000);  // wait until we've wrapped around for a while

  OB_DIE_ON_ERROR (pool_withdraw (hose));

  done = true;
  return NULL;
}

int mainish (int argc, char *argv[])
{
  pool_hose ph;
  ob_retort pret;
  pool_cmd_info cmd;
  int retcode = EXIT_SUCCESS;
  int c;
  bool statistics = false;
  int64 proteins_read = 0;
  int64 proteins_attempted = 0;
  bool valgrind = false;

  memset (&cmd, 0, sizeof (cmd));
  while ((c = getopt (argc, argv, "i:s:t:S")) != -1)
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
          case 'S':
            statistics = true;
            break;
          default:
            usage ();
        }
    }

  pool_cmd_setup_options (&cmd);
  if (pool_cmd_get_poolname (&cmd, argc, argv, optind))
    usage ();

  // See bug 739 comment 11.  I have problems with unfair scheduling
  // both on single-core Windows and under valgrind.
  // Although really I shouldn't have written a test
  // where one thread can starve the other in the first place.
  if (ob_get_system_info (OB_SYSINFO_NUM_CORES) < 2
      || ob_running_under_valgrind ())
    valgrind = true;

  pret = pool_create (cmd.pool_name, cmd.type, cmd.create_options);
  if (pret != OB_OK)
    {
      OB_FATAL_ERROR_CODE (0x2040c002,
                           "no can create %s (%" OB_FMT_64 "u): %s\n",
                           cmd.pool_name, cmd.size, ob_error_string (pret));
    }

  pool_cmd_open_pool (&cmd);
  ph = cmd.ph;

  OB_DIE_ON_ERROR (pool_hose_clone (ph, &hose));

  float64 begin = ob_current_time ();

  pthread_t th;
  if (pthread_create (&th, NULL, thread_main, NULL) != 0)
    {
      OB_LOG_ERROR_CODE (0x2040c003, "pthread_create failed\n");
      retcode = EXIT_FAILURE;
      goto cleanup;
    }

  while (!done)
    {
      int64 newest;
      ob_retort tort;
      do
        {
          tort = pool_newest_index (ph, &newest);
          if (valgrind)
            ob_micro_sleep (50);
        }
      while (tort == POOL_NO_SUCH_PROTEIN);
      if (tort < OB_OK)
        {
          OB_LOG_ERROR_CODE (0x2040c004, "pool_newest_index: %s\n",
                             ob_error_string (tort));
          retcode = EXIT_FAILURE;
          goto thread_cleanup;
        }
      int64 idx = ob_rand_int32 (0, 1 + (int32) newest);
      protein p = NULL;
      tort = pool_nth_protein (ph, idx, &p, NULL);
      if (tort == POOL_NO_SUCH_PROTEIN)
        {
          int64 oldest;
          pret = pool_oldest_index (ph, &oldest);
          if (pret < OB_OK)
            {
              OB_LOG_ERROR_CODE (0x2040c005, "pool_oldest_index: %s\n",
                                 ob_error_string (pret));
              retcode = EXIT_FAILURE;
              goto thread_cleanup;
            }
          if (oldest <= idx)
            {
              OB_LOG_ERROR_CODE (
                0x2040c006, "oldest = %" OB_FMT_64 "d, idx = %" OB_FMT_64 "d\n",
                oldest, idx);
              retcode = EXIT_FAILURE;
              goto thread_cleanup;
            }
        }
      else if (tort < OB_OK)
        {
          OB_LOG_ERROR_CODE (0x2040c007, "pool_nth_protein: %s\n",
                             ob_error_string (tort));
          retcode = EXIT_FAILURE;
          goto thread_cleanup;
        }
      else
        {
          int64 contents = slaw_path_get_int64 (p, "bad wolf", -456);
          if (contents != idx)
            {
              OB_LOG_ERROR_CODE (0x2040c008,
                                 "contents = %" OB_FMT_64 "d, idx = %" OB_FMT_64
                                 "d\n",
                                 contents, idx);
              retcode = EXIT_FAILURE;
              goto thread_cleanup;
            }
          proteins_read++;
          protein_free (p);
        }

      // When running under valgrind, somehow the deposit thread
      // (which determines when we are done) seems to get starved.
      // So this is a hack to try to let it run occasionally.
      if (valgrind && (proteins_attempted++ % 200) == 0)
        ob_micro_sleep (50);
    }

  if (statistics)
    {
      float64 delta = ob_current_time () - begin;
      printf ("elapsed: %f seconds; wrote %" OB_FMT_64
              "d proteins; read %" OB_FMT_64 "d proteins\n",
              delta, proteins_written, proteins_read);
      printf ("writes per second: %f\n", proteins_written / delta);
      printf ("reads per second:  %f\n", proteins_read / delta);
    }

thread_cleanup:
  if (pthread_join (th, NULL) != 0)
    {
      OB_LOG_ERROR_CODE (0x2040c009, "pthread_join failed\n");
      retcode = EXIT_FAILURE;
      goto cleanup;
    }

cleanup:
  pret = pool_withdraw (ph);
  if (pret != OB_OK)
    {
      OB_LOG_ERROR_CODE (0x2040c00a, "pool_withdraw failed with '%s'\n",
                         ob_error_string (pret));
      retcode = EXIT_FAILURE;
    }

  pret = pool_dispose (cmd.pool_name);
  if (pret != OB_OK)
    {
      OB_FATAL_ERROR_CODE (0x2040c00b, "no can stop %s: %s\n", cmd.pool_name,
                           ob_error_string (pret));
    }

  pool_cmd_free_options (&cmd);

  return retcode;
}

int main (int argc, char **argv)
{
  OB_DIE_ON_ERROR (OB_CHECK_ABI ());

  return check_for_leaked_file_descriptors_scoped (mainish, argc, argv);
}
