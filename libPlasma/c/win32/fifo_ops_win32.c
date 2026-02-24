
/* (c)  oblong industries */

/* So, here's the thing:
 * libPlasma/c/fifo_ops.c - only used on non-Windows
 * libPlasma/c/win32/fifo_ops_win32.c - only used on Windows
 * libPlasma/c/pool-fifo.c - used on both!
 * libPlasma/c/private/fifo_ops.h - prototypes for all of the above .c files
 */

///
/// Await/awake via fifos
///

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <math.h>

#include "libLoam/c/ob-sys.h"

#include "../pool.h"
#include "../private/pool_impl.h"
#include "../private/pool_multi.h"

/// "fifos" are implemented on win32 using manual reset events, created
/// using the following windows method -
/// HANDLE WINAPI CreateEvent(
///  __in_opt  LPSECURITY_ATTRIBUTES lpEventAttributes,
///  __in      BOOL bManualReset,
///  __in      BOOL bInitialState,
///  __in_opt  LPCTSTR lpName
///  );
///
/// CreateEvent(0, TRUE, 0, fifo_name);
///
/// on linux/osx, fifos were (allegedly chosen because of their ability to be
/// select()ed alongside socket handles, making it possible to simultaneously
/// wait on pool deposits as well as incoming packets from connected clients
///
/// on Windows, we are able to use the WaitForMultipleObjects function to
/// wait on manual-reset events alongside sockets - see pool_net.c for the
/// actual spot where this takes place
///
/// However, we still create files inside the notification directory
/// on Windows, too.  It appears that these are empty, regular files and
/// exist only to keep track of which manual-reset events we've created.
/// (i. e. so we can iterate over the files in the directory, in order
/// to notify each manual-reset event).  This seems a bit hackish, but
/// presumably Trey had a reason.

/// XXX shouldn't this go into some other file? pool.h / pool.c?

void pool_fifo_multi_destroy_awaiter (pool_hose ph) {}

DWORD
pool_timeout_to_wait_millis (pool_timestamp timeout)
{
  DWORD rval = 0;

  if (timeout == POOL_WAIT_FOREVER)
    {
      // wait forever => INFINITE to WaitForSingle/MultipleObject
      rval = INFINITE;
    }
  else if (timeout == POOL_NO_WAIT)
    {
      // don't wait => 0 to WaitForSingle/MultipleObject
      rval = 0;
    }
  else if (timeout < 0)
    {
      OB_LOG_BUG_CODE (0x2010b000,
                       "negative timeout in pool_timeout_to_wait_millis %lf\n",
                       timeout);
      rval = 0;
    }
  else
    {
      // Using floor() can lead to not using up the rest of this respiration
      // cycle's time, which means next call to RespireSingly takes zero
      // time, which causes test failures.  So use ceil() instead.
      rval = (DWORD) ceil (timeout * 1000.0);
    }

  return rval;
}

void pool_fifo_multi_remove_awaiter (pool_hose ph)
{
  pool_notification_lock (ph);

  if (ph->notify_handle == 0)
    {
      // We never awaited, nothing to do
      pool_notification_unlock (ph);
      return;
    }

  BOOL res = CloseHandle ((HANDLE) ph->notify_handle);
  ph->notify_handle = 0;

  ob_log (OBLV_DBUG, 0x2010b002,
          "%d: removing awaiter for pool %s: unlinking %s\n", getpid (),
          ph->name, ph->fifo_path);

  //this unlink may not necessarily be a significant failure because
  //if we were woke by a notifier in wake_awaiters, they already
  //unlinked this file
  if (unlink (ph->fifo_path) != 0)
    ob_log (OBLV_DBUG, 0x2010b003, "Couldn't unlink fifo %s: ", ph->fifo_path);

  pool_notification_unlock (ph);
}

ob_retort pool_fifo_multi_add_awaiter (pool_hose ph, protein *ret_prot,
                                       pool_timestamp *ret_ts, int64 *ret_index)
{
  ob_log (OBLV_DBUG, 0x2010b004, "%d: making fifo %s\n", getpid (),
          ph->fifo_path);

  pool_notification_lock (ph);

  // We can't absolutely guarantee a unique fifo name without a lot of
  // trouble.  So we create a probably unique fifo name and if it
  // collides with another pool hose's fifo name, we create another
  // probably unique file name, until we succeed.

  HANDLE new_notify_handle = 0;
  char new_name[MAX_PATH];

  while (1)
    {
      snprintf (new_name, sizeof (new_name), "%s", ph->fifo_path);

      prepare_windows_handle_name (new_name);

      HANDLE new_event = CreateEvent (NULL,   //no extra security attributes
                                      TRUE,   //manual-reset
                                      false,  //initially non-signaled
                                      new_name);

      DWORD err = GetLastError ();

      if (new_event == 0)
        OB_FATAL_ERROR_CODE (0x2010b005,
                             "unable to create wait event for pool "
                             "notification, error %d",
                             err);

      if (err == ERROR_ALREADY_EXISTS)
        {
          //we arent going to use this event object so make sure we let
          //go of our handle to it
          CloseHandle (new_event);

          //we will re-generate a fifo name and loop around
          pool_create_fifo_path (ph);
        }
      else
        {
          HANDLE notify_file = CreateFile (ph->fifo_path, GENERIC_WRITE, 0, 0,
                                           CREATE_ALWAYS, 0, 0);

          if (notify_file)
            {
              //ok cool, we have a fresh event object and we were able to leave a file
              //in the notification folder which will be used (in name only) to wake up
              //awaiters later on
              new_notify_handle = new_event;

              //(dont leave the file open)
              CloseHandle (notify_file);
            }
          else
            {
              //close the event we created
              CloseHandle (new_event);

              OB_FATAL_ERROR_CODE (0x2010b006,
                                   "couldn't create notification file: %s",
                                   ph->fifo_path);
            }

          //break out of while(), we're done
          break;
        }
    }

  ob_log (OBLV_DBUG, 0x2010b007, "pid %d: opening fifo %s\n", getpid (),
          new_name);
  ph->notify_handle = new_notify_handle;
  ob_log (OBLV_DBUG, 0x2010b008,
          "pid %d: notification set up for %s fifo %s fd %d\n", getpid (),
          ph->name, new_name, ph->notify_handle);

  // XXX Note that there is a pathological case where a process
  // repeatedly dies without calling pool_withdraw(), filling up the
  // notification dir with fifos to the point that the directory (or
  // the file system, or whatever) runs out of links (or space or
  // whatever).

  // (linux/osx) This can be remedied by catching ENOSPC and ENFILE,
  // grabbing the notification lock, opening each fifo, and removing
  // any that return ENXIO.  The fifo notification function does
  // exactly this, so we can steal code from there to fix this problem
  // if necessary.
  //
  // XXX unclear what the appropriate behavior is on windows

  pool_notification_unlock (ph);

  // Attempt a pool_next() - this is mainly here so that we can
  // implement this function efficiently for network pools, instead of
  // calling add awaiter and then the pool_next() and THEN
  // start_await(), etc. etc.
  ob_retort pret = pool_next (ph, ret_prot, ret_ts, ret_index);
  if (pret == OB_OK)
    {
      // Notifiers are supposed to tear down the await state for us,
      // but notification for this protein may have occurred before we
      // set up our await state.  Tear it down ourselves just in case.
      pool_fifo_multi_remove_awaiter (ph);
    }
  return pret;
}

ob_retort pool_fifo_wake_awaiters (pool_hose ph)
{
  // Read each entry in the notification dir and write a byte to it
  pool_notification_lock (ph);

  const char *notifydir = ph->notify_dir_path;
  char notify_path[MAX_PATH];
  snprintf (notify_path, sizeof (notify_path), "%s\\*", ph->notify_dir_path);

  WIN32_FIND_DATA find_data;
  HANDLE find_handle = FindFirstFile (notify_path, &find_data);
  while (1)
    {
      WIN32_FIND_DATA find_data;

      if (!FindNextFile (find_handle, &find_data))
        break;  //no more files to deal with

      if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
          //nothing
        }
      else
        {
          char file_name[MAX_PATH];
          snprintf (file_name, sizeof (file_name), "%s\\%s",
                    ph->notify_dir_path, find_data.cFileName);

          char event_name[MAX_PATH];
          strcpy (event_name, file_name);

          prepare_windows_handle_name (event_name);

          HANDLE wake_up_handle =
            OpenEvent (EVENT_MODIFY_STATE, FALSE, event_name);

          //in any case, we are going to delete the notification file
          if (unlink (file_name) != 0)
            {
              OB_LOG_ERROR_CODE (0x2010b009,
                                 "wake_awaiters could not unlink "
                                 "%s - errno %d\n",
                                 file_name, errno);
            }
          if (wake_up_handle)
            {
              //this should wake the waiter
              SetEvent (wake_up_handle);

              CloseHandle (wake_up_handle);
            }
        }
    }

  FindClose (find_handle);

  pool_notification_unlock (ph);
  return OB_OK;
}

ob_retort pool_fifo_await_next_single (pool_hose ph, pool_timestamp timeout,
                                       protein *ret_prot,
                                       pool_timestamp *ret_ts, int64 *ret_index)
{
  // Add a fifo and do a final check for a protein deposit
  ob_retort pret =
    pool_fifo_multi_add_awaiter (ph, ret_prot, ret_ts, ret_index);
  // Return on either success or non-transient failure, otherwise we wait
  if (pret != POOL_NO_SUCH_PROTEIN)
    return pret;

  //remember, notify_handle is a windows manual-reset event. it is created at the
  //top of this function, and will be non-signaled until someone else calls
  //the notification routines to wake us up

  DWORD wait_milliseconds = pool_timeout_to_wait_millis (timeout);

  while (1)
    {
      HANDLE wait_handles[2];
      int num_handles = 0;

      wait_handles[num_handles] = ph->notify_handle;
      num_handles++;

      //include the wakeup event too?
      if (ph->w.wakeup_event_handle != 0)
        {
          wait_handles[num_handles] = ph->w.wakeup_event_handle;
          num_handles++;
        }

      DWORD wait_milliseconds = pool_timeout_to_wait_millis (timeout);

      DWORD wait_res = WaitForMultipleObjects (num_handles, wait_handles, false,
                                               wait_milliseconds);

      switch (wait_res)
        {
          case WAIT_OBJECT_0:
            //we've been notified by pool deposit
            pret = OB_OK;
            break;

          case WAIT_OBJECT_0 + 1:
            //we've been notified by hose wakeup
            pret = POOL_AWAIT_WOKEN;
            break;

          case WAIT_TIMEOUT:
            //we timed out before being notified (not expected)
            ob_log (OBLV_DBUG, 0x2010b00a, "WaitForSingleObject timed out\n");
            pret = POOL_AWAIT_TIMEDOUT;
            break;

          default:
            OB_LOG_ERROR_CODE (
              0x2010b00b, "unknown error WaitForSingleObject - %d\n", wait_res);
            pret = POOL_FIFO_BADTH;
            break;
        }

      if (pret != OB_OK)
        break;

      pret = pool_next (ph, ret_prot, ret_ts, ret_index);

      if (pret == OB_OK)
        break;

      // Quit for non-transient errors
      if (pret != POOL_NO_SUCH_PROTEIN)
        break;

      //i suppose i'm going to wait a second time now?
      //OutputDebugString("await_single waiting again...\n");
    }

  pool_fifo_multi_remove_awaiter (ph);
  return pret;
}
