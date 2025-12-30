
/* (c)  oblong industries */

#include "../Mutex.h"
#include <libLoam/c/ob-log.h>

namespace oblong {
namespace loam {

Mutex::Mutex (bool _recursive)
{
  recursive = _recursive;

  if (recursive)
    InitializeCriticalSection (&c_section);
  else
    semaphore = CreateSemaphore (0, 1, 1, 0);
}

Mutex::~Mutex ()
{
  if (recursive)
    DeleteCriticalSection (&c_section);
  else
    CloseHandle (semaphore);
}

void Mutex::Lock ()
{
  if (recursive)
    {
      EnterCriticalSection (&c_section);
    }
  else
    {
      ob_retort tort;
      DWORD lasterr;
      DWORD result = WaitForSingleObject (semaphore, INFINITE);

      switch (result)
        {
          case WAIT_OBJECT_0:
            //we have the lock
            return;

          case WAIT_TIMEOUT:
            //we weren't able to get the lock
            OB_FATAL_BUG_CODE (0x11010000,
                               "WaitForSingleObject with INFINITE wait"
                               " unexpectedly timed out");
            return;

          case WAIT_FAILED:
            lasterr = GetLastError ();
            tort = ob_win32err_to_retort (lasterr);
            OB_FATAL_BUG_CODE (0x11010003,
                               "WaitForSingleObject failed with:\n"
                               "GetLastError = 0x%08x\n"
                               "tort = 0x%016" OB_FMT_64 "x\n"
                               "semaphore = %d\n"
                               "%s\n",
                               lasterr, tort, semaphore,
                               ob_error_string (tort));
            return;

          default:
            //we weren't able to get the lock
            OB_FATAL_BUG_CODE (
              0x11010001, "WaitForSingleObject returned unexpected %d", result);
            return;
        }
    }
}

void Mutex::Unlock ()
{
  if (recursive)
    {
      LeaveCriticalSection (&c_section);
    }
  else
    {
      if (!ReleaseSemaphore (semaphore, 1, 0))
        {
          OB_LOG_BUG_CODE (0x11010002,
                           "Mutex::Unlock() - unnecessary ReleaseSemaphore?\n");
          //SVDebugMsg("Semaphore didn't need to be released\n");
        }
    }
}
}
}  // namespace oblong::loam
