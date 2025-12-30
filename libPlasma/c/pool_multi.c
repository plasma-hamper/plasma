
/* (c)  oblong industries */

//
// Multi-pool await support
//

#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include "libLoam/c/ob-sys.h"
#include "libLoam/c/ob-dirs.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-file.h"
#include "libLoam/c/ob-atomic.h"
#include "libLoam/c/ob-string.h"

#include "libPlasma/c/slaw.h"
#include "libPlasma/c/slaw-string.h"
#include "libPlasma/c/pool.h"
#include "libPlasma/c/private/pool_impl.h"
#include "libPlasma/c/private/pool_multi.h"

#ifdef __GNUC__
#include "libLoam/c/valgrind/memcheck.h"
#else
#define VALGRIND_CHECK_MEM_IS_ADDRESSABLE(ig, nore) 0
#define VALGRIND_CHECK_MEM_IS_DEFINED(ig, nore) 0
#endif

/// Multi-pool await allows the caller to wait for a deposit on any of
/// several pools.  It only waits in the case that the caller wants the
/// very next protein to be deposited, so whenever there are waiters
/// for a pool, they are all waiting for the same protein.  The
/// depositor must check for awaiters and wake them up.  There can't be
/// a race condition allowing a protein notification to be missed
/// between when we determine we need to wait and when we actually
/// wait.
///
/// The main source of pain and anguish in this code is that we must
/// wait atomically on both local and network pools - and we want to
/// keep the number of extra processes at a minimum.  We simply have to
/// have an extra server process up and running to accept connections
/// over the network, but otherwise we want very much to avoid this
/// source of pain.  In general, we don't want to deal with processes
/// at all - pids are our only identification method, and they are
/// terribly painful to work with, /var/run/*.pid and pid recycling and
/// signals, etc.
///
/// Notification of a deposit on a network pool is done by sending the
/// protein over the network socket to the client.  Waiting on multiple
/// network pools is easy: just select() over the sockets.  Local pool
/// await is implemented with fifos.
///
/// select() really is the right mechanism to use underneath multi-pool
/// await.  The main difficulty is in translating local pool deposits
/// into bytes deposited into a socket that the awaiter can select()
/// on.  In addition, all awaiters must be notified, when the usual
/// semantics for I/O on a socket with multiple readers is wake-one.
/// The next hard part is setting up some method for the depositor to
/// find and write to the sockets that the clients are listening to
/// that does not require a separate process.
///
/// The solution in the end is to put the state into the file system,
/// and clean up left-behind state when we discover it.
///
/// Current design:
///
/// Each pool has a notification directory.  Awaiters who want to be
/// told when a new protein is deposited create a fifo in this
/// directory and then select() on it and the other fds for the pools
/// it is interested in.
///
/// For details on implementation of the actual fifo operations, see
/// fifo_ops.[ch].

#ifdef _MSC_VER
typedef HANDLE select_winner_t;
#else
typedef fd_set select_winner_t;
#endif

/// Returns true if \a ph is one of the hoses contained in the
/// OS-dependent \a winner set.
static bool is_a_winner (pool_hose ph, const select_winner_t *winner)
{
  if (!winner)
    return false;
#ifdef _MSC_VER
  return ph->notify_handle == *winner;
#elif defined(__APPLE__)
  // OS X's FD_ISSET requires a non-const fd_set *, so make a copy
  select_winner_t annoying = *winner;
  return !!(FD_ISSET (ph->notify_handle, &annoying));
#else
  // Linux's FD_ISSET accepts a const fd_set *, like you'd hope for
  return !!(FD_ISSET (ph->notify_handle, winner));
#endif
}

typedef struct gang_array_entry
{
  pool_gang_member member;
  unt32 precedence;
  int64 idx;
} gang_array_entry;

#define NON_WINNER_MASK (0x80000000U)

typedef struct gang_array
{
  unt32 capacity;
  unt32 nmembers;
  gang_array_entry entries[1];
} gang_array;

#define DEFAULT_CAPACITY(x) (sizeof ((x)->entries) / sizeof ((x)->entries[0]))

static gang_array *gang_array_new (void)
{
  gang_array *result = (gang_array *) malloc (sizeof (gang_array));
  if (result)
    {
      result->nmembers = 0;
      result->capacity = DEFAULT_CAPACITY (result);
    }
  return result;
}

static ob_retort gang_array_add (gang_array **ptr, pool_gang_member member,
                                 unt32 precedence)
{
  (*ptr)->nmembers++;
  if ((*ptr)->nmembers > (*ptr)->capacity)
    {
      (*ptr)->capacity *= 2;
      gang_array *re = (gang_array *) realloc (
        *ptr, sizeof (gang_array)
                + sizeof (gang_array_entry)
                    * ((*ptr)->capacity - DEFAULT_CAPACITY (*ptr)));
      if (!re)
        {
          free (*ptr);
          *ptr = NULL;
          return OB_NO_MEM;
        }
      *ptr = re;
    }
  (*ptr)->entries[(*ptr)->nmembers - 1].member = member;
  (*ptr)->entries[(*ptr)->nmembers - 1].precedence = precedence;
  (*ptr)->entries[(*ptr)->nmembers - 1].idx = member->ph->index;
  return OB_OK;
}

static int gang_array_compare (const void *v1, const void *v2)
{
  const gang_array_entry *e1 = (gang_array_entry *) v1;
  const gang_array_entry *e2 = (gang_array_entry *) v2;
  if (e1->precedence < e2->precedence)
    return -1;
  else if (e1->precedence > e2->precedence)
    return 1;
  else
    return 0;
}

static void gang_array_sort (gang_array *a)
{
  qsort (a->entries, a->nmembers, sizeof (gang_array_entry),
         gang_array_compare);
}

static OB_UNUSED void gang_array_print (const gang_array *a)
{
  unt32 i;
  for (i = 0; i < a->nmembers; i++)
    {
      pool_hose ph = a->entries[i].member->ph;
      unt32 precedence = a->entries[i].precedence;
      printf ("%02d: %08x - %s\n", i, precedence, ph->name);
    }
}

#ifdef _MSC_VER
/// Windows uses a linked list to keep track of all the wait events
/// that a gang may be waiting on, these functions help manipulate that
/// structure

//simple recursive list count
int pool_gang_wait_list_count (pool_gang_wait_list_struct *l)
{
  if (l == NULL)
    return 0;
  return 1 + pool_gang_wait_list_count (l->next);
}

//simple recursive list free
void pool_gang_wait_list_free (pool_gang_wait_list_struct *l)
{
  if (l == NULL)
    return;

  if (l->next)
    pool_gang_wait_list_free (l->next);

  free (l);
}

//assuming wait_list_count has been used to allocate a properly sized out_array,
//this will fill in the array with the contents of the list. this array structure
//is required for the call to WaitForMultipleObjects
void pool_gang_wait_list_arrayify (pool_gang_wait_list_struct *l,
                                   HANDLE *out_array)
{
  if (l == NULL || out_array == NULL)
    return;

  out_array[0] = l->wait_handle;
  if (l->next)
    pool_gang_wait_list_arrayify (l->next, out_array + 1);
}
#endif

static ob_retort pool_gang_init_wakeup (pool_gang gang)
{
  ob_initialize_wakeup (&gang->w);
  return ob_enable_wakeup (&gang->w);
}

static void pool_gang_cleanup_wakeup (pool_gang gang)
{
  ob_cleanup_wakeup (&gang->w);
}

static pool_gang global_gang_list;

static pool_gang alloc_gang_structure (void)
{
  pool_gang g, oldhead;
  pool_gang *ptr = &global_gang_list;
  for (;;)
    {
      g = ob_atomic_pointer_ref (ptr);
      if (!g)
        break;
      if (ob_atomic_int32_compare_and_swap (&(g->mgmt.status), Gang_Unused,
                                            Gang_Used))
        {
          memset (&(g->leader), 0, sizeof (*g) - sizeof (gang_management_info));
          return g;
        }
      ptr = &(g->mgmt.next);
    }
  g = (pool_gang) calloc (1, sizeof (*g));
  do
    {
      oldhead = ob_atomic_pointer_ref (&global_gang_list);
      g->mgmt.next = oldhead;
      g->mgmt.status = Gang_Used;
    }
  while (!ob_atomic_pointer_compare_and_swap (&global_gang_list, oldhead, g));
  return g;
}

static void dealloc_gang_structure (pool_gang g)
{
  ob_make_undefined (&(g->leader), sizeof (*g) - sizeof (gang_management_info));
  ob_atomic_int32_set (&(g->mgmt.status), Gang_Unused);
}

void pool_fork_gangs (void)
{
  pool_gang *ptr = &global_gang_list;
  for (;;)
    {
      pool_gang g = ob_atomic_pointer_ref (ptr);
      if (!g)
        break;
      if (ob_atomic_int32_compare_and_swap (&(g->mgmt.status), Gang_Used,
                                            Gang_Needs_Reconnection))
        pool_gang_cleanup_wakeup (g);
      ptr = &(g->mgmt.next);
    }
}

static ob_retort pool_check_gang_validity (pool_gang g)
{
  if (!g)
    return POOL_NULL_GANG;

  int32 status = ob_atomic_int32_ref (&(g->mgmt.status));
  switch (status)
    {
      case Gang_Unused:
        OB_FATAL_BUG_CODE (0x2010502e, "Use of freed pool gang\n");
      case Gang_Used:
        return OB_OK;
      case Gang_Needs_Reconnection:
        return POOL_INVALIDATED_BY_FORK;
      default:
        OB_FATAL_BUG_CODE (0x2010502f,
                           "Apparent memory corruption: gang has status %d\n",
                           status);
    }
}

#define CHECK_GANG_VALIDITY(gng)                                               \
  {                                                                            \
    ob_retort tortellini = pool_check_gang_validity (gng);                     \
    if (tortellini < OB_OK)                                                    \
      return tortellini;                                                       \
  }

ob_retort pool_new_gang (pool_gang *gang)
{
  if (!gang)
    return OB_ARGUMENT_WAS_NULL;

  pool_gang g = alloc_gang_structure ();
  if (!g)
    return OB_NO_MEM;

  ob_retort ret = pool_gang_init_wakeup (g);
  if (ret != OB_OK)
    {
      free (g);
      return ret;
    }

  *gang = g;

  return OB_OK;
}

ob_retort pool_join_gang (pool_gang gang, pool_hose ph)
{
  if (!gang)
    return POOL_NULL_GANG;
  if (!ph)
    return POOL_NULL_HOSE;

  // Pool hoses can only be members of one gang at a time (and can't
  // join the same gang twice) or else multi-pool await will break.
  // Check to see if the pool hose is already part of a gang and
  // refuse to let it join if so.
  if (ph->is_gang_member)
    return POOL_ALREADY_GANG_MEMBER;
  pool_gang_member m = (pool_gang_member) calloc (1, sizeof (*m));
  if (!m)
    return OB_NO_MEM;
  m->ph = ph;
  ph->is_gang_member = true;
  //Insert at the head of the pool list
  m->next = gang->leader;
  gang->leader = m;
  // Initialize last read if necessary
  if (!gang->last_checked)
    gang->last_checked = gang->leader;
  return OB_OK;
}

ob_retort pool_leave_gang (pool_gang gang, pool_hose ph)
{
  if (!gang)
    return POOL_NULL_GANG;
  if (!ph)
    return POOL_NULL_HOSE;

  ob_log (OBLV_DBUG, 0x20105003, "pool_leave_gang\n");
  // Look up the pool hose in our list via pool hose address
  pool_gang_member search = gang->leader;
  pool_gang_member previous = search;
  while (search && search->ph != ph)
    {
      previous = search;
      search = search->next;
    }

  if (!search)
    return POOL_NOT_A_GANG_MEMBER;

  pool_gang_member m = search;
  // Remove from list
  if (gang->leader == m)
    gang->leader = m->next;
  else
    previous->next = m->next;
  // If we were the last read pool, set last read to the pool behind
  // us so the following pool gets read next - unless we're the head
  // of the list, in which case we'd have to walk the list, which is
  // annoying.
  if (gang->last_checked == m)
    {
      if (previous != m)
        gang->last_checked = previous;
      else
        gang->last_checked = m->next;
    }
  m->ph->is_gang_member = false;
  free (m);
  return OB_OK;
}

ob_retort pool_disband_gang (pool_gang gang, bool withdraw)
{
  ob_retort tort = OB_OK;

  if (gang)
    {
      // Free each element
      pool_gang_member search = gang->leader;
      while (search)
        {
          pool_gang_member next = search->next;
          pool_hose ph;
          ob_err_accum (&tort, pool_leave_gang (gang, (ph = search->ph)));
          // Withdraw from pool if requested
          /* do this AFTER we leave the gang, since withdraw makes ph
             invalid */
          if (withdraw)
            ob_err_accum (&tort, pool_withdraw (ph));
          search = next;
        }

      pool_gang_cleanup_wakeup (gang);

      dealloc_gang_structure (gang);
    }

  return tort;
}

/// Get the next gang member in this gang in round-robin fashion.

static pool_gang_member pool_next_gang_member (pool_gang gang,
                                               pool_gang_member curr)
{
  if (curr->next)
    return curr->next;
  // Wrap if we hit the end of the list
  return gang->leader;
}

static ob_retort
pool_next_multi_internal2 (pool_gang gang, pool_hose *ret_ph, protein *ret_prot,
                           pool_timestamp *ret_ts, int64 *ret_index,
                           const gang_array *ga, next_func_t next)
{
  unt32 count;
  ob_retort pret = POOL_EMPTY_GANG;

  for (count = 0; count < ga->nmembers; count++)
    {
      pool_gang_member search = ga->entries[count].member;
      ob_log (OBLV_DBUG, 0x20105005, "looking for protein in pool %s\n",
              search->ph->name);
      pret = next (search->ph, ret_prot, ret_ts, ret_index);
      ob_log (OBLV_DBUG, 0x20105006, "pool_next returned %s\n",
              ob_error_string (pret));
      gang->last_checked = search;
      if (pret == OB_OK)
        {
          if (ret_ph)
            *ret_ph = search->ph;
          return pret;
        }
      // XXX - should continue on if one pool dies? error propagation?
      if (pret != POOL_NO_SUCH_PROTEIN)
        return pret;
    }

  return pret;
}

static gang_array *array_from_gang (pool_gang gang,
                                    const select_winner_t *winner)
{
  unt32 count = 0;
  gang_array *ga = gang_array_new ();
  if (!ga)
    return NULL;

  if (!gang->last_checked)
    OB_FATAL_BUG_CODE (0x20105025,
                       "Invariant violated: gang->leader was non-NULL "
                       "but gang->last_checked was NULL\n");

  ob_retort pret;
  pool_gang_member start = gang->last_checked;
  pool_gang_member search = start;
  do
    {
      search = pool_next_gang_member (gang, search);
      pret = gang_array_add (
        &ga, search,
        count++ | (is_a_winner (search->ph, winner) ? 0 : NON_WINNER_MASK));
      if (pret < OB_OK)
        return NULL;
    }
  while (search != start);
  return ga;
}

static ob_retort pool_next_multi_internal1 (pool_gang gang, pool_hose *ret_ph,
                                            protein *ret_prot,
                                            pool_timestamp *ret_ts,
                                            int64 *ret_index, next_func_t next)
{
  CHECK_GANG_VALIDITY (gang);
  if (!gang->leader)
    return POOL_EMPTY_GANG;

  gang_array *ga = array_from_gang (gang, NULL);
  if (!ga)
    return OB_NO_MEM;
  ob_retort tort = pool_next_multi_internal2 (gang, ret_ph, ret_prot, ret_ts,
                                              ret_index, ga, next);
  free (ga);
  return tort;
}

ob_retort pool_next_multi (pool_gang gang, pool_hose *ret_ph, protein *ret_prot,
                           pool_timestamp *ret_ts, int64 *ret_index)
{
  return pool_next_multi_internal1 (gang, ret_ph, ret_prot, ret_ts, ret_index,
                                    pool_next);
}

/// Tear down await state for a gang member which was previously
/// placed in the await state.

static void pool_multi_remove_awaiter (pool_gang_member m)
{
  pool_hose ph = m->ph;
  ob_log (OBLV_DBUG, 0x20105007, "pool_multi_remove_awaiter for pool %s\n",
          ph->name);
  ph->multi_remove_awaiter (ph);
}

/// Tear down await state for part of a pool gang.  Begin with the
/// start gang member and cycle through the gang.  When the stop gang
/// member is reached, do not unawait it, just exit - this means the
/// stop gang member is the gang member following the last gang member
/// to unawait.  Remember, do not unawait a gang member that has not
/// been first awaited.

static void pool_unawait_some (pool_gang gang, pool_gang_member start,
                               pool_gang_member stop)
{
  pool_gang_member search = start;
  do
    {
      search = pool_next_gang_member (gang, search);
      pool_multi_remove_awaiter (search);
    }
  while (search != stop);
}

/// Tear down await state for an entire gang.

static void pool_unawait_all (pool_gang gang)
{
  pool_unawait_some (gang, gang->leader, gang->leader);
}

/// Add this gang member as an awaiter.  For efficiency, we return
/// immediately if we found a protein, filling in the ret_prot,
/// ret_ts, and ret_index pointers.  If we did not, we add this gang
/// member's notification fd to the cached fd set (used for select()).

static ob_retort pool_multi_add_awaiter (pool_gang gang, pool_gang_member m,
                                         protein *ret_prot,
                                         pool_timestamp *ret_ts,
                                         int64 *ret_index)
{
  ob_retort pret;

  pool_hose ph = m->ph;
  ob_log (OBLV_DBUG, 0x20105008, "pool_multi_add_awaiter for pool %s\n",
          ph->name);
  // Add this pool's notification fd to our fd set
  pret = ph->multi_add_awaiter (ph, ret_prot, ret_ts, ret_index);
  // Did we find a protein?
  if (pret == OB_OK)
    return OB_OK;
  // POOL_NO_SUCH_PROTEIN means we successfullly set up the await but
  // there was no protein available right now.
  if (pret != POOL_NO_SUCH_PROTEIN)
    {
      ob_log (OBLV_DBUG, 0x20105009, "can't get wait fd for pool %s\n",
              ph->name);
      return pret;
    }

#ifdef _MSC_VER
  //add this wait handle to the gang's wait list
  pool_gang_wait_list_struct *g =
    (pool_gang_wait_list_struct *) malloc (sizeof (pool_gang_wait_list_struct));
  g->next = gang->wait_list;
  g->wait_handle = ph->notify_handle;
  gang->wait_list = g;
#else
  ob_log (OBLV_DBUG, 0x2010500a, "setting fd %d (%s) in gang\n",
          ph->notify_handle, ph->fifo_path);
  FD_SET (ph->notify_handle, &gang->wait_fds);
  // fd = 0 -> maxfd = 1
  if (gang->maxfd < (ph->notify_handle + 1))
    {
      gang->maxfd = ph->notify_handle + 1;
      ob_log (OBLV_DBUG, 0x2010500b, "maxfd now %d\n", gang->maxfd);
    }
#endif

  return pret;
}

/// Wake any multi-pool awaiters on this pool.

// Remote multi-pool awaiters have a local fifo listener, so the below
// call awakes both local and remote multi-pool awaiters.

ob_retort pool_multi_wake_awaiters (pool_hose ph)
{
  return pool_fifo_wake_awaiters (ph);
}

/// Await all gang members.
///
/// Put all gang members into await mode.  We follow the rules for
/// safe awaiting (safe awaiting guarantees we will not miss a
/// deposit).  (1) Set up the mechanism to detect protein deposits,
/// (2) check to see if a protein was deposited since we added the
/// awaiter (to avoid missing a protein arrival), (3) start the actual
/// await.  Step 3 is done in the caller; in this fuction we do steps
/// 1 and 2 for gang member 1, then steps 1 and 2 for gang member 2,
/// etc.  If we find a protein while setting up the gang members, we
/// unwind await state for only the pools we awaited and fill in
/// ret_prot, ret_ts, and ret_index, and return OB_OK to indicate we
/// found a protein).  Otherwise, we return POOL_NO_SUCH_PROTEIN, which
/// tells the caller to go ahead with the actual await.

static ob_retort pool_multi_prepare_await (pool_gang gang, pool_hose *ret_ph,
                                           protein *ret_prot,
                                           pool_timestamp *ret_ts,
                                           int64 *ret_index)
{
  ob_retort tort = OB_OK;
  pool_gang_member start = gang->last_checked;
  pool_gang_member search = start;

#ifdef _MSC_VER
  pool_gang_wait_list_free (gang->wait_list);
  gang->wait_list = 0;
#else
  FD_ZERO (&gang->wait_fds);
  FD_SET (gang->w.wakeup_read_fd, &gang->wait_fds);
  gang->maxfd = gang->w.wakeup_read_fd + 1;
#endif

  do
    {
      search = pool_next_gang_member (gang, search);
      ob_log (OBLV_DBUG, 0x2010500c, "adding awaiter for %s\n",
              search->ph->name);
      ob_retort pret =
        pool_multi_add_awaiter (gang, search, ret_prot, ret_ts, ret_index);
      if (pret == OB_OK)
        {
          if (ret_ph)
            *ret_ph = search->ph;
          ob_log (OBLV_DBUG, 0x2010500d, "found protein in %s\n",
                  search->ph->name);
          goto found_it;
        }
      else if (pret == POOL_NO_SUCH_PROTEIN)
        {
          ob_log (OBLV_DBUG, 0x2010500e, "didn't find protein in %s\n",
                  search->ph->name);
        }
      else
        ob_err_accum (&tort, pret);
      gang->last_checked = search;
    }
  while (search != start);

  ob_err_accum (&tort, POOL_NO_SUCH_PROTEIN);

found_it:

  // If we found a protein, only unawait the pools we started
  if (tort != POOL_NO_SUCH_PROTEIN)
    pool_unawait_some (gang, start, search);

  return tort;
}

/// Thread- and signal-safe wakeup for pool_await_next_multi.

ob_retort pool_gang_wake_up (pool_gang gang)
{
  CHECK_GANG_VALIDITY (gang);
  return ob_wake_up (&(gang->w));
}

/// Select over all the notification fds for the pools in this gang,
/// waiting for a protein deposit.  The fd set is created during
/// pool_multi_add_awaiter().  The timeout is interpreted in the
/// standard manner; see pool.h for details.

static ob_retort pool_multi_select (pool_gang gang, pool_timestamp timeout,
                                    select_winner_t *winner)
{
  static const fd_set zero_fds = {{0}};  // initialize to 0 for grumpy compiler

  // Interrupted select() requires retry, so loop until we get
  // something.
  while (1)
    {
      ob_log (OBLV_DBUG, 0x20105012, "selecting...\n");

#ifdef _MSC_VER
      int numWaitObjects = pool_gang_wait_list_count (gang->wait_list);

      //include room for the gang wakeup handle too
      HANDLE wakeupHandle = gang->w.wakeup_event_handle;
      if (wakeupHandle != 0)
        numWaitObjects += 1;

      if (numWaitObjects <= 0)
        {
          *winner = OB_NULL_HANDLE;
          break;
        }

      HANDLE *waitObjectArray =
        (HANDLE *) malloc (sizeof (HANDLE) * numWaitObjects);

      //fill the first part of the array with the gang members wait objects
      pool_gang_wait_list_arrayify (gang->wait_list, waitObjectArray);

      //fill the last entry of the array with the gang wakeup handle if it exists
      if (wakeupHandle != 0)
        waitObjectArray[numWaitObjects - 1] = wakeupHandle;

      DWORD wait_milliseconds = pool_timeout_to_wait_millis (timeout);

      DWORD wait_res = WaitForMultipleObjects (numWaitObjects, waitObjectArray,
                                               false, wait_milliseconds);

      if (wait_res >= WAIT_OBJECT_0
          && wait_res < (WAIT_OBJECT_0 + numWaitObjects))
        {
          //good wakeup, but was it the wakeup_event that did it?
          if (wait_res == WAIT_OBJECT_0 + numWaitObjects - 1)
            {
              free (waitObjectArray);
              return POOL_AWAIT_WOKEN;
            }

          //nah it was probably a deposit
          *winner = waitObjectArray[wait_res - WAIT_OBJECT_0];
          free (waitObjectArray);
          return OB_OK;
        }

      free (waitObjectArray);

      if (wait_res == WAIT_TIMEOUT)
        {
          //we weren't able to get the lock
          ob_log (OBLV_DBUG, 0x20105013, "multi_select timed out");
          return POOL_AWAIT_TIMEDOUT;
        }
      else if (wait_res == WAIT_FAILED)
        {
          DWORD lasterr = GetLastError ();
          ob_retort tort = ob_win32err_to_retort (lasterr);
          OB_LOG_ERROR_CODE (0x20105014,
                             "WaitForMultipleObjects failed with \"%s\"\n",
                             ob_error_string (tort));
          return tort;
        }

      // http://msdn.microsoft.com/en-us/library/ms687025(VS.85).aspx
      OB_LOG_ERROR_CODE (0x20105015,
                         "unexpected return value for "
                         "WaitForMultipleObjects = 0x%08x\n",
                         wait_res);
      return POOL_FIFO_BADTH;

#else

      struct timeval *tp;
      struct timeval timeout_tv;

      tp = pool_timeout_to_timeval (timeout, &timeout_tv);

      // The fd set we call select with will get stomped, make a copy
      fd_set read_fds = gang->wait_fds;
      fd_set except_fds = gang->wait_fds;

      int nready = select (gang->maxfd, &read_fds, NULL, &except_fds, tp);

      ob_log (OBLV_DBUG, 0x20105016, "woke up!\n");

      // We could have been woken for any of the following reasons:
      //
      // - One of our sockets died
      // - We were interrupted during the select() system call
      // - We were woken via pool_hose_wake_up()
      // - Someone notified us of a protein we already read
      // - Someone deposited a protein in one of our pools
      //
      // The caller must distinguish between the last two cases by
      // calling pool_next() - if you don't get anything back, restart
      // the select.

      if (nready < 0)
        {
          // select() returned an error
          if (errno == EINTR)
            {
              // No problem, just restart
              ob_log (OBLV_DBUG, 0x20105017, "woke for EINTR\n");
              continue;
            }
          // Real error - give up, we can't fix this
          OB_PERROR_CODE (0x2010502a, "select() failed in pool_multi.c");
          // Technically, could be a network socket causing the problem
          return POOL_FIFO_BADTH;
        }
      else if (nready == 0)
        {
          // Timeout
          ob_log (OBLV_DBUG, 0x20105018, "woke for timeout\n");
          return POOL_AWAIT_TIMEDOUT;
        }
      else
        {
          if (FD_ISSET (gang->w.wakeup_read_fd, &read_fds))
            {
              // Awoken
              unt8 buf[PRETTY_DARN_LARGE];
              ob_log (OBLV_DBUG, 0x20105019, "explicitly woken\n");

              // flush the wake buffer (avoid stalling writer; fixes bug 771)
              if (read (gang->w.wakeup_read_fd, &buf, sizeof (buf)) < 0)
                OB_LOG_ERROR_CODE (0x2010501a, "read failed with '%s'\n",
                                   strerror (errno));

              if (nready == 1)
                return POOL_AWAIT_WOKEN;
              /* otherwise fall through, as we were woken for some other
                 reason */
            }

          // Some fds are ready - maybe for reading, maybe an exception
          // XXX deal with pools that have died in some way
          if (memcmp (&read_fds, &zero_fds, sizeof (read_fds)) == 0)
            {
              // There isn't anything to read
              ob_log (OBLV_DBUG, 0x2010501b, "woke for some other reason\n");
              continue;
            }
          // Okay!  We have a protein, maybe!  Go check in the caller.
          *winner = read_fds;
          break;
        }
// XXX clear out any except fds?
#endif
    }
  ob_log (OBLV_DBUG, 0x2010501c, "successful select\n");
  return OB_OK;
}

static ob_retort pool_opportunistic_next (pool_hose ph, protein *ret_prot,
                                          pool_timestamp *ret_ts,
                                          int64 *ret_index)
{
  return (ph->opportunistic_next ? ph->opportunistic_next
                                 : pool_next) (ph, ret_prot, ret_ts, ret_index);
}

ob_retort pool_await_next_multi (pool_gang gang, pool_timestamp timeout,
                                 pool_hose *ret_ph, protein *ret_prot,
                                 pool_timestamp *ret_ts, int64 *ret_index)
{
  select_winner_t winner;
  pool_timestamp target = OB_NAN;

  // Are proteins already available in some pool?
  ob_retort pret = pool_next_multi (gang, ret_ph, ret_prot, ret_ts, ret_index);

  if (timeout == POOL_NO_WAIT || pret != POOL_NO_SUCH_PROTEIN)
    return (pret == POOL_NO_SUCH_PROTEIN) ? POOL_AWAIT_TIMEDOUT : pret;

  // Now loop, setting up await state and then going to sleep waiting
  // for the deposit.
  while (true)
    {
      pret =
        pool_multi_prepare_await (gang, ret_ph, ret_prot, ret_ts, ret_index);
      if (pret != POOL_NO_SUCH_PROTEIN)
        // Woot!  Found something without going to sleep, return now
        return pret;

      // Sleep until we are woken by a protein deposit or the timeout.
      pret = pool_multi_select (
        gang, private_incremental_timeout (timeout, &target), &winner);

      // Must do this before the pool_unawait_all(), which destroys
      // the notify handles, which we need to determine the winner.
      gang_array *ga = (pret == OB_OK ? array_from_gang (gang, &winner) : NULL);

      // We must unawait everyone before we go on, regardless of how
      // or if we found a protein, since we can't set up await for a
      // pool hose already set up for await.
      pool_unawait_all (gang);

      // Now that we are unawaited, check for error and exit.
      if (pret != OB_OK)
        break;

      // We want the gang member(s) identified by the select() to be
      // checked first, but otherwise go in the order of the gang list.
      // (So, putting "count" in the precedence makes the sort stable,
      // and putting NON_WINNER_MASK in the precedence makes all the
      // non-winners ordered after the winners.)
      gang_array_sort (ga);

      // We can be legitimately woken and have nothing to read,
      // since deposits and notifications are not atomic
      // (intentionally so).  So we can get a protein via
      // pool_next(), then ask to be notified for that pool, and
      // then get notified for the protein we already read.  If
      // this happens, just go back around.
      // (But if we go back around, we subtract the
      // time we've already waited from the timeout.)

      pret = pool_next_multi_internal2 (gang, ret_ph, ret_prot, ret_ts,
                                        ret_index, ga, pool_opportunistic_next);
      free (ga);
      ob_log (OBLV_DBUG, 0x20105024,
              "%d: pool_next_multi() returned %s"
              " in pool_await_next_multi()\n",
              getpid (), ob_error_string (pret));

      if (pret == OB_OK)
        {
          ob_log (OBLV_DBUG, 0x2010501e, "found a protein, exiting\n");
          break;
        }
      else if (pret != POOL_NO_SUCH_PROTEIN)
        return pret;  // an unexpected error; probably not recoverable
      ob_log (OBLV_DBUG, 0x2010501f, "didn't find a protein, trying again\n");
    }

  if (pret == POOL_AWAIT_TIMEDOUT)
    {
      pret = pool_next_multi (gang, ret_ph, ret_prot, ret_ts, ret_index);
      if (pret == POOL_NO_SUCH_PROTEIN)
        pret = POOL_AWAIT_TIMEDOUT;
    }

  return pret;
}

ob_retort pool_await_multi (pool_gang gang, pool_timestamp timeout)
{
  // This function isn't supposed to increment the index, but since
  // incrementing the index is so baked-in in so many places, we
  // hackishly work around it by saving the index and restoring it.

  CHECK_GANG_VALIDITY (gang);

  gang_array *ga = array_from_gang (gang, NULL);
  if (!ga)
    return OB_NO_MEM;

  pool_hose ph = NULL;
  unt32 i;

  ob_retort tort = pool_await_next_multi (gang, timeout, &ph, NULL, NULL, NULL);
  for (i = 0; i < ga->nmembers; i++)
    {
      if (ph == ga->entries[i].member->ph)
        ga->entries[i].member->ph->index = ga->entries[i].idx;
      else if (ga->entries[i].member->ph->index != ga->entries[i].idx)
        {
          OB_LOG_BUG_CODE (0x20105026,
                           "entry %u, hose '%s': index was %" OB_FMT_64
                           "d and is now %" OB_FMT_64 "d\n",
                           i, ga->entries[i].member->ph->hose_name,
                           ga->entries[i].idx,
                           ga->entries[i].member->ph->index);
          ob_err_accum (&tort, OB_UNKNOWN_ERR);
        }
    }
  free (ga);
  return tort;
}

static inline size_t saturating_decrement (size_t x)
{
  return (x > 0 ? x - 1 : x);
}

/* Given "foo/bar/baz" and "crud", produces "foo/bar/crud/baz".
 * May reallocate *pathp, hence the indirection.
 *
 * Implementation note: Yes, I should be banished to the ninth
 * level of Hell for trying to do this much string manipulation
 * in plain old C.  We have this wonderful Str class, but it is
 * in C++, and we are not allowed to use it from here.  Hence all
 * of the extra code to help with debugging when running under
 * valgrind.
 */
ob_retort ob_insert_penultimate_directory (char **pathp, const char *insert)
{
  char *path = *pathp;
  const size_t pathlen = strlen (path);
  const size_t inslen = strlen (insert);
  const size_t slashpos = saturating_decrement (ob_basename (path) - path);
  const size_t newlen = pathlen + inslen + 2;
  path = (char *) realloc (path, newlen);
  if (!path)
    return OB_NO_MEM;

  char *dest = path + slashpos + inslen + 1;
  const char *src = path + slashpos;
  size_t len = 1 + pathlen - slashpos;
  if (0 != VALGRIND_CHECK_MEM_IS_ADDRESSABLE (dest, len))
    OB_LOG_ERROR_CODE (0x20105030,
                       "dest not addressable in '%s'\n"
                       "pathlen = %" OB_FMT_SIZE "u\n"
                       "newlen = %" OB_FMT_SIZE "u\n"
                       "dest = %" OB_FMT_SIZE "u\n"
                       "src = %" OB_FMT_SIZE "u\n"
                       "len = %" OB_FMT_SIZE "u\n",
                       path, pathlen, newlen, (size_t) (dest - path),
                       (size_t) (src - path), len);
  if (0 != VALGRIND_CHECK_MEM_IS_DEFINED (src, len))
    OB_LOG_ERROR_CODE (0x20105031,
                       "src not defined in '%s'\n"
                       "pathlen = %" OB_FMT_SIZE "u\n"
                       "newlen = %" OB_FMT_SIZE "u\n"
                       "dest = %" OB_FMT_SIZE "u\n"
                       "src = %" OB_FMT_SIZE "u\n"
                       "len = %" OB_FMT_SIZE "u\n",
                       path, pathlen, newlen, (size_t) (dest - path),
                       (size_t) (src - path), len);
  memmove (dest, src, len);

  dest = path + slashpos + 1;
  src = insert;
  len = inslen;
  if (0 != VALGRIND_CHECK_MEM_IS_ADDRESSABLE (dest, len))
    OB_LOG_ERROR_CODE (0x20105032, "dest not addressable in '%s'\n", path);
  if (0 != VALGRIND_CHECK_MEM_IS_DEFINED (src, len))
    OB_LOG_ERROR_CODE (0x20105033, "src not defined in '%s'\n", path);
  memcpy (dest, src, len);

  *pathp = path;
  return OB_OK;
}

static ob_retort single_file_notification_dir (const char *single_file,
                                               char **path_ret)
{
  char *path = NULL;
  ob_retort tort = ob_realpath (single_file, &path);
  if (tort < OB_OK)
    return tort;
  tort = ob_insert_penultimate_directory (&path, ".notification");
  if (tort < OB_OK)
    {
      free (path);
      return tort;
    }
  *path_ret = path;
  return tort;
}

ob_retort pool_notify_dir_path (char *dir_path, const char *poolName, unt8 pdv)
{
  ob_retort pret = pool_build_pool_dir_path (dir_path, poolName);
  if (pret != OB_OK)
    return pret;
  if (pdv == POOL_DIRECTORY_VERSION_SINGLE_FILE)
    {
      char *notpath = NULL;
      pret = single_file_notification_dir (dir_path, &notpath);
      if (pret < OB_OK)
        return pret;
      ob_safe_copy_string (dir_path, PATH_MAX, notpath);
      free (notpath);
    }
  else
    {
      pret = pool_add_path_element (dir_path, "notification");
      if (pret != OB_OK)
        return pret;
    }
  return OB_OK;
}

ob_retort pool_init_multi_await (pool_hose ph, int mode, int uid, int gid)
{
  char dir_path[PATH_MAX];
  ob_retort pret =
    pool_notify_dir_path (dir_path, ph->name, ph->pool_directory_version);
  if (pret != OB_OK)
    return pret;

#ifndef _MSC_VER
  if (mode < 0)
    mode = 0777;
  mode |= S_ISVTX;
#endif
  return ob_mkdir_p_with_permissions (dir_path, mode, uid, gid);
}

ob_retort pool_load_multi_await (pool_hose ph, int mode, int uid, int gid)
{
  ob_retort tort = pool_init_multi_await (ph, mode, uid, gid);
  if (tort < OB_OK)
    return tort;
  return pool_create_fifo_path (ph);
}

static int filter_out_directories (const struct dirent *entry)
{
  return (entry->d_type != DT_DIR);
}

ob_retort pool_destroy_multi_await (pool_hose ph)
{
  char dir_path[PATH_MAX];
  ob_retort pret =
    pool_notify_dir_path (dir_path, ph->name, ph->pool_directory_version);
  if (pret != OB_OK)
    return pret;

  struct dirent **namelist = NULL;
  const int nentries =
    ob_scandir (dir_path, &namelist, filter_out_directories, NULL);

  if (nentries < 0 && errno == ENOENT)
    {
      // Don't consider missing directory an error.
      return OB_OK;
    }
  else if (nentries < 0)
    return ob_errno_to_retort (errno);

  int entno;
  for (entno = 0; entno < nentries; entno++)
    {
      struct dirent *entry = namelist[entno];

      slaw s_fifo_path =
        slaw_string_format ("%s%c%s", dir_path, OB_DIR_CHAR, entry->d_name);
      const char *fifo_path = slaw_string_emit (s_fifo_path);

      ob_log (OBLV_DBUG, 0x20105023, "Unlinking %s during pool destruction\n",
              fifo_path);
      OB_CHECK_POSIX_CODE (0x2010502b, unlink (fifo_path));
      slaw_free (s_fifo_path);
    }

  ob_scandir_free (namelist, nentries);

  // Now remove the dir itself
  ob_retort err = ob_rmdir_p (dir_path);
  if (err < OB_OK)
    OB_PERRORF_CODE (0x2010502d, "Couldn't remove %s during pool destruction",
                     dir_path);
  // The fifo is per-pool_hose, not per-pool, don't destroy it here
  return err;
}


int64 pool_gang_count (pool_gang gang)
{
  if (!gang)
    return -1;

  pool_gang_member member;
  int64 count = 0;

  for (member = gang->leader; member != NULL; member = member->next)
    count++;

  return count;
}


pool_hose pool_gang_nth (pool_gang gang, int64 n)
{
  if (!gang || n < 0)
    return NULL;

  int64 i;
  pool_gang_member member = gang->leader;

  for (i = 0; i < n && member != NULL; i++)
    member = member->next;

  if (member)
    return member->ph;
  else
    return NULL;
}
