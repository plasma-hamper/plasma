
/* (c)  oblong industries */

#include "pool_cmd.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-vers.h"
#include "libLoam/c/ob-sys.h"

static void usage (void)
{
  fprintf (stderr, "Usage: participate-create [-t <type>] [-s <size>]\n"
                   "[-i <toc cap>] <pool_name>\n");
  exit (1);
}

int mainish (int argc, char **argv)
{
  pool_cmd_info cmd;
  int c;
  ob_retort expected;

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
  if (pret != (expected = POOL_CREATED))
    OB_FATAL_ERROR_CODE (0x20409000, "Dude, expected %s but got %s\n",
                         ob_error_string (expected), ob_error_string (pret));
  OB_DIE_ON_ERROR (pool_withdraw (cmd.ph));

  pret = pool_participate (cmd.pool_name, &cmd.ph, NULL);
  // POOL_EXISTS is not a valid return value for pool_participate
  if (pret != OB_OK)
    {
      fprintf (stderr, "no can participate %s (%" OB_FMT_64 "u): %s\n",
               cmd.pool_name, cmd.size, ob_error_string (pret));
      exit (1);
    }
  OB_DIE_ON_ERROR (pool_withdraw (cmd.ph));

  pret = pool_participate_creatingly (cmd.pool_name, cmd.type, &cmd.ph,
                                      cmd.create_options);
  if (pret != (expected = OB_OK))
    OB_FATAL_ERROR_CODE (0x20409001, "Dude, expected %s but got %s\n",
                         ob_error_string (expected), ob_error_string (pret));

  OB_DIE_ON_ERROR (pool_withdraw (cmd.ph));

  pret = pool_dispose (cmd.pool_name);
  if (pret != OB_OK)
    {
      fprintf (stderr, "no can stop %s: %s\n", cmd.pool_name,
               ob_error_string (pret));
      return 1;
    }

  pool_cmd_free_options (&cmd);

  return 0;
}

int main (int argc, char **argv)
{
  OB_DIE_ON_ERROR (OB_CHECK_ABI ());

  return check_for_leaked_file_descriptors_scoped (mainish, argc, argv);
}
