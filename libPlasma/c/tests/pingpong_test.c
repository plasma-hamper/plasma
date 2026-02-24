
/* (c)  oblong industries */

///
/// Pass the buck - token-passing test with multiple threads and
/// pools.  Designed to test pool_multi_await_next().
///
/// Create N pools and N threads.  Each thread opens M of the pools.
/// When a thread reads the "buck" protein from a pool, it deposits
/// the buck into the next pool.
///

#include "libLoam/c/ob-sys.h"
#include "pool_cmd.h"
#include "libPlasma/c/slaw-string.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-vers.h"
#include "libLoam/c/ob-time.h"
#include "libLoam/c/ob-rand.h"
#include "libLoam/c/ob-pthread.h"
#include "libPlasma/c/protein.h"
#include "libPlasma/c/slaw.h"

static int test_secs;
static char **pool_names;
static int pool_count;
// Must be at least one
static int pools_to_join = 1;

// Make this global so all threads can read it and do participate
static pool_cmd_info cmd;

// Having a single "running" variable leads to an interesting race
// condition where threads that don't start by the time the test is
// over get stuck forever.  This happens, e.g., during debugging. :)
static int start = 0;
static int stop = 0;

// Thread state struct - various per-thread variables needed

struct thread_info
{
  // Pthread struct
  pthread_t pthread;
  // Which thread number we are
  int thread_num;
  // Number of bucks passed
  unt64 count;
  // Is this thread all set up and ready to run yet?
  int ready;
  // Pthread return values are super painful, put ours here
  int ret;
};

static void wait_for_start (struct thread_info *ti)
{
  ob_log (OBLV_DBUG, 0x2040b000, "waiting for start...\n");

  // Tell main thread we're ready
  ti->ready = 1;
  // Might get EINTR
  while (start != 1)
    usleep (10000);

  ob_log (OBLV_DBUG, 0x2040b001, "starting\n");
}

// We get the list of pools on the command line, but each thread needs
// to join separately (as pool_hoses are not thread-safe [and faster
// for it]).
//
// The pool list is available as a global variable in argv[] format,
// as is the number of pools.  Don't write to the global variables or
// you'll screw up the other threads.
//
// XXX We don't join all pools because we quickly run out of (a) file
// descriptors, (b) SEM_UNDO structs.  Instead we join pools_to_join,
// which is 1 by default at minimum, so we can listen to the pool
// where our buck will arrive.

static pool_gang join_pools (int thread_num)
{
  ob_retort pret;
  pool_gang gang;
  int i;

  ob_log (OBLV_DBUG, 0x2040b002, "%d: joining %d pools\n", thread_num,
          pools_to_join);

  pret = pool_new_gang (&gang);
  OB_DIE_ON_ERROR (pret);

  // Thread number n listens to pool n and the following pools_to_join
  // - 1 pools.  This necessitates annoying modulo arithmatic.
  for (i = 0; i < pools_to_join; i++)
    {
      pool_hose new_ph;
      int pool_num = (thread_num + i) % pool_count;
      char *poolName = pool_names[pool_num];
      ob_log (OBLV_DBUG, 0x2040b003, "%d: adding pool %s\n", thread_num,
              poolName);
      pret = pool_participate_creatingly (poolName, cmd.type, &new_ph,
                                          cmd.create_options);
      if ((pret != OB_OK) && (pret != POOL_CREATED))
        {
          pool_disband_gang (gang, true);
          OB_FATAL_ERROR_CODE (0x2040b004,
                               "pool_participate_creatingly failed %s\n",
                               ob_error_string (pret));
        }
      pret = pool_join_gang (gang, new_ph);
      if (pret != OB_OK)
        {
          pool_disband_gang (gang, true);
          OB_FATAL_ERROR_CODE (0x2040b005, "Pool %s failed to join gang: %s\n",
                               poolName, ob_error_string (pret));
        }
    }

  ob_log (OBLV_DBUG, 0x2040b006, "%d: joined\n", thread_num);

  return gang;
}

static protein create_buck_protein (int thread_num)
{
  // When we get the buck, we pass it on in the form of a protein
  // whose descrip is the thread number of the recipient.  Create that
  // protein.

  slaw desc = slaw_string_format ("%d", thread_num);
  // No ingests - we want to measure the pure transactional cost of
  // passing around proteins via pool_await_next_multi().  Hopefully
  // nothing will barf.
  return protein_from_ff (slaw_list_inline_f (desc, NULL), NULL);
}

static void *pingpong_main (void *arg)
{
  struct thread_info *ti = (struct thread_info *) arg;
  ob_retort pret;

  pool_gang gang = join_pools (ti->thread_num);

  // Pass the buck.  Each thread reads from all the pools.  When it
  // gets a protein with its thread number, it passes (deposits) the
  // buck (a protein with the recipient's thread number) to the next
  // thread in the chain.
  protein my_buck = create_buck_protein (ti->thread_num);
  int next_thread = (ti->thread_num + 1) % pool_count;
  protein buck_to_pass = create_buck_protein (next_thread);

  // The buck is deposited in the nth pool, where n = thread number of
  // the next thread.
  pool_hose deposit_ph;
  // This pool may not have been created yet if we didn't join it
  // previously in join_pools, so be sure to use creatingly.
  pret = pool_participate_creatingly (pool_names[next_thread], cmd.type,
                                      &deposit_ph, cmd.create_options);
  if ((pret != OB_OK) && (pret != POOL_CREATED))
    {
      pool_disband_gang (gang, true);
      OB_FATAL_ERROR_CODE (0x2040b007, "participate in pool %s failed\n",
                           pool_names[next_thread]);
    }

  wait_for_start (ti);

  // Someone's got to start passing the buck... Wait until after the
  // start, or our partner may open the pool after we deposit and end
  // up with a pool index past the buck protein =><= hang.

  unt64 bucks_passed = 0;
  if (ti->thread_num == 0)
    {
      ob_log (OBLV_DBUG, 0x2040b008, "starting the buck\n");
      pret = pool_deposit (deposit_ph, buck_to_pass, NULL);
      if (pret != OB_OK)
        {
          pool_disband_gang (gang, true);
          OB_FATAL_ERROR_CODE (0x2040b009, "pool_deposit() failed\n");
        }
      bucks_passed++;
    }

  // Wait for the buck to come to us and then pass it on until we get
  // the stop signal.
  pool_hose read_ph;
  protein read_prot;
  pool_timestamp ts;
  unt64 i;

  for (i = 0; stop != 1; i++)
    {
      ob_log (OBLV_DBUG, 0x2040b00a, "pool_await_next_multi\n");
      pret = pool_await_next_multi (gang, POOL_WAIT_FOREVER, &read_ph,
                                    &read_prot, &ts, NULL);
      if (pret != OB_OK)
        {
          pool_disband_gang (gang, true);
          OB_FATAL_ERROR_CODE (0x2040b00b,
                               "pool_await_next_multi() failed: %s\n",
                               ob_error_string (pret));
        }
      // Did we get the buck?
      if (proteins_equal (my_buck, read_prot))
        {
          // Pass it!
          ob_log (OBLV_DBUG, 0x2040b00c,
                  "got the buck from pool %s, passing to %u\n",
                  pool_name (read_ph), next_thread);
          pret = pool_deposit (deposit_ph, buck_to_pass, NULL);
          if (pret != OB_OK)
            {
              pool_disband_gang (gang, true);
              OB_FATAL_ERROR_CODE (0x2040b00d, "pool_deposit() failed\n");
            }
          ob_log (OBLV_DBUG, 0x2040b00e, "passed buck to %d via pool %s\n",
                  next_thread, pool_name (deposit_ph));
          bucks_passed++;
        }
      else
        {
          ob_log (OBLV_DBUG, 0x2040b00f,
                  "protein from pool %s is not my buck\n", pool_name (read_ph));
        }
      protein_free (read_prot);
    }

  // Send one last protein to wake up the other thread if it's sleeping
  pret = pool_deposit (deposit_ph, buck_to_pass, NULL);
  OB_DIE_ON_ERROR (pret);

  ti->count = bucks_passed;
  protein_free (my_buck);
  protein_free (buck_to_pass);
  OB_DIE_ON_ERROR (pool_withdraw (deposit_ph));
  OB_DIE_ON_ERROR (pool_disband_gang (gang, true));

  ob_log (OBLV_DBUG, 0x2040b010, "returning %" OB_FMT_64 "u bucks passed\n",
          ti->count);
  return NULL;
}

// XXX cut n paste from await_test.c

// Create n threads to do something
static struct thread_info *create_threads (int nthreads,
                                           void *(*thread_main) (void *) )
{
  int i;
  struct thread_info *threads =
    (struct thread_info *) malloc (nthreads * sizeof (*threads));
  struct thread_info *ti;
  if (!threads)
    {
      fprintf (stderr, "Can't allocate thread info array for %d threads\n",
               nthreads);
      exit (1);
    }

  for (i = 0; i < nthreads; i++)
    {
      ti = &threads[i];
      ti->thread_num = i;
      ti->ready = 0; /* initialize before wait_for_children tries to read it */
      ti->ret = 0;   /* I don't think anyone else assigns this, so we should */
      if (pthread_create (&ti->pthread, NULL, thread_main, (void *) ti) != 0)
        {
          fprintf (stderr, "Can't create thread %d\n", i);
          exit (1);
        }
    }
  return threads;
}

// Wait for n threads to be ready
static void wait_for_children (int nthreads, struct thread_info threads[])
{
  int i;
  struct thread_info *ti;

  for (i = 0; i < nthreads; i++)
    {
      ti = &threads[i];
      while (ti->ready == 0)
        usleep (10000);
      ob_log (OBLV_DBUG, 0x2040b011, "ready\n");
    }
  return;
}

// Reap threads and check for errors, protein counting version

static int reap_threads_count (int nthreads, struct thread_info threads[],
                               unt64 *total_count)
{
  struct thread_info *ti;
  int err = 0;
  int i;
  for (i = 0; i < nthreads; i++)
    {
      ti = &threads[i];
      if (pthread_join (ti->pthread, NULL) != 0)
        {
          fprintf (stderr, "Problem joining thread %d\n", i);
          exit (1);
        }
      if (ti->ret == -1)
        err = 1;
      else
        *total_count += ti->count;
    }
  free (threads);
  return err;
}

static ob_retort set_size (pool_hose ph, unt64 sz)
{
  slaw opts = slaw_map_inline_cf ("size", slaw_unt64 (sz), NULL);
  ob_retort tort = pool_change_options (ph, opts);
  slaw_free (opts);
  return tort;
}

static void test_resize (void)
{
  const int n = 25;
  const int usecs = 1000000 * test_secs / n;

  int i;
  for (i = 0; i < n; i++)
    {
      OB_DIE_ON_ERROR (ob_micro_sleep (usecs));
      const int pnum = ob_rand_int32 (0, pool_count);
      const int shift = ob_rand_int32 (-2, 3);  // -2 to +2, inclusive
      int64 newsize = cmd.size;
      if (shift > 0)
        newsize <<= shift;
      else if (shift < 0)
        newsize >>= abs (shift);
      const char *pname = pool_names[pnum];
      pool_hose ph = NULL;
      printf ("Going to resize '%s' to %" OB_FMT_64 "d\n", pname, newsize);
      OB_DIE_ON_ERROR (
        pool_participate_creatingly (pname, cmd.type, &ph, cmd.create_options));
      OB_DIE_ON_ERROR (set_size (ph, newsize));
      OB_DIE_ON_ERROR (pool_withdraw (ph));
    }
}

static void require_resizable (pool_cmd_info *pci)
{
  slaw re = slaw_map_inline_cf ("resizable", slaw_boolean (true), NULL);
  protein p = protein_from_ff (
    NULL, slaw_maps_merge (protein_ingests (pci->create_options), re, NULL));
  Free_Protein (pci->create_options);
  pci->create_options = p;
  slaw_free (re);
}

static void usage (void)
{
  fprintf (stderr, "Usage: pingpong_test [-t <type>] [-s <size>] [-j <# pools "
                   "to join> ]\n"
                   "\t[-S <test seconds>] [-i <toc cap>] [-r]\n"
                   "\t<pool_name1> <pool_name2> [<pool_nameN> ... ]\n");
  exit (1);
}

int main (int argc, char *argv[])
{
  OB_DIE_ON_ERROR (OB_CHECK_ABI ());

  struct thread_info *pingpong_threads;
  unt64 total_bucks = 0;
  int err = 0;
  int c;
  int i;
  bool resize = false;

  test_secs = 1;

  while ((c = getopt (argc, argv, "i:j:s:S:t:r")) != -1)
    {
      switch (c)
        {
          case 'i':
            cmd.toc_capacity = strtoll (optarg, NULL, 0);
            break;
          case 'j':
            pools_to_join = strtoll (optarg, NULL, 0);
            if (pools_to_join < 1)
              {
                fprintf (stderr, "Minimum pools to join is 1\n");
                exit (1);
              }
            break;
          case 's':
            cmd.size = strtoll (optarg, NULL, 0);
            break;
          case 'S':
            test_secs = strtoll (optarg, NULL, 0);
            if (test_secs < 1)
              {
                fprintf (stderr, "Minimum test seconds is 1\n");
                exit (1);
              }
            break;
          case 't':
            cmd.type = optarg;
            break;
          case 'r':
            resize = true;
            break;
          default:
            usage ();
        }
    }
  pool_cmd_setup_options (&cmd);
  if (resize)
    require_resizable (&cmd);

  // Post option arguments are pool names, which will be created if
  // they don't already exist.  If pools are to be created, they have
  // to all be the same type (one -t <type> argument accepted above in
  // pool_default_getopt()).

  // Need a minimum of two pools to play pingpong
  int first_pool_arg = optind;
  if ((argc - first_pool_arg) < 2)
    usage ();

  // Copy to global variables for use by various threads
  pool_count = argc - first_pool_arg;
  pool_names = &argv[first_pool_arg];

  for (i = 0; i < pool_count; i++)
    {
      ob_log (OBLV_DBUG, 0x2040b012, "pool %s\n", pool_names[i]);
      if (resize)
        {
          // let's get something in those pools to make it more challenging
          pool_hose ph;
          OB_DIE_ON_ERROR (pool_participate_creatingly (
            pool_names[i], cmd.type, &ph, cmd.create_options));
          // Fill pool almost full, but not quite
          // (So can't call pool_cmd_fill_pool(), which will wrap)
          const int goal = 7000;
          protein p = pool_cmd_create_test_protein ("first");
          int j;
          for (j = 0; j < goal; j++)
            OB_DIE_ON_ERROR (pool_deposit (ph, p, NULL));
          Free_Protein (p);
          OB_DIE_ON_ERROR (pool_withdraw (ph));
        }
    }

  pingpong_threads = create_threads (pool_count, pingpong_main);
  // Wait for all the threads to finish initializing
  wait_for_children (pool_count, pingpong_threads);

  // Give threads the go signal
  start = 1;
  if (resize)
    test_resize ();
  else
    sleep (test_secs);
  // Tell threads to stop
  stop = 1;
  // deposit a protein in all pools to wake up anyone sleeping in select()
  ob_log (OBLV_DBUG, 0x2040b013, "waking any remaining awaiters\n");
  protein wake_prot = pool_cmd_create_test_protein ("wake");
  for (i = 0; i < pools_to_join; i++)
    {
      pool_hose ph;
      char *poolName = pool_names[i];
      ob_retort pret = pool_participate_creatingly (poolName, cmd.type, &ph,
                                                    cmd.create_options);
      if ((pret != OB_OK) && (pret != POOL_CREATED))
        OB_FATAL_ERROR_CODE (0x2040b014, "couldn't participate in pool %s\n",
                             poolName);
      pret = pool_deposit (ph, wake_prot, NULL);
      if (pret != OB_OK)
        OB_FATAL_ERROR_CODE (0x2040b015, "couldn't deposit in pool %s\n",
                             poolName);
      OB_DIE_ON_ERROR (pool_withdraw (ph));
    }
  // Gather up the stats
  err |= reap_threads_count (pool_count, pingpong_threads, &total_bucks);

  if (err)
    printf ("ERROR!!!  Do not trust the stats!\n");

  printf ("%" OB_FMT_64 "u bucks passed/sec (%" OB_FMT_64 "u"
          " passes %d seconds)\n",
          total_bucks / test_secs, total_bucks, test_secs);

  protein_free (wake_prot);
  pool_cmd_free_options (&cmd);

  return err;
}
