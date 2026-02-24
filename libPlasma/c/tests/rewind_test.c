
/* (c)  oblong industries */

// test bug 22, among other things

#include "libLoam/c/ob-dirs.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-vers.h"
#include "libPlasma/c/pool.h"
#include "libPlasma/c/pool_cmd.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void usage (void)
{
  fprintf (stderr, "Usage: rewind_test [-i <toc cap>] [-s size] [-t type] "
                   "<pool_name>\n");
  exit (EXIT_FAILURE);
}

int mainish (int argc, char *argv[])
{
  pool_hose ph;
  ob_retort pret;
  pool_cmd_info cmd;
  int retcode = EXIT_SUCCESS;
  int c;
  const char *actual;
  const char *expected;

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
      OB_FATAL_ERROR_CODE (0x2040d000,
                           "no can create %s (%" OB_FMT_64 "u): %s\n",
                           cmd.pool_name, cmd.size, ob_error_string (pret));
    }

  pool_cmd_open_pool (&cmd);
  ph = cmd.ph;

  // bug 22: check that rewinding an empty pool returns OB_OK

  pret = pool_rewind (ph);

  if (pret != OB_OK)
    {
      OB_LOG_ERROR_CODE (0x2040d001,
                         "pool_rewind on empty pool returned %s, "
                         "but expected OB_OK\n",
                         ob_error_string (pret));
      retcode = EXIT_FAILURE;
      goto cleanup;
    }

  // not related to rewind, but as long as we have this nice pool
  // hose here, let's test the hose name stuff...

  // check pool name is correct
  if (strcmp ((expected = cmd.pool_name), (actual = pool_name (ph))) != 0)
    {
      OB_LOG_ERROR_CODE (0x2040d002,
                         "pool_name returned '%s' but expected '%s'\n", actual,
                         expected);
      retcode = EXIT_FAILURE;
    }

  expected = ob_basename (expected);

  // hose name should default to pool name (but without the protocol and host)
  if (strcmp (expected, (actual = pool_get_hose_name (ph))) != 0)
    {
      OB_LOG_ERROR_CODE (0x2040d003,
                         "pool_get_hose_name returned '%s' but expected '%s'\n",
                         actual, expected);
      retcode = EXIT_FAILURE;
    }

  // change hose name
  expected = "Gallifrey";
  pret = pool_set_hose_name (ph, expected);
  if (pret < OB_OK)
    {
      OB_LOG_ERROR_CODE (0x2040d004, "pool_set_hose_name returned %s\n",
                         ob_error_string (pret));
      retcode = EXIT_FAILURE;
      goto cleanup;
    }

  // hose name should be changed
  if (strcmp (expected, (actual = pool_get_hose_name (ph))) != 0)
    {
      OB_LOG_ERROR_CODE (0x2040d005,
                         "pool_get_hose_name returned '%s' but expected '%s'\n",
                         actual, expected);
      retcode = EXIT_FAILURE;
    }

  // but pool name should be unchanged
  if (strcmp ((expected = cmd.pool_name), (actual = pool_name (ph))) != 0)
    {
      OB_LOG_ERROR_CODE (0x2040d006,
                         "pool_name returned '%s' but expected '%s'\n", actual,
                         expected);
      retcode = EXIT_FAILURE;
    }

cleanup:
  pret = pool_withdraw (ph);
  if (pret != OB_OK)
    {
      OB_LOG_ERROR_CODE (0x2040d007, "pool_withdraw failed with '%s'\n",
                         ob_error_string (pret));
      retcode = EXIT_FAILURE;
    }

  pret = pool_dispose (cmd.pool_name);
  if (pret != OB_OK)
    {
      OB_FATAL_ERROR_CODE (0x2040d008, "no can stop %s: %s\n", cmd.pool_name,
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
