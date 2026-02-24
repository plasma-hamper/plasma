
/* (c)  oblong industries */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE  // needed to get _sys_nerr and _sys_errlist on Linux
#endif

#include "libLoam/c/ob-retorts.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-sys.h"
#include <stdio.h>

// TODO(cleanup)
#if 0
#ifdef __gnu_linux__
// Linux issues a warning if we use sys_nerr or sys_errlist.
// Using the underscore-prefixed versions avoids the warning.
#define NERR _sys_nerr
#define ERRLIST _sys_errlist
#else
// Other OSes don't suffer from this affliction
#define NERR sys_nerr
#define ERRLIST sys_errlist
#endif
#endif

static ob_translation_func translation_funcs[500];

ob_retort ob_add_error_names (ob_translation_func func)
{
  int i;
  const int len = sizeof (translation_funcs) / sizeof (translation_funcs[0]);

  for (i = 0; i < len; i++)
    {
      if (translation_funcs[i] == NULL)
        {
          translation_funcs[i] = func;
          return OB_OK;
        }
      else if (translation_funcs[i] == func)
        {
          OB_LOG_BUG_CODE (0x10010000, "func already registered!\n");
          return OB_OK;
        }
    }

  OB_LOG_BUG_CODE (0x10010001, "translation_funcs table exceeded %d entries\n",
                   len);

  return OB_NO_MEM;
}

#define E(x)                                                                   \
  case x:                                                                      \
    return #x

const char *ob_error_string_literal (ob_retort err)
{
  int i, e;
  const int len = sizeof (translation_funcs) / sizeof (translation_funcs[0]);

  switch (err)
    {
      // add new retorts between OB_RETORTS_COMMON_FIRST and
      // OB_RETORTS_COMMON_LAST here (retorts outside this range
      // should be registered with ob_add_error_names() instead)
      E (OB_ALREADY_PRESENT);
      E (OB_ARGUMENT_WAS_NULL);
      E (OB_BAD_INDEX);
      E (OB_BOUNCE);
      E (OB_DISCONNECTED);
      E (OB_EMPTY);
      E (OB_INADEQUATE_CLASS);
      E (OB_INVALID_ARGUMENT);
      E (OB_INVALID_OPERATION);
      E (OB_NO);
      E (OB_NO_MEM);
      E (OB_NOT_FOUND);
      E (OB_NOTHING_TO_DO);
      E (OB_OK);
      E (OB_PARSE_ERROR);
      E (OB_STOP);
      E (OB_UNKNOWN_ERR);
      E (OB_VERSION_MISMATCH);
      E (OB_YES);
      default:
        // See if this was an errno wrapped in a retort
        e = ob_retort_to_errno (err);
        const char *s = strerror (e);
        if (s)
          return s;
        // if (e > 0 && e < NERR)
        //   {
        //     const char *s = ERRLIST[e];
        //     if (s)
        //       return s;
        //   }
        // Try the registered translation functions
        for (i = 0; i < len && translation_funcs[i] != NULL; i++)
          {
            const char *s = translation_funcs[i](err);
            if (s)
              return s;
          }
#ifdef _MSC_VER
        return ob_private_w32_translation (err);
#else
        return NULL;
#endif
    }
}

const char *ob_error_string (ob_retort err)
{
  static char buf[50];

  const char *s = ob_error_string_literal (err);
  if (s)
    return s;
  else
    {
      /* If all else fails, use the numeric error code.
       * No, this is not thread-safe, but since we really shouldn't be
       * encountering unknown error codes anyway, we can probably
       * squeak by with this. */
      snprintf (buf, sizeof (buf), "ob_retort %" OB_FMT_RETORT "d", err);
      return buf;
    }
}

bool ob_retort_exists (ob_retort ret)
{
  return (NULL != ob_error_string_literal (ret));
}

void ob_ignore_retort (OB_UNUSED ob_retort ret) {}
