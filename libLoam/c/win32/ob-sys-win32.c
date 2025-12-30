
/* (c)  oblong industries */

#include <crtdefs.h>
#include <stdio.h>
#include "libLoam/c/ob-sys.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-util.h"
#include "libLoam/c/ob-atomic.h"
#include "libLoam/c/ob-pthread.h"
#include "libLoam/c/ob-string.h"

void prepare_windows_handle_name (char *name)
{
  if (name == NULL)
    return;

  //clean non-alphanumeric chars out of the name so that
  //we can use it as a valid windows event object name
  //
  //(replace with _)
  char *c = name;
  while (*c)
    {
      if ((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z')
          || (*c >= '0' && *c <= '9'))
        {
          //it's valid, do nothing
        }
      else
        {
          //ok, its no good, replace it
          *c = '_';
        }

      c++;
    }
}

/* C99 snprintf always NUL-terminates (and thus writes at most n-1 chars)
 * but Windows _snprintf will go ahead and write n chars and leave
 * them unterminated.  So, we try to simulate the C99 behavior by
 * clearing the buffer and then passing a size one byte less.
 *
 * http://msdn.microsoft.com/en-us/library/1kt27hek(VS.80).aspx
 * http://www.winehq.org/pipermail/wine-devel/2003-November/022493.html
 *
 * We also emulate the correct, C99 return value, as well:
 *
 * http://bytes.com/groups/c/590845-snprintf-return-value
 */
int ob_vsnprintf (char *buf, size_t len, const char *fmt, va_list ap)
{
  va_list atmp = ap;  // don't have va_copy, but assignment seems to work
  int ret = -1;

  if (buf && len > 0)
    {
      memset (buf, 0, len);  // make sure the string will be NUL-terminated
      len--;  // and then subtract one to make sure at least one NUL is left
      ret = _vsnprintf (buf, len, fmt, ap);
    }

  if (ret < 0)
    {
      ap = atmp;
      ret = _vscprintf (fmt, ap);
    }

  return ret;
}

int ob_snprintf (char *buf, size_t len, const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
  int ret = ob_vsnprintf (buf, len, fmt, ap);
  va_end (ap);
  return ret;
}

int fork ()
{
  OB_LOG_BUG_CODE (0x10030001,
                   "fork() does not exist on windows, "
                   "must re-write code to spawn child processes explicitly");
  return -1;
}

//win32 replacement for basename()
char *basename (char *name)
{
  char *rval = name;

  while (name && *name)
    {
      if (*name == '/' || *name == '\\')
        rval = name + 1;

      name++;
    }
  return rval;
}

long volatile winsock_refcount = 0;

void winsock_init ()
{
  long rval = InterlockedIncrement (&winsock_refcount);
  //fprintf(stderr,"winsock_refcount - %d\n", rval);

  WORD wVersionRequested;
  WSADATA wsaData;

  // WORD wVersionRequested = MAKEWORD( 2, 0 ); // Version 2.0
  // Vicon HAL requests MAKEWORD( 2, 0 ), MSDN sample code requests MAKEWORD( 2, 2 )
  // currently use 2.2
  // if we have trouble with Vicon connectivity, perhaps try going to 2.0
  wVersionRequested = MAKEWORD (2, 2);
  WSAStartup (wVersionRequested, &wsaData);
}

void winsock_shutdown ()
{
  WSACleanup ();

  long rval = InterlockedDecrement (&winsock_refcount);
  //fprintf(stderr,"winsock_refcount - %d\n", rval);
  if (rval < 0)
    OB_LOG_BUG_CODE (0x10030002, "winsock_refcount is < 0 - %d\n", rval);
}

//replacement for getopt()
int opterr = 1;
int optind = 1;
int optopt;
char *optarg;

int optinda ()
{
  return optind;
}

#define ERR(s, c)                                                              \
  if (opterr)                                                                  \
    {                                                                          \
      char errbuf[2];                                                          \
      errbuf[0] = c;                                                           \
      errbuf[1] = '\n';                                                        \
      fputs (argv[0], stderr);                                                 \
      fputs (s, stderr);                                                       \
      fputc (c, stderr);                                                       \
      fputc ('\n', stderr);                                                    \
    }

int getopt (int argc, char **argv, char *opts)
{
  static int sp = 1;
  register int c;
  register char *cp;

  if (sp == 1)
    if (optind >= argc || argv[optind][0] != '-' || argv[optind][1] == '\0')
      return (EOF);
    else if (strcmp (argv[optind], "--") == 0)
      {
        optind++;
        return (EOF);
      }
  optopt = c = argv[optind][sp];
  if (c == ':' || (cp = strchr (opts, c)) == NULL)
    {
      ERR (": illegal option -- ", c);
      if (argv[optind][++sp] == '\0')
        {
          optind++;
          sp = 1;
        }
      return ('?');
    }
  if (*++cp == ':')
    {
      if (argv[optind][sp + 1] != '\0')
        optarg = &argv[optind++][sp + 1];
      else if (++optind >= argc)
        {
          ERR (": option requires an argument -- ", c);
          sp = 1;
          return ('?');
        }
      else
        optarg = argv[optind++];
      sp = 1;
    }
  else
    {
      if (argv[optind][++sp] == '\0')
        {
          sp = 1;
          optind++;
        }
      optarg = NULL;
    }
  return (c);
}


void gettimeofday (struct timeval *p, void *tz /* IGNORED */)
{
  union
  {
    long long ns100;  // time since 1 Jan 1601 in 100ns units
    FILETIME ft;
  } now;

  GetSystemTimeAsFileTime (&(now.ft));
  p->tv_usec = (long) ((now.ns100 / 10LL) % 1000000LL);
  p->tv_sec = (long) ((now.ns100 - (116444736000000000LL)) / 10000000LL);
}

int usleep (unsigned useconds)
{
  // Windows does not actually provide a way to wait in microseconds
  if (useconds == 0)
    Sleep (0);
  else
    Sleep ((useconds / 1000) + 1);
  return 0;
}

int sleep (unsigned seconds)
{
  Sleep (seconds * 1000);
  return 0;
}

int sched_yield ()
{
  Sleep (0);
  return 0;
}

int ob_asprintf (char **out_ptr, const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
  int ret = ob_vasprintf (out_ptr, fmt, ap);
  va_end (ap);
  return ret;
}

void ms_windows_not_ready ()
{
  OB_LOG_BUG_CODE (0x10030000, "not implemented\n");
}

int ob_vasprintf (char **out_ptr, const char *fmt, va_list ap)
{
  va_list atmp = ap;

  if (out_ptr == NULL)
    return -1;

  *out_ptr = NULL;

  int num_chars = _vscprintf (fmt, ap);
  if (num_chars < 0)
    return -1;

  size_t len = 1 + num_chars;  //+1 for trailing NUL
  char *buf = (char *) malloc (len);
  if (buf == NULL)
    return -1;

  if (num_chars != ob_vsnprintf (buf, len, fmt, ap))
    return -1;

  *out_ptr = buf;
  return num_chars;
}

CRITICAL_SECTION *ob_fetch_critical (void **vpp)
{
  void *vp = ob_atomic_pointer_ref (vpp);
  if (!vp)
    {
      CRITICAL_SECTION *cs =
        (CRITICAL_SECTION *) calloc (1, sizeof (CRITICAL_SECTION));
      if (!cs || !InitializeCriticalSectionAndSpinCount (cs, 4000))
        {
          free (cs);
          return NULL;
        }
      if (ob_atomic_pointer_compare_and_swap (vpp, NULL, cs))
        vp = cs;  // we won
      else
        {
          // somebody else beat us to it, so deallocate our critical
          // section, because it didn't "win"
          DeleteCriticalSection (cs);
          free (cs);
          vp = ob_atomic_pointer_ref (vpp);  // get the guy who did win
        }
    }
  return (CRITICAL_SECTION *) vp;
}

FARPROC ob_w32_wish_for_func (ob_w32_func_wish *wish)
{
  CRITICAL_SECTION *cs = ob_fetch_critical (&wish->critical);
  EnterCriticalSection (cs);

  if (!wish->proc && !wish->misfortune)
    {
      wish->module = LoadLibrary (wish->dll);
      if (wish->module)
        wish->proc = GetProcAddress (wish->module, wish->func);
      if (!wish->proc)
        wish->misfortune = GetLastError ();
    }

  FARPROC result = wish->proc;
  DWORD lasterr = wish->misfortune;

  LeaveCriticalSection (cs);
  if (lasterr)
    SetLastError (lasterr);
  return result;
}

// -------------------------- pthread emulation ---------------------------

//because windows threads define their entry functions differently,
//our pthreads have to enter through the a windows entry function
//first. this structure carries the pthread function and args to
//call.
typedef struct
{
  pthread_func_t pthread_main;
  void *pthread_args;
} thread_entry_struct;

//all pthreads will enter through here
static unsigned int __stdcall pthread_entry (void *args)
{
  //copy the data out of the struct and free it
  thread_entry_struct *tes = (thread_entry_struct *) args;

  pthread_func_t pthread_main = tes->pthread_main;
  void *pthread_args = tes->pthread_args;
  free (tes);

  //call the thread function
  unsigned int rval = (unsigned int) pthread_main (pthread_args);

  return rval;
}

int pthread_create (pthread_t *out_handle, void *attr_ignored,
                    pthread_func_t thread_func, void *thread_args)
{
  //allocate the entry struct (it will be freed by the thread when the thread
  //exits)
  thread_entry_struct *tes =
    (thread_entry_struct *) malloc (sizeof (thread_entry_struct));
  tes->pthread_main = thread_func;
  tes->pthread_args = thread_args;

  //start the thread
  HANDLE new_handle =
    (HANDLE) _beginthreadex (NULL, 0, pthread_entry, tes, 0, NULL);

  if (new_handle == 0)
    {
      OB_FATAL_ERROR_CODE (
        0x10030003, "error creating thread - %s\n",
        ob_error_string (ob_win32err_to_retort (GetLastError ())));
    }

  *out_handle = new_handle;
  return 0;  //0 means success
}

int pthread_timedjoin_np (pthread_t thread, void **value_ptr,
                          DWORD dwMilliseconds)
{
  //wait for the thread to finish
  DWORD res = WaitForSingleObject (thread, dwMilliseconds);
  if (res == WAIT_OBJECT_0)
    {
      DWORD exit_code = 0;
      if (!GetExitCodeThread (thread, &exit_code))
        {
          OB_LOG_ERROR_CODE (
            0x10030004, "error getting thread exit code - %s\n",
            ob_error_string (ob_win32err_to_retort (GetLastError ())));
          exit_code = 1;
        }

      //copy the exit code into the return value_ptr
      if (value_ptr)
        *value_ptr = (void *) exit_code;

      return 0;
    }
  else if (res == WAIT_TIMEOUT)
    // Linux's pthread_timedjoin_np returns ETIMEDOUT on timeout, but
    // Windows doesn't have ETIMEDOUT, so return EBUSY instead.  This
    // is not too far off, since Linux's pthread_tryjoin_np does return
    // EBUSY.
    return EBUSY;
  else if (res == WAIT_FAILED)
    {
      OB_FATAL_ERROR_CODE (
        0x10030006, "error on pthread_join: WAIT_FAILED: %s\n",
        ob_error_string (ob_win32err_to_retort (GetLastError ())));
    }

  //any error causes an exit
  OB_FATAL_ERROR_CODE (0x10030005,
                       "error on pthread_join: WaitForSingleObject "
                       "returned 0x%08x\n",
                       res);
}

// Since we used _beginthreadex to create the thread, we just close the thread
// handle using CloseHandle.
// From the spec: If the call succeeds, pthread_detach() shall return 0
int pthread_detach (pthread_t thread)
{
  return (CloseHandle (thread) == FALSE);
}

// ------------------------ emulation of scandir() ------------------------

/* Rather than use an unsupported function that breaks in win10, emulate it. */
static void ob_dosmaperr (unsigned long e);

typedef BOOL (*findfunc) (const char *, HANDLE *, WIN32_FIND_DATA *);

static BOOL find_first_file (const char *d, HANDLE *h, WIN32_FIND_DATA *f)
{
  // add wildcard to d
  const size_t len = strlen (d);
  char *w = (char *) alloca (len + 3);
  memcpy (w, d, len);
  w[len] = '\\';
  w[len + 1] = '*';
  w[len + 2] = 0;
  *h = FindFirstFile (w, f);
  return (*h != INVALID_HANDLE_VALUE);
}

static BOOL find_next_file (const char *pacify_compiler_ignored_d, HANDLE *h,
                            WIN32_FIND_DATA *f)
{
  return FindNextFile (*h, f);
}

typedef int (*qsort_cmp_func) (const void *, const void *);

int ob_scandir (const char *dirp, struct dirent ***namelist,
                int (*filter) (const struct dirent *),
                int (*compar) (const struct dirent **, const struct dirent **))
{
  HANDLE find_handle = INVALID_HANDLE_VALUE;
  struct dirent *ent = NULL;

  int size_ents = 32;
  int n_ents = 0;
  struct dirent **ents = calloc (size_ents, sizeof (struct dirent *));

  if (!ents)
    SetLastError (ERROR_NOT_ENOUGH_MEMORY);
  else
    for (findfunc find_file = find_first_file;; find_file = find_next_file)
      {
        if (!ent)
          ent = (struct dirent *) calloc (1, sizeof (struct dirent));
        if (!ent)
          {
            SetLastError (ERROR_NOT_ENOUGH_MEMORY);
            break;
          }

        if (!find_file (dirp, &find_handle, &(ent->d_win32)))
          break;

        // set d_name and d_type
        ent->d_name = ent->d_win32.cFileName;
        if (ent->d_win32.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
          ent->d_type = DT_DIR;
        else if (ent->d_win32.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
          ent->d_type = DT_LNK;
        else
          ent->d_type = DT_REG;

        if (!filter || filter (ent))
          {
            if (n_ents == size_ents)
              {
                size_ents *= 2;
                struct dirent **tmp =
                  calloc (size_ents, sizeof (struct dirent *));
                if (!tmp)
                  {
                    SetLastError (ERROR_NOT_ENOUGH_MEMORY);
                    break;
                  }
                memcpy (tmp, ents, n_ents * sizeof (struct dirent *));
                free (ents);
                ents = tmp;
              }
            ents[n_ents++] = ent;
            ent = NULL;
          }
      }

  DWORD lasterr = GetLastError ();
  free (ent);

  if (find_handle != INVALID_HANDLE_VALUE && !FindClose (find_handle)
      && lasterr == ERROR_NO_MORE_FILES)
    lasterr = GetLastError ();

  if (lasterr == ERROR_NO_MORE_FILES)
    {
      if (compar)
        qsort (ents, n_ents, sizeof (struct dirent *), (qsort_cmp_func) compar);
      *namelist = ents;
      return n_ents;
    }

  // some kind of error; "lasterr" should be set

  ob_dosmaperr (lasterr); /* translates lasterr and sets errno */
  return -1;
}

// --------------- encapsulation of Win32 errors in retorts ---------------

ob_retort ob_win32err_to_retort (DWORD w32err)
{
  return -(w32err | OB_RETORTS_WIN32_FIRST);
}

DWORD ob_retort_to_win32err (ob_retort tort)
{
  if (tort <= -OB_RETORTS_WIN32_FIRST && tort >= -OB_RETORTS_WIN32_LAST)
    return (DWORD) -tort;
  else
    return ~(DWORD) 0;
}

// --------------- code stolen from strsep.c ---------------

// From http://svn.freebsd.org/viewvc/base/head/lib/libc/string/strsep.c?revision=188080&view=markup

/*-
 * Copyright (c) 1990, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

static const char sccsid[] = "@(#)strsep.c    8.1 (Berkeley) 6/4/93";

/*
 * Get next token from string *stringp, where tokens are possibly-empty
 * strings separated by characters from delim.
 *
 * Writes NULs into the string at *stringp to end tokens.
 * delim need not remain constant from call to call.
 * On return, *stringp points past the last NUL written (if there might
 * be further tokens), or is NULL (if there are definitely no more tokens).
 *
 * If *stringp is NULL, strsep returns NULL.
 */
char *strsep (char **stringp, const char *delim)
{
  char *s;
  const char *spanp;
  int c, sc;
  char *tok;

  if ((s = *stringp) == NULL)
    return (NULL);
  for (tok = s;;)
    {
      c = *s++;
      spanp = delim;
      do
        {
          if ((sc = *spanp++) == c)
            {
              if (c == 0)
                s = NULL;
              else
                s[-1] = 0;
              *stringp = s;
              return (tok);
            }
        }
      while (sc != 0);
    }
  /* NOTREACHED */
}

#if defined(_MSC_VER) && _MSC_VER < 1800
long int lrint (double x)
{
  return (long int) (floor (x + (x > 0) ? 0.5 : -0.5));
}
#endif

#if defined(_MSC_VER)
/* Windows does not ship with an implementation of strptime, so grab one from
 * https://github.com/heimdal/heimdal/blob/master/lib/roken/strptime.c
 * by way of
 * https://raw.githubusercontent.com/DMS-Aus/MapServer/master/strptime.c
 * with a minor change to ignore the '#' modifier.
 */
/*
 * Copyright (c) 1999 Kungliga Tekniska Hgskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of KTH nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY KTH AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL KTH OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include <ctype.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static const char *abb_weekdays[] = {"Sun", "Mon", "Tue", "Wed",
                                     "Thu", "Fri", "Sat", NULL};

static const char *full_weekdays[] = {"Sunday",    "Monday",   "Tuesday",
                                      "Wednesday", "Thursday", "Friday",
                                      "Saturday",  NULL};

static const char *abb_month[] = {"Jan", "Feb", "Mar", "Apr", "May",
                                  "Jun", "Jul", "Aug", "Sep", "Oct",
                                  "Nov", "Dec", NULL};

static const char *full_month[] = {
  "January", "February",  "Mars",    "April",    "May",      "June", "July",
  "August",  "September", "October", "November", "December", NULL,
};

static const char *ampm[] = {"am", "pm", NULL};

/*
 * tm_year is relative this year
 */
const int tm_year_base = 1900;

/*
 * Return TRUE iff `year' was a leap year.
 * Needed for strptime.
 */
static int is_leap_year (int year)
{
  return (year % 4) == 0 && ((year % 100) != 0 || (year % 400) == 0);
}

/* Needed for strptime. */
static int match_string (const char **buf, const char **strs)
{
  int i = 0;

  for (i = 0; strs[i] != NULL; ++i)
    {
      int len = strlen (strs[i]);

      if (strncasecmp (*buf, strs[i], len) == 0)
        {
          *buf += len;
          return i;
        }
    }
  return -1;
}

/* Needed for strptime. */
static int first_day (int year)
{
  int ret = 4;

  for (; year > 1970; --year)
    ret = (ret + 365 + is_leap_year (year) ? 1 : 0) % 7;
  return ret;
}

/*
 * Set `timeptr' given `wnum' (week number [0, 53])
 * Needed for strptime
 */

static void set_week_number_sun (struct tm *timeptr, int wnum)
{
  int fday = first_day (timeptr->tm_year + tm_year_base);

  timeptr->tm_yday = wnum * 7 + timeptr->tm_wday - fday;
  if (timeptr->tm_yday < 0)
    {
      timeptr->tm_wday = fday;
      timeptr->tm_yday = 0;
    }
}

/*
 * Set `timeptr' given `wnum' (week number [0, 53])
 * Needed for strptime
 */

static void set_week_number_mon (struct tm *timeptr, int wnum)
{
  int fday = (first_day (timeptr->tm_year + tm_year_base) + 6) % 7;

  timeptr->tm_yday = wnum * 7 + (timeptr->tm_wday + 6) % 7 - fday;
  if (timeptr->tm_yday < 0)
    {
      timeptr->tm_wday = (fday + 1) % 7;
      timeptr->tm_yday = 0;
    }
}

/*
 * Set `timeptr' given `wnum' (week number [0, 53])
 * Needed for strptime
 */
static void set_week_number_mon4 (struct tm *timeptr, int wnum)
{
  int fday = (first_day (timeptr->tm_year + tm_year_base) + 6) % 7;
  int offset = 0;

  if (fday < 4)
    offset += 7;

  timeptr->tm_yday = offset + (wnum - 1) * 7 + timeptr->tm_wday - fday;
  if (timeptr->tm_yday < 0)
    {
      timeptr->tm_wday = fday;
      timeptr->tm_yday = 0;
    }
}

/* strptime: roken */
/* extern "C" */
char *
/* strptime (const char *buf, const char *format, struct tm *timeptr) */
strptime (const char *buf, const char *format, struct tm *timeptr)
{
  char c;

  for (; (c = *format) != '\0'; ++format)
    {
      char *s;
      int ret;

      if (isspace (c))
        {
          while (isspace (*buf))
            ++buf;
        }
      else if (c == '%' && format[1] != '\0')
        {
          c = *++format;
          /* Skip modifiers this function does not implement yet */
          if (c == 'E' || c == 'O' || c == '#')
            c = *++format;
          switch (c)
            {
              case 'A':
                ret = match_string (&buf, full_weekdays);
                if (ret < 0)
                  return NULL;
                timeptr->tm_wday = ret;
                break;
              case 'a':
                ret = match_string (&buf, abb_weekdays);
                if (ret < 0)
                  return NULL;
                timeptr->tm_wday = ret;
                break;
              case 'B':
                ret = match_string (&buf, full_month);
                if (ret < 0)
                  return NULL;
                timeptr->tm_mon = ret;
                break;
              case 'b':
              case 'h':
                ret = match_string (&buf, abb_month);
                if (ret < 0)
                  return NULL;
                timeptr->tm_mon = ret;
                break;
              case 'C':
                ret = strtol (buf, &s, 10);
                if (s == buf)
                  return NULL;
                timeptr->tm_year = (ret * 100) - tm_year_base;
                buf = s;
                break;
              case 'c':
                abort ();
              case 'D': /* %m/%d/%y */
                s = strptime (buf, "%m/%d/%y", timeptr);
                if (s == NULL)
                  return NULL;
                buf = s;
                break;
              case 'd':
              case 'e':
                ret = strtol (buf, &s, 10);
                if (s == buf)
                  return NULL;
                timeptr->tm_mday = ret;
                buf = s;
                break;
              case 'H':
              case 'k':
                ret = strtol (buf, &s, 10);
                if (s == buf)
                  return NULL;
                timeptr->tm_hour = ret;
                buf = s;
                break;
              case 'I':
              case 'l':
                ret = strtol (buf, &s, 10);
                if (s == buf)
                  return NULL;
                if (ret == 12)
                  timeptr->tm_hour = 0;
                else
                  timeptr->tm_hour = ret;
                buf = s;
                break;
              case 'j':
                ret = strtol (buf, &s, 10);
                if (s == buf)
                  return NULL;
                timeptr->tm_yday = ret - 1;
                buf = s;
                break;
              case 'm':
                ret = strtol (buf, &s, 10);
                if (s == buf)
                  return NULL;
                timeptr->tm_mon = ret - 1;
                buf = s;
                break;
              case 'M':
                ret = strtol (buf, &s, 10);
                if (s == buf)
                  return NULL;
                timeptr->tm_min = ret;
                buf = s;
                break;
              case 'n':
                if (*buf == '\n')
                  ++buf;
                else
                  return NULL;
                break;
              case 'p':
                ret = match_string (&buf, ampm);
                if (ret < 0)
                  return NULL;
                if (timeptr->tm_hour == 0)
                  {
                    if (ret == 1)
                      timeptr->tm_hour = 12;
                  }
                else
                  timeptr->tm_hour += 12;
                break;
              case 'r': /* %I:%M:%S %p */
                s = strptime (buf, "%I:%M:%S %p", timeptr);
                if (s == NULL)
                  return NULL;
                buf = s;
                break;
              case 'R': /* %H:%M */
                s = strptime (buf, "%H:%M", timeptr);
                if (s == NULL)
                  return NULL;
                buf = s;
                break;
              case 'S':
                ret = strtol (buf, &s, 10);
                if (s == buf)
                  return NULL;
                timeptr->tm_sec = ret;
                buf = s;
                break;
              case 't':
                if (*buf == '\t')
                  ++buf;
                else
                  return NULL;
                break;
              case 'T': /* %H:%M:%S */
              case 'X':
                s = strptime (buf, "%H:%M:%S", timeptr);
                if (s == NULL)
                  return NULL;
                buf = s;
                break;
              case 'u':
                ret = strtol (buf, &s, 10);
                if (s == buf)
                  return NULL;
                timeptr->tm_wday = ret - 1;
                buf = s;
                break;
              case 'w':
                ret = strtol (buf, &s, 10);
                if (s == buf)
                  return NULL;
                timeptr->tm_wday = ret;
                buf = s;
                break;
              case 'U':
                ret = strtol (buf, &s, 10);
                if (s == buf)
                  return NULL;
                set_week_number_sun (timeptr, ret);
                buf = s;
                break;
              case 'V':
                ret = strtol (buf, &s, 10);
                if (s == buf)
                  return NULL;
                set_week_number_mon4 (timeptr, ret);
                buf = s;
                break;
              case 'W':
                ret = strtol (buf, &s, 10);
                if (s == buf)
                  return NULL;
                set_week_number_mon (timeptr, ret);
                buf = s;
                break;
              case 'x':
                s = strptime (buf, "%Y:%m:%d", timeptr);
                if (s == NULL)
                  return NULL;
                buf = s;
                break;
              case 'y':
                ret = strtol (buf, &s, 10);
                if (s == buf)
                  return NULL;
                if (ret < 70)
                  timeptr->tm_year = 100 + ret;
                else
                  timeptr->tm_year = ret;
                buf = s;
                break;
              case 'Y':
                ret = strtol (buf, &s, 10);
                if (s == buf)
                  return NULL;
                timeptr->tm_year = ret - tm_year_base;
                buf = s;
                break;
              case 'Z':
                abort ();
              case '\0':
                --format;
              /* FALLTHROUGH */
              case '%':
                if (*buf == '%')
                  ++buf;
                else
                  return NULL;
                break;
              default:
                if (*buf == '%' || *++buf == c)
                  ++buf;
                else
                  return NULL;
                break;
            }
        }
      else
        {
          if (*buf == c)
            ++buf;
          else
            return NULL;
        }
    }
  return (char *) buf;
}

// not quite sure when _dosmaperr went away... is
// it the compiler (msvc2015), or the operating system (win10)?

// code stolen from postgresql-9.5-9.5.5/src/port/win32error.c
// License: https://www.postgresql.org/about/licence/

static const struct
{
  DWORD winerr;
  int doserr;
} doserrors[] = {{ERROR_INVALID_FUNCTION, EINVAL},
                 {ERROR_FILE_NOT_FOUND, ENOENT},
                 {ERROR_PATH_NOT_FOUND, ENOENT},
                 {ERROR_TOO_MANY_OPEN_FILES, EMFILE},
                 {ERROR_ACCESS_DENIED, EACCES},
                 {ERROR_INVALID_HANDLE, EBADF},
                 {ERROR_ARENA_TRASHED, ENOMEM},
                 {ERROR_NOT_ENOUGH_MEMORY, ENOMEM},
                 {ERROR_INVALID_BLOCK, ENOMEM},
                 {ERROR_BAD_ENVIRONMENT, E2BIG},
                 {ERROR_BAD_FORMAT, ENOEXEC},
                 {ERROR_INVALID_ACCESS, EINVAL},
                 {ERROR_INVALID_DATA, EINVAL},
                 {ERROR_INVALID_DRIVE, ENOENT},
                 {ERROR_CURRENT_DIRECTORY, EACCES},
                 {ERROR_NOT_SAME_DEVICE, EXDEV},
                 {ERROR_NO_MORE_FILES, ENOENT},
                 {ERROR_LOCK_VIOLATION, EACCES},
                 {ERROR_SHARING_VIOLATION, EACCES},
                 {ERROR_BAD_NETPATH, ENOENT},
                 {ERROR_NETWORK_ACCESS_DENIED, EACCES},
                 {ERROR_BAD_NET_NAME, ENOENT},
                 {ERROR_FILE_EXISTS, EEXIST},
                 {ERROR_CANNOT_MAKE, EACCES},
                 {ERROR_FAIL_I24, EACCES},
                 {ERROR_INVALID_PARAMETER, EINVAL},
                 {ERROR_NO_PROC_SLOTS, EAGAIN},
                 {ERROR_DRIVE_LOCKED, EACCES},
                 {ERROR_BROKEN_PIPE, EPIPE},
                 {ERROR_DISK_FULL, ENOSPC},
                 {ERROR_INVALID_TARGET_HANDLE, EBADF},
                 {ERROR_INVALID_HANDLE, EINVAL},
                 {ERROR_WAIT_NO_CHILDREN, ECHILD},
                 {ERROR_CHILD_NOT_COMPLETE, ECHILD},
                 {ERROR_DIRECT_ACCESS_HANDLE, EBADF},
                 {ERROR_NEGATIVE_SEEK, EINVAL},
                 {ERROR_SEEK_ON_DEVICE, EACCES},
                 {ERROR_DIR_NOT_EMPTY, ENOTEMPTY},
                 {ERROR_NOT_LOCKED, EACCES},
                 {ERROR_BAD_PATHNAME, ENOENT},
                 {ERROR_MAX_THRDS_REACHED, EAGAIN},
                 {ERROR_LOCK_FAILED, EACCES},
                 {ERROR_ALREADY_EXISTS, EEXIST},
                 {ERROR_FILENAME_EXCED_RANGE, ENOENT},
                 {ERROR_NESTING_NOT_ALLOWED, EAGAIN},
                 {ERROR_NOT_ENOUGH_QUOTA, ENOMEM}};

static void ob_dosmaperr (unsigned long e)
{
  int i;

  if (e == 0)
    {
      errno = 0;
      return;
    }

  for (i = 0; i < sizeof (doserrors) / sizeof (doserrors[0]); i++)
    {
      if (doserrors[i].winerr == e)
        {
          int doserr = doserrors[i].doserr;
          errno = doserr;
          return;
        }
    }

  fprintf (stderr, "unrecognized win32 error code: %lu\n", e);
  errno = EINVAL;
  return;
}
#endif
