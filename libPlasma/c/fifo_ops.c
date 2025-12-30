
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
//#undef _GNU_SOURCE
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <math.h>

#include "libPlasma/c/pool.h"
#include "libPlasma/c/private/pool_impl.h"
#include "libPlasma/c/private/pool_multi.h"
#include "libLoam/c/ob-dirs.h"
#include "libLoam/c/ob-file.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-string.h"

/// Some problems arise when a fifo has no listener, detected by an
/// ENXIO return value from a write.  This can happen if a process
/// dies unexpectedly during an await and fails to remove its fifo.
/// This is solved by having the notifier remove any notification fds
/// that return ENXIO.
///
/// This solution creats a new problem: if a fifo has just been
/// created but not opened yet by the process (fifos do not have an
/// equivalent to open() with O_CREAT), it will returns ENXIO when a
/// notifier opens it, and the notifier will then remove it.  This is
/// solved by having an exclusive notification lock which is held
/// while setting up the await fifo and while a depositor is notifying
/// awaiters.
///
/// Fifos are opened in non-blocking mode since otherwise the opener
/// will block until a corresponding reader or writer opens the same
/// fifo.  We don't want the notifier to block, since it needs to
/// finish notifying all the other awaiters and drop the notification
/// lock.  We don't want the listener to block since we need to go on
/// to do the select().  We can't just block on the open, waiting for
/// a deposit notification, for two reasons: we may have a timeout on
/// the await, which requires us to go on to the select(), and we may
/// be awaiting on multiple pools, in which case we need to go on to
/// the next pool hose.  So we use O_NONBLOCK on open and check for
/// EAGAIN on reads and ENXIO on writes.  Note that we have to be very
/// careful about opening the channel write-only from the depositor
/// and read-only from the reader, because we only get the right
/// return values when, e.g., no one has the channel open for writing
/// when no notifications are going to come in.  Note that select()
/// will block on fifos opened with O_NONBLOCK.

struct timeval *pool_timeout_to_timeval (pool_timestamp timeout,
                                         struct timeval *ret_tv)
{
  // timeout == -1 => wait forever => NULL arg to select
  if (timeout == POOL_WAIT_FOREVER)
    return NULL;
  // timeout == 0 => don't wait => timeval with all zeroes to select
  if (timeout == POOL_NO_WAIT)
    timeout = 0;
  // Fill out timeval
  ret_tv->tv_sec = floor (timeout);
  ret_tv->tv_usec = (timeout - floor (timeout)) / 0.000001;
  return ret_tv;
}

//the data in the fifo isn't used but we clear it
//to use readability as a signal that there are new proteins
static char junk_buff[PIPE_BUF];
void pool_fifo_multi_remove_awaiter (pool_hose ph)
{
  pool_notification_lock (ph);

  if (ph->notify_handle < 0)
    {
      // We never awaited, nothing to do
      pool_notification_unlock (ph);
      return;
    }

  ssize_t rds = 0;
  while ((rds = read (ph->notify_handle, junk_buff, PIPE_BUF)) > 0)
    ;
  if (rds < 0 && errno != EAGAIN)
    ob_log (OBLV_DBUG, 0x2010002b, "error clearing out fifo in remove %s\n",
            ph->name);

  pool_notification_unlock (ph);
}

void pool_fifo_multi_destroy_awaiter (pool_hose ph)
{
  // this is called from the clean up code, which can be called before
  // or after the notication lock operations are set.  in that case, we
  // don't have to clean up our notification detritus (fifo) because
  // we didn't create one yet.
  if (ph->lock_ops == &pool_null_ops)
    {
      //double-checking, things could go wrong here.
      if (ph->notify_handle > 0)
        {
          close (ph->notify_handle);  //shouldn't get here.
          ph->notify_handle = -1;
        }
      if (ph->fifo_path[0] != 0)
        {
          unlink (ph->fifo_path);  //shouldn't get here.
          ph->fifo_path[0] = 0;
        }

      return;
    }

  pool_notification_lock (ph);

  if (ph->notify_handle < 0)
    {
      // We never awaited, nothing to do
      pool_notification_unlock (ph);
      return;
    }

  OB_CHECK_POSIX_CODE (0x2010001f, close (ph->notify_handle));
  ph->notify_handle = -1;
  ob_log (OBLV_DBUG, 0x20100003, "removing awaiter for pool %s: unlinking %s\n",
          ph->name, ph->fifo_path);
  if (ph->fifo_path[0] != 0)
    if (unlink (ph->fifo_path) != 0)
      ob_log (OBLV_DBUG, 0x20100004,
              "Couldn't unlink fifo %s: ", ph->fifo_path);

  ph->fifo_path[0] = 0;
  pool_notification_unlock (ph);
}

static ob_retort
make_fifo_and_permissionize (const char *path, const pool_perms *perms,
                             const char *eventual_path_after_rename)
{
  // Originally the mode for mkfifo was 0644 ("rw-r--r--"), but
  // unfortunately Val didn't leave any clue for why she chose that.
  // But my (Mignon) reasoning is that only the current process
  // (and thus only the current user) needs to read from the notification
  // fifo, but any process (and thus any user) might need to write to
  // the fifo to notify us of a protein deposit.  Therefore, I think
  // the correct mode is 0622 ("rw--w--w-").
  const int mode = 0622;
  if (mkfifo (path, mode) < 0)
    {
      const int erryes = errno;
      bool thats_an_error = true; /* Ugh; control flow contortions */
#ifdef __APPLE__
      if (erryes == EINVAL)
        {
          // bug 1088: on OS X, when ACLs are in effect, mkfifo
          // fails with EINVAL, although the fifo seems to be created
          // successfully.  So, if it looks like the fifo was created
          // despite the error, we will assume it is this bizarre
          // case and ignore the EINVAL.
          struct stat ss;
          if (0 == stat (path, &ss) && S_ISFIFO (ss.st_mode))
            thats_an_error = false;
        }
#endif
      if (thats_an_error)
        {
          if (erryes != EEXIST)
            {
              // Unknown error, give up
              OB_LOG_ERROR_CODE (0x20100006,
                                 "mkfifo (\"%s\", %04o) failed: errno %d (%s)",
                                 path, mode, erryes, strerror (erryes));
              return POOL_FIFO_BADTH;
            }

          return ob_errno_to_retort (erryes);
        }
    }

  // The user can override the default 0622 if they want, by specifying
  // options when the pool is created.
  // XXX: I store the permission info in the config file.  Which could cause
  // various sorts of issues if the pool is moved to another machine
  // with different users or groups.  Hmmmmm.
  // YYY: Now I'm storing it in the backing file instead of the config
  // file, but the argument is the same: new machine might have different
  // users or groups.
  const int new_mode = pool_combine_modes (perms->mode, 0622);
  const ob_retort err =
    ob_permissionize (path, new_mode, perms->uid, perms->gid);
  if (err < OB_OK)
    OB_LOG_WARNING_CODE (0x20100027,
                         "When creating the notification fifo:\n%s\n"
                         "the call:\n"
                         "ob_permissionize (\"%s\", %04o, %d, %d);\n"
                         "returned the error:\n%s\n"
                         "which could potentially result in some programs\n"
                         "being unable to notify us of new proteins, because\n"
                         "they do not have permission to write to our fifo.\n",
                         eventual_path_after_rename, path, new_mode, perms->uid,
                         perms->gid, ob_error_string (err));
  return OB_OK;
}

typedef ob_retort (*fifo_func) (const char *fifaux, const char *fifo_path,
                                const pool_perms *perms);

static ob_retort rename_fifo (const char *fifaux, const char *fifo_path,
                              OB_UNUSED const pool_perms *perms)
{
  int ret = ob_non_overwriting_rename (fifaux, fifo_path);
  if (ret < 0)
    return ob_errno_to_retort (errno);
  return OB_OK;
}

static ob_retort mfap_wrap (OB_UNUSED const char *fifaux, const char *fifo_path,
                            const pool_perms *perms)
{
  return make_fifo_and_permissionize (fifo_path, perms, fifo_path);
}

static inline bool is_empty_permission (const pool_perms *perms)
{
  return (perms->mode == -1 && perms->uid == -1 && perms->gid == -1);
}

/* This is code factored out of pool_fifo_multi_add_awaiter(),
 * simply to avoid having to release the lock on every error path. */
static ob_retort pool_fifo_multi_add_awaiter1 (pool_hose ph)
{
  char fifaux[PATH_MAX + 40];
  fifo_func fifun = mfap_wrap;
  bool created_tmp_fifo = false;
  ob_retort tort = ob_errno_to_retort (EEXIST);

  if (!ph->different_filesystem_than_tmp && !is_empty_permission (&ph->perms))
    {
      const char *tmpdir = ob_get_standard_path (ob_tmp_dir);

      while (tort < OB_OK)
        {
          if (tort != ob_errno_to_retort (EEXIST))
            return tort;

          ob_safe_copy_string (fifaux, PATH_MAX, tmpdir);
          char *name_buf =
            fifaux + ob_safe_append_string (fifaux, PATH_MAX, "/6943gub-");
          tort = pool_random_name (name_buf);
          if (tort < OB_OK)
            return tort;

          tort =
            make_fifo_and_permissionize (fifaux, &ph->perms, ph->fifo_path);
        }

      created_tmp_fifo = true;
      fifun = rename_fifo;
    }

  // We can't absolutely guarantee a unique fifo name without a lot of
  // trouble.  So we create a probably unique fifo name and if it
  // collides with another pool hose's fifo name, we create another
  // probably unique file name, until we succeed.
  //
  // For some background on why we check for EPERM in addition to EXDEV, see
  // https://lists.oblong.com/pipermail/buildtools/2012-September/000230.html

  while ((tort = fifun (fifaux, ph->fifo_path, &ph->perms)) < OB_OK)
    {
      if (tort != ob_errno_to_retort (EEXIST) && fifun == rename_fifo)
        {
          /* We only "expect" EXDEV or EPERM, so print a message if it's
           * not one of those. */
          if (tort != ob_errno_to_retort (EXDEV)
              && tort != ob_errno_to_retort (EPERM))
            OB_LOG_WARNING_CODE (0x20100028,
                                 "Unexpectedly got %" OB_FMT_64 "d (%s)\n"
                                 "when trying to move fifo.  Tell Mignon "
                                 "about it.\n",
                                 tort, ob_error_string (tort));
          /* delete the fifo we weren't able to move, so that they
           * don't accumulate in /tmp */
          unlink (fifaux);
          /* so we don't try to delete it a second time */
          created_tmp_fifo = false;
          /* temp dir is on a different filesystem than destination,
           * so we must create fifo in-place (and set the permissions
           * non-atomically), rather than moving the pre-made fifo. */
          fifun = mfap_wrap;
          ph->different_filesystem_than_tmp = true;
          continue;
        }

      if (tort != ob_errno_to_retort (EEXIST))
        return tort;

      // The fifo already exists.  Note that the most common cause of
      // this case is a programming bug, so print an error message.
      ob_log (OBLV_INFO, 0x20100007, "fifo %s already exists, probably a bug\n",
              ph->fifo_path);
      // Generate a new fifo name and try again
      const ob_retort err = pool_create_fifo_path (ph);
      if (err < OB_OK)
        {
          // Only a few unlikely errors can happen here, but all the more
          // reason to report them if they do...
          OB_LOG_ERROR_CODE (0x20100025,
                             "rather freaky: pool_create_fifo_path failed\n"
                             "because of '%s'\n",
                             ob_error_string (err));
          return err;
        }
    }

  if (created_tmp_fifo && fifun == mfap_wrap)
    {
      /* We created a temporary fifo but didn't use it.  So now, must
       * delete it. */
      OB_CHECK_POSIX_CODE (0x20100026, unlink (fifaux));
    }

  // XXX Note that there is a pathological case where a process
  // repeatedly dies without calling pool_withdraw(), filling up the
  // notification dir with fifos to the point that the directory (or
  // the file system, or whatever) runs out of links (or space or
  // whatever).  This can be remedied by catching ENOSPC and ENFILE,
  // grabbing the notification lock, opening each fifo, and removing
  // any that return ENXIO.  The fifo notification function does
  // exactly this, so we can steal code from there to fix this problem
  // if necessary.

  ob_log (OBLV_DBUG, 0x20100008, "opening fifo %s\n", ph->fifo_path);
  ph->notify_handle = ob_open_cloexec (ph->fifo_path, O_RDWR | O_NONBLOCK, 0);
  const int erryes = errno;
  ob_log (OBLV_DBUG, 0x20100009, "notification set up for %s fifo %s fd %d\n",
          ph->name, ph->fifo_path, ph->notify_handle);

  if (ph->notify_handle < 0)
    {
      unlink (ph->fifo_path);
      ob_log (OBLV_ERROR, 0x2010000a, "open %s failed: errno %d (%s)",
              ph->fifo_path, erryes, strerror (erryes));
      return POOL_FIFO_BADTH;
    }

#ifdef __linux
  if (fcntl (ph->notify_handle, F_SETPIPE_SZ, PIPE_BUF) < 0)
    {
      ob_log (OBLV_DBUG, 0x2010002c, "couldn't resize pipe for %s\n", ph->name);
    }
#endif

  return OB_OK;
}

/* if needed later */
/* ob_retort pool_fifo_multi_create_awaiter (pool_hose ph) */
/* { ob_log (OBLV_DBUG, 0x20100029, "making fifo %s\n", ph->fifo_path); */

/*   ob_retort err = pool_notification_lock (ph); */
/*   if (err != OB_OK) */
/*     return err; */

/*   err = pool_fifo_multi_add_awaiter1 (ph); */
/*   if (err < OB_OK) */
/*     ph->notify_handle = -1; */

/*   ob_err_accum (&err, pool_notification_unlock (ph)); */

/*   return err; */
/* } */

ob_retort pool_fifo_multi_add_awaiter (pool_hose ph, protein *ret_prot,
                                       pool_timestamp *ret_ts, int64 *ret_index)
{
  ob_log (OBLV_DBUG, 0x20100005, "continuing with fifo %s\n", ph->fifo_path);

  ob_retort err = pool_notification_lock (ph);
  if (err != OB_OK)
    return err;

  if (ph->notify_handle <= 0)
    {
      err = pool_fifo_multi_add_awaiter1 (ph);
      if (err < OB_OK)
        ph->notify_handle = -1;
    }


  ssize_t rds = read (ph->notify_handle, junk_buff, PIPE_BUF);
  if (rds < 0 && errno != EAGAIN)
    {
      ob_log (OBLV_DBUG, 0x2010002d, "error clearing out fifo pre-await (%s)\n",
              ph->name);
      pool_notification_unlock (ph);
      return POOL_FIFO_BADTH;
    }

  ob_err_accum (&err, pool_notification_unlock (ph));
  if (err != OB_OK)
    return err;

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
  struct dirent *entry;
  char fifo_path[PATH_MAX + 256];

  // Read each entry in the notification dir and write a byte to it

  ob_retort err = pool_notification_lock (ph);
  if (err != OB_OK)
    return err;

  DIR *dir = opendir (ph->notify_dir_path);
  if (!dir)
    {
      const int erryes = errno;
      OB_PERROR_CODE (0x20100020, ph->notify_dir_path);
      err = ob_errno_to_retort (erryes);
      goto finished;
    }

  // readdir() and friends aren't thread-safe, so use them only
  // inside the lock.  It may make sense to use readdir_r() instead.
  //
  // Very important!  readdir() caches directory entries.  You have to
  // begin the readdir() afresh inside the notification lock or you'll
  // get old stale data and whoa, that will screw things up.  In
  // particular, you'll miss awaiters who were added since the last
  // readdir() call.  It's okay to open the directory outside of the
  // lock - as long as it doesn't do something nasty like
  // pre-emptively read part of the directory (no implementation I
  // know of does this).

  ob_log (OBLV_DBUG, 0x2010000b, "notifying listeners for pool %s\n", ph->name);
  while ((entry = readdir (dir)) != NULL)
    {
      ob_log (OBLV_DBUG, 0x2010000c, "entry %s\n", entry->d_name);
      if ((strcmp (entry->d_name, ".") == 0)
          || (strcmp (entry->d_name, "..") == 0))
        continue;
      snprintf (fifo_path, sizeof (fifo_path), "%s/%s", ph->notify_dir_path,
                entry->d_name);
      ob_log (OBLV_DBUG, 0x2010000d, "notifying %s\n", fifo_path);
      // Don't exit on error on any particular listener; we want to
      // notify all listeners regardless of whether any individual
      // listener is screwed up.
      int fd = ob_open_cloexec (fifo_path, O_WRONLY | O_NONBLOCK, 0);
      if (fd < 0)
        {
          const int erryes = errno;
          if (erryes == ENXIO)
            {
              // The listener died without cleaning up - e.g., exited
              // without doing a pool_withdraw(), or crashed in the
              // middle of pool operations.
              ob_log (OBLV_DBUG, 0x2010000e, "Unlinking orphaned fifo %s\n",
                      fifo_path);
              if (unlink (fifo_path) != 0)
                ob_log (OBLV_DBUG, 0x2010000f,
                        "Couldn't unlink fifo %s: ", fifo_path);
              continue;
            }
          else if (erryes == EACCES)
            {
              // fifo not awaiting
              ob_log (OBLV_DBUG, 0x2010002a,
                      "ignoring file as we don't have permissions %s\n",
                      fifo_path);
              continue;
            }
          else
            {
              OB_LOG_ERROR_CODE (0x20100021,
                                 "encountered '%s' when opening notification "
                                 "fifo:\n%s\n",
                                 strerror (erryes), fifo_path);
              if (unlink (fifo_path) != 0)
                ob_log (OBLV_DBUG, 0x20100011,
                        "Couldn't unlink fifo %s: ", fifo_path);
              continue;
            }
        }
      // Write one byte to wake up the listener
      byte one;
      /* value doesn't matter, but leaving it uninitialized bothers valgrind */
      one = 0;
      int n = write (fd, &one, sizeof (one));
      if (n != sizeof (one))
        {
          if (errno == EAGAIN)
            {
              // Listener died case, as above
              ob_log (OBLV_DBUG, 0x20100012, "No one listening on fifo %s\n",
                      fifo_path);
            }
          else
            {
              // Some other error!  Don't die, but complain
              ob_log (OBLV_DBUG, 0x20100013,
                      "Error notifying via fifo %s: errno %d", fifo_path,
                      errno);
            }
        }
      // Very important not to leak open file descriptors!
      OB_CHECK_POSIX_CODE (0x20100022, close (fd));
    }
  ob_log (OBLV_DBUG, 0x20100015, "done notifying pool %s\n", ph->name);

  OB_CHECK_POSIX_CODE (0x20100023, closedir (dir));

finished:
  ob_err_accum (&err, pool_notification_unlock (ph));
  return err;
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
  // Select on the notification fd
  fd_set fds;
  FD_ZERO (&fds);
  fd_set zero_fds = fds;
  FD_SET (ph->notify_handle, &fds);
  int maxfd = ph->notify_handle + 1;
  if (ph->w.wakeup_read_fd >= 0)
    {
      FD_SET (ph->w.wakeup_read_fd, &fds);
      if (ph->w.wakeup_read_fd > ph->notify_handle)
        maxfd = ph->w.wakeup_read_fd + 1;
    }
  int nready;
  while (1)
    {
      // The fd set we call select with will get stomped, make a copy
      fd_set read_fds = fds;
      fd_set except_fds = fds;
      ob_log (OBLV_DBUG, 0x20100016, "selecting...\n");
      struct timeval *tp;
      struct timeval timeout_tv;
      tp = pool_timeout_to_timeval (timeout, &timeout_tv);
      nready = select (maxfd, &read_fds, NULL, &except_fds, tp);
      ob_log (OBLV_DBUG, 0x20100017, "woke up!\n");

      if (nready < 0)
        {
          // select() returned an error
          if (errno == EINTR)
            {
              // No problem, just restart
              ob_log (OBLV_DBUG, 0x20100018, "woke for EINTR\n");
              continue;
            }
          // Real error - give up, we can't fix this
          OB_PERROR_CODE (0x20100024, "select() failed in fifo_ops.c");
          pret = POOL_FIFO_BADTH;
          break;
        }
      else if (nready == 0)
        {
          // Timeout
          ob_log (OBLV_DBUG, 0x20100019, "woke for timeout\n");
          pret = POOL_AWAIT_TIMEDOUT;
          break;
        }
      else
        {
          // Some fds are ready - maybe for reading, maybe an exception
          // XXX deal with pools that have died in some way
          if (memcmp (&read_fds, &zero_fds, sizeof (read_fds)) == 0)
            {
              // There isn't anything to read
              ob_log (OBLV_DBUG, 0x2010001a, "woke for some other reason\n");
              continue;
            }
          // Maybe we were awoken!
          if (ph->w.wakeup_read_fd >= 0
              && FD_ISSET (ph->w.wakeup_read_fd, &read_fds))
            {
              byte one;
              // read off the byte
              if (read (ph->w.wakeup_read_fd, &one, sizeof (one)) < 0)
                OB_LOG_ERROR_CODE (0x2010001b, "read failed with '%s'\n",
                                   strerror (errno));
              pret = POOL_AWAIT_WOKEN;
              break;
            }

          if (ph->notify_handle >= 0 && FD_ISSET (ph->notify_handle, &read_fds))
            {
              pret = OB_OK;
            }
          if (ph->notify_handle >= 0
              && FD_ISSET (ph->notify_handle, &except_fds))
            {
              pret = OB_OK;
            }
          // Okay!  We have a protein, maybe!  Go check - we may have
          // gotten the notification for a protein that we already
          // checked for, so may have to try again.
          pret = pool_next (ph, ret_prot, ret_ts, ret_index);
          if (pret == OB_OK)
            break;
          // Quit for non-transient errors
          if (pret != POOL_NO_SUCH_PROTEIN)
            break;
          // Otherwise, we got a spurious wakeup, retry
        }
    }
  pool_fifo_multi_remove_awaiter (ph);
  return pret;
}
