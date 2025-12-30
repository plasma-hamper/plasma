
/* (c)  oblong industries */

///
/// Loop forever reading proteins from a pool with pool_await_next().
///
#include "pool_cmd.h"
#include "libLoam/c/ob-sys.h"
#include "libLoam/c/ob-vers.h"
#include "libLoam/c/ob-log.h"
#include "libPlasma/c/slaw.h"
#include "libPlasma/c/protein.h"

static void usage (void)
{
  ob_banner (stderr);
  fprintf (stderr, "Usage: p-await <pool name>\n");
  exit (EXIT_FAILURE);
}

int main (int argc, char **argv)
{
  OB_CHECK_ABI ();

  ob_retort pret;
  protein p;
  pool_timestamp ts;
  pool_cmd_info cmd;

  memset (&cmd, 0, sizeof (cmd));
  if (pool_cmd_get_poolname (&cmd, argc, argv, optind))
    usage ();

  pool_cmd_open_pool (&cmd);

  while (1)
    {
      pret = pool_await_next (cmd.ph, POOL_WAIT_FOREVER, &p, &ts, NULL);
      if (OB_OK != pret)
        {
          pool_withdraw (cmd.ph);
          fprintf (stderr, "problem with pool_await_next(): %s\n",
                   ob_error_string (pret));
          return pool_cmd_retort_to_exit_code (pret);
        }
      slaw_spew_overview (p, stdout, NULL);
      fputc ('\n', stdout);
      protein_free (p);
    }

  // Not reached at present.
  OB_DIE_ON_ERROR (pool_withdraw (cmd.ph));
  pool_cmd_free_options (&cmd);

  return EXIT_SUCCESS;
}
