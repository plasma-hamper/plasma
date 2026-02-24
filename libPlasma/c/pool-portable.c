
/* (c)  oblong industries */

#include "libPlasma/c/private/pool-portable.h"
#include "libPlasma/c/private/fifo_ops.h"
#include "libPlasma/c/private/pool_impl.h"

#include "libLoam/c/ob-file.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-util.h"

ob_retort ob_select2_prepare (ob_select2_t *sel, ob_select2_dir dir, int sock,
                              ob_handle_t event)
{
  OB_CLEAR (*sel);
  sel->sock_fd = sock;
#ifdef _MSC_VER
  //create an event that allows us to WaitForMultipleObjects using the
  //socket
  if (sock >= 0)
    {
      WSAEVENT socketEvent = WSACreateEvent ();
      if (socketEvent == WSA_INVALID_EVENT)
        {
          OB_LOG_ERROR_CODE (0x20107000, "WSACreateEvent failed - %d\n",
                             WSAGetLastError ());
          return OB_UNKNOWN_ERR;
        }
      if (0
          != WSAEventSelect (sock, socketEvent,
                             FD_CLOSE
                               | (dir == OB_SEL2_SEND ? FD_WRITE : FD_READ)))
        {
          OB_LOG_ERROR_CODE (0x20107001, "WSAEventSelect failed - %d\n",
                             WSAGetLastError ());
          return OB_UNKNOWN_ERR;
        }
      sel->waitHandles[0] = socketEvent;
    }
  sel->waitHandles[1] = event;
#else
  int i, fd, nfds;
  sel->wake_fd = event;
  FD_ZERO (&(sel->read_fds));
  FD_ZERO (&(sel->write_fds));
  FD_ZERO (&(sel->except_fds));
  nfds = 0;
  for (i = 0, fd = sock; i < 2; i++, fd = event)
    {
      if (fd >= 0)
        {
          if (fd >= FD_SETSIZE)
            {
              OB_LOG_ERROR_CODE (0x20107002,
                                 "file descriptor %d exceeds FD_SETSIZE (%d)\n",
                                 fd, FD_SETSIZE);
              return POOL_FILE_BADTH;
            }
          if (fd >= nfds)
            nfds = fd + 1;
          FD_SET (fd, &(sel->except_fds));
          if (i == 0 && dir == OB_SEL2_SEND)
            FD_SET (fd, &(sel->write_fds));
          else
            FD_SET (fd, &(sel->read_fds));
        }
    }
  sel->nfds = nfds;
#endif
  return OB_OK;
}

ob_retort ob_select2 (ob_select2_t *sel, pool_timestamp timeout, bool consume)
{
#ifdef _MSC_VER
  ob_retort tort;
  DWORD lasterr;
  DWORD wait_milliseconds = pool_timeout_to_wait_millis (timeout);
  DWORD wait_res =
    WaitForMultipleObjects (2 - (sel->waitHandles[1] == OB_NULL_HANDLE),
                            sel->waitHandles, false, wait_milliseconds);

  switch (wait_res)
    {
      case WAIT_OBJECT_0:
        // the socket woke us up
        return OB_OK;

      case WAIT_OBJECT_0 + 1:
        // the event woke us up
        return POOL_AWAIT_WOKEN;

      case WAIT_TIMEOUT:
        return POOL_AWAIT_TIMEDOUT;

      case WAIT_FAILED:
        lasterr = GetLastError ();
        tort = ob_win32err_to_retort (lasterr);
        OB_LOG_ERROR_CODE (0x20107003,
                           "WaitForMultipleObjects failed with \"%s\"\n",
                           ob_error_string (tort));
        return tort;

      default:
        // http://msdn.microsoft.com/en-us/library/ms687025(VS.85).aspx
        OB_LOG_ERROR_CODE (0x20107004,
                           "unexpected return value for "
                           "WaitForMultipleObjects = 0x%08x\n",
                           wait_res);
        return OB_UNKNOWN_ERR;
    }
#else
  struct timeval *tp;
  struct timeval timeout_tv;
  pool_timestamp target = OB_NAN;

  for (;;)
    {
      fd_set read_fds, write_fds, except_fds;
      tp = pool_timeout_to_timeval (
        private_incremental_timeout (timeout, &target), &timeout_tv);
      read_fds = sel->read_fds;
      write_fds = sel->write_fds;
      except_fds = sel->except_fds;
      int nready = select (sel->nfds, &read_fds, &write_fds, &except_fds, tp);
      if (nready < 0)
        {
          if (errno == EINTR)
            continue;
          return ob_errno_to_retort (errno);
        }
      else if (nready == 0)
        return POOL_AWAIT_TIMEDOUT;
      else if (sel->wake_fd >= 0
               && (FD_ISSET (sel->wake_fd, &read_fds)
                   || FD_ISSET (sel->wake_fd, &except_fds)))
        {
          if (consume)
            {
              byte one[PRETTY_DARN_LARGE];
              // read off any waiting bytes (get them all to avoid bug 771)
              if (read (sel->wake_fd, &one, sizeof (one)) < 0)
                return ob_errno_to_retort (errno);
            }
          return POOL_AWAIT_WOKEN;
        }
      else if (sel->sock_fd >= 0
               && (FD_ISSET (sel->sock_fd, &read_fds)
                   || FD_ISSET (sel->sock_fd, &write_fds)
                   || FD_ISSET (sel->sock_fd, &except_fds)))
        return OB_OK;
      else
        OB_LOG_ERROR_CODE (0x20107005,
                           "Shouldn't happen: I woke up but don't know why. "
                           "Hitting snooze...\n");
    }
#endif
}

ob_retort ob_select2_finish (ob_select2_t *sel)
{
#ifdef _MSC_VER
  if (sel->waitHandles[0] != OB_NULL_HANDLE)
    {
      if (0 != WSAEventSelect (sel->sock_fd, 0, 0))
        {
          OB_LOG_ERROR_CODE (0x20107006, "WSAEventSelect failed - %d\n",
                             WSAGetLastError ());
          return OB_UNKNOWN_ERR;
        }

      if (!WSACloseEvent (sel->waitHandles[0]))
        {
          OB_LOG_ERROR_CODE (0x20107007, "WSACloseSocket returned false\n");
          return OB_UNKNOWN_ERR;
        }
    }
#else
// Nothing to do on UNIX?
#endif
  OB_CLEAR (*sel);
  return OB_OK;
}

/* ---------------------------------------------------------------------- */

ob_retort ob_wake_up (const wakeup_stuff *w)
{
#ifdef _MSC_VER
  if (w->wakeup_event_handle == 0)
    {
      OB_LOG_ERROR_CODE (0x20107008,
                         "can't wakeup because wakeup_event_handle==0\n");
      return POOL_WAKEUP_NOT_ENABLED;
    }

  if (!SetEvent (w->wakeup_event_handle))
    {
      const ob_retort tort = ob_win32err_to_retort (GetLastError ());
      OB_LOG_ERROR_CODE (0x20107009, "error on call to SetEvent - %s\n",
                         ob_error_string (tort));
      return tort;
    }

#else
  const byte one = 0;

  if (w->wakeup_write_fd < 0)
    return POOL_WAKEUP_NOT_ENABLED;

  if (write (w->wakeup_write_fd, &one, sizeof (one)) != sizeof (one))
    {
      const int erryes = errno;
      if (erryes == EAGAIN)
        return OB_OK;  // this is okay; expected when in non-blocking mode
      OB_LOG_ERROR_CODE (0x2010700a, "error waking up process: %s\n",
                         strerror (erryes));
      return ob_errno_to_retort (erryes);
    }
#endif

  return OB_OK;
}

void ob_initialize_wakeup (wakeup_stuff *w)
{
#ifdef _MSC_VER
  w->wakeup_event_handle = 0;
#else
  w->wakeup_write_fd = -1;
  w->wakeup_read_fd = -1;
#endif
}

ob_retort ob_enable_wakeup (wakeup_stuff *w)
{
#ifdef _MSC_VER
  // DO NOT DO ANYTHING if already enabled!
  if (w->wakeup_event_handle != 0)
    return OB_OK;

  //creates a manual-reset event
  HANDLE new_handle = CreateEvent (NULL,   //no extra security attributes
                                   false,  //auto reset, NOT manual-reset
                                   false,  //initially non-signaled
                                   NULL);  //unnamed

  if (new_handle == 0 || new_handle == INVALID_HANDLE_VALUE)
    {
      DWORD lasterr = GetLastError ();
      ob_retort tort = ob_win32err_to_retort (lasterr);
      OB_LOG_ERROR_CODE (0x2010700b, "Error on CreateEvent - %s\n",
                         ob_error_string (tort));
      return tort;
    }

  w->wakeup_event_handle = new_handle;
#else
  // DO NOT DO ANYTHING if already enabled!
  if (w->wakeup_write_fd >= 0)
    return OB_OK;

  int pair[2];
  ob_retort tort = ob_socketpair_cloexec (AF_UNIX, SOCK_STREAM, 0, pair);
  if (tort < OB_OK)
    {
      OB_LOG_ERROR_CODE (0x2010700c, "error creating wakeup socketpair: %s\n",
                         ob_error_string (tort));
      return tort;
    }

  if (-1 == fcntl (pair[0], F_SETFL, fcntl (pair[0], F_GETFL, 0) | O_NONBLOCK))
    OB_PERROR_CODE (0x2010700d, "fcntl badness");

  w->wakeup_write_fd = pair[0];
  w->wakeup_read_fd = pair[1];
#endif

  return OB_OK;
}

ob_retort ob_cleanup_wakeup (wakeup_stuff *w)
{
  ob_retort tort = OB_OK;
#ifdef _MSC_VER
  if (w->wakeup_event_handle != OB_NULL_HANDLE
      && !CloseHandle (w->wakeup_event_handle))
    {
      tort = ob_win32err_to_retort (GetLastError ());
      OB_LOG_ERROR_CODE (0x2010700e, "error closing wakeup_event_handle - %s\n",
                         ob_error_string (tort));
    }
  w->wakeup_event_handle = 0;
#else
  if (w->wakeup_write_fd >= 0)
    OB_CHECK_POSIX_CODE (0x2010700f, close (w->wakeup_write_fd));
  if (w->wakeup_read_fd >= 0)
    OB_CHECK_POSIX_CODE (0x20107010, close (w->wakeup_read_fd));
  w->wakeup_write_fd = w->wakeup_read_fd = -1;
#endif
  return tort;
}
