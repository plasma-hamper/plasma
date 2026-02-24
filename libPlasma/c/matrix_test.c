
/* (c)  oblong industries */

///
/// Enormous multi-faceted test program for pools.  Spawns processes
/// to read or deposit to some number of pools, awaiting or not.  It
/// is the basis of many of our performance and correctness tests.
///

#include <errno.h>
#include <signal.h>
#include <fcntl.h>

#include "libLoam/c/ob-sys.h"
#include "libPlasma/c/pool_cmd.h"
#include "libLoam/c/ob-log.h"
#include "libPlasma/c/private/pool-ugly-callback.h"
#include "libLoam/c/ob-vers.h"
#include "libLoam/c/ob-string.h"
#include "libPlasma/c/protein.h"
#include "libPlasma/c/slaw.h"

static int test_secs = 1;
static const char *pool_prefix;

static int depositors = 1;
static int readers = 1;
static int total_children;

static int total_pools = 1;
static int pools_to_read_from = 1;
// Timeout for await, 0 means don't wait
static int await = POOL_NO_WAIT;
// Size of ingest in proteins to deposit
static int ingest_size = 1;
// Used to calculate total time each process ran
static struct timeval start_time;
// This is our desired end time, set before we fork children
static struct timeval goal_end_time;

static pool_cmd_info cmd;

// Each process has a unique number
static int forked_process_num = 0;

// Type is "parent", "reader", or "depositor"
static const char *forked_process_type = "parent";

// Global variables needed in signal handler
static unsigned long long forked_process_results = 0;

// flag flipped by the SIGALRM handler to exit the program
static volatile sig_atomic_t keep_on_chugging = 1;

// Join just one pool.  For the single-pool case, we want to avoid the
// overhead of the multi-pool infrastructure.

static void join_one_pool (pool_hose *ret_ph, int proc_num,
                           const char *process_type)
{
  ob_retort pret;
  pool_hose ph;
  size_t pn_size;
  char *poolName = (char *) malloc (pn_size = (strlen (pool_prefix) + 100));
  if (poolName == NULL)
    OB_FATAL_ERROR_CODE (0x20201000,
                         "Couldn't allocate a teensy-weensy bit of memory\n");
  int pool_num = proc_num % total_pools;
  snprintf (poolName, pn_size, "%s%d", pool_prefix, pool_num);
  ob_log (OBLV_DBUG, 0x20201001, "%s %d: participating in just one pool %s\n",
          process_type, proc_num, poolName);
  pret =
    pool_participate_creatingly (poolName, cmd.type, &ph, cmd.create_options);
  if (pret < 0)
    {
      if (pret == POOL_SERVER_BUSY)
        OB_FATAL_ERROR_CODE (0x20201002,
                             "Server for pool %s too busy (i. e. can't fork)\n",
                             poolName);
      else if (pret == POOL_SERVER_UNREACH)
        OB_FATAL_ERROR_CODE (0x20201003, "Can't get to server for pool %s\n",
                             poolName);
      else
        OB_FATAL_ERROR_CODE (0x20201004,
                             "pool_participate_creatingly in %s failed: %s\n",
                             poolName, ob_error_string (pret));
    }
  *ret_ph = ph;
  free (poolName);
}

// If we are doing multi-pool reads, join the specified number of
// pools, beginning with the one named by the proc_num.

static pool_gang join_pools (int proc_num, const char *process_type)
{
  ob_retort pret;
  pool_gang gang;
  int i;

  ob_log (OBLV_DBUG, 0x20201005, "%s %d: joining %d pools\n", process_type,
          proc_num, pools_to_read_from);

  pret = pool_new_gang (&gang);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x20201006, "Couldn't create gang: %s\n",
                         ob_error_string (pret));

  size_t pn_size;
  char *poolName = (char *) malloc (pn_size = (strlen (pool_prefix) + 100));
  if (poolName == NULL)
    OB_FATAL_ERROR_CODE (0x20201007,
                         "Couldn't allocate a teensy-weensy bit of memory\n");

  for (i = 0; i < pools_to_read_from; i++)
    {
      pool_hose new_ph;
      int pool_num = (proc_num + i) % total_pools;
      snprintf (poolName, pn_size, "%s%d", pool_prefix, pool_num);
      ob_log (OBLV_DBUG, 0x20201008, "%s %d: adding pool %s\n", process_type,
              proc_num, poolName);
      pret = pool_participate_creatingly (poolName, cmd.type, &new_ph,
                                          cmd.create_options);
      if (pret < 0)
        {
          pool_disband_gang (gang, true);
          OB_FATAL_ERROR_CODE (0x20201009,
                               "pool_participate_creatingly in %s failed: %s\n",
                               poolName, ob_error_string (pret));
        }
      pret = pool_join_gang (gang, new_ph);
      if (pret != OB_OK)
        {
          pool_disband_gang (gang, true);
          OB_FATAL_ERROR_CODE (0x2020100a, "Pool %s failed to join gang: %s\n",
                               poolName, ob_error_string (pret));
        }
    }

  ob_log (OBLV_DBUG, 0x2020100b, "%s %d: joined\n", process_type, proc_num);
  free (poolName);

  return gang;
}

static protein create_protein (int size)
{
  char *buffer = (char *) malloc (size);
  if (buffer == NULL)
    OB_FATAL_ERROR_CODE (0x2020100c, "Couldn't allocate %d bytes\n", size);

  int i;
  for (i = 0; i < size; i++)
    buffer[i] = 'a';
  // Null terminate
  buffer[size - 1] = '\0';

  slaw descrips = slaw_list_inline_c ("matrix_test", NULL);
  slaw ingests = slaw_map_inline_cc ("test_string", buffer, NULL);
  if (!descrips || !ingests)
    OB_FATAL_ERROR_CODE (0x2020100d, "Allocation failure\n");
  protein prot = protein_from_ff (descrips, ingests);
  free (buffer);

  return prot;
}

// Print out our stats

static void report_results (int proc_num, const char *process_type,
                            unsigned long long num_results,
                            struct timeval end_time)
{
  char buf[100];
  unsigned long long total_usecs =
    (end_time.tv_sec - start_time.tv_sec) * 1000000ULL;
  total_usecs += end_time.tv_usec - start_time.tv_usec;
  unsigned long long results_per_sec = (num_results * 1000000ULL) / total_usecs;

  if (cmd.verbose)
    {
      char buf2[32];
      size_t n;
      float64 secs = total_usecs;
      secs *= 1e-6;
      ob_safe_copy_string (buf, sizeof (buf), "p/sec ");
      buf2[0] = 0;
      n = ob_safe_append_int64 (buf2, sizeof (buf2), (int64) results_per_sec);
      while (n++ < 10)
        ob_safe_append_string (buf, sizeof (buf), " ");
      ob_safe_append_string (buf, sizeof (buf), buf2);
      ob_safe_append_string (buf, sizeof (buf), " (");
      buf2[0] = 0;
      n = ob_safe_append_int64 (buf2, sizeof (buf2), (int64) num_results);
      while (n++ < 10)
        ob_safe_append_string (buf, sizeof (buf), " ");
      ob_safe_append_string (buf, sizeof (buf), buf2);
      ob_safe_append_string (buf, sizeof (buf), "/");
      ob_safe_append_float64 (buf, sizeof (buf), secs, 6);
      const size_t len = ob_safe_append_string (buf, sizeof (buf), ")\n");
      ob_ignore (write (2, buf, len));
    }

  // avoid using stdio because it is not signal-safe:
  ob_safe_copy_string (buf, sizeof (buf), process_type);
  ob_safe_append_string (buf, sizeof (buf), " ");
  ob_safe_append_int64 (buf, sizeof (buf), (int64) results_per_sec);
  const size_t len = ob_safe_append_string (buf, sizeof (buf), "\n");
  ob_ignore (write (1, buf, len));
}

static bool is_time_to_quit (void)
{
  struct timeval now_time;
  gettimeofday (&now_time, NULL);
  long long remaining_usecs =
    (goal_end_time.tv_sec - now_time.tv_sec) * 1000000ULL;
  remaining_usecs += goal_end_time.tv_usec - now_time.tv_usec;
  if (remaining_usecs < 0)
    return true;

  return false;
}

static void set_flag_for_exit (int signo)
{
  /* Posix says gettimeofday is not async-signal-safe, but it's a syscall on darwin and linux */
  if (is_time_to_quit ())
    keep_on_chugging = 0;
}

#ifndef _MSC_VER

// on unix/osx, we use a signal to end the reader/depositor processes
// Set a timer to expire when the test is over.  Timers are not
// inherited across a fork() so this must be done in the child.
// 6/2016 Optionally use the timer to pester the app with signals
// and interrupt syscalls; the signal handler will check whether
// it's time to quit.

static void set_timer (void)
{
  struct itimerval iv = {};
#if 0
  /* how long we actually want to run */
  iv.it_value.tv_sec = test_secs;
#else
  /* Signal handler will check how long we actually want to run itself;
   * let's send LOTS of signals to trigger any bugs related to
   * interrupted signals calls (especially during connect).
   * 4 ms here usually causes a problem in several children in a
   * one second test on a macbook pro, so let's set it to 3ms to
   * balance fault injection with performance.
   * Note: on the Mac, at least some of the problems this solves aren't treated
   * as fatal by stress_tests.sh, you have to look at the log for e.g.
   * 52: warning: 82214: Jun 3, 2016 20:26:08.21 <20108000> send() returned -1 with errno 'Socket is not connected' with 88 bytes left
   * 52: info: 82214: Jun 3, 2016 20:26:08.21 <20108005> When connecting to 'tcpo://localhost/test_pool3', got POOL_SEND_BADTH on try 1 of 3
   */
  iv.it_value.tv_usec = 3000;
  iv.it_interval.tv_usec = 3000;
#endif
  if (setitimer (ITIMER_REAL, &iv, NULL) < 0)
    {
      OB_PERROR_CODE (0x20201029, "setitimer");
      exit (1);
    }

  // If the goal end time has already passed, exit immediately.  Don't
  // bother reporting out - it slows down the threads that actually
  // have something to report.
  if (is_time_to_quit ())
    exit (0);
}
#endif

static unsigned int read_proteins (int proc_num,
                                   unsigned long long *output_results,
                                   struct timeval *output_result_time)
{
  pool_hose ph;
  pool_gang gang = NULL;
  // We want to be able to compare the simpler single-pool operations
  // with the multi-pool operations, so only use multi-pool
  // infrastructure if we have more than one pool.
  if (pools_to_read_from == 1)
    join_one_pool (&ph, proc_num, "reader");
  else
    gang = join_pools (proc_num, "reader");

  while (keep_on_chugging)
    {
      ob_retort pret;
      pool_timestamp ts;
      protein read_prot = NULL;
      if (pools_to_read_from == 1)
        pret = pool_await_next (ph, await, &read_prot, &ts, NULL);
      else
        pret = pool_await_next_multi (gang, await, &ph, &read_prot, &ts, NULL);
      // If we're not awaiting, we'll run out of proteins every so
      // often.  This is fine, just rewind and keep going.
      if (((await == POOL_NO_WAIT || await == POOL_WAIT_FOREVER)
           && pret == POOL_NO_SUCH_PROTEIN)
          || (await == POOL_NO_WAIT && pret == POOL_AWAIT_TIMEDOUT))
        {
          if (pools_to_read_from == 1)
            {
              ob_retort tt = pool_rewind (ph);
              if (tt < OB_OK)
                pool_withdraw (ph);
              OB_DIE_ON_ERR_CODE (0x20201020, tt);
            }
          else
            {
              int64 i;
              const int64 n = pool_gang_count (gang);
              for (i = 0; i < n; i++)
                {
                  ob_retort tt = pool_rewind (pool_gang_nth (gang, i));
                  if (tt < OB_OK)
                    pool_disband_gang (gang, true);
                  OB_DIE_ON_ERR_CODE (0x20201021, tt);
                }
            }
          continue;
        }
      // Otherwise, we shouldn't have an error
      if (pret != OB_OK)
        {
          if (pools_to_read_from == 1)
            pool_withdraw (ph);
          else
            pool_disband_gang (gang, true);

          OB_FATAL_ERROR_CODE (0x20201012,
                               "%s: await_next returned (with timeout %d) %s\n",
                               pool_name (ph), await, ob_error_string (pret));
        }
      else if (read_prot == NULL)
        {
          if (pools_to_read_from == 1)
            pool_withdraw (ph);
          else
            pool_disband_gang (gang, true);

          error_exit ("%s: await_next returned read_prot == NULL\n",
                      pool_name (ph));
        }
      protein_free (read_prot);

      //if ((results % 1000) == 0)
      //ob_log (OBLV_DBUG, 0x20201013, "r");

      *output_results = *output_results + 1;
      gettimeofday (output_result_time, NULL);

#ifdef _MSC_VER
      if (is_time_to_quit ())
        return 0;
#endif
    }


  if (pools_to_read_from == 1)
    pool_withdraw (ph);
  else
    pool_disband_gang (gang, true);


  struct timeval end_time;
  gettimeofday (&end_time, NULL);
  report_results (proc_num, "reader", *output_results, end_time);
  _exit (0);

  //not reached
  return 1;
}

static unsigned int deposit_proteins (int proc_num,
                                      unsigned long long *output_results,
                                      struct timeval *output_result_time)
{
  protein deposit_prot = create_protein (ingest_size);
  pool_hose ph;
  join_one_pool (&ph, proc_num, "depositor");

  while (keep_on_chugging)
    {
      ob_retort pret = pool_deposit (ph, deposit_prot, NULL);
      if (pret != OB_OK)
        OB_FATAL_ERROR_CODE (0x20201014, "%s: pool_deposit returned %s\n",
                             pool_name (ph), ob_error_string (pret));
      //if ((results % 1000) == 0)
      //ob_log (OBLV_DBUG, 0x20201015, "d");

      *output_results = *output_results + 1;
      gettimeofday (output_result_time, NULL);

#ifdef _MSC_VER
      if (is_time_to_quit ())
        return 0;
#endif
    }
  struct timeval end_time;
  gettimeofday (&end_time, NULL);

  report_results (proc_num, "depositor", *output_results, end_time);
  _exit (0);

  //not reached
  return 1;
}

#ifdef _MSC_VER

/// on Windows we tracks stats and result time per-thread

typedef struct thread_data_s
{
  bool is_depositor;
  int thread_num;
  HANDLE thread_handle;
  unsigned long long thread_results;
  struct timeval thread_result_time;
  struct thread_data_s *next;
} thread_data;

struct thread_data *g_thread_list = 0;

void free_thread_list (thread_data *l)
{
  if (l == NULL)
    return;

  free_thread_list (l->next);
  free (l);
}

unsigned int __stdcall child_depositor_thread (void *args)
{
  thread_data *l = (thread_data *) args;

  return deposit_proteins (l->thread_num, &l->thread_results,
                           &l->thread_result_time);
}

unsigned int __stdcall child_reader_thread (void *args)
{
  thread_data *l = (thread_data *) args;

  return read_proteins (l->thread_num, &l->thread_results,
                        &l->thread_result_time);
}

#endif

// An alternative to the timer scheme is to save all the child pids
// and signal them (using any of the signal-all-processes schemes
// frequently has unexpected results, esp. in the "make check"
// environment).  Instead, wait for them and collect results.
static void create_children (void)
{
  int i;

#ifdef _MSC_VER

  int max = depositors > readers ? depositors : readers;
  int proc_num = 0;

  for (i = 0; i < max; i++)
    {
      // Start readers and depositors alternately; with large numbers
      // of processes the last processes won't start until the test is
      // already over, and we would like some of each.
      proc_num++;
      if ((i < depositors) && (!is_time_to_quit ()))
        {
          thread_data *d = (thread_data *) malloc (sizeof (thread_data));
          d->thread_num = proc_num;
          d->thread_results = 0;
          d->is_depositor = true;

          d->thread_handle = (HANDLE) _beginthreadex (
            NULL, 0, child_depositor_thread, (void *) d, 0, NULL);

          if (d->thread_handle == 0)
            {
              OB_PERROR_CODE (0x2020102a, "beginthreadex");
              return;
            }

          d->next = g_thread_list;
          g_thread_list = d;
        }

      if ((i < readers) && (!is_time_to_quit ()))
        {
          thread_data *d = (thread_data *) malloc (sizeof (thread_data));
          d->thread_num = proc_num;
          d->thread_results = 0;
          d->is_depositor = false;

          d->thread_handle = (HANDLE) _beginthreadex (
            NULL, 0, child_reader_thread, (void *) d, 0, NULL);

          if (d->thread_handle == 0)
            {
              OB_PERROR_CODE (0x2020102b, "beginthreadex");
              return;
            }

          d->next = g_thread_list;
          g_thread_list = d;
        }
    }

#else

  // Set up the signal handler for the children's quit signal
  struct sigaction act;
  memset (&act, 0, sizeof (act));
  act.sa_handler = set_flag_for_exit;
  sigaction (SIGALRM, &act, NULL);

  /* SIGPIPE can pop up from time to time and cause nondeterministic
   * failures.  To avoid these failures, let's ignore SIGPIPE.
   * XXX: Shouldn't really do this here; it should be libPlasma-wide.
   * Unfortunately, ignoring SIGPIPE in a multithreaded library is tricky:
   * http://archives.postgresql.org/pgsql-patches/2003-11/msg00256.php
   */
  act.sa_handler = SIG_IGN;
  if (sigaction (SIGPIPE, &act, NULL) != 0)
    OB_PERROR_CODE (0x2020102c, "sigaction");

  pid_t pid;
  int max = depositors > readers ? depositors : readers;

  for (i = 0; i < max; i++)
    {
      // Start readers and depositors alternately; with large numbers
      // of processes the last processes won't start until the test is
      // already over, and we would like some of each.
      forked_process_num++;
      if (i < depositors)
        {
          if ((pid = fork ()) < 0)
            {
              // Probably ran out of child processes
              OB_PERROR_CODE (0x2020102d, "fork");
              return;
            }
          if (pid == 0)
            {
              // Child
              forked_process_type = "depositor";
              set_timer ();
              struct timeval dummy;
              deposit_proteins (forked_process_num, &forked_process_results,
                                &dummy);
              // Doesn't return
            }
        }

      if (i < readers)
        {
          if ((pid = fork ()) < 0)
            {
              // Probably ran out of child processes
              OB_PERROR_CODE (0x2020102e, "fork");
              return;
            }
          if (pid == 0)
            {
              // Child
              forked_process_type = "reader";
              set_timer ();
              struct timeval dummy;
              read_proteins (forked_process_num, &forked_process_results,
                             &dummy);
              // Doesn't return
            }
        }
    }
#endif
}

static int reap_children (void)
{
  int all_errors = 0;
  ob_log (OBLV_DBUG, 0x20201016, "reaping children\n");

#ifdef _MSC_VER
  //wait until we reach the goal time
  while (is_time_to_quit () == false)
    Sleep (1);

  //wait for all the depositors
  thread_data *l = g_thread_list;
  while (l != NULL)
    {
      DWORD wait_millis = INFINITE;

      if (l->is_depositor == false)
        {
          //wait up to 10 seconds for a reader to finish, then
          //give up and terminate the thread. this is unfortunately
          //the design of matrix_test, using signals to terminate
          //the child processes even while they may have an open hose
          //waiting for deposits.
          wait_millis = 1000 * 10;
        }

      DWORD res = WaitForSingleObject (l->thread_handle, wait_millis);

      DWORD exit_code = 0;

      if (res == WAIT_OBJECT_0)
        {
          //it exited, yay
          if (!GetExitCodeThread (l->thread_handle, &exit_code))
            {
              OB_LOG_ERROR_CODE (0x20201017, "Could not get thread exit code, "
                                             "assuming error\n");
              exit_code = 1;
            }
        }
      else if (res == WAIT_TIMEOUT)
        {
          //well, we've gotta terminate it
          OB_LOG_ERROR_CODE (0x20201018, "Terminating reader thread after "
                                         "waiting 10 seconds, assuming "
                                         "success\n");
          TerminateThread (l->thread_handle, 0);
          exit_code = 0;
        }

      CloseHandle (l->thread_handle);

      if (l->is_depositor)
        report_results (l->thread_num, "depositor", l->thread_results,
                        l->thread_result_time);
      else
        report_results (l->thread_num, "reader", l->thread_results,
                        l->thread_result_time);

      if (exit_code != 0)
        all_errors++;

      l = l->next;
    }

  free_thread_list (g_thread_list);

#else

  // Wait till they all exit
  int i;
  for (i = 0; i < total_children; i++)
    {
      int status;
      ob_log (OBLV_DBUG, 0x20201019, "waiting for child %d\n", i);
      int err = wait (&status);
      if (err < 0)
        {
          if (errno == EINTR)
            {
              i--;
              continue;
            }
          OB_PERROR_CODE (0x2020102f, "wait");
          exit (1);
        }
      // Figure out if anyone had an error in which case the results
      // are not trustworthy.
      if (WIFEXITED (status))
        {
          if (WEXITSTATUS (status))
            {
              OB_LOG_ERROR_CODE (0x2020101a,
                                 "child %d exited with non-zero status %d\n",
                                 err, WEXITSTATUS (status));
              all_errors |= WEXITSTATUS (status);
            }
        }
      else if (WIFSIGNALED (status))
        {
          OB_LOG_ERROR_CODE (0x2020101b, "child %d terminated by signal %d\n",
                             err, WTERMSIG (status));
          all_errors |= 0xffff;
        }
      else
        {
          OB_LOG_ERROR_CODE (
            0x2020101c, "child process %d ceased to exist... but why?\n", err);
          all_errors |= 0xffff;
        }
    }
#endif

  return all_errors;
}

static void usage (void)
{
  ob_banner (stderr);
  fprintf (stderr,
           "Usage: matrix_test [-t <type>] [-s <pool size>] [-p <num pools>] \n"
           "\t[-d <num depositors>] [-r <num readers>] [-m <num pools to read "
           "from>] \n"
           "\t[-S <seconds>] [-g <ingest size>] [-a <await timeout>]\n"
           "\t[-i <toc cap>] <poolname prefix>\n");
  exit (EXIT_FAILURE);
}

static void get_args (int argc, char *argv[])
{
  int c;

  while ((c = getopt (argc, argv, "a:d:i:g:m:p:r:s:t:S:v")) != -1)
    {
      switch (c)
        {
          case 'a':
            await = strtoll (optarg, NULL, 0);
            break;
          case 'd':
            depositors = strtoll (optarg, NULL, 0);
            break;
          case 'i':
            cmd.toc_capacity = strtoll (optarg, NULL, 0);
            break;
          case 'g':
            ingest_size = strtoll (optarg, NULL, 0);
            if (ingest_size < 1)
              {
                fprintf (stderr, "Minimum ingest size is 1\n");
                exit (1);
              }
            break;
          case 'm':
            pools_to_read_from = strtoll (optarg, NULL, 0);
            if (pools_to_read_from < 1)
              {
                fprintf (stderr, "Minimum number of pools to read from is 1\n");
                exit (1);
              }
            break;
          case 'p':
            total_pools = strtoll (optarg, NULL, 0);
            if (total_pools < 1)
              {
                fprintf (stderr, "Minimum number of pools is 1\n");
                exit (1);
              }
            break;
          case 'r':
            readers = strtoll (optarg, NULL, 0);
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
          case 'v':
            cmd.verbose++;
            break;
          default:
            usage ();
        }
    }
  pool_cmd_setup_options (&cmd);
  if (pool_cmd_get_poolname (&cmd, argc, argv, optind))
    usage ();
  pool_prefix = cmd.pool_name;

  total_children = readers + depositors;
}

#ifdef _MSC_VER

static void setup_ugly_callback (void)
{
  /* do nothing */
}

#else

static volatile int *mignon_hack;

static void my_ugly_callback (pool_ugly_callback_what what,
                              pool_ugly_callback_when when, const char *file,
                              int line)
{
  sigset_t alarm_set;

  switch (when)
    {
      case POOL_UGLY_CALLBACK_PRE_ACQUIRE:
        OB_CHECK_POSIX_CODE (0x20201022, sigemptyset (&alarm_set));
        OB_CHECK_POSIX_CODE (0x20201023, sigaddset (&alarm_set, SIGALRM));
        OB_CHECK_POSIX_CODE (0x20201024,
                             sigprocmask (SIG_BLOCK, &alarm_set, NULL));
        break;
      case POOL_UGLY_CALLBACK_POST_ACQUIRE:
        if (what == POOL_UGLY_CALLBACK_CONFIG_LOCK)
          {
            if ((*mignon_hack) != 0)
              ob_log_loc_fatal (file, line, OBLV_BUG, 0, -1,
                                "POOL PARANOIA: expected 0 but got %d\n",
                                (*mignon_hack));
            (*mignon_hack)++;
          }
        break;
      case POOL_UGLY_CALLBACK_PRE_RELEASE:
        if (what == POOL_UGLY_CALLBACK_CONFIG_LOCK)
          {
            if ((*mignon_hack) != 1)
              ob_log_loc_fatal (file, line, OBLV_BUG, 0, -1,
                                "POOL PARANOIA: expected 1 but got %d\n",
                                (*mignon_hack));
            (*mignon_hack)--;
          }
        break;
      case POOL_UGLY_CALLBACK_POST_RELEASE:
        OB_CHECK_POSIX_CODE (0x20201025, sigemptyset (&alarm_set));
        OB_CHECK_POSIX_CODE (0x20201026, sigaddset (&alarm_set, SIGALRM));
        OB_CHECK_POSIX_CODE (0x20201027,
                             sigprocmask (SIG_UNBLOCK, &alarm_set, NULL));
        break;
      case POOL_UGLY_CALLBACK_ASSERT_OWNED:
        if (what == POOL_UGLY_CALLBACK_CONFIG_LOCK)
          {
            if ((*mignon_hack) != 1)
              ob_log_loc_fatal (file, line, OBLV_BUG, 0, -1,
                                "POOL PARANOIA: expected 1 but got %d\n",
                                (*mignon_hack));
          }
        break;
    }
}

static void setup_ugly_callback (void)
{
  mignon_hack = (volatile int *) mmap (NULL, 4096, PROT_READ | PROT_WRITE,
                                       MAP_SHARED | MAP_ANON, -1, 0);
  if (mignon_hack == MAP_FAILED)
    OB_PERROR_CODE (0x20201028, "mmap");
  else
    pool_ugly_callback = my_ugly_callback;
}

#endif

int main (int argc, char *argv[])
{
  OB_CHECK_ABI ();

  // log pids (because we fork)
  pool_cmd_modify_default_logging (true, true, false);

#ifdef _MSC_VER
  // enable debug messages...
  pool_cmd_enable_debug_messages (0x20201000, 64 - 12);
#endif

  // Figure out how many processes, etc. we should start
  get_args (argc, argv);

  setup_ugly_callback ();

  // Timers are a bit tricky.  Initially, each process independently
  // set a timer for N seconds from now, and ran until the timer
  // signal.  Each process calculated its own elapsed time.  The
  // problem with this is that under heavy load the processes would
  // end up starting their timers some time after the processes had
  // started, and end up executing at different times, rather than
  // overlapping.  The result is artificially inflated stats from the
  // processes running over a range of times instead of during the
  // same time period.
  gettimeofday (&start_time, NULL);
  // Figure out when all our processes should end.
  goal_end_time = start_time;
  goal_end_time.tv_sec += test_secs;

  // Spawn off a bunch of processes to do the reads and deposits
  create_children ();

  // Wait for the children to finish and report out
  int err = reap_children ();

  if (err)
    fprintf (stderr, "At least one child had an error, don't use results!\n");

  pool_cmd_free_options (&cmd);

  return err;
}
