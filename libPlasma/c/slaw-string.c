
/* (c)  oblong industries */

#include "libLoam/c/ob-sys.h"
#include "libPlasma/c/slaw.h"
#include "libPlasma/c/slaw-string.h"
#include "libPlasma/c/private/plasma-private.h"
#include "libPlasma/c/private/plasma-util.h"

#include <stdlib.h>
#include <string.h>


slaw slaw_string_vformat (const char *fmt, va_list ap)
{
  va_list atmp;
  char buf[32];
  int len;
  slaw result;

  va_copy (atmp, ap);
  len = vsnprintf (buf, sizeof (buf), fmt, ap);

  if (len < 0)
    result = NULL;
  else if (len < sizeof (buf))
    result = slaw_string (buf);
  else
    {
      // length passed to slaw_string_raw does not include the terminating NUL
      result = slaw_string_raw (len);
      if (result)
        // but length passed to vsnprintf *does* include the terminating NUL
        vsnprintf (SLAW_STRING_EMIT_WRITABLE (result), len + 1, fmt, atmp);
    }
  va_end (atmp);

  return result;
}

slaw slaw_string_format (const char *fmt, ...)
{
  va_list ap;
  slaw result;

  va_start (ap, fmt);
  result = slaw_string_vformat (fmt, ap);
  va_end (ap);

  return result;
}

slabu *slabu_of_strings_from_split (const char *str, const char *sep)
{
  int sepLen = strlen (sep);
  slabu *sb = slabu_new ();
  ob_retort err = OB_OK;

  if (!sb)
    return NULL;

  while (*str)
    {
      const char *next = strstr (str, sep);
      if (next == NULL)
        {
          ob_err_accum (&err, slabu_list_add_x (sb, slaw_string (str)));
          break;
        }
      else
        {
          ob_err_accum (&err, slabu_list_add_x (sb, slaw_string_from_substring (
                                                      str, next - str)));
          str = next + sepLen;
        }
    }

  if (err < OB_OK)
    {
      slabu_free (sb);
      return NULL;
    }

  return sb;
}

slaw slaw_strings_concat_c (const char *str1, ...)
{
  va_list vargies;
  int64 len = 0;
  slaw sout;
  char *st;
  const char *str;

  va_start (vargies, str1);
  str = str1;
  while (str)
    {
      len += strlen (str);
      str = va_arg (vargies, const char *);
    }
  va_end (vargies);

  if (!(sout = slaw_string_raw (len))
      || !(st = SLAW_STRING_EMIT_WRITABLE (sout)))
    return NULL;

  va_start (vargies, str1);
  str = str1;
  while (str)
    {
      len = strlen (str);
      memcpy (st, str, len);
      st += len;
      str = va_arg (vargies, const char *);
    }
  va_end (vargies);

  *st = '\0';
  return sout;
}

slaw slaw_strings_join (bslaw strlist, const char *sep)
{
  int64 q, num, len = 0;
  bslaw s;
  slaw sout;
  char *st;
  const char *str;
  size_t sepLen;

  if (!strlist)
    return NULL;

  num = slaw_list_count (strlist);
  sepLen = ((sep && num) ? strlen (sep) : 0);

  for (s = slaw_list_emit_first (strlist); s != NULL;
       s = slaw_list_emit_next (strlist, s))
    {
      str = slaw_string_emit (s);
      if (str)
        len += strlen (str);
    }

  len += sepLen * (num - 1);

  if (!(sout = slaw_string_raw (len))
      || !(st = SLAW_STRING_EMIT_WRITABLE (sout)))
    return NULL;

  for (q = 0, s = slaw_list_emit_first (strlist); s != NULL;
       q++, s = slaw_list_emit_next (strlist, s))
    {
      if (sepLen && q)
        {
          memcpy (st, sep, sepLen);
          st += sepLen;
        }
      str = slaw_string_emit (s);
      if (!str)
        continue;
      len = strlen (str);
      memcpy (st, str, len);
      st += len;
    }

  *st = '\0';
  return sout;
}

slaw slaw_strings_join_f (slaw strlist, const char *sep)
{
  slaw result = slaw_strings_join (strlist, sep);
  if (result)
    slaw_free (strlist);

  return result;
}

slaw slaw_strings_join_slabu_f (slabu *sb, const char *sep)
{
  slaw result = slaw_strings_join_slabu (sb, sep);
  if (result)
    slabu_free (sb);

  return result;
}

bool slaw_string_is_valid_utf8 (bslaw s)
{
  bool invalid, dummy;

  if (!s || !slaw_is_string (s))
    return false;

  ob_analyze_utf8 (slaw_string_emit (s), slaw_string_emit_length (s), &invalid,
                   &dummy);
  return !invalid;
}
