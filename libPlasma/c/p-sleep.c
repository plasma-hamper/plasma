
/* (c)  oblong industries */

#include <errno.h>

#include "libLoam/c/ob-vers.h"
#include "libPlasma/c/pool_cmd.h"

static void usage (void)
{
  ob_banner (stderr);
  fprintf (stderr, "Usage: %s <pool name>\n", ob_get_prog_name ());
  exit (1);
}

int main (int argc, char **argv)
{
  OB_CHECK_ABI ();

  pool_cmd_info cmd;

  memset (&cmd, 0, sizeof (cmd));

  if (pool_cmd_get_poolname (&cmd, argc, argv, optind))
    usage ();

  ob_retort tort = pool_sleep (cmd.pool_name);
  if (tort < OB_OK)
    fprintf (stderr, "%s: %s\n", cmd.pool_name, ob_error_string (tort));

  return pool_cmd_retort_to_exit_code (tort);
}
