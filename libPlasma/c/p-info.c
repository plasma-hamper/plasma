
/* (c)  oblong industries */

// Command-line wrapper for pool_get_info(), which can return
// information about a pool like size, type, and version.

#include "libPlasma/c/protein.h"
#include "libPlasma/c/slaw.h"
#include "libPlasma/c/slaw-io.h"
#include "libPlasma/c/slaw-path.h"
#include "libPlasma/c/pool_cmd.h"
#include "libLoam/c/ob-sys.h"
#include "libLoam/c/ob-vers.h"
#include "libLoam/c/ob-log.h"
#include <time.h>

static void usage (void)
{
  ob_banner (stderr);
  fprintf (stderr, "Usage: p-info <pool name>\n");
  exit (EXIT_FAILURE);
}

typedef ob_retort (*idx_func) (pool_hose ph, int64 *index_out);

static void print_index (pool_hose ph, idx_func func, const char *label)
{
  ob_retort tort;
  int64 idx;

  printf ("%s: ", label);
  tort = func (ph, &idx);
  if (tort < OB_OK)
    {
      // not a fatal error because it might just be "POOL_NO_SUCH_PROTEIN"
      // (i. e. an empty pool)
      printf ("%s\n", ob_error_string (tort));
    }
  else
    {
      protein p = NULL;
      pool_timestamp ts;
      printf ("%20" OB_FMT_64 "d  ", idx);
      tort = pool_nth_protein (ph, idx, &p, &ts);
      if (tort < OB_OK)
        {
          // don't treat this as fatal either; there's a race condition
          // where POOL_NO_SUCH_PROTEIN (e. g. discarded due to wrap)
          // is possible.
          printf ("%s\n", ob_error_string (tort));
        }
      else
        {
          time_t t = (time_t) ts;
          printf ("%s", ctime (&t));  // ctime includes newline
        }
    }
}

int main (int argc, char **argv)
{
  OB_CHECK_ABI ();

  pool_cmd_info cmd;
  bool terminal;
  int64 hops;
  int ret = EXIT_SUCCESS;
  slaw f;

  memset (&cmd, 0, sizeof (cmd));
  if (pool_cmd_get_poolname (&cmd, argc, argv, optind))
    usage ();

  pool_cmd_open_pool (&cmd);

  f = slaw_boolean (false);

  print_index (cmd.ph, pool_oldest_index, "oldest");
  print_index (cmd.ph, pool_newest_index, "newest");

  printf ("\n");

  for (terminal = false, hops = 0; !terminal; hops++)
    {
      protein p = NULL;
      bslaw info = NULL;
      slaw yaml = NULL;
      ob_retort tort = pool_get_info (cmd.ph, hops, &p);
      if (tort < OB_OK)
        {
          fprintf (stderr, "pool_get_info(%" OB_FMT_64 "d): %s\n", hops,
                   ob_error_string (tort));
          ret = EXIT_FAILURE;
          break;
        }
      info = protein_ingests (p);
      terminal = slaw_path_get_bool (info, "terminal", true);
      tort = slaw_to_string_options_f (
        info, &yaml,
        slaw_map_inline_cl ("tag_numbers", f, "directives", f, "ordered_maps",
                            f, NULL));
      protein_free (p);
      if (tort < OB_OK)
        {
          fprintf (stderr, "slaw_to_string(%" OB_FMT_64 "d): %s\n", hops,
                   ob_error_string (tort));
          ret = EXIT_FAILURE;
          break;
        }
      printf ("hop %" OB_FMT_64 "d:\n%s\n", hops, slaw_string_emit (yaml));
      slaw_free (yaml);
    }

  slaw_free (f);

  OB_DIE_ON_ERROR (pool_withdraw (cmd.ph));

  return ret;
}
