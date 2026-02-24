
/* (c)  oblong industries */

///
/// Semaphore-based synchronization
///
//#include <sys/sem.h>
//#include <sys/ipc.h>
//#include <sys/file.h>
#include <errno.h>
#include <stdlib.h>

#include "../pool.h"
#include "../private/pool_impl.h"

/// We use Windows mutexes to implement per-pool locks.  Each pool has
/// its own mutexes, one for depositing into the pool, one for syncronizing
/// access to the notification folder. There is a semaphore id
/// stored in the pool configuration file but it is NOT used to acquire
/// the mutexes for the pool. Rather, the name of the mutexes is based on
/// the path to the pool in the filesystem. These mutexes are for local
/// pools only; remote pools handle all locking on the host machine.

/// Windows mutexes will conveniently auto-release ownership if a thread
/// or process exits prematurely (likely due to a crash). This will prevent
/// the system from deadlocking. However, when taking ownership after an
/// abandoned mutex, the state of the shared resource is obviously in a
/// dubious condition - someone crashed or forgot to cleanly give up the
/// resource. So warnings (at a minimum) should be issued.

//these #defines are moved to pool_impl.h
//#define POOL_SEM_DEPOSIT_LOCK_IDX      0
//#define POOL_SEM_NOTIFICATION_LOCK_IDX 1
//#define POOL_SEM_SET_COUNT             2

// Destroy a semaphore set - used in pool_dispose() and the failure
// path of pool_create().

ob_retort pool_destroy_semaphores (pool_hose ph, bool ignored_arg)
{
  bool allOK = true;

  int i;
  for (i = 0; i < POOL_SEM_SET_COUNT; i++)
    {
      if (ph->mutex_handles[i])
        {
          if (CloseHandle (ph->mutex_handles[i]) == false)
            allOK = false;
          ph->mutex_handles[i] = 0;
        }
    }

  if (allOK)
    return OB_OK;

  return POOL_SEMAPHORES_BADTH;
}

// __pool_create_semaphores() is called from two places:
// pool_create_semaphores() and pool_open_semaphores().  In the first
// case, we want to write out the configuration file and in the second
// case we do not, so we separate out the actual creation of the
// semaphores from the configration file write.

static ob_retort __pool_create_semaphores (pool_hose ph)
{
  // We need a unique file per-pool to generate the semaphore key.
  // The config file will do nicely.
  char path[PATH_MAX];
  ob_retort pret = pool_config_file_path (path, NULL, ph->name);
  if (pret != OB_OK)
    return pret;

  //windows CreateSemaphore does not return an array of semaphores like SysV
  //so we have to create 1 for every possible type (defined by POOL_SEM_COUNT)
  int i;
  for (i = 0; i < POOL_SEM_SET_COUNT; i++)
    {
      //the mutex will be the name of the pool config file, slightly adjusted
      //so that is a name for windows mutex object
      char mutex_name[MAX_PATH];
      snprintf (mutex_name, sizeof (mutex_name), "%s\\%d", path, i);

      //mutex name will be transformed from something like this -
      //C:\ob\pools\remote_ui\pool_conf\0
      //to something like this -
      //C__ob_pools_remote_ui_pool_conf_0
      prepare_windows_handle_name (mutex_name);

      HANDLE mutex_handle = CreateMutex (0, 0, mutex_name);
      DWORD err = GetLastError ();
      if (mutex_handle == 0)
        {
          OB_LOG_ERROR_CODE (0x2010c000,
                             "__pool_create_semaphores could not "
                             "create mutex %s",
                             mutex_name);
          return POOL_SEMAPHORES_BADTH;
        }

      if (err == ERROR_ALREADY_EXISTS)
        {
          ob_log (OBLV_DBUG, 0x2010c001, "opened existing mutex %s - %u",
                  mutex_name, mutex_handle);
        }
      else
        {
          ob_log (OBLV_DBUG, 0x2010c002, "created mutex %s - %u\n", mutex_name,
                  mutex_handle);
        }

      ph->mutex_handles[i] = mutex_handle;
    }

  ph->sem_key = -1;

  return OB_OK;
}

ob_retort pool_create_semaphores (pool_hose ph)
{
  return __pool_create_semaphores (ph);
}

ob_retort pool_open_semaphores (pool_hose ph)
{
  //on windows, opening/creating is the same method
  return __pool_create_semaphores (ph);
}

ob_retort pool_close_semaphores (pool_hose ph)
{
  return OB_OK;
}

/// Lock a semaphore.  See pool_impl.h for the correct index value.

// XXX If someone destroys a pool in active use, the semops will fail.
// Need reference counting or something.

static ob_retort pool_sem_lock (pool_hose ph, int index)
{
  if (index < 0 || index >= POOL_SEM_SET_COUNT)
    {
      OB_LOG_ERROR_CODE (0x2010c003,
                         "invalid index %d in call to pool_sem_lock", index);
      return POOL_SEMAPHORES_BADTH;
    }

  DWORD result = WaitForSingleObject (ph->mutex_handles[index], INFINITE);

  switch (result)
    {
      case WAIT_OBJECT_0:
        //we have mutex ownership
        return OB_OK;

      case WAIT_ABANDONED_0:
        //we have the mutex but it is because someone abandoned it
        OB_LOG_ERROR_CODE (0x2010c004,
                           "pool_sem_lock gained ownership of an abandoned "
                           "mutex, "
                           "state of notification/pool folder is dubious.");
        return OB_OK;

      default:
        OB_LOG_ERROR_CODE (0x2010c005,
                           "unhandled WaitForSingleObject error in "
                           "pool_sem_lock - %d",
                           result);
        break;
    }

  return POOL_SEMAPHORES_BADTH;
}

/// Unlock a semaphore.

static ob_retort pool_sem_unlock (pool_hose ph, int index)
{
  if (index < 0 || index >= POOL_SEM_SET_COUNT)
    {
      OB_LOG_ERROR_CODE (0x2010c006,
                         "invalid index %d in call to pool_sem_lock", index);
      return POOL_SEMAPHORES_BADTH;
    }

  if (!ReleaseMutex (ph->mutex_handles[index]))
    {
      OB_LOG_ERROR_CODE (0x2010c007,
                         "pool_sem_unlock - ReleaseMutex returned false");
    }

  return OB_OK;
}

ob_retort pool_deposit_lock (pool_hose ph)
{
  return pool_sem_lock (ph, POOL_SEM_DEPOSIT_LOCK_IDX);
}

ob_retort pool_deposit_unlock (pool_hose ph)
{
  return pool_sem_unlock (ph, POOL_SEM_DEPOSIT_LOCK_IDX);
}

ob_retort pool_notification_lock (pool_hose ph)
{
  return pool_sem_lock (ph, POOL_SEM_NOTIFICATION_LOCK_IDX);
}

ob_retort pool_notification_unlock (pool_hose ph)
{
  return pool_sem_unlock (ph, POOL_SEM_NOTIFICATION_LOCK_IDX);
}
