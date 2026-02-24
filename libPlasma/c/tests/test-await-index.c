
/* (c)  oblong industries */

#include "libPlasma/c/pool_cmd.h"
#include "libPlasma/c/protein.h"
#include "libPlasma/c/slaw.h"
#include "libPlasma/c/slaw-path.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-vers.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libLoam/c/ob-pthread.h>

#define ITERATIONS 1000

static void usage (void)
{
  fprintf (stderr,
           "Usage: test-await-index [-t <type>] [-s <size>] [-i <toc cap>]"
           "<pool_name>\n");
  exit (EXIT_FAILURE);
}

static void *send_some_proteins (void *v)
{
  const char *poolName = (const char *) v;
  pool_hose ph;

  ob_retort err = pool_participate (poolName, &ph, NULL);
  if (err != OB_OK)
    OB_FATAL_ERROR_CODE (0x2040f000, "no can participate %s: %s\n", poolName,
                         ob_error_string (err));

  int i;
  int64 prev = -1;
  for (i = 0; i < ITERATIONS; i++)
    {
      protein p = protein_from_ff (
        slaw_list_inline_c ("inaugural", "balls", NULL),
        slaw_map_inline_ff (slaw_string ("count"), slaw_int64 (i), NULL));
      int64 idx;
      err = pool_deposit (ph, p, &idx);
      if (err != OB_OK)
        OB_FATAL_ERROR_CODE (0x2040f001, "pool_deposit returned %s\n",
                             ob_error_string (err));
      protein_free (p);
      if (prev != -1)
        {
          if (idx != prev + 1)
            OB_FATAL_ERROR_CODE (0x2040f002,
                                 "previous index was %" OB_FMT_64
                                 "d and current "
                                 "index is %" OB_FMT_64 "d\n",
                                 prev, idx);
        }
      prev = idx;
    }

  err = pool_withdraw (ph);
  if (err != OB_OK)
    OB_FATAL_ERROR_CODE (0x2040f003, "couldn't withdraw: %" OB_FMT_64 "d\n",
                         err);

  return NULL;
}

static int mainish (int argc, char **argv)
{
  pool_cmd_info cmd;
  int c;

  memset (&cmd, 0, sizeof (cmd));
  while ((c = getopt (argc, argv, "i:s:t:")) != -1)
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
          default:
            usage ();
        }
    }
  pool_cmd_setup_options (&cmd);
  if (pool_cmd_get_poolname (&cmd, argc, argv, optind))
    usage ();

  ob_retort pret = pool_participate_creatingly (cmd.pool_name, cmd.type,
                                                &cmd.ph, cmd.create_options);
  if (pret < 0 && pret != POOL_EXISTS)  // POOL_EXISTS in previous versions
    OB_FATAL_ERROR_CODE (0x2040f004,
                         "no can participate_creatingly %s (%" OB_FMT_64 "u"
                         "): %s\n",
                         cmd.pool_name, cmd.size, ob_error_string (pret));

  pthread_t thr;

  if (pthread_create (&thr, NULL, send_some_proteins, (void *) cmd.pool_name)
      != 0)
    {
      perror ("pthread_create");
      return EXIT_FAILURE;
    }

  int i;
  int64 prev = -1;
  for (i = 0; i < ITERATIONS; i++)
    {
      protein p;
      pool_timestamp ts;
      int64 idx, got;
      pret = pool_await_next (cmd.ph, 60, &p, &ts, &idx);
      if (pret != OB_OK)
        OB_FATAL_ERROR_CODE (0x2040f005, "problem with pool_await_next(): %s\n",
                             ob_error_string (pret));
      if (i != (got = slaw_path_get_int64 (p, "count", -1)))
        OB_FATAL_ERROR_CODE (0x2040f006,
                             "expected %d but got %" OB_FMT_64 "d\n", i, got);
      protein_free (p);
      if (prev != -1)
        {
          if (idx != prev + 1)
            OB_FATAL_ERROR_CODE (0x2040f007,
                                 "previous index was %" OB_FMT_64
                                 "d and current "
                                 "index is %" OB_FMT_64 "d\n",
                                 prev, idx);
        }
      prev = idx;
    }

  if (pthread_join (thr, NULL) != 0)
    {
      perror ("pthread_join");
      return EXIT_FAILURE;
    }

  pret = pool_withdraw (cmd.ph);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x2040f008, "couldn't withdraw: %" OB_FMT_64 "d\n",
                         pret);

  pret = pool_dispose (cmd.pool_name);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x2040f009, "no can stop %s: %s\n", cmd.pool_name,
                         ob_error_string (pret));

  pool_cmd_free_options (&cmd);

  return EXIT_SUCCESS;
}

int main (int argc, char **argv)
{
  OB_DIE_ON_ERROR (OB_CHECK_ABI ());

  return check_for_leaked_file_descriptors_scoped (mainish, argc, argv);
}
