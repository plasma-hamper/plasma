
/* (c)  oblong industries */

#include "libLoam/c/ob-retorts.h"
#include "libPlasma/c/slaw-io.h"
#include "libPlasma/c/protein.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-sys.h"
#include "libLoam/c/ob-vers.h"

#include <stdlib.h>
#include <stdio.h>

static void usage (void)
{
  ob_banner (stderr);
  fprintf (stderr, "Usage: bin2yaml [-L] [-y] infile.bin outfile.yaml\n"
                   "    -L lossy (untagged numbers and unordered maps) "
                   "[default is lossless]\n"
                   "    -y no %%YAML directives [default has directives]\n");
  exit (EXIT_FAILURE);
}

int main (int argc, char **argv)
{
  OB_CHECK_ABI ();

  ob_retort err;
  protein p;
  slaw_output out;
  slaw_input in;
  int c;
  bool lossy = false, directives = true;

  while ((c = getopt (argc, argv, "Ly")) != -1)
    {
      switch (c)
        {
          case 'L':
            lossy = true;
            break;
          case 'y':
            directives = false;
            break;
          default:
            usage ();
        }
    }

  argc -= optind;
  argv += optind;

  if (argc != 2)
    usage ();

  slaw options = slaw_map_inline_cf (
    "tag_numbers", slaw_boolean (!lossy), "ordered_maps", slaw_boolean (!lossy),
    "directives", slaw_boolean (directives), NULL);

  err = slaw_input_open_binary (argv[0], &in);
  OB_DIE_ON_ERROR (err);

  err = slaw_output_open_text_options_f (argv[1], &out, options);
  OB_DIE_ON_ERROR (err);

  while ((err = slaw_input_read (in, &p)) == OB_OK)
    {
      err = slaw_output_write (out, p);
      OB_DIE_ON_ERROR (err);
      protein_free (p);
    }

  if (err != SLAW_END_OF_FILE)
    OB_DIE_ON_ERROR (err);

  err = slaw_output_close (out);
  OB_DIE_ON_ERROR (err);

  err = slaw_input_close (in);
  OB_DIE_ON_ERROR (err);

  return EXIT_SUCCESS;
}
