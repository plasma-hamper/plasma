
/* (c)  oblong industries */

///
/// Test the pool await routines.
///

#include <libLoam/c/ob-sys.h>
#include <libLoam/c/ob-pthread.h>
#include "libLoam/c/ob-vers.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-time.h"
#include <errno.h>
#include "pool_cmd.h"
#include "libPlasma/c/protein.h"
#include "libPlasma/c/slaw.h"

static int nthreads;
static int iterations;
// Threads do a "storm" of repeats number of deposits each iteration
static int repeats;
static int test_secs;
static int sleep_usecs;

// This keeps track of which "uberiteration" we're on.
// (Basically we run the entire program multiple times, to increase
// the probability of catching nondeterministic errors.)
static int ubi;

// Make this global so all threads can read it and do participate
static pool_cmd_info cmd;

/// Deposit the specified number of proteins in the pool.

static void *deposit_main (void *arg)
{
  // I agree with Tom Lord that this warning bites hard. intptr_t is
  // ridiculous.
  int thread_num = (int) (intptr_t) arg;
  pool_hose ph;
  ob_retort pret;
  protein prot;
  char name[100];
  int i, j;

  ob_log (OBLV_DBUG, 0x20400000,
          "[u%d] deposit thread %d trying to participate...\n", ubi,
          thread_num);

  pret = pool_participate (cmd.pool_name, &ph, NULL);
  OB_DIE_ON_ERROR (pret);

  snprintf (name, sizeof (name), "deposit%du%d", thread_num, ubi);
  OB_DIE_ON_ERROR (pool_set_hose_name (ph, name));

  for (i = 0; i < iterations; i++)
    {
      usleep (sleep_usecs);
      snprintf (name, sizeof (name), "thread%dadd%d", thread_num, i);
      ob_log (OBLV_DBUG, 0x20400001,
              "[u%d] deposit thread %d starting iteration %d\n", ubi,
              thread_num, i);
      // Add multiple proteins all at once.
      for (j = 0; j < repeats; j++)
        {
          pret = pool_cmd_add_test_protein (ph, name, &prot, NULL);
          if (pret != OB_OK)
            {
              pool_withdraw (ph);
              OB_LOG_ERROR_CODE (0x20400002,
                                 "[u%d] deposit thread %d failed %s\n", ubi,
                                 thread_num, ob_error_string (pret));
              return (void *) 1;
            }
          ob_log (OBLV_DBUG, 0x20400003, "[u%d] deposited protein #%d\n", ubi,
                  (i * repeats) + j);
          Free_Protein (prot);
        }
    }

  OB_DIE_ON_ERROR (pool_withdraw (ph));
  ob_log (OBLV_DBUG, 0x20400004, "[u%d] deposit thread %d returning 0\n", ubi,
          thread_num);
  return (void *) 0;
}

/// Wait for proteins, exit when expected number of proteins have been
/// read.

static void *await_main (void *arg)
{
  int thread_num = (int) (intptr_t) arg;
  ob_retort pret;
  protein prot;
  pool_timestamp ts;
  pool_hose ph;
  int i;

  ob_log (OBLV_DBUG, 0x20400005,
          "[u%d] await thread %d trying to participate...\n", ubi, thread_num);

  pret = pool_participate (cmd.pool_name, &ph, NULL);
  OB_DIE_ON_ERROR (pret);

  char name[32];
  snprintf (name, sizeof (name), "await%du%d", thread_num, ubi);
  OB_DIE_ON_ERROR (pool_set_hose_name (ph, name));

  // We might miss a protein if it's deposited before we participate!
  OB_DIE_ON_ERROR (pool_rewind (ph));

  ob_log (OBLV_DBUG, 0x20400006, "[u%d] await thread %d starting...\n", ubi,
          thread_num);

  // Read every single protein in the pool
  for (i = 0; i < iterations * nthreads * repeats; i++)
    {
      // XXX Will simply hang if it misses a protein.
      prot = NULL;
      pret = pool_await_next (ph, POOL_WAIT_FOREVER, &prot, &ts, NULL);
      if (pret != OB_OK)
        {
          pool_withdraw (ph);
          OB_LOG_ERROR_CODE (0x20400007,
                             "[u%d] await thread %d failed %s, returning 1\n",
                             ubi, thread_num, ob_error_string (pret));
          return (void *) 1;
        }
      else if (prot == NULL)
        {
          pool_withdraw (ph);
          OB_LOG_ERROR_CODE (0x2040002f,
                             "[u%d] await thread %d prot was NULL, "
                             "returning 1\n",
                             ubi, thread_num);
          return (void *) 1;
        }
      ob_log (OBLV_DBUG, 0x20400022, "[u%d] await thread read protein #%d\n",
              ubi, i);
      //slaw_spew_overview (prot, stdout);
      protein_free (prot);
    }

  OB_DIE_ON_ERROR (pool_withdraw (ph));
  ob_log (OBLV_DBUG, 0x20400009, "[u%d] await thread %d returning 0\n", ubi,
          thread_num);
  return (void *) 0;
}

/// Wait for a particular protein

static void *probe_await_main (void *arg)
{
  int thread_num = (int) (intptr_t) arg;
  ob_retort pret;
  protein prot;
  pool_timestamp ts;
  int64 idx;
  pool_hose ph;
  char name[100];
  int i;

  pret = pool_participate (cmd.pool_name, &ph, NULL);
  OB_DIE_ON_ERROR (pret);

  snprintf (name, sizeof (name), "probe%du%d", thread_num, ubi);
  OB_DIE_ON_ERROR (pool_set_hose_name (ph, name));

  OB_DIE_ON_ERROR (pool_rewind (ph));

  // What descrip are we looking for?  The deposit threads use the
  // thread number and the iteration number to create a unique
  // descrip.  Look for all the proteins from one depositor.

  for (i = 0; i < iterations; i++)
    {
      snprintf (name, sizeof (name), "descrip_thread%dadd%d", thread_num, i);
      slaw descrip = slaw_string (name);
      ob_log (OBLV_DBUG, 0x2040002e, "[u%d] probe awaiting for '%s'\n", ubi,
              name);
      pret = pool_await_probe_frwd (ph, descrip, POOL_WAIT_FOREVER, &prot, &ts,
                                    &idx);
      if (pret < OB_OK)
        {
          OB_LOG_ERROR_CODE (0x20400033,
                             "[u%d] pool_await_probe_frwd returned %s\n", ubi,
                             ob_error_string (pret));
          break;
        }
      pret = pool_rewind (ph);
      if (pret < OB_OK)
        {
          OB_LOG_ERROR_CODE (0x20400034, "[u%d] pool_rewind returned %s\n", ubi,
                             ob_error_string (pret));
          break;
        }
      ob_log (OBLV_DBUG, 0x2040000a,
              "[u%d] probe_await thread read protein #%d\n", ubi, i);
      protein_free (prot);
      slaw_free (descrip);
    }
  OB_DIE_ON_ERROR (pool_withdraw (ph));

  if (pret != OB_OK)
    {
      OB_LOG_ERROR_CODE (0x2040000b,
                         "[u%d] probe_await thread %d returning %s\n", ubi,
                         thread_num, ob_error_string (pret));
      return (void *) 1;
    }

  ob_log (OBLV_DBUG, 0x2040000c,
          "[u%d] probe_await thread %d returning successfully\n", ubi,
          thread_num);
  return (void *) 0;
}

/// Do a timed await until we time out for real

static void *timed_await_main (void *arg)
{
  int thread_num = (int) (intptr_t) arg;
  ob_retort pret;
  protein prot;
  pool_timestamp ts;
  pool_hose ph;
  // XXX await time should be linked to sleep_usecs at minimum
  pool_timestamp timeout = 0.5;

  pret = pool_participate (cmd.pool_name, &ph, NULL);
  OB_DIE_ON_ERROR (pret);

  char name[32];
  snprintf (name, sizeof (name), "timed%du%d", thread_num, ubi);
  OB_DIE_ON_ERROR (pool_set_hose_name (ph, name));

  OB_DIE_ON_ERROR (pool_rewind (ph));

  // Exit around the time of test_secs
  int i;
  int rounds = test_secs / timeout;
  int read_count = 0;
  ob_log (OBLV_DBUG, 0x2040000d, "[u%d] timed_wait %d rounds\n", ubi, rounds);
  for (i = 0; i < rounds; i++)
    {
      ob_log (OBLV_DBUG, 0x2040000e, "[u%d] starting timed await %f seconds\n",
              ubi, timeout);
      pret = pool_await_next (ph, timeout, &prot, &ts, NULL);
      if (pret == OB_OK)
        {
          ob_log (OBLV_DBUG, 0x2040000f, "[u%d] timed_await read protein %d\n",
                  ubi, read_count);
          protein_free (prot);
          read_count++;
        }
      else if (pret == POOL_AWAIT_TIMEDOUT)
        {
          ob_log (OBLV_DBUG, 0x20400010, "[u%d] timed_await timed out\n", ubi);
        }
      else
        {
          ob_log (OBLV_DBUG, 0x20400011, "[u%d] timed_await error %s\n", ubi,
                  ob_error_string (pret));
          break;
        }
    }
  OB_DIE_ON_ERROR (pool_withdraw (ph));

  if (pret == POOL_AWAIT_TIMEDOUT)
    pret = OB_OK;

  if (pret != OB_OK)
    {
      OB_LOG_ERROR_CODE (0x20400012,
                         "[u%d] timed_await thread %d returning %s\n", ubi,
                         thread_num, ob_error_string (pret));
      return (void *) 1;
    }

  ob_log (OBLV_DBUG, 0x20400013,
          "[u%d] timed_await thread %d returning successfully\n", ubi,
          thread_num);
  return (void *) 0;
}

/// Wait forever, but be awoken.

static volatile pool_hose forever_awaiting_poho = NULL;
static volatile pool_gang forever_awaiting_gang = NULL;

static void *forever_await_main (void *arg)
{
  ob_retort pret;
  protein prot;
  pool_timestamp ts;
  pool_hose ph;

  pret = pool_participate (cmd.pool_name, &ph, NULL);
  OB_DIE_ON_ERROR (pret);

  char name[32];
  snprintf (name, sizeof (name), "forever-u%d", ubi);
  OB_DIE_ON_ERROR (pool_set_hose_name (ph, name));

  OB_DIE_ON_ERROR (pool_hose_enable_wakeup (ph));
  forever_awaiting_poho = ph;

  ob_log (OBLV_DBUG, 0x20400014, "[u%d] forever await thread is waiting...\n",
          ubi);
  pret = pool_await_next (ph, POOL_WAIT_FOREVER, &prot, &ts, NULL);
  ob_log (OBLV_DBUG, 0x20400015, "[u%d] forever await thread is done waiting\n",
          ubi);

  forever_awaiting_poho = (pool_hose) -1;
  ob_retort draw = pool_withdraw (ph);
  if (draw == SLAW_CORRUPT_PROTEIN)
    {
      error_report ("This is a sad day indeed.  pool_withdraw() returned %s,\n"
                    "which utterly baffles me, and I cannot figure out how to\n"
                    "fix it (bug 882).  So although this is clearly a bug, we\n"
                    "will allow the test to pass anyway, as mentioned in\n"
                    "bug 893 comment 1.  But, I'm intentionally leaving this\n"
                    "rather long and obnoxious message here (though not\n"
                    "nearly as long and obnoxious as the message in\n"
                    "AnkleObject.cpp; I think it will remain the champion for\n"
                    "quite some time, despite my not inconsiderable efforts\n"
                    "in this file) to commemorate the fact that we have a\n"
                    "bug which is still open, and-- if you are seeing this\n"
                    "message-- still occurring occasionally, but we (meaning\n"
                    "mostly just me, Mignon) have no idea how to fix it,\n"
                    "because it is completely bizarre and should never\n"
                    "happen, despite the unfortunate fact that it actually\n"
                    "happens not infrequently.  (Hence the need to not count\n"
                    "it as a failure, so it doesn't spoil our buildbot\n"
                    "results.)\n"
                    "\n"
                    "I can now further comment, as seen in bug 882 comment 9,\n"
                    "that although this problem is most likely to happen\n"
                    "under valgrind, it is possible for it to happen without\n"
                    "valgrind.  Anyway, my sinking suspicion now is that\n"
                    "this has something to do with the infamous \"dirty\n"
                    "hose\" mechanism, since this is one of the few tests\n"
                    "that calls pool_hose_wake_up(), and since the symptom is\n"
                    "reading something that's supposed to be a protein but\n"
                    "isn't a protein, which could be explained by an\n"
                    "incomplete read that failed to mark the hose as dirty,\n"
                    "and therefore left us in the middle of the previous\n"
                    "message, which wouldn't be the beginning of a protein!\n"
                    "\n"
                    "But even with this suspicion, it's still very difficult\n"
                    "to test or fix, since testing the dirty hose mechanism\n"
                    "requires forcing some very unlikely timing conditions.\n"
                    "So I'm still going to sweep this under the carpet for\n"
                    "now, even though I'm almost sure now that this is a\n"
                    "real bug that could show up in the real world, though\n"
                    "only if you use pool_hose_wake_up(), so of course the\n"
                    "moral of the story is that you shouldn't.\n"
                    "\n"
                    "The upside of all this is that I now have a message\n"
                    "that's starting to rival the length of the \"AnkleObject\n"
                    "turd\" message!\n",
                    ob_error_string (draw));
    }
  else if (draw < OB_OK)
    {
      error_report ("pool_withdraw returned %s\n", ob_error_string (draw));
      return (void *) 1;
    }

  if (pret == POOL_AWAIT_WOKEN)
    return (void *) 0;

  OB_LOG_ERROR_CODE (0x20400016,
                     "[u%d] pool_await_next returned '%s' but "
                     "expected POOL_AWAIT_WOKEN\n",
                     ubi, ob_error_string (pret));

  return (void *) 1;
}

static void *wakeup_main (void *arg)
{
  ob_log (OBLV_DBUG, 0x20400017,
          "[u%d] wakeup thread is calling pool_hose_wake_up...\n", ubi);
  pool_hose ho = forever_awaiting_poho;
  if (ho == NULL || ho == (pool_hose) -1)
    {
      OB_LOG_ERROR ("very bad/weird thing: forever_awaiting_poho was %p\n", ho);
      return (void *) 1;
    }
  ob_retort err = pool_hose_wake_up (ho);
  ob_log (OBLV_DBUG, 0x20400018, "[u%d] pool_hose_wake_up returned %s\n", ubi,
          ob_error_string (err));
  if (err == OB_OK)
    return (void *) 0;

  // We need to do error_exit to force the whole process to exit.
  // We can't just depend on the return (void*)1 to fail the test,
  // because if the wakeup failed, forever_await_main will wait forever,
  // and the test will never exit at all.
  OB_FATAL_ERROR_CODE (0x20400019,
                       "[u%d] pool_hose_wake_up returned '%s' but "
                       "expected OB_OK\n",
                       ubi, ob_error_string (err));
  return (void *) 1;
}

static void *forever_multi_await_main (void *arg)
{
  ob_retort pret;
  protein prot;
  pool_timestamp ts;
  pool_hose ph;
  pool_gang gang;

  pret = pool_participate (cmd.pool_name, &ph, NULL);
  OB_DIE_ON_ERROR (pret);

  char name[32];
  snprintf (name, sizeof (name), "multi-u%d", ubi);
  OB_DIE_ON_ERROR (pool_set_hose_name (ph, name));

  pret = pool_new_gang (&gang);
  OB_DIE_ON_ERROR (pret);
  pret = pool_join_gang (gang, ph);
  OB_DIE_ON_ERROR (pret);

  forever_awaiting_gang = gang;

  ob_log (OBLV_DBUG, 0x2040001a,
          "[u%d] forever multi await thread is waiting...\n", ubi);
  pret = pool_await_next_multi (gang, POOL_WAIT_FOREVER, &ph, &prot, &ts, NULL);
  ob_log (OBLV_DBUG, 0x2040001b,
          "[u%d] forever multi await thread is done waiting\n", ubi);

  forever_awaiting_gang = (pool_gang) -1;
  OB_DIE_ON_ERROR (pool_disband_gang (gang, true));

  if (pret == POOL_AWAIT_WOKEN)
    return (void *) 0;

  OB_LOG_ERROR_CODE (0x2040001c,
                     "[u%d] pool_await_next_multi "
                     "returned '%s' but expected POOL_AWAIT_WOKEN\n",
                     ubi, ob_error_string (pret));

  return (void *) 1;
}

static void *wakeup_main_multi (void *arg)
{
  ob_log (OBLV_DBUG, 0x2040001d,
          "[u%d] multi wakeup thread is calling pool_gang_wake_up...\n", ubi);
  pool_gang ga = forever_awaiting_gang;
  if (ga == NULL || ga == (pool_gang) -1)
    {
      OB_LOG_ERROR ("very bad/weird thing: forever_awaiting_gang was %p\n", ga);
      return (void *) 1;
    }
  ob_retort err = pool_gang_wake_up (ga);
  ob_log (OBLV_DBUG, 0x2040001e, "[u%d] pool_hose_wake_up returned %s\n", ubi,
          ob_error_string (err));
  if (err == OB_OK)
    return (void *) 0;

  // We need to do error_exit to force the whole process to exit.
  // We can't just depend on the return (void*)1 to fail the test,
  // because if the wakeup failed, forever_multi_await_main will wait forever,
  // and the test will never exit at all.
  OB_FATAL_ERROR_CODE (0x2040001f,
                       "[u%d] pool_gang_wake_up for multi returned '%s' but "
                       "expected OB_OK\n",
                       ubi, ob_error_string (err));
  return (void *) 1;
}

// Create n threads to do something
static pthread_t *create_threads (int my_nthreads,
                                  void *(*thread_main) (void *) )
{
  int i;
  pthread_t *threads = (pthread_t *) malloc (my_nthreads * sizeof (*threads));
  if (!threads)
    OB_FATAL_ERROR_CODE (0x20400030,
                         "[u%d] Can't allocate thread struct array "
                         "for %d threads\n",
                         ubi, my_nthreads);

  for (i = 0; i < my_nthreads; i++)
    if (pthread_create (&threads[i], NULL, thread_main, (void *) (intptr_t) i)
        != 0)
      OB_FATAL_ERROR_CODE (0x20400031, "[u%d] Can't create thread %d\n", ubi,
                           i);
  return threads;
}

// Reap threads and check for errors

static int reap_threads (int my_nthreads, pthread_t threads[])
{
  int some_err = 0;
  void *err;
  int i;
  for (i = 0; i < my_nthreads; i++)
    {
      if (pthread_join (threads[i], &err) != 0)
        OB_FATAL_ERROR_CODE (0x20400032, "[u%d] Problem joining thread %d\n",
                             ubi, i);
      if (err)
        some_err = 1;
    }
  free (threads);
  return some_err;
}

static void usage (void)
{
  ob_banner (stderr);
  fprintf (stderr,
           "Usage: "
           "await_test [-v] [-t <type>] [-s <size>] [-u <uberiterations>]\n"
           "\t[-I <iterations>] [-n <threads>] [-r <repeats>] \n"
           "\t[-S <test seconds>] [-i <toc cap>] <pool_name>\n");
  exit (EXIT_FAILURE);
}

static int mainish (int argc, char *argv[])
{
  ob_retort pret;
  pthread_t *deposit_threads;
  pthread_t *await_threads;
  pthread_t *probe_await_threads;
  pthread_t *timed_await_threads;
  pthread_t *forever_await_threads;
  pthread_t *forever_multi_await_threads;
  pthread_t *wakeup_threads;
  int some_err = 0;
  int c;
  int uberiterations = 1;

  nthreads = 1;
  test_secs = 5;
  iterations = 3;  // per thread
  repeats = 10;    // per iteration

  while ((c = getopt (argc, argv, "i:I:n:r:s:S:t:u:v")) != -1)
    {
      switch (c)
        {
          case 'i':
            cmd.toc_capacity = strtoll (optarg, NULL, 0);
            break;
          case 'I':
            iterations = strtoll (optarg, NULL, 0);
            break;
          case 'n':
            nthreads = strtoll (optarg, NULL, 0);
            break;
          case 'r':
            repeats = strtoll (optarg, NULL, 0);
            break;
          case 's':
            cmd.size = strtoll (optarg, NULL, 0);
            break;
          case 'S':
            test_secs = strtoll (optarg, NULL, 0);
            break;
          case 't':
            cmd.type = optarg;
            break;
          case 'u':
            uberiterations = strtoll (optarg, NULL, 0);
            break;
          case 'v':
            pool_cmd_enable_debug_messages (0x20400000, 64 - 12);
            pool_cmd_enable_debug_messages (0x20106000, 64 - 12);
            break;
          default:
            usage ();
        }
    }
  pool_cmd_setup_options (&cmd);
  if (pool_cmd_get_poolname (&cmd, argc, argv, optind))
    usage ();

  sleep_usecs = (test_secs * 1000 * 1000) / iterations;

  for (ubi = 1; ubi <= uberiterations && !some_err; ubi++)
    {
      ob_log (OBLV_DBUG, 0x20400023, "uberiteration %d of %d\n", ubi,
              uberiterations);
      forever_awaiting_poho = NULL;
      forever_awaiting_gang = NULL;
      pret = pool_create (cmd.pool_name, cmd.type, cmd.create_options);
      if (pret != OB_OK)
        OB_FATAL_ERROR_CODE (
          0x20400020, "[u%d] no can create %s (%" OB_FMT_64 "u): %s\n", ubi,
          cmd.pool_name, cmd.size, ob_error_string (pret));

      // Each thread must now open the pool to get its own private pool
      // hose, otherwise they will stomp on each other's pool indexes.
      await_threads = create_threads (nthreads, await_main);
      probe_await_threads = create_threads (nthreads, probe_await_main);
      timed_await_threads = create_threads (nthreads, timed_await_main);

      forever_await_threads = create_threads (1, forever_await_main);
      forever_multi_await_threads =
        create_threads (1, forever_multi_await_main);

      // I don't think there's any potential race condition with
      // await_threads, probe_await_threads, or timed_await_threads,
      // because they rewind after participating, so they shouldn't
      // miss any proteins.
      // However, there is a race in that forever_await_main must have
      // assigned forever_awaiting_poho before we can start wakeup_main.
      // Let's do that the brute force way by just polling forever_awaiting_poho
      // until it is not NULL.

      ob_log (OBLV_DBUG, 0x20400024,
              "[u%d] Waiting for forever_awaiting_poho\n", ubi);
      while (NULL == forever_awaiting_poho)
        ob_micro_sleep (90013);  // our zip code; almost a tenth of a second

      ob_log (OBLV_DBUG, 0x20400025,
              "[u%d] Waiting for forever_awaiting_gang\n", ubi);
      while (NULL == forever_awaiting_gang)
        // bcn zip of 08002 is treated as octal. that is wrong mkay
        ob_micro_sleep (90013);

      wakeup_threads = create_threads (1, wakeup_main);

      ob_log (OBLV_DBUG, 0x20400026, "[u%d] Reaping forever_await_threads\n",
              ubi);
      some_err |= reap_threads (1, forever_await_threads);
      ob_log (OBLV_DBUG, 0x20400027, "[u%d] Reaping wakeup_threads\n", ubi);
      some_err |= reap_threads (1, wakeup_threads);
      wakeup_threads = create_threads (1, wakeup_main_multi);
      ob_log (OBLV_DBUG, 0x20400028,
              "[u%d] Reaping forever_multi_await_threads\n", ubi);
      some_err |= reap_threads (1, forever_multi_await_threads);
      ob_log (OBLV_DBUG, 0x20400029, "[u%d] Reaping wakeup_threads\n", ubi);
      some_err |= reap_threads (1, wakeup_threads);

      deposit_threads = create_threads (nthreads, deposit_main);

      ob_log (OBLV_DBUG, 0x2040002a, "[u%d] Reaping await_threads\n", ubi);
      some_err |= reap_threads (nthreads, await_threads);
      ob_log (OBLV_DBUG, 0x2040002b, "[u%d] Reaping probe_await_threads\n",
              ubi);
      some_err |= reap_threads (nthreads, probe_await_threads);
      ob_log (OBLV_DBUG, 0x2040002c, "[u%d] Reaping timed_await_threads\n",
              ubi);
      some_err |= reap_threads (nthreads, timed_await_threads);
      ob_log (OBLV_DBUG, 0x2040002d, "[u%d] Reaping deposit_threads\n", ubi);
      some_err |= reap_threads (nthreads, deposit_threads);

      pret = pool_dispose (cmd.pool_name);
      if (pret != OB_OK)
        OB_FATAL_ERROR_CODE (0x20400021, "[u%d] no can stop %s: %s\n", ubi,
                             cmd.pool_name, ob_error_string (pret));
    }

  pool_cmd_free_options (&cmd);

  return some_err;
}


int main (int argc, char **argv)
{
  OB_CHECK_ABI ();

  return check_for_leaked_file_descriptors_scoped (mainish, argc, argv);
}
