
/* (c)  oblong industries */

///
/// Create a pool from the command line.
///
#include "pool_cmd.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-sys.h"
#include "libLoam/c/ob-vers.h"
#include "libPlasma/c/pool_options.h"
#include "libPlasma/c/protein.h"
#include "libPlasma/c/slaw.h"

static inline const char *dflt (int x)
{
  return x ? " (default)" : "";
}

static void usage (void)
{
  ob_banner (stderr);
  fprintf (stderr,
           "Usage: p-create [-q] [-z] [-G | -R | -F] [-S] [-l | -4] [-c]\n"
           "                [-t <type>] [-s <size>] [-i <toc cap>]\n"
           "                "
           "[-m <octal mode>] [-u <owner>] [-g <group>] <pool_name> [...]\n"
           "    -q Don't complain if pool exists (but leave it alone)\n"
           "    -z \"zap\" (delete and re-create) pool if it already exists\n"
           "    -G create a pool which is compatible with plasma < 2.0%s\n"
           "    -R create a pool which can be resized%s\n"
           "    -F create a single-file pool (which is also resizable)\n"
           "    -S set the pool's 'sync' option to true\n"
           "    -l use flock for locking (default on Mac OS X)\n"
           "    -4 use semaphores for locking (default on Linux)\n"
           "    -c compute and verify a checksum for each protein\n"
           "\t<type> defaults to \"mmap\"\n"
           "\t<size> defaults to %llu bytes\n"
           "\t<toc cap> defaults to 0 proteins\n"
           "\t<octal mode> works best if it only contains 7's and 0's\n"
           "\t<owner> and <group> may be name or number\n",
           dflt (!OB_RESIZABLE_BY_DEFAULT), dflt (OB_RESIZABLE_BY_DEFAULT),
           POOL_MMAP_DEFAULT_SIZE);
  exit (EXIT_FAILURE);
}

int main (int argc, char **argv)
{
  OB_CHECK_ABI ();

  pool_cmd_info cmd;
  int c, i;
  int ret = EXIT_SUCCESS;
  bool zap = false;
  bool quiet = false;
  slabu *sb = slabu_new ();
  int gr = 0;

  memset(&cmd, 0, sizeof(cmd));
  while ((c = getopt (argc, argv, "g:m:s:t:i:u:vzqGRSFl4c")) != -1)
    {
      switch (c)
        {
          case 'g':
            slabu_map_put_cc (sb, "group", optarg);
            break;
          case 'm':
            slabu_map_put_cc (sb, "mode", optarg);
            break;
          case 's':
            cmd.size = pool_cmd_parse_size (optarg);
            break;
          case 't':
            cmd.type = optarg;
            break;
          case 'i':
            cmd.toc_capacity = pool_cmd_parse_size (optarg);
            break;
          case 'u':
            slabu_map_put_cc (sb, "owner", optarg);
            break;
          case 'v':
            cmd.verbose = 1;
            break;
          case 'z':
            zap = true;
            break;
          case 'q':
            quiet = true;
            break;
          case 'G':
            slabu_map_put_cf (sb, "resizable", slaw_boolean (false));
            gr++;
            break;
          case 'S':
            slabu_map_put_cf (sb, "sync", slaw_boolean (true));
            break;
          case 'R':
            slabu_map_put_cf (sb, "resizable", slaw_boolean (true));
            gr++;
            break;
          case 'F':
            slabu_map_put_cf (sb, "single-file", slaw_boolean (true));
            gr++;
            break;
          case 'l':
            slabu_map_put_cf (sb, "flock", slaw_boolean (true));
            break;
          case '4':
            slabu_map_put_cf (sb, "flock", slaw_boolean (false));
            break;
          case 'c':
            slabu_map_put_cf (sb, "checksum", slaw_boolean (true));
            break;
          default:
            usage ();
        }
    }

  argc -= optind;
  argv += optind;

  if (argc < 1)
    usage ();

  if (gr > 1)
    {
      fprintf (stderr, "make up your mind: -G or -R or -F?\n");
      return EXIT_FAILURE;
    }

  pool_cmd_setup_options (&cmd);

  slaw perm = slaw_map_f (sb);
  protein opts =
    protein_from_ff (NULL,
                     slaw_maps_merge (protein_ingests (cmd.create_options),
                                      perm, NULL));

  for (i = 0; i < argc; i++)
    {
      const char *pname = argv[i];
      ob_retort pret = pool_create (pname, cmd.type, opts);
      if (pret == POOL_EXISTS && zap)
        {
          OB_DIE_ON_ERR_CODE (0x20203001, pool_dispose (pname));
          pret = pool_create (pname, cmd.type, opts);
        }
      else if (pret == POOL_EXISTS && quiet)
        pret = OB_OK;

      if (pret != OB_OK)
        {
          OB_LOG_ERROR_CODE (0x20203000,
                             "Can't create %s size %" OB_FMT_64 "u: %s\n",
                             pname, cmd.size, ob_error_string (pret));
          ret = pool_cmd_retort_to_exit_code (pret);
        }
    }

  pool_cmd_free_options (&cmd);
  protein_free (opts);
  slaw_free (perm);

  return ret;
}
