
/* (c)  oblong industries */

///
/// Print the oldest index of this pool.
///
#include "pool_cmd.h"
#include "libLoam/c/ob-sys.h"
#include "libLoam/c/ob-vers.h"
#include "libLoam/c/ob-log.h"

static void usage (void)
{
  ob_banner (stderr);
  fprintf (stderr, "Usage: p-oldest-idx <pool name>\n");
  exit (EXIT_FAILURE);
}

int main (int argc, char **argv)
{
  OB_CHECK_ABI ();

  ob_retort pret;
  int64 idx;
  pool_cmd_info cmd;

  memset (&cmd, 0, sizeof (cmd));

  if (pool_cmd_get_poolname (&cmd, argc, argv, optind))
    usage ();

  pool_cmd_open_pool (&cmd);

  int rval = EXIT_SUCCESS;

  pret = pool_oldest_index (cmd.ph, &idx);
  if (pret == POOL_NO_SUCH_PROTEIN)
    {
      fprintf (stderr, "pool is empty\n");
      OB_DIE_ON_ERROR (pool_withdraw (cmd.ph));
      rval = pool_cmd_retort_to_exit_code (pret);
    }
  else if (pret != OB_OK)
    {
      fprintf (stderr, "could not fetch index: %s\n", ob_error_string (pret));
      pool_withdraw (cmd.ph);
      rval = pool_cmd_retort_to_exit_code (pret);
    }
  else
    {
      fprintf (stdout, "%" OB_FMT_64 "d\n", idx);
      OB_DIE_ON_ERROR (pool_withdraw (cmd.ph));
    }

  return rval;
}
