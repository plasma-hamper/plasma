
/* (c)  oblong industries */

// see bug 97

#include "libLoam/c/ob-log.h"
#include "libPlasma/c/pool.h"
#include "libPlasma/c/pool_cmd.h"
#include "libPlasma/c/protein.h"
#include "libPlasma/c/slaw.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void usage (void)
{
  fprintf (stderr, "Usage: deposit-timestamp [-s size] [-t type] "
                   "[-i <toc cap>] <pool_name>\n");
  exit (EXIT_FAILURE);
}

int mainish (int argc, char *argv[])
{
  pool_hose ph;
  ob_retort pret;
  pool_cmd_info cmd;
  int retcode = EXIT_SUCCESS;
  int c;
  protein ret_prot = NULL;
  pool_timestamp ret_ts, dep_ts;
  int64 ret_idx, dep_idx;

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
          // Would be nice to have a verbose option
          default:
            usage ();
        }
    }

  pool_cmd_setup_options (&cmd);
  if (pool_cmd_get_poolname (&cmd, argc, argv, optind))
    usage ();

  pret = pool_create (cmd.pool_name, cmd.type, cmd.create_options);
  if (pret != OB_OK)
    {
      OB_FATAL_ERROR_CODE (0x20402000,
                           "no can create %s (%" OB_FMT_64 "u): %s\n",
                           cmd.pool_name, cmd.size, ob_error_string (pret));
    }

  pool_cmd_open_pool (&cmd);
  ph = cmd.ph;

  protein p = pool_cmd_create_test_protein ("sonic screwdriver");
  pret = pool_deposit_ex (ph, p, &dep_idx, &dep_ts);
  if (pret != OB_OK)
    {
      OB_LOG_ERROR_CODE (0x20402001, "pool_deposit_ex: %s\n",
                         ob_error_string (pret));
      retcode = EXIT_FAILURE;
      goto cleanup;
    }

  pret = pool_rewind (ph);

  if (pret != OB_OK)
    {
      OB_LOG_ERROR_CODE (0x20402002,
                         "pool_rewind returned %s, "
                         "but expected OB_OK\n",
                         ob_error_string (pret));
      retcode = EXIT_FAILURE;
      goto cleanup;
    }

  pret = pool_next (ph, &ret_prot, &ret_ts, &ret_idx);

  if (pret != OB_OK)
    {
      OB_LOG_ERROR_CODE (0x20402003,
                         "pool_next returned %s, "
                         "but expected OB_OK\n",
                         ob_error_string (pret));
      retcode = EXIT_FAILURE;
      goto cleanup;
    }

  if (ret_idx != dep_idx)
    {
      OB_LOG_ERROR_CODE (0x20402004,
                         "dep_idx was %" OB_FMT_64
                         "d, but ret_idx was %" OB_FMT_64 "d\n",
                         dep_idx, ret_idx);
      retcode = EXIT_FAILURE;
    }

  if (ret_ts != dep_ts)
    {
      OB_LOG_ERROR_CODE (0x20402005, "dep_ts was %f, but ret_ts was %f\n",
                         dep_ts, ret_ts);
      retcode = EXIT_FAILURE;
    }

cleanup:
  pret = pool_withdraw (ph);
  if (pret != OB_OK)
    {
      OB_LOG_ERROR_CODE (0x20402006, "pool_withdraw failed with '%s'\n",
                         ob_error_string (pret));
      retcode = EXIT_FAILURE;
    }

  pret = pool_dispose (cmd.pool_name);
  if (pret != OB_OK)
    {
      OB_FATAL_ERROR_CODE (0x20402007, "no can stop %s: %s\n", cmd.pool_name,
                           ob_error_string (pret));
    }

  pool_cmd_free_options (&cmd);
  protein_free (p);
  protein_free (ret_prot);

  return retcode;
}

int main (int argc, char **argv)
{
  return check_for_leaked_file_descriptors_scoped (mainish, argc, argv);
}
