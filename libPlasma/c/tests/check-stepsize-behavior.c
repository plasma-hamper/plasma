
/* (c)  oblong industries */

// If the number of proteins in a pool at any given time never exceeds the
// index capacity, then the step size should remain 1, no matter how many
// proteins we go through.  (We do this in the extreme by only having one
// protein in the pool at once, by making the protein larger than half the
// pool size.)

#include "libLoam/c/ob-log.h"
#include "libPlasma/c/pool_cmd.h"
#include "libPlasma/c/slaw-path.h"
#include "libPlasma/c/protein.h"
#include "libPlasma/c/slaw.h"

static void usage (void)
{
  fprintf (stderr,
           "Usage: check-stepsize-behavior [-t <type>] [-s <size>] "
           "[-i <toc cap>] <pool_name>\n"
           "\t<type> defaults to \"mmap\"\n"
           "\t<size> defaults to %" OB_FMT_64 "u bytes\n",
           (unt64) POOL_MMAP_DEFAULT_SIZE);
  exit (EXIT_FAILURE);
}

static int mainish (int argc, char **argv)
{
  pool_cmd_info cmd;
  int c;
  int64 idx;
  protein terminal_info = NULL;
  unt64 stepsize;

  memset (&cmd, 0, sizeof (cmd));

  while ((c = getopt (argc, argv, "i:s:t:v")) != -1)
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
          case 'v':
            cmd.verbose = 1;
            break;
          default:
            usage ();
        }
    }

  pool_cmd_setup_options (&cmd);
  if (pool_cmd_get_poolname (&cmd, argc, argv, optind))
    usage ();

  ob_retort pret = pool_create (cmd.pool_name, cmd.type, cmd.create_options);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x20401000,
                         "Can't create %s size %" OB_FMT_64 "u: %s\n",
                         cmd.pool_name, cmd.size, ob_error_string (pret));

  pool_cmd_open_pool (&cmd);

  // Let's make a protein slightly bigger than half the pool
  int ret = EXIT_SUCCESS;
  protein p = protein_from_ff (
    slaw_list_inline_c ("There should be room for one of",
                        "these proteins, but not two of",
                        "these proteins in the pool at once.", NULL),
    slaw_map_inline_cf ("very large array",
                        slaw_int8_array_filled (cmd.size / 2, -1), NULL));

  for (idx = 0; idx < cmd.toc_capacity * 10; idx++)
    if (OB_OK != (pret = pool_deposit (cmd.ph, p, NULL)))
      {
        OB_LOG_ERROR_CODE (0x20401001, "pool_deposit said %s\n",
                           ob_error_string (pret));
        ret = EXIT_FAILURE;
        goto withdraw;
      }

  pret = pool_get_info (cmd.ph, -1, &terminal_info);
  if (pret != OB_OK)
    {
      OB_LOG_ERROR_CODE (0x20401002, "pool_get_info(-1) failed with: %s\n",
                         ob_error_string (pret));
      ret = EXIT_FAILURE;
      goto withdraw;
    }

  stepsize = slaw_path_get_unt64 (terminal_info, "index-step", 0);
  if (stepsize > 1)
    {
      OB_LOG_ERROR_CODE (0x20401003,
                         "index-step was %" OB_FMT_64 "u, but I don't think it "
                         "should be greater than 1\n",
                         stepsize);
      ret = EXIT_FAILURE;
      goto withdraw;
    }

withdraw:
  pret = pool_withdraw (cmd.ph);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x20401004, "pool_withdraw failed with: %s\n",
                         ob_error_string (pret));

  pret = pool_dispose (cmd.pool_name);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x20401005, "pool_dispose failed with: %s\n",
                         ob_error_string (pret));

  protein_free (p);
  protein_free (terminal_info);
  pool_cmd_free_options (&cmd);

  return ret;
}

int main (int argc, char **argv)
{
  return check_for_leaked_file_descriptors_scoped (mainish, argc, argv);
}
