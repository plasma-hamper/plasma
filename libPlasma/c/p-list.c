
/* (c)  oblong industries */

///
/// List all pools sharing our OB_POOLS_DIR environment variable.
///
#include "libPlasma/c/slaw-string.h"
#include "libPlasma/c/slaw-path.h"
#include "libPlasma/c/pool_options.h"
#include "libPlasma/c/pool_cmd.h"
#include "libLoam/c/ob-vers.h"
#include "libLoam/c/ob-util.h"

static void usage (void)
{
  ob_banner (stderr);
  fprintf (stderr, "Usage: p-list [-i] [server]\n");
  fprintf (stderr, "  -i print additional information about each pool\n");
  fprintf (stderr, "  server is an optional URI of the form "
                   "tcp://example.com:1234/\n");
  exit (EXIT_FAILURE);
}

/// Modifies size_inout to be in some reasonable units instead of
/// bytes, and returns a character to indicates what units it chose.
static char humanize (float64 *size_inout)
{
  float64 nicesize = *size_inout;
  char unit = ' ';
  if (nicesize >= GIGABYTE)
    {
      nicesize /= GIGABYTE;
      unit = 'G';
    }
  else if (nicesize >= MEGABYTE)
    {
      nicesize /= MEGABYTE;
      unit = 'M';
    }
  else if (nicesize >= 1024)
    {
      nicesize /= 1024;
      unit = 'K';
    }
  *size_inout = nicesize;
  return unit;
}

static slaw summarize_info (const char *poolName)
{
  int64 bytesize, slawvers, mmapvers;
  int64 sizeused, istep;
  pool_hose ph = NULL;
  protein p = NULL;
  slabu *sb = slabu_new ();
  ob_retort tort;
  const char *flag_str;

  char c;
  tort = pool_check_in_use (poolName);
  switch (tort)
    {
      case OB_OK:
        c = ' ';
        break;
      case POOL_IN_USE:
        c = 'X';
        break;
      default:
        c = '?';
        break;
    }

  tort = pool_participate (poolName, &ph, NULL);
  if (tort < OB_OK)
    {
      slabu_list_add_x (sb, slaw_string (ob_error_string (tort)));
      goto done;
    }

  tort = pool_get_info (ph, -1, &p);
  ob_err_accum (&tort, pool_withdraw (ph));
  if (tort < OB_OK)
    {
      slabu_list_add_x (sb, slaw_string (ob_error_string (tort)));
      goto done;
    }

  bytesize = slaw_path_get_int64 (p, "size", -1);
  sizeused = slaw_path_get_int64 (p, "size-used", -1);
  slawvers = slaw_path_get_int64 (p, "slaw-version", -1);
  mmapvers = slaw_path_get_int64 (p, "mmap-pool-version", -1);
  istep = slaw_path_get_int64 (p, "index-step", -1);
  flag_str = slaw_path_get_string (p, "flags", "");

  if (bytesize >= 0)
    {
      float64 nicesize = bytesize;
      char unit = humanize (&nicesize);
      slabu_list_add_x (sb,
                        slaw_string_format ("%c %6.2f%c", c, nicesize, unit));
    }

  if (sizeused >= 0)
    {
      float64 nicesize = sizeused;
      char unit = humanize (&nicesize);
      if (unit == ' ')
        slabu_list_add_x (sb, slaw_string_format ("(%.0f)", nicesize));
      else
        slabu_list_add_x (sb, slaw_string_format ("(%.2f%c)", nicesize, unit));
    }

  if (slawvers >= 0 && mmapvers >= 0)
    {
      slabu_list_add_x (sb, slaw_string_format ("s%" OB_FMT_64 "d"
                                                "m%" OB_FMT_64 "d",
                                                slawvers, mmapvers));
    }

  if (istep > 0)
    slabu_list_add_x (
      sb, slaw_string_format ("%s*%" OB_FMT_64 "d", flag_str, istep));
  else if (flag_str[0] != 0)
    slabu_list_add_x (sb, slaw_string (flag_str));

  protein_free (p);

done:
  return slaw_strings_join_slabu_f (sb, ", ");
}

int main (int argc, char **argv)
{
  OB_CHECK_ABI ();

  bool info = false;
  int c;
  const char *server = NULL;

  while ((c = getopt (argc, argv, "i")) != -1)
    {
      switch (c)
        {
          case 'i':
            info = true;
            break;
          default:
            usage ();
        }
    }

  argc -= optind;
  argv += optind;

  if (argc == 1)
    server = argv[0];
  else if (argc != 0)
    usage ();

  int rval = 0;

  slaw list_slaw;
  ob_retort tort = pool_list_ex (server, &list_slaw);

  if (tort >= OB_OK)
    {
      if (slaw_is_list (list_slaw))
        {
          bslaw string_slaw = slaw_list_emit_first (list_slaw);
          while (string_slaw && slaw_is_string (string_slaw))
            {
              const char *poolName = slaw_string_emit (string_slaw);
              if (info)
                {
                  slaw fullPoolName;
                  if (server)
                    fullPoolName =
                      slaw_string_format ("%s%s", server, poolName);
                  else
                    fullPoolName = slaw_string (poolName);
                  slaw extra = summarize_info (slaw_string_emit (fullPoolName));
                  printf ("%-40s %s\n", poolName, slaw_string_emit (extra));
                  slaw_free (extra);
                  slaw_free (fullPoolName);
                }
              else
                printf ("%s\n", poolName);
              string_slaw = slaw_list_emit_next (list_slaw, string_slaw);
            }
        }

      slaw_free (list_slaw);
    }
  else
    {
      if (tort == POOL_POOLNAME_BADTH)
        {
          fprintf (stderr,
                   "'%s' was not acceptable:\nA remote poolserver "
                   "URI should be of the form tcp://host[:port]/\n"
                   "e. g. tcp://redbean.oblong.com/ or "
                   "tcp://squash.local:1234/\n",
                   server);
        }
      else
        fprintf (stderr, "problem listing pools: %s\n", ob_error_string (tort));
      rval = 1;
    }

  return rval;
}
