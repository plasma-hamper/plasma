
/* (c)  oblong industries */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE  // necessary to get pthread_timedjoin_np() on Linux
#endif

#include "libLoam/c/ob-atomic.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-pthread.h"
#include "libLoam/c/ob-sys.h"
#include "libLoam/c/ob-file.h"
// XXX: including a private libLoam header from libPlasma is semi-naughty
#include "libLoam/c/private/ob-syslog.h"
#include "libPlasma/c/pool.h"
#include "libPlasma/c/pool-log.h"
#include "libPlasma/c/slaw.h"
#include "libPlasma/c/protein.h"

/// We use a linked list of these structures to communicate
/// log messages from the threads with stuff to log, to the
/// single thread that deposits everything in the pool.  We
/// manipulate the pointers with atomic operations so that
/// multiple threads can add links to the head of the list
/// in a way which is both atomic and lock-free.
/// (Additionally, the depositor thread can atomically
/// "empty" the list by taking ownership of everything on
/// it while setting the head to NULL.)
///
/// We use this threaded approach so we don't have to deal
/// with re-entrancy in the pool code.  (Since the pool code
/// does a lot of logging, and it would be rude to call back
/// into it directly from the logging callback function.)
typedef struct log_entry
{
  unt64 code;
  float64 t;
  int64 lnum;
  int64 lcount;
  char *prefix;
  char *msg;
  char *stack;
  char *file;
  int line;
  int pid;
  unt32 tid;
  bool is_main;
  struct log_entry *next;
} log_entry;

/// This is the linked list.  All threads can add entries to
/// the head, and the depositor thread can atomically empty
/// it.  Besides pointing to the head of the list or NULL
/// to indicate an empty list, it can also contain the
/// special value (log_entry *) -1, which indicates that
/// the depositor thread is shutting down, and nothing
/// more is allowed to be put on the list.
static log_entry *log_head;

/// We need some way to notify the depositor thread to wake
/// up when log_head becomes non-NULL, and rather than using
/// something trickier like condition variables, we just use
/// the good old self-pipe trick.
///
/// There is an interesting invariant going on here: there
/// will only ever be at most one byte in this pipe.  That
/// is because we write a single byte to the pipe whenever
/// log_head transitions from NULL to non-NULL.  And when
/// the depositor thread wakes up because of this byte, it
/// empties the linked list and sets the head to NULL again,
/// which sets things up for another NULL to non-NULL
/// transition in the future.  So there's this interesting
/// ping-pong going on between the depositor thread and the
/// other threads.
///
/// Normally, the byte written is the ASCII code for ' '.
/// However, when we want to ask the depositor thread to
/// finish (so we can join with it for graceful program
/// termination that won't lose any log messages), we
/// break the invariant of at most one byte in the pipe,
/// and write the ASCII code for 'Q'.  So in this special
/// case that can only happen once, there might be two
/// bytes in the pipe.
static int selfpipe[2] = {-1, -1};

/// This is the identity of the depositor thread, in the
/// form returned by ob_get_small_integer_thread_id().
/// It is used to avoid adding to the linked list from
/// the depositor thread, since that could lead to an
/// unfortunate positive feedback effect.  We use the
/// small integer instead of pthread_t, because we don't
/// have a good pthread_self() emulation on Windows.
static unt32 logging_thread;

/// This is the identity of the depositor thread, as a
/// pthread_t.  This is used for joining with the depositor
/// thread on program termination, so we know that all
/// outstanding log messages have been deposited to the pool.
static pthread_t logging_pthread;

/// This is a special log level, used for logging messages
/// from the pool logging code, to avoid an uncomfortable
/// circularity.
static ob_log_level emergency_log = {
  OB_DST_FD | OB_DST_VALGRIND | OB_FLG_SHOW_CODE_OR_WHERE
    | OB_FLG_SHOW_TID_NONMAIN,
  OB_FOREGROUND_ENABLE | OB_FOREGROUND_RED, /* red */
  LOG_ERR,
  2,
  "error in pool logging: ",
  NULL,
  NULL,
  NULL,
  NULL};

static const ob_log_level tmp_level_prototype = {
  OB_DST_FD, 0, LOG_ERR, 2, "", NULL, NULL, NULL, NULL};

#define NOMORE ((log_entry *) -1)

static void free_log_entry (log_entry *le)
{
  if (le)
    {
      free (le->prefix);
      free (le->msg);
      free (le->stack);
      free (le->file);
      free (le);
    }
}

static void log_callback (const struct ob_log_level *lvl, unt64 code, float64 t,
                          int64 lnum, int64 lcount, const char *file, int line,
                          const char *msg, const char *fmtdmsg,
                          const char *stack)
{
  unt32 tid;
  bool is_main;
  log_entry *le = NULL;
  ob_get_small_integer_thread_id (&tid, &is_main);

  if (tid != logging_thread
      && NULL != (le = (log_entry *) calloc (1, sizeof (log_entry))))
    {
      char *p;
      le->code = code;
      le->t = t;
      le->lnum = lnum;
      le->lcount = lcount;
      le->prefix = lvl->prefix[0] ? strdup (lvl->prefix) : NULL;
      le->msg = msg ? strdup (msg) : NULL;
      le->stack = stack ? strdup (stack) : NULL;
      le->file = file ? strdup (file) : NULL;
      le->line = line;
      le->pid = getpid ();
      le->tid = tid;
      le->is_main = is_main;

      // Chop the trailing ": " off the prefix, to get a reasonable
      // name for the log level
      if (le->prefix && (p = strrchr (le->prefix, ':')))
        *p = 0;

      log_entry *head;
      do
        {
          head = ob_atomic_pointer_ref (&log_head);
          if (head == NOMORE)
            goto failed;
          ob_atomic_pointer_set (&le->next, head);
        }
      while (!ob_atomic_pointer_compare_and_swap (&log_head, head, le));
      if (head == NULL)
        {
          char c = ' ';
          errno = 0;
          while (1 != write (selfpipe[1], &c, 1))
            if (errno != EINTR)
              {
                ob_log (&emergency_log, 0x2010d000, "write failed with: %s\n",
                        strerror (errno));
                abort ();
              }
        }
    }
  else
    {
failed:
      /* only need to do this if not already logging to stderr */
      if (!(lvl->flags & OB_DST_FD))
        {
          ob_log_level tmp_level = tmp_level_prototype;
          tmp_level.color = lvl->color;
          ob_log (&tmp_level, 0, "%s", fmtdmsg);
          if (stack)
            {
              fflush (stderr);
              fprintf (stderr, "%s", stack);
              fflush (stderr);
            }
        }
      free_log_entry (le);
    }
}

static log_entry *detach_chain (log_entry *sentinel)
{
  log_entry *head;
  do
    {
      head = ob_atomic_pointer_ref (&log_head);
    }
  while (!ob_atomic_pointer_compare_and_swap (&log_head, head, sentinel));
  return head;
}

static log_entry *reverse_chain (log_entry *old_head)
{
  log_entry *new_head = NULL;

  while (old_head)
    {
      log_entry *le = old_head;
      old_head = le->next;
      le->next = new_head;
      new_head = le;
    }

  return new_head;
}

static protein protein_from_entry (const log_entry *le)
{
  slabu *sb = slabu_new ();

  if (le->lcount > 1)
    slabu_map_put_cf (sb, "m-of-n",
                      slaw_list_inline_f (slaw_int64 (le->lnum),
                                          slaw_int64 (le->lcount), NULL));
  slabu_map_put_cc (sb, "msg", le->msg);
  if (le->code)
    slabu_map_put_cf (sb, "code", slaw_unt64 (le->code));
  if (le->file)
    {
      slabu_map_put_cf (sb, "line", slaw_int32 (le->line));
      slabu_map_put_cc (sb, "file", le->file);
    }
  slabu_map_put_cf (sb, "time", slaw_float64 (le->t));
  slabu_map_put_cc (sb, "prog", ob_get_prog_name ());
  slabu_map_put_cf (sb, "pid", slaw_int32 (le->pid));
  if (le->is_main)
    slabu_map_put_cc (sb, "thread", "main");
  else
    slabu_map_put_cf (sb, "thread", slaw_unt32 (le->tid));
  if (le->stack)
    slabu_map_put_cc (sb, "stack", le->stack);
  slaw ing = slaw_map_f (sb);

  sb = slabu_new ();
  slabu_list_add_c (sb, "log");
  if (le->prefix)
    slabu_list_add_c (sb, le->prefix);
  slaw des = slaw_list_f (sb);

  return protein_from_ff (des, ing);
}

static void *thread_main (void *arg)
{
  char c;
  bool dummy;
  pool_hose ph = (pool_hose) arg;
  ob_get_small_integer_thread_id (&logging_thread, &dummy);
  do
    {
      errno = 0;
      while (1 != read (selfpipe[0], &c, 1))
        if (errno != EINTR)
          {
            ob_log (&emergency_log, 0x2010d001, "read failed with: %s\n",
                    strerror (errno));
            abort ();
          }

      log_entry *le = detach_chain (c == 'Q' ? NOMORE : NULL);
      le = reverse_chain (le);

      while (le)
        {
          protein p = protein_from_entry (le);
          ob_retort tort = pool_deposit (ph, p, NULL);
          if (tort < OB_OK)
            ob_log (&emergency_log, 0x2010d002,
                    "pool_deposit failed with: %s\n", ob_error_string (tort));
          protein_free (p);
          log_entry *next = le->next;
          free_log_entry (le);
          le = next;
        }
    }
  while (c != 'Q');

  ob_retort tort = pool_withdraw (ph);
  if (tort < OB_OK)
    ob_log (&emergency_log, 0x2010d003, "pool_withdraw failed with: %s\n",
            ob_error_string (tort));

  return NULL;
}

static ob_retort level_to_pool (pool_hose dest, bool disableTerminal,
                                ob_log_level *lvl)
{
  if (!ob_atomic_pointer_compare_and_swap (&lvl->cookie, NULL, dest)
      || !ob_atomic_pointer_compare_and_swap (&lvl->callback, NULL,
                                              log_callback))
    {
      ob_log (&emergency_log, 0x2010d004,
              "log level '%s' already had a callback\n", lvl->prefix);
      return OB_UNKNOWN_ERR;
    }

  int32 oldflags, newflags;
  do
    {
      newflags = oldflags = ob_atomic_int32_ref (&lvl->flags);
      newflags |= OB_DST_CALLBACK;
      if (disableTerminal)
        newflags &= ~OB_DST_FD;
    }
  while (!ob_atomic_int32_compare_and_swap (&lvl->flags, oldflags, newflags));
  return OB_OK;
}

/// Waits up to \a seconds seconds to join with thread
/// \a thr.  Returns 0 if successfully joined, or returns
/// an errno if an error occurred (which includes timing
/// out).
static int timed_join (pthread_t thr, unt32 seconds)
{
#if defined(__gnu_linux__)
  struct timespec ts;
  if (clock_gettime (CLOCK_REALTIME, &ts) < 0)
    return errno;
  ts.tv_sec += seconds;
  return pthread_timedjoin_np (thr, NULL, &ts);
#elif defined(_MSC_VER)
  return pthread_timedjoin_np (thr, NULL, 1000 * seconds);
#else
  // No timed join on Mac, so wait forever
  return pthread_join (thr, NULL);
#endif
}

/// This is registered by atexit() to be called at exit(),
/// so that we can join with the depositor thread before
/// actually terminating the process.  This avoids losing
/// messages that are still queued at exit.
static void my_exit_func (void)
{
  // Determine whether we *are* the logging thread; in that case,
  // don't attempt to join ourselves.  Use ob_get_small_integer_thread_id()
  // instead of pthread_self(), because we don't have a good pthread_self()
  // emulation on Windows.
  unt32 mytid;
  bool dummy;
  ob_get_small_integer_thread_id (&mytid, &dummy);
  if (mytid == logging_thread)
    return;

  char c = 'Q';
  errno = 0;
  while (1 != write (selfpipe[1], &c, 1))
    if (errno != EINTR)
      {
        ob_log (&emergency_log, 0x2010d005, "write failed with: %s\n",
                strerror (errno));
        return;
      }

  // If pool code calls error_exit or error_abort while holding a lock
  // it's possible the logging thread might not be able to make
  // progress, and we'll have a deadlock.  For this reason, just
  // wait a limited amount of time (5 seconds should be enough, I hope)
  // on platforms that support it, rather than waiting forever.
  int erryes = timed_join (logging_pthread, 5);
  if (erryes)
    ob_log (&emergency_log, 0x2010d006, "pthread_join failed with: %s\n",
            strerror (erryes));
}

static ob_abort_func_t saved_abort;

static void my_abort_func (void)
{
  my_exit_func ();
  saved_abort ();
}

// Note: C's varargs requires int instead of bool here, see bug 19214

ob_retort ob_log_to_pool (pool_hose dest, int disableTerminal, ...)
{
  // Create the pipe
  if (selfpipe[0] >= 0 || selfpipe[1] >= 0)
    {
      ob_log (&emergency_log, 0x2010d007,
              "It appears ob_log_to_pool() was called more than once.\n"
              "That's not legal.\n");
      return OB_UNKNOWN_ERR;
    }
  ob_retort tort = ob_pipe_cloexec (selfpipe);
  if (tort < OB_OK)
    return tort;

  // Create the thread
  int erryes = pthread_create (&logging_pthread, NULL, thread_main, dest);
  if (erryes)
    return ob_errno_to_retort (erryes);

  // Install our cleanup handler for abort() ...
  do
    saved_abort = ob_atomic_pointer_ref (&ob_abort_func);
  while (!ob_atomic_pointer_compare_and_swap (&ob_abort_func, saved_abort,
                                              my_abort_func));

  // ... and for exit()
  OB_CHECK_POSIX_CODE (0x2010d008, atexit (my_exit_func));

  // Hook into all the desired log levels
  ob_log_level *lvl;
  va_list v;
  ob_retort t = OB_OK;
  va_start (v, disableTerminal);
  while (NULL != (lvl = va_arg (v, ob_log_level *)))
    ob_err_accum (&t, level_to_pool (dest, (bool) disableTerminal, lvl));
  va_end (v);
  return t;
}
