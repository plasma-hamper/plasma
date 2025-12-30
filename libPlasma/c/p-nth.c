
/* (c)  oblong industries */

///
/// Retrieve the nth protein from this pool and print it to stdout.
///
#include "pool_cmd.h"
#include "libLoam/c/ob-sys.h"
#include "libLoam/c/ob-vers.h"
#include "libLoam/c/ob-log.h"
#include "libPlasma/c/slaw-io.h"
#include "libPlasma/c/protein.h"
#include "libPlasma/c/slaw.h"

static void usage (void)
{
  ob_banner (stderr);
  fprintf (stderr, "Usage: p-nth <pool name> <protein index> [file]\n");
  exit (EXIT_FAILURE);
}

int main (int argc, char **argv)
{
  OB_CHECK_ABI ();

  ob_retort pret;
  protein p;
  pool_timestamp ts;
  pool_cmd_info cmd;

  if (argc < 3 || argc > 4)
    usage ();

  memset (&cmd, 0, sizeof (cmd));
  cmd.pool_name = argv[1];
  int64 ind = strtoll (argv[2], NULL, 0);

  pool_cmd_open_pool (&cmd);

  pret = pool_nth_protein (cmd.ph, ind, &p, &ts);

  if (POOL_NO_SUCH_PROTEIN == pret)
    fprintf (stderr, "that protein is not there\n");
  else if (OB_OK != pret)
    fprintf (stderr, "some trouble or other retrieving protein: %s\n",
             ob_error_string (pret));

  if (OB_OK != pret)
    {
      pool_withdraw (cmd.ph);
      return pool_cmd_retort_to_exit_code (pret);
    }

  if (argc == 3)
    {
      printf ("time %f, size %" OB_FMT_64 "u - this protein: \n", ts,
              protein_len (p));
      slaw_spew_overview (p, stdout, NULL);
      fputc ('\n', stdout);
    }
  else
    OB_DIE_ON_ERROR (slaw_write_to_binary_file (argv[3], p));

  protein_free (p);

  OB_DIE_ON_ERROR (pool_withdraw (cmd.ph));

  return EXIT_SUCCESS;
}
