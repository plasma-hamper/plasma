
/* (c)  oblong industries */

#include "libLoam/c/ob-sys.h"
#include "libLoam/c/ob-atomic.h"
#include "libLoam/c/ob-string.h"

#include <map>

typedef std::map<DWORD, const char *> MsgMap;
typedef std::pair<DWORD, const char *> MsgMapEntry;

static void *mmprotector;
static MsgMap *mmptr;

/// Takes an error code from GetLastError and returns a string.  This is
/// convenient in three ways:
/// 1. Handles the complicated invocation of FormatMessage()
/// 2. Removes the trailing newline from the string
/// 3. Reallocates the string using normal malloc(), so you can free
///    with normal free(), and not mess with weird Windows functions.
static char *ob_string_from_last_error (DWORD err)
{
  char *msg = NULL;
  if (0
      != FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM
                          | FORMAT_MESSAGE_IGNORE_INSERTS
                          | FORMAT_MESSAGE_ALLOCATE_BUFFER,
                        NULL, err, 0, (LPTSTR) &msg, 0, NULL))
    {
      char *ret = strdup (msg);
      LocalFree (msg);
      ob_chomp (ret);
      return ret;
    }
  else
    return NULL;
}

extern "C" const char *ob_private_w32_translation (ob_retort t)
{
  DWORD w = ob_retort_to_win32err (t);
  if (w == ~(DWORD) 0)
    return NULL;

  CRITICAL_SECTION *cs = ob_fetch_critical (&mmprotector);
  if (!cs)
    return NULL;

  MsgMap::iterator found;
  const char *result = NULL;
  EnterCriticalSection (cs);

  if (!mmptr)
    mmptr = new MsgMap;

  if (!mmptr)
    goto done;

  found = mmptr->find (w);
  if (found != mmptr->end ())
    {
      result = found->second;
      goto done;
    }

  result = ob_string_from_last_error (w);
  if (!result)
    {
      const size_t cap = 80;
      char *s = (char *) calloc (1, cap);
      if (!s)
        goto done;
      snprintf (s, cap, "win32 API error 0x%08lx", (unsigned long) w);
      result = s;
    }

  mmptr->insert (MsgMapEntry (w, result));

done:
  LeaveCriticalSection (cs);
  return result;
}
