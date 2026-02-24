
/* (c)  oblong industries */

///
/// Test that proteins correctly wraparound the end of the pool (i.e.,
/// deposits into a full pool work correctly).
///
#include "pool_cmd.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-vers.h"
#include "libLoam/c/ob-sys.h"
#include "libPlasma/c/protein.h"
#include "libPlasma/c/slaw.h"

static unt64 pool_size = (10 * 1024);
static unt64 deposits = 100;
static unt64 iterations = 50;

static void usage (void)
{
  fprintf (stderr,
           "Usage: wrap_test [-t <type>] [-s <size>] "
           "[-I <iterations>] \n"
           "\t[-d <deposits per iteration>] [-i <toc cap>] <pool_name>\n");
  exit (1);
}

static void DepositProtein (pool_hose poho, int pool_size_unused, protein p)
{
  int i = 0;
  ob_retort pret;
  for (i = 0; i < deposits; i++)
    {
      pret = pool_deposit (poho, p, NULL);
      // Ignore bigger than pool errors - expected.  XXX should test
      // that protein really is too big, but requires lots of internal
      // pool structure knowledge.
      if (pret == POOL_PROTEIN_BIGGER_THAN_POOL)
        return;
      if (pret != OB_OK)
        {
          fprintf (stderr, "bad deposit: %s\n", ob_error_string (pret));
          exit (1);
        }
    }
}

static void CreateProtein (int str_len, protein *prot)
{
  char *buffer = (char *) malloc (str_len + 1);
  int j;
  for (j = 0; j < str_len; j++)
    buffer[j] = 'a';
  // Null terminate
  buffer[j] = '\0';

  slaw m = slaw_map_inline_cf (
    "time", slaw_int32 (165955708), "numdots", slaw_int32 (0), "points",
    slaw_list_f (slabu_new ()), "test_string", slaw_string (buffer), NULL);
  *prot = protein_from_ff (slaw_list_inline_c ("dotframe", NULL), m);
  free (buffer);

  return;
}

int mainish (int argc, char **argv)
{
  pool_cmd_info cmd;
  pool_hose ph;
  protein prot;
  int str_len = 0;
  int c;
  int i;

  memset (&cmd, 0, sizeof (cmd));
  cmd.size = pool_size;

  while ((c = getopt (argc, argv, "d:i:I:s:t:")) != -1)
    {
      switch (c)
        {
          case 'd':
            deposits = strtoll (optarg, NULL, 0);
            break;
          case 'i':
            cmd.toc_capacity = strtoll (optarg, NULL, 0);
            break;
          case 'I':
            iterations = strtoll (optarg, NULL, 0);
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

  // Some bugs only show up on a fresh pool.
  ob_retort pret = pool_create (cmd.pool_name, cmd.type, cmd.create_options);
  if (pret != OB_OK)
    {
      fprintf (stderr, "no can create %s (%" OB_FMT_64 "u): %s\n",
               cmd.pool_name, cmd.size, ob_error_string (pret));
      exit (1);
    }

  pool_cmd_open_pool (&cmd);
  ph = cmd.ph;

  for (i = 0; i < iterations; i++)
    {
      // Note that this is test is actually reproducible, since rand()
      // is pseudo-random.
      //
      // Note also that proteins can be just slightly larger than the
      // pool.  This is good; tests the too big case.
      str_len = ((double) rand () / (RAND_MAX)) * cmd.size;
      CreateProtein (++str_len, &prot);
      printf ("string size: %d.  protein len: %" OB_FMT_64 "u"
              "\n",
              str_len, protein_len (prot));
      DepositProtein (ph, cmd.size, prot);
      protein_free (prot);
    }

  OB_DIE_ON_ERROR (pool_withdraw (ph));

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
