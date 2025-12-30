
/* (c)  oblong industries */

///
/// Test for leaks in pool_create(), pool_dispose(),
/// pool_participate(), and pool_withdraw() by executing many of them
/// in the same process.
///
#include "pool_cmd.h"
#include "libLoam/c/ob-sys.h"
#include "libPlasma/c/slaw-string.h"
#include "libPlasma/c/slaw-coerce.h"
#include "libLoam/c/ob-log.h"
#include <errno.h>

static int creates;

static void usage (void)
{
  fprintf (stderr, "Usage: many_creates [-t <type>] [-s <size>] "
                   "[-n # creates] [-i <toc cap>] <pool_name>\n");
  exit (1);
}

/* Since a common way for this test to fail is if the user hasn't
 * configured the correct number of semaphore sets for their Linux
 * kernel, let's be helpful and warn them about it. */
static void warn_about_semaphores (void)
{
  char buf[300];
  FILE *f = fopen ("/proc/sys/kernel/sem", "r");

  if (!f)
    {
      /* Most likely reason this failed is because we're not on Linux,
       * in which case the semaphores don't seem to be a problem. */
      return;
    }

  char *p = fgets (buf, sizeof (buf), f);
  if (!p)
    return;

  p = strrchr (p, '\n'); /* delete trailing newline */
  if (p)
    *p = 0;

  slabu *sb = slabu_of_strings_from_split (buf, "\t");
  fclose (f);
  int64 semsets = 0;
  if (OB_OK == slaw_to_int64 (slabu_list_nth (sb, 3), &semsets)
      && semsets < creates)
    {
      fprintf (stderr,
               "warning: %d semaphore sets are needed, and you "
               "only have %" OB_FMT_64 "d\n",
               creates, semsets);
      fprintf (stderr, "If this test fails, try this:\n");
      fprintf (stderr,
               "sudo sh -c 'echo %s %s %s %" OB_FMT_64
               "d > /proc/sys/kernel/sem'\n",
               slaw_string_emit (slabu_list_nth (sb, 0)),
               slaw_string_emit (slabu_list_nth (sb, 1)),
               slaw_string_emit (slabu_list_nth (sb, 2)), creates + semsets);
      fprintf (stderr, "or see "
                       "https://gitlab.oblong.com/platform/docs/-/wikis/"
                       "increasing-Linux-semaphores-to-allow-tests-like-many_"
                       "creates-to-pass\n");

      /* Note: the above recommendation is sufficient to get this test
         to pass, but our general recommendation is "250 65536 32
         32768" */
    }

  slabu_free (sb);
}

int main (int argc, char **argv)
{
  pool_cmd_info cmd;
  int c;

  memset (&cmd, 0, sizeof (cmd));
  while ((c = getopt (argc, argv, "i:n:s:t:")) != -1)
    {
      switch (c)
        {
          case 'i':
            cmd.toc_capacity = strtoll (optarg, NULL, 0);
            break;
          case 'n':
            creates = strtoll (optarg, NULL, 0);
            if (creates < 1)
              {
                fprintf (stderr, "Minimum creates is 1\n");
                exit (1);
              }
            break;
          case 's':
            cmd.size = strtoll (optarg, NULL, 0);
            break;
          case 't':
            cmd.type = optarg;
            break;
          // Would be nice to have a verbose option
          default:
            usage ();
        }
    }

  pool_cmd_setup_options (&cmd);
  if (pool_cmd_get_poolname (&cmd, argc, argv, optind))
    usage ();

  warn_about_semaphores ();

  int i;
  for (i = 0; i < creates; i++)
    {
      ob_retort pret =
        pool_create (cmd.pool_name, cmd.type, cmd.create_options);
      if (cmd.verbose)
        printf ("%d\n", i);
      if (pret < OB_OK)
        OB_FATAL_ERROR_CODE (0x20405000,
                             "no can create %s (%" OB_FMT_64 "u"
                             "): %s\n",
                             cmd.pool_name, cmd.size, ob_error_string (pret));
      // While we're at it, participate and withdraw, too
      pool_cmd_open_pool (&cmd);
      pret = pool_withdraw (cmd.ph);
      if (pret < OB_OK)
        OB_FATAL_ERROR_CODE (0x20405001,
                             "no can withdraw from %s (%" OB_FMT_64 "u"
                             "): %s\n",
                             cmd.pool_name, cmd.size, ob_error_string (pret));
      pret = pool_dispose (cmd.pool_name);
      if (pret < OB_OK)
        OB_FATAL_ERROR_CODE (0x20405002,
                             "no can dispose %s (%" OB_FMT_64 "u"
                             "): %s\n",
                             cmd.pool_name, cmd.size, ob_error_string (pret));
    }

  // Now create a whole bunch without destroying to catch other kinds
  // of leaks.
  size_t pn_size;
  char *poolName = (char *) malloc (pn_size = (strlen (cmd.pool_name) + 100));
  if (poolName == NULL)
    OB_FATAL_ERROR_CODE (0x20405003,
                         "Couldn't allocate a teensy-weensy bit of memory\n");

  for (i = 0; i < creates; i++)
    {
      snprintf (poolName, pn_size, "%s%d", cmd.pool_name, i);
      if (cmd.verbose)
        printf ("%d\n", i);
      ob_retort pret = pool_create (poolName, cmd.type, cmd.create_options);
      if (pret != OB_OK)
        {
          // fprintf (stderr, "errno = '%s'\n", strerror (errno));
          OB_FATAL_ERROR_CODE (0x20405004,
                               "no can create %s (%" OB_FMT_64 "u): %s\n",
                               poolName, cmd.size, ob_error_string (pret));
        }
    }

  // And destroy them all
  for (i = 0; i < creates; i++)
    {
      snprintf (poolName, pn_size, "%s%d", cmd.pool_name, i);
      if (cmd.verbose)
        printf ("%d\n", i);
      ob_retort pret = pool_dispose (poolName);
      if (pret != OB_OK)
        OB_FATAL_ERROR_CODE (0x20405005,
                             "no can dispose %s (%" OB_FMT_64 "u"
                             "): %s\n",
                             poolName, cmd.size, ob_error_string (pret));
    }

  free (poolName);
  pool_cmd_free_options (&cmd);

  return 0;
}
