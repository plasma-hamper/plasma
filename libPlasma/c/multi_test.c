
/* (c)  oblong industries */

///
/// Wait for a protein to be deposited in any of several pools.  The
/// deposits come from some external program.
///
#include "pool_cmd.h"
#include "libLoam/c/ob-vers.h"
#include "libLoam/c/ob-log.h"
#include "libPlasma/c/protein.h"
#include "libPlasma/c/slaw.h"

static void usage (void)
{
  ob_banner (stderr);
  fprintf (stderr, "Usage: multi_test [-t <type>] [-s <size>] <poolname> "
                   "[<poolname> ...]\n");
  exit (EXIT_FAILURE);
}

int main (int argc, char *argv[])
{
  OB_CHECK_ABI ();

  pool_cmd_info cmd;
  ob_retort pret;
  int c;

  memset (&cmd, 0, sizeof (cmd));
  while ((c = getopt (argc, argv, "s:t:")) != -1)
    {
      switch (c)
        {
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
  int first_pool_arg = optind;

  // Must have at least one pool
  if (first_pool_arg == argc)
    usage ();

  pool_gang gang;
  pret = pool_new_gang (&gang);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x20202000, "New gang creation failed: %s\n",
                         ob_error_string (pret));

  // Iterate through the pools listed and add them to our await gang
  int i;
  for (i = first_pool_arg; i < argc; i++)
    {
      pool_hose new_ph;
      const char *poolName = argv[i];
      ob_log (OBLV_DBUG, 0x20202001, "adding pool %s\n", poolName);
      pret = pool_participate_creatingly (poolName, cmd.type, &new_ph,
                                          cmd.create_options);
      if ((pret != OB_OK) && (pret != POOL_CREATED))
        {
          pool_disband_gang (gang, true);
          OB_FATAL_ERROR_CODE (0x20202002,
                               "Participate_creatingly in pool %s failed: %s\n",
                               poolName, ob_error_string (pret));
        }
      OB_DIE_ON_ERR_CODE (0x20202008, pool_rewind (new_ph));
      // XXX join_by_name ?
      pret = pool_join_gang (gang, new_ph);
      if (pret != OB_OK)
        {
          pool_disband_gang (gang, true);
          OB_FATAL_ERROR_CODE (0x20202003, "Pool %s failed to join gang: %s\n",
                               poolName, ob_error_string (pret));
        }
    }

  pool_hose ph;
  protein prot;
  pool_timestamp ts;

  do
    {
      pret = pool_next_multi (gang, &ph, &prot, &ts, NULL);
      if (pret != OB_OK)
        break;

      slaw_spew_overview (prot, stdout, NULL);
      fputc ('\n', stdout);
      fflush (stdout);
      protein_free (prot);
    }
  while (pret == OB_OK);

  if ((pret != OB_OK) && (pret != POOL_NO_SUCH_PROTEIN))
    {
      pool_disband_gang (gang, true);
      OB_FATAL_ERROR_CODE (0x20202004, "pool_next_multi failed: %s\n",
                           ob_error_string (pret));
    }

  // Not going to exit until I add some check
  do
    {
      pret =
        pool_await_next_multi (gang, POOL_WAIT_FOREVER, &ph, &prot, &ts, NULL);
      if (pret != OB_OK)
        OB_FATAL_ERROR_CODE (0x20202005, "pool_await_next_multi failed: %s\n",
                             ob_error_string (pret));

      slaw_spew_overview (prot, stdout, NULL);
      fputc ('\n', stdout);
      fflush (stdout);
      protein_free (prot);
    }
  while (pret == OB_OK);

  OB_DIE_ON_ERR_CODE (0x20202007, pool_disband_gang (gang, true));
  pool_cmd_free_options (&cmd);

  ob_log (OBLV_DBUG, 0x20202006, "Success!\n");
  return 0;
}
