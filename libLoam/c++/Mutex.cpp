
/* (c)  oblong industries */

#include <libLoam/c++/Mutex.h>
#include "libLoam/c/ob-log.h"


namespace oblong {
namespace loam {


Mutex::Mutex (bool recursive)
{
  pthread_mutexattr_t attr;
  OB_CHECK_PTHREAD_CODE (0x110b0000, pthread_mutexattr_init (&attr));
  if (recursive)
    OB_CHECK_PTHREAD_CODE (
      0x110b0001, pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_RECURSIVE));
  OB_CHECK_PTHREAD_CODE (0x110b0002, pthread_mutex_init (&mutex_, &attr));
  OB_CHECK_PTHREAD_CODE (0x110b0003, pthread_mutexattr_destroy (&attr));
}

Mutex::~Mutex ()
{
  OB_CHECK_PTHREAD_CODE (0x110b0004, pthread_mutex_destroy (&mutex_));
}

void Mutex::Lock ()
{
  OB_CHECK_PTHREAD_CODE (0x110b0005, pthread_mutex_lock (&mutex_));
}

void Mutex::Unlock ()
{
  OB_CHECK_PTHREAD_CODE (0x110b0006, pthread_mutex_unlock (&mutex_));
}
}
}  // namespace oblong::loam
