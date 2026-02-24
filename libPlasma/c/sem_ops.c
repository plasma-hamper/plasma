
/* (c)  oblong industries */

///
/// Semaphore-based synchronization
///
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/file.h>
#include <errno.h>
#include <stdlib.h>
#include <math.h>

#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-rand.h"
#include "libLoam/c/ob-time.h"

#include "libPlasma/c/pool.h"
#include "libPlasma/c/private/pool_impl.h"
#include "libPlasma/c/private/pool-ugly-callback.h"

/// We use SysV semaphores to implement per-pool locks.  Each pool has
/// its own semaphore set, created either at pool creation time or
/// first access after the system has booted.  The semaphore id (used
/// to retrieve the semaphores) is stored in the pool configuration
/// file, and each pool participate must get a reference to the
/// semaphores.  Semaphores are for local pools only; remote pools
/// handle all locking on the host machine.
///
/// Semaphore descriptions:
///
/// Deposit lock: While this lock is held, no one else can deposit in
/// the pool.  This is useful for (a) making a deposit, (b) eliminating
/// race windows while checking for deposits.
///
/// Notification lock: This lock is held while pools are being notified
/// via the socket-based await mechanism, and while we are adding
/// ourselves to the notification list for this mechanism.  It could be
/// combined with the deposit lock, but we want to decouple deposits
/// from reads as much as possible.
///
/// Locks are implemented by setting the initial value to 1 (only one
/// "resource" available).  The lock operation subtracts 1 from the
/// value (taking the resource) and the unlock operation adds 1
/// (releasing the resource).  Both operations need the SEM_UNDO flag
/// so that the lock operation is reversed if the program dies before
/// releasing the lock itself.  The unlock operation needs the
/// SEM_UNDO flag because this feature is implemented by incrementing
/// the undo value when you lock the semaphore and decrementing the
/// undo value when you unlock the semaphore, and then adding the undo
/// value to the value of the semaphore when the program exits.  Note
/// that the OS has limits on how many simultaneous SEM_UNDO
/// operations can be in progress in the same process (that is, how
/// many threads in a process can be simultaneously calling semop()
/// with SEM_UNDO).  See the OS interface and limits documents for
/// more details.
///
/// All semaphore ops could be replaced with their pthread equivalents
/// if (1) cross-process pthread synchronization is supported, which
/// it is if the POSIX_THREAD_PROCESS_SHARED sysconf() variable is
/// defined (it is on both Linux and Mac OS X) and (2) you use the
/// incredibly irritating pthread_cleanup_push() and
/// pthread_cleanup_pop() routines to execute an unlock function if
/// the function dies while holding the lock.  You would have to keep
/// the locks in a SysV shared memory segment.

/// SysV semaphores come in sets (arrays).  Allocate one array and
/// index into it to get each of the different semaphore types we
/// need.

// This is a little hint from the testing environment that
// ob_sleep() isn't being used.  (And obviously a reboot doesn't
// occur in the middle of the test suite.)  We want to make sure
// semaphores are being destroyed, but it's hard to know what's
// expected unless we have this guarantee.
static bool dont_expect_sleep = false;

OB_PRE_POST (
  dont_expect_sleep =
    (NULL != getenv ("OB_FOR_TESTING_PURPOSES_ASSUME_POOLS_ARENT_SLEEPING")),
  ob_nop ());

/// EINTR-safe versions of some semaphore operations.  Note that
/// semctl() has variadic args and is a bit of a pain to deal with, so
/// we only create wrappers for the variants we use.

/// EINTR-safe semop().

static int semop_safe (int sem_id, struct sembuf *sops, size_t nsops)
{
  int ret;
  while ((ret = semop (sem_id, sops, nsops)) != 0)
    {
      // If we were just interrupted, restart, otherwise quit
      if (errno != EINTR)
        break;
    }
  return ret;
}

/// EINTR-safe SETVAL semctl() - for setting the value of a semaphore
/// after creation.

static int sem_setval_safe (int sem_id, int sem_num, int val)
{
  int ret;
  while ((ret = semctl (sem_id, sem_num, SETVAL, val)) < 0)
    {
      if (errno != EINTR)
        break;
      // Otherwise, retry
    }
  return ret;
}

/// EINTR-safe GETVAL semctl() - for getting the value of a semaphore.
/// This is used for debugging purposes only.

static int sem_getval_safe (int sem_id, int sem_num)
{
  int ret;
  while ((ret = semctl (sem_id, sem_num, GETVAL, 0)) < 0)
    {
      if (errno != EINTR)
        break;
      // Otherwise, retry
    }
  return ret;
}

/// Print semaphore values for this pool - for debugging only.

static void print_semaphores (pool_hose ph)
{
  if (ob_log_is_level_enabled (OBLV_DBUG))
    {
      int count = sem_getval_safe (ph->sem_id, POOL_SEM_DEPOSIT_LOCK_IDX);
      ob_log (OBLV_DBUG, 0x2010a000, "deposit lock %d\n", count);
      count = sem_getval_safe (ph->sem_id, POOL_SEM_NOTIFICATION_LOCK_IDX);
      ob_log (OBLV_DBUG, 0x2010a001, "notification count %d\n", count);
    }
}

// Destroy a semaphore set - used in pool_dispose() and the failure
// path of pool_create().

static ob_retort pool_sem_destroy_semaphores (pool_hose ph,
                                              bool it_should_exist)
{
  // If this is called from pool_dispose(), the sem id is not set
  // because we don't open the pool.  But if we are on the error exit
  // path of pool_create(), the sem id is set.  If the sem id isn't
  // set, open the semaphores ourselves (and if that fails, then we
  // don't have any semaphores to destroy).
  if (ph->sem_id < 0)
    {
      ph->sem_id = semget (ph->sem_key, 0, 0666);
      if (ph->sem_id < 0)
        {
          // If we're on the error path of pool_create(), the semaphore
          // might not exist.  If someone has called pool_sleep() on the
          // pool, or we have rebooted since last use, the semaphore won't
          // exist.  If we are sure neither of these conditions are
          // true (the latter can only be guaranteed during testing),
          // then complain if the semaphore didn't exist.
          if (it_should_exist && dont_expect_sleep)
            {
              OB_LOG_ERROR_CODE (0x2010a01e,
                                 "For pool '%s',\n"
                                 "expected key 0x%08x to exist,\n"
                                 "but got '%s'\n",
                                 ph->name, ph->sem_key, strerror (errno));
              return POOL_SEMAPHORES_BADTH;
            }
          else
            return OB_OK;
        }
    }
  // Open-code EINTR handling - we only use this once.
  while (semctl (ph->sem_id, 0, IPC_RMID, 0) < 0)
    {
      if (errno == EINTR)
        continue;
      OB_LOG_ERROR_CODE (0x2010a002, "semaphore destroy failed: %s\n",
                         strerror (errno));
      return POOL_SEMAPHORES_BADTH;
    }

  return OB_OK;
}

typedef struct
{
  ob_retort tort;
  key_t sem_key;
} retort_and_key;

/* Traditionally, sem_key is generated using ftok(), which typically
 * fills the 32 bits (from most to least significant) as follows:
 * ppppppppddddddddiiiiiiiiiiiiiiii
 * where "p" is the "proj_id" supplied to ftok(), "d" is the device
 * number for the disk containing the file passed to ftok(), and "i"
 * is the inode for the file passed to ftok().
 * However, using ftok() requires that we write the config file twice
 * (first to create it so that ftok can use its inode, and then again
 * to write the resulting sem_key into the config file).  Also, 16 bits
 * of inode are not enough to be unique, so in practice we get conflicts,
 * especially on systems that haven't been rebooted in a while.
 *
 * For these reasons, we're not going to use ftok().  Instead, we'll
 * just generate sem_key randomly (and try again if we get a conflict).
 * However, to avoid conflicting with other programs on the system that
 * are using ftok(), we don't want to generate all 32 bits randomly.
 * We'll still use the upper 8 bits for "p", since this is the primary
 * means for programs to avoid stepping on each other.  Furthermore,
 * we'll set the most-significant "d" bit to 1, since device numbers
 * tend to be small integers; this will further reduce conflicts with
 * ftok users.  Thus, we have a fixed bit pattern for the upper 9 bits,
 * and we generate the lower 23 bits randomly.
 *
 * As for the choice of "p", we chose 0x0b, not only because it can
 * be pronounced "ob", but also because it seems to be a value for "p"
 * which is not commonly used.  Using Google Code Search to find uses
 * of ftok() across existing software, it appears that ASCII characters
 * are common, 1 is common, and 0 is sometimes used (even though 0 is
 * supposed to be illegal).  So a value between 2 and 31, such as 0x0b
 * (11) seems like a good choice.
 */
static retort_and_key random_sem_key (void)
{
  byte bites[3];
  retort_and_key ret;
  ret.sem_key = 0;
  ret.tort = ob_truly_random (bites, sizeof (bites));
  if (ret.tort < OB_OK)
    return ret;
  ret.sem_key = (0x0b800000 | (bites[0] << 16) | (bites[1] << 8) | bites[2]);
  return ret;
}

// __pool_create_semaphores() is called from two places:
// pool_sem_create_semaphores() and pool_sem_open_semaphores().  In the first
// case, we want to write out the configuration file and in the second
// case we do not, so we separate out the actual creation of the
// semaphores from the configration file write.

static ob_retort __pool_create_semaphores (pool_hose ph)
{
  retort_and_key rk = random_sem_key ();
  if (rk.tort < OB_OK)
    return rk.tort;
  OB_LOG_DEBUG_CODE (0x2010a003, "%s: trying sem_key %08x\n", ph->name,
                     (unt32) rk.sem_key);
  int sem_id;
  while ((sem_id = semget (rk.sem_key, POOL_SEM_SET_COUNT,
                           0666 | IPC_CREAT | IPC_EXCL))
         < 0)
    {
      if (errno == EEXIST)
        {
          // collision occurred, try another key
          rk = random_sem_key ();
          if (rk.tort < OB_OK)
            return rk.tort;
          OB_LOG_DEBUG_CODE (0x2010a004, "%s: trying sem_key %08x\n", ph->name,
                             (unt32) rk.sem_key);
        }
      else if (errno != EINTR)
        break;
    }
  if (sem_id < 0)
    {
      int erryes = errno;
      OB_LOG_ERROR_CODE (0x2010a01a, "semget: %s\n%s", strerror (erryes),
                         (erryes == ENOSPC ? "This means you need to increase "
                                             "the number of semaphores.\n"
                                             "Try doing this:\n"
                                             "sudo sh -c \"echo 250 65536 32 "
                                             "32768 > /proc/sys/kernel/sem\"\n"
                                           : ""));
      errno = erryes;
      return POOL_SEMAPHORES_BADTH;
    }
  ob_log (OBLV_DBUG, 0x2010a005, "created sem_id %d\n", sem_id);

  // Set the pool write lock to unlocked
  if (sem_setval_safe (sem_id, POOL_SEM_DEPOSIT_LOCK_IDX, 1) < 0)
    {
      OB_PERROR_CODE (0x2010a01b, "[2] sem_setval_safe");
      return POOL_SEMAPHORES_BADTH;
    }
  // Set the notification lock to unlocked
  if (sem_setval_safe (sem_id, POOL_SEM_NOTIFICATION_LOCK_IDX, 1) < 0)
    {
      OB_PERROR_CODE (0x2010a01c, "[3] sem_setval_safe");
      return POOL_SEMAPHORES_BADTH;
    }

  ph->sem_key = rk.sem_key;
  ph->sem_id = sem_id;
  print_semaphores (ph);

  return OB_OK;
}

static ob_retort pool_sem_create_semaphores (pool_hose ph)
{
  return __pool_create_semaphores (ph);
}

/* 0 is the value originally set by pool_new_pool_hose(), and
 * -1 is the value set by pool_sleep().  IPC_PRIVATE is special
 * to semget() so we want to make sure not to use it, although
 * on both Linux and Mac it is 0, so checking for it is redundant.
 */
static bool is_valid_key (key_t k)
{
  return (k != 0 && k != -1 && k != IPC_PRIVATE);
}

static ob_retort pool_sem_open_semaphores (pool_hose ph)
{
  // System V semaphores all disappear on reboot, so we have to create
  // them if they don't exist.  We avoid the inherent create/set race
  // window because we hold the config lock when we do this.
  //
  // Besides reboot, it's now also possible to intentionally
  // deallocate semaphores for pools which are not going to be used
  // for a while, using pool_sleep() or the "p-sleep" utility.
  // But to us, that looks just like the reboot case.

  int sem_id;
  errno = ENOENT;
  if (!is_valid_key (ph->sem_key)
      || (sem_id = semget (ph->sem_key, 0, 0666)) < 0)
    {
      if (errno != ENOENT)
        {
          OB_PERROR_CODE (0x2010a01d, "[4] semget");
          return POOL_SEMAPHORES_BADTH;
        }

      // If the semaphores don't exist yet, just create them
      ob_retort pret = __pool_create_semaphores (ph);
      if (pret != OB_OK)
        return pret;

      if (POOL_DIRECTORY_VERSION_CONFIG_IN_FILE == ph->pool_directory_version)
        {
          // In the old days when we used ftok(), there was a significant
          // chance we'd get the same semaphore key, so we optimized
          // that case by not rewriting the config file if the key
          // didn't change.  But now that we generate semaphore keys
          // randomly, there's only a 1 in 2**23 chance the key will
          // be unchanged, and that's a silly corner case to optimize
          // for.  Better to just be uniform and always rewrite the
          // config file.

          OB_LOG_INFO_CODE (0x2010a006,
                            "Recreated semaphore; rewriting config for '%s'\n",
                            ph->name);
          pret = pool_save_default_config (ph);
          if (pret != OB_OK)
            return pret;
        }
    }
  else
    ph->sem_id = sem_id;
  ob_log (OBLV_DBUG, 0x2010a007, "pool open, got sem_id %d\n", ph->sem_id);
  print_semaphores (ph);
  errno = 0;
  return OB_OK;
}

static void random_delay (float64 lo, float64 hi)
{
  ob_rand_t *ayn = ob_rand_allocate_state (OB_RAND_COMPLETELY_RANDOM_PLEASE);
  float64 secs = ob_rand_state_float64 (lo, hi, ayn);

  OB_LOG_INFO_CODE (0x2010a021,
                    "Delaying %f seconds (a random amount between\n"
                    "%f and %f) to reduce the chance of an unfortunate\n"
                    "race condition, in the face of a semaphore which\n"
                    "apparently has been deleted and recreated.\n",
                    secs, lo, hi);

  float64 micros = secs * 1e6;
  OB_DIE_ON_ERROR (ob_micro_sleep ((unt32) micros));

  ob_rand_free_state (ayn);
}

static void recreate_maliciously_deleted_semaphore (pool_hose ph)
{
  const int old_sem_id = ph->sem_id;
  const char *where = "semget IPC_CREAT";

  int new_sem_id;
  while ((new_sem_id = semget (ph->sem_key, POOL_SEM_SET_COUNT,
                               0666 | IPC_CREAT | IPC_EXCL))
         < 0)
    {
      if (errno == EEXIST)
        {
          /* So, the old sem_id does not exist, but a semaphore with
           * the sem_key does exist.  This probably means somebody else
           * beat us to recreating the semaphore.  To avoid a race
           * condition (by giving them time to do the two sem_setval_safes,
           * since we don't hold the config lock right now), let's sleep
           * a random amount of seconds, somewhere between the golden
           * ratio and pi.  (Yes, I am not taking this very seriously.
           * Who actually thinks their program should still work after they
           * start deleting semaphores out from under it?)
           */
          random_delay ((1 + sqrt (5)) / 2, M_PI);
          if ((new_sem_id = semget (ph->sem_key, 0, 0666)) >= 0)
            {
              OB_LOG_INFO_CODE (0x2010a022, "semaphore that was %d is now %d\n",
                                ph->sem_id, new_sem_id);
              ph->sem_id = new_sem_id;
              return;
            }
          where = "semget";
        }
      if (errno != EINTR)
        {
/* Who knows what deep doo-doo we're in at this point,
           * and since the user in question feels they shouldn't
           * have to look at error codes returned by our API,
           * we will exit for them.
           */
ugh:
          ob_nop ();  // what, you don't like declarations right after a label?
          const int erryes = errno;
          OB_FATAL_ERROR_CODE (0x2010a01f,
                               "Something unfortunate and unexpected\n"
                               "(specifically, errno %d '%s' for %s)\n"
                               "has occurred while trying to recreate\n"
                               "a maliciously deleted semaphore.\n"
                               "I am too confused.  Bailing out!\n",
                               erryes, strerror (erryes), where);
        }
      where = "semget IPC_CREAT retry";
    }

  // Set the pool write lock to unlocked
  where = "deposit lock";
  if (sem_setval_safe (new_sem_id, POOL_SEM_DEPOSIT_LOCK_IDX, 1) < 0)
    goto ugh;

  // Set the notification lock to unlocked
  where = "notification lock";
  if (sem_setval_safe (new_sem_id, POOL_SEM_NOTIFICATION_LOCK_IDX, 1) < 0)
    goto ugh;

  ph->sem_id = new_sem_id;

  OB_LOG_INFO_CODE (0x2010a020,
                    "Recreated a maliciously deleted semaphore:\n"
                    "hose = '%s'\n"
                    "pool = '%s'\n"
                    "sem_key = 0x%08x\n"
                    "old sem_id = %u\n"
                    "new sem_id = %u\n"
                    "This is NOT supposed to happen and is NOT a good\n"
                    "thing, and various unpleasantness like race\n"
                    "conditions might occur.  If you are randomly\n"
                    "deleting semaphores on your machine for the\n"
                    "fun of it, you are advised to not do that!\n",
                    ph->hose_name, ph->name, ph->sem_key, old_sem_id,
                    new_sem_id);
}

static const char *idx_to_str (int idx)
{
  switch (idx)
    {
      case POOL_SEM_DEPOSIT_LOCK_IDX:
        return "deposit";
      case POOL_SEM_NOTIFICATION_LOCK_IDX:
        return "notification";
      default:
        return "unknown";
    }
}

static const char *extra_advice (int e)
{
  if (e == EINVAL || e == EIDRM)
    return " which probably means somebody deleted a pool you were still using";
  else
    return "";
}

// Lock a semaphore.  See pool_impl.h for the correct index value.

static ob_retort pool_sem_lock (pool_hose ph, int idx)
{
  // Generic lock op - give it the index of the semaphore you want to lock
  struct sembuf lock_op = {
    .sem_op = -1,
    .sem_num = idx,
    // Unlock the semaphore if the process dies
    .sem_flg = SEM_UNDO,
  };
  // Debug/sanity check
  int count = sem_getval_safe (ph->sem_id, idx);
  ob_log (OBLV_DBUG, 0x2010a008, "%s: lock entry %d:   count %d\n", ph->name,
          idx, count);
  if (count == -1)  // this indicates an error
    {
      const int erryes = errno;
      if (erryes == EINVAL || erryes == EIDRM)
        {
          /* This means the semaphore was deleted at some point after
           * the hose was opened.  Normally, this would be considered
           * "unexpected" and treated as an "error", but apparently
           * there are some users who like to go through their machines
           * and delete semaphores at random:
           * https://bugs.oblong.com/show_bug.cgi?id=2800
           * So, try to accommodate such users by recreating the
           * semaphore on the fly, even though various race conditions
           * could occur, since we're not holding the config lock.
           */
          recreate_maliciously_deleted_semaphore (ph);
          goto gross;
        }
      OB_LOG_ERROR_CODE (0x2010a009,
                         "hose '%s' pool '%s': '%s lock' semctl failed "
                         "with '%s' (%d)%s\n",
                         ph->hose_name, ph->name, idx_to_str (idx),
                         strerror (errno), errno, extra_advice (errno));
      return POOL_SEMAPHORES_BADTH;
    }
  // Count can legitimately be 1 if it's locked by someone else, but never 2
  if (count > 1)
    OB_FATAL_BUG_CODE (0x2010a00a,
                       "hose '%s' pool '%s': '%s lock' count %d "
                       "(should be <= 1)\n",
                       ph->hose_name, ph->name, idx_to_str (idx), count);

gross:
  // Actual lock
  if (semop_safe (ph->sem_id, &lock_op, 1) != 0)
    {
      if ((errno == EINVAL) || (errno == EIDRM))
        {
          OB_LOG_ERROR_CODE (0x2010a00b,
                             "hose '%s' pool '%s' sem_id %d:\n"
                             "'%s lock' semop failed "
                             "with '%s' (%d),\nwhich probably "
                             "means somebody deleted a pool you "
                             "were still using\n",
                             ph->hose_name, ph->name, ph->sem_id,
                             idx_to_str (idx), strerror (errno), errno);
          return POOL_SEMAPHORES_BADTH;
        }
      if ((errno == EINVAL) || (errno == ENOMEM))
        {
          // There are two possible meanings of EINVAL on OS X: invalid
          // semid, probably meaning that the pool was disposed, or that
          // "too many" SEM_UNDO operations are going on concurrently in
          // this process.  On Linux, the latter case is indicated by
          // ENOMEM.
          OB_LOG_ERROR_CODE (0x2010a00c,
                             "hose '%s' pool '%s': '%s lock' semop failed, "
                             "errno %d ('%s')\n",
                             ph->hose_name, ph->name, idx_to_str (idx), errno,
                             strerror (errno));
          return POOL_SEMAPHORES_BADTH;
        }
      // otherwise - something we don't expect!
      OB_FATAL_BUG_CODE (0x2010a00d,
                         "hose '%s' pool '%s': '%s lock' semop failed, "
                         "errno %d ('%s')\n",
                         ph->hose_name, ph->name, idx_to_str (idx), errno,
                         strerror (errno));
      // obviously, we never get here. this is just a future idea
      return POOL_SEMAPHORES_BADTH;
    }

  // Debug/sanity check
  count = sem_getval_safe (ph->sem_id, idx);
  ob_log (OBLV_DBUG, 0x2010a00e, "%s: lock exit %d:    count %d\n", ph->name,
          idx, count);
  if (count == -1)  // this indicates an error
    {
      OB_LOG_ERROR_CODE (0x2010a00f,
                         "hose '%s' pool '%s': '%s lock' semctl failed "
                         "with '%s' (%d)%s\n",
                         ph->hose_name, ph->name, idx_to_str (idx),
                         strerror (errno), errno, extra_advice (errno));
      return POOL_SEMAPHORES_BADTH;
    }
  if (count != 0)
    OB_FATAL_BUG_CODE (0x2010a010,
                       "hose '%s' pool '%s': '%s lock' locked but "
                       "count %d (should be 0)\n",
                       ph->hose_name, ph->name, idx_to_str (idx), count);
  return OB_OK;
}

/// Unlock a semaphore.

static ob_retort pool_sem_unlock (pool_hose ph, int idx)
{
  struct sembuf unlock_op = {
    .sem_op = 1,
    .sem_num = idx,
    // Update undo count
    .sem_flg = SEM_UNDO,
  };

  // Debug/sanity check
  int count = sem_getval_safe (ph->sem_id, idx);
  ob_log (OBLV_DBUG, 0x2010a011, "%s: unlock entry %d: count %d\n", ph->name,
          idx, count);
  if (count == -1)  // this indicates an error
    {
      OB_LOG_ERROR_CODE (0x2010a012,
                         "hose '%s' pool '%s': '%s lock' semctl failed "
                         "with '%s' (%d)%s\n",
                         ph->hose_name, ph->name, idx_to_str (idx),
                         strerror (errno), errno, extra_advice (errno));
      return POOL_SEMAPHORES_BADTH;
    }
  if (count != 0)
    OB_FATAL_BUG_CODE (0x2010a013,
                       "hose '%s' pool '%s': '%s lock' count %d "
                       "(should be 0)\n",
                       ph->hose_name, ph->name, idx_to_str (idx), count);

  // Actual unlock
  if (semop_safe (ph->sem_id, &unlock_op, 1) != 0)
    {
      if ((errno == EINVAL) || (errno == EIDRM))
        {
          OB_LOG_ERROR_CODE (0x2010a014,
                             "hose '%s' pool '%s': '%s lock' semop failed "
                             "with '%s' (%d), which probably "
                             "means somebody deleted a pool you were still "
                             "using\n",
                             ph->hose_name, ph->name, idx_to_str (idx),
                             strerror (errno), errno);
          return POOL_SEMAPHORES_BADTH;
        }
      if ((errno == EINVAL) || (errno == ENOMEM))
        {
          OB_LOG_ERROR_CODE (0x2010a015,
                             "hose '%s' pool '%s': '%s lock' semop failed, "
                             "errno %d ('%s')\n",
                             ph->hose_name, ph->name, idx_to_str (idx), errno,
                             strerror (errno));
          return POOL_SEMAPHORES_BADTH;
        }
      // otherwise - something we don't expect!
      OB_FATAL_BUG_CODE (0x2010a016,
                         "hose '%s' pool '%s': '%s lock' semop failed, "
                         "errno %d ('%s')\n",
                         ph->hose_name, ph->name, idx_to_str (idx), errno,
                         strerror (errno));
      // obviously, we never get here. this is just an idea for the future
      return POOL_SEMAPHORES_BADTH;
    }

  // Debug/sanity check
  count = sem_getval_safe (ph->sem_id, idx);
  ob_log (OBLV_DBUG, 0x2010a017, "%s: unlock exit %d:  count %d\n", ph->name,
          idx, count);
  if (count == -1)  // this indicates an error
    {
      OB_LOG_ERROR_CODE (0x2010a018,
                         "hose '%s' pool '%s': '%s lock' semctl failed "
                         "with '%s' (%d)%s\n",
                         ph->hose_name, ph->name, idx_to_str (idx),
                         strerror (errno), errno, extra_advice (errno));
      return POOL_SEMAPHORES_BADTH;
    }
  // Count can legitimately be 1 if it's locked by someone else, but never 2
  if (count > 1)
    OB_FATAL_BUG_CODE (0x2010a019,
                       "hose '%s' pool '%s': '%s lock' count %d "
                       "(should be <= 1)\n",
                       ph->hose_name, ph->name, idx_to_str (idx), count);
  return OB_OK;
}

static ob_retort pool_sem_dummy_close (pool_hose ph)
{
  return OB_OK;
}

const pool_lock_ops pool_sem_ops = {pool_sem_create_semaphores,
                                    pool_sem_destroy_semaphores,
                                    pool_sem_open_semaphores,
                                    pool_sem_dummy_close,
                                    pool_sem_lock,
                                    pool_sem_unlock};
