
/* (c)  oblong industries */

//
// Test that null arguments to pool_next/prev/etc. are okay
//

#include "libLoam/c/ob-sys.h"
#include "slaw.h"
#include "pool_cmd.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-vers.h"
#include "libPlasma/c/protein.h"
#include "libPlasma/c/slaw.h"

#define NUM_PROTEINS 20

static void usage (void)
{
  fprintf (stderr, "Usage: null_test [-t <type>] [-s <size>] [-i <toc cap>] "
                   "<pool_name>\n");
  exit (1);
}

int mainish (int argc, char *argv[])
{
  pool_hose ph;
  protein proteins[NUM_PROTEINS];
  protein curr_prot;
  ob_retort pret;
  char protein_name[100];
  int i;
  pool_cmd_info cmd;
  pool_timestamp ts;
  int64 idx;
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
      fprintf (stderr, "no can create %s (%" OB_FMT_64 "u): %s\n",
               cmd.pool_name, cmd.size, ob_error_string (pret));
      exit (1);
    }

  pool_cmd_open_pool (&cmd);
  ph = cmd.ph;

  for (i = 0; i < NUM_PROTEINS; i++)
    {
      snprintf (protein_name, sizeof (protein_name), "%d", i);
      pret = pool_cmd_add_test_protein (ph, protein_name, &proteins[i], NULL);
      if (pret != OB_OK)
        OB_FATAL_ERROR_CODE (0x20408000, "deposit protein failed: %s\n",
                             ob_error_string (pret));
    }

  // Check that null timestamp and index is okay - crashes if not

  pret = pool_next (ph, &curr_prot, NULL, NULL);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x20408001,
                         "pool_next with null timestamp and index "
                         "addr failed: %s\n",
                         ob_error_string (pret));
  Free_Protein (curr_prot);

  pret = pool_prev (ph, &curr_prot, NULL, NULL);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x20408002,
                         "pool_prev with null timestamp and index "
                         "addr failed: %s\n",
                         ob_error_string (pret));
  Free_Protein (curr_prot);

  pret = pool_await_next (ph, 0, &curr_prot, NULL, NULL);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x20408003,
                         "pool_next with null timestamp and index "
                         "addr failed: %s\n",
                         ob_error_string (pret));
  Free_Protein (curr_prot);

  // Check non-null timestamp and indexb

  OB_DIE_ON_ERROR (pool_rewind (ph));

  pret = pool_next (ph, &curr_prot, &ts, &idx);
  if ((pret != OB_OK) || (ts == 0) || idx != 0)
    OB_FATAL_ERROR_CODE (0x20408004,
                         "pool_next with non-null timestamp and "
                         "index addr failed: %s\n",
                         ob_error_string (pret));
  Free_Protein (curr_prot);

  pret = pool_prev (ph, &curr_prot, &ts, &idx);
  if ((pret != OB_OK) || (ts == 0) || idx != 0)
    OB_FATAL_ERROR_CODE (0x20408005,
                         "pool_prev with null timestamp and index "
                         "addr failed: %s\n",
                         ob_error_string (pret));
  Free_Protein (curr_prot);

  pret = pool_await_next (ph, 0, &curr_prot, &ts, &idx);
  if ((pret != OB_OK) || (ts == 0) || idx != 0)
    OB_FATAL_ERROR_CODE (0x20408006,
                         "pool_next with null timestamp and index "
                         "addr failed: %s\n",
                         ob_error_string (pret));
  Free_Protein (curr_prot);

  OB_DIE_ON_ERROR (pool_withdraw (ph));

  pret = pool_dispose (cmd.pool_name);
  if (pret != OB_OK)
    {
      fprintf (stderr, "no can stop %s: %s\n", cmd.pool_name,
               ob_error_string (pret));
      return 1;
    }

  for (i = 0; i < NUM_PROTEINS; i++)
    Free_Protein (proteins[i]);

  pool_cmd_free_options (&cmd);

  return 0;
}

int main (int argc, char **argv)
{
  OB_DIE_ON_ERROR (OB_CHECK_ABI ());

  return check_for_leaked_file_descriptors_scoped (mainish, argc, argv);
}
