
/* (c)  oblong industries */

///
/// Fill up a pool, creating it if necessary.
///
#include "pool_cmd.h"
#include "libLoam/c/ob-vers.h"
#include "libLoam/c/ob-log.h"

static void usage (void)
{
  ob_banner (stderr);
  fprintf (stderr, "Usage: fill_pool [-t <type>] [-s <size>] <pool_name>\n");
  exit (EXIT_FAILURE);
}

int main (int argc, char *argv[])
{
  OB_CHECK_ABI ();

  pool_cmd_info cmd;
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
  if (pool_cmd_get_poolname (&cmd, argc, argv, optind))
    usage ();

  ob_retort pret = pool_create (cmd.pool_name, cmd.type, cmd.create_options);
  if ((pret != OB_OK) && (pret != POOL_EXISTS))
    OB_FATAL_ERROR_CODE (0x20200000,
                         "Can't create %s: %" OB_FMT_RETORT "d"
                         "\n",
                         cmd.pool_name, pret);

  pool_cmd_fill_pool (&cmd);

  pool_cmd_free_options (&cmd);

  return 0;
}
