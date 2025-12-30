
/* (c)  oblong industries */

//
// Incredibly simple single-process pool read test to use for
// performance profiling with gprof.
//
#include <errno.h>
#include <signal.h>
#include <sys/time.h>

#include "pool_cmd.h"
#include "libLoam/c/ob-vers.h"
#include "libLoam/c/ob-log.h"
#include "libPlasma/c/protein.h"
#include "libPlasma/c/slaw.h"

// When the time is up, a signal handler sets this to 1 so we know
// when to stop.
static volatile sig_atomic_t stop;

static void end_test (int signo)
{
  stop = 1;
}

static void usage (void)
{
  ob_banner (stderr);
  fprintf (stderr, "Usage: simple_read [-t <type>] [-s <size>] "
                   "[-S <test seconds>] <pool_name>\n");
  exit (EXIT_FAILURE);
}

int main (int argc, char *argv[])
{
  OB_CHECK_ABI ();

  ob_retort pret;
  pool_cmd_info cmd;
  pool_hose ph;
  unsigned long long test_secs = 1;
  unsigned long long i;
  int c;

  memset (&cmd, 0, sizeof (cmd));
  while ((c = getopt (argc, argv, "s:S:t:")) != -1)
    {
      switch (c)
        {
          case 's':
            cmd.size = strtoll (optarg, NULL, 0);
            break;
          case 'S':
            test_secs = strtoll (optarg, NULL, 0);
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

  // Make and destroy a new pool every time for better reproducibility
  pret = pool_create (cmd.pool_name, cmd.type, cmd.create_options);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x20205000, "no can create %s (%" OB_FMT_64 "u): %s\n",
                         cmd.pool_name, cmd.size, ob_error_string (pret));

  // Fill up pool - the number of proteins in the pool heavily affect
  // the performance results.
  pool_cmd_fill_pool (&cmd);

  pool_cmd_open_pool (&cmd);
  ph = cmd.ph;

  // Set an alarm handler
  struct sigaction act;
  memset (&act, 0, sizeof (act));
  act.sa_handler = end_test;
  sigaction (SIGALRM, &act, NULL);

  // Set an alarm to wake us after the time is up.
  struct itimerval iv;
  memset (&iv, 0, sizeof (iv));
  iv.it_value.tv_sec = test_secs;
  if (setitimer (ITIMER_REAL, &iv, NULL) < 0)
    {
      OB_PERROR_CODE (0x20205003, "setitimer");
      exit (1);
    }

  // Read proteins as fast as we can until the test is over
  protein prot;
  pool_timestamp ts;
  for (i = 0; stop != 1; i++)
    {
      pret = pool_next (ph, &prot, &ts, NULL);
      // Did we run out?
      if (pret == POOL_NO_SUCH_PROTEIN)
        {
          // Start over at the beginning
          OB_DIE_ON_ERR_CODE (0x20205005, pool_rewind (ph));
          // We didn't actually read a protein, so subtract from the count
          i--;
        }
      else if (pret != OB_OK)
        {
          OB_FATAL_ERROR_CODE (0x20205001, "pool_next returns %s\n",
                               ob_error_string (pret));
        }
      else
        {
          // Success!
          protein_free (prot);
        }
    }

  OB_DIE_ON_ERR_CODE (0x20205004, pool_withdraw (ph));
  pool_cmd_free_options (&cmd);
  printf ("read %llu proteins/sec\n", i / test_secs);

  pret = pool_dispose (cmd.pool_name);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x20205002, "no can stop %s: %s\n", cmd.pool_name,
                         ob_error_string (pret));
  return 0;
}
