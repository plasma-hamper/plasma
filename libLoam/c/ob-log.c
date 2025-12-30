
/* (c)  oblong industries */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE  // necessary to enable non-portable shenanigans on Linux
#endif

#include "libLoam/c/ob-log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>
#ifndef _MSC_VER
#include <spawn.h>
#endif

#include "libLoam/c/ob-sys.h"
#include "libLoam/c/ob-attrs.h"
#include "libLoam/c/ob-util.h"
#include "libLoam/c/ob-dirs.h"
#include "libLoam/c/ob-atomic.h"
#include "libLoam/c/ob-file.h"
#include "libLoam/c/ob-string.h"
#include "libLoam/c/ob-time.h"
#include "libLoam/c/ob-pthread.h"
#include "libLoam/c/private/ob-syslog.h"

#if 0
// valgrind.h uses gcc extensions, so we must not use it on other compilers
#include "libLoam/c/valgrind/valgrind.h"
#endif

/* snprintf is not thread-safe on Mac, at least not on OS X 10.5.

  We think this is because snprintf causes a call to localeconv(), and
  localeconv() is not thread-safe.

  Since it is not feasible to avoid snprintf everywhere, we made
  a global, implicit workaround, which was to make an initial
  "priming" call to localeconv().  This appears to make subsequent
  calls to snprintf thread-safe.

  This problem with snprintf was exposed by warnings about double free
  calls in pingpong_test.

  These warnings were subsequently turned into errors by setting
  MallocErrorAbort=1 in TESTS_ENVIRONMENT.

  Using gdb to trap this abort revealed the following stack.

    #0  0xN in __semwait_signal_nocancel ()
    #1  0xN in nanosleep$NOCANCEL$UNIX2003 ()
    #2  0xN in usleep$NOCANCEL$UNIX2003 ()
    #3  0xN in abort ()
    #4  0xN in szone_error ()
    #5  0xN in szone_free ()
    #6  0xN in free ()
    #7  0xN in localeconv_l ()
    #8  0xN in __vfprintf ()
    #9  0xN in snprintf ()
    #10 0xN in pool_dir_path (path "") at libPlasma/pool.c:177
    #11 0xN in pool_config_lock () at libPlasma/pool.c:1028
    #12 0xN in pool_participate_creatingly (
                        pool_name "test_pool2",
                        type "mmap",
                        ret_ph,
                        options) at libPlasma/pool.c:1344
    #13 0xN in join_pools (thread_num=1) at libPlasma/pingpong_test.c:88
    #14 0xN in pingpong_main (arg) at libPlasma/pingpong_test.c:134
    #15 0xN in _pthread_start ()
    #16 0xN in thread_start ()

  This stack is from the "asterisked" (starred) thread in the thread
  list below.

      6 process N thread 0xN  0xN in flock ()
      5 process N thread 0xN  0xN in flock ()
      4 process N thread 0xN  0xN in flock ()
    * 3 process N thread 0xN  0xN in __semwait_signal_nocancel ()
      2 process N thread 0xN  0xN in ?? ()
      1 process N local thread 0xN  0xN in __semwait_signal ()

  We are pretty confident this is a bug in Mac OS X, because it has
  been seen elsewhere:

  http://trac.videolan.org/vlc/ticket/1703
  http://trac.videolan.org/vlc/ticket/1724
  http://lists.apple.com/archives/xcode-users/2007/Oct/msg00214.html
  http://foldingforum.org/viewtopic.php?f=6&t=4452#p44845

  All this trouble is caused by printf trying to get the decimal
  point character.  In __vfprintf() in Libc-498.1.5/stdio/FreeBSD/vfprintf.c:

  #ifndef NO_FLOATING_POINT
        dtoaresult = NULL;
        decimal_point = localeconv()->decimal_point;
  #endif

  It appears that gnu-darwin.org has fixed this in their version
  (http://tinyurl.com/8pu26y) by using a non-global locale:

   #ifndef NO_FLOATING_POINT
        dtoaresult = NULL;
  -     decimal_point = localeconv()->decimal_point;
  +     decimal_point = localeconv_l(loc)->decimal_point;
   #endif

*/

static void process_env (const char *env);
static void ob_log_bye (void);
static void ob_log_atfork_child (void);

static void ob_log_init (void)
{
  // See big comment above about Mac snprintf being non-thread-safe.
  // It can be made thread-safe by an initial "priming" call to localeconv().
  localeconv ();

  // process OB_LOG environment variable if present
  const char *env = getenv ("OB_LOG");
  if (env)
    process_env (env);

#ifndef _MSC_VER
  // Set up an atfork handler, so that rule counts can be reinitialized
  // to zero in the child process.
  const int erryes = pthread_atfork (NULL, NULL, ob_log_atfork_child);

  if (erryes)
    OB_LOG_ERROR_CODE (0x10060005, "atfork() said '%s'\n", strerror (erryes));
#endif
}

OB_PRE_POST (ob_log_init (), ob_log_bye ());

struct ob_log_file_rule
{
  struct ob_log_file_rule *next;
  bool print;
  char pattern[1];
};

/* The string below is automatically generated from the Doxygen comment
 * in ob-log.h.  To regenerate, simply run "./generate-ob-log-usage.hs"
 * in this directory, with no arguments.
 */

static const char help[] =
  // BEGIN AUTO-GENERATED (DO NOT EDIT)
  "The OB_LOG environment variable consists of space-separated\n"
  "tokens, like this:\n"
  "\n"
  "LEVEL [option [option ...]] [LEVEL [option [option ...]] ...]\n"
  "\n"
  "Where LEVEL is one of: BUG, ERROR, DEPRECATION, WARN, INFO, or DEBUG\n"
  "\n"
  "And options are:\n"
  "  (flag-like options can be prefixed with \"no\" to turn them off)\n"
  "\n"
  "stderr - log to standard error\n"
  "\n"
  "./file/name - log to the named file (must contain a slash)\n"
  "\n"
  "nofd - don't log to previously specified file or stderr\n"
  "\n"
  "[no]syslog - log to syslog\n"
  "\n"
  "[no]stack - include stack trace when printing message\n"
  "\n"
  "[no]time - print time along with message\n"
  "\n"
  "[no]full - print full filename and line number with message\n"
  "\n"
  "[no]base - print file basename and line number with message\n"
  "\n"
  "[no]code - print 64-bit \"code\" (see below) with message\n"
  "\n"
  "[no]pid - print process ID with message\n"
  "\n"
  "[no]prog - print program name with message\n"
  "\n"
  "[no]tid - print thread ID with message\n"
  "\n"
  "none - turn off logging for all codes (same as \"????????????????"
  "=off\")\n"
  "\n"
  "+pattern - always turn on logging from source files that match\n"
  "           \"pattern\" (which is a glob pattern) regardless of\n"
  "           the code matching (below)\n"
  "\n"
  "-pattern - always turn off logging from source files that match\n"
  "           \"pattern\" (which is a glob pattern) regardless of\n"
  "           the code matching (below)\n"
  "\n"
  "           The glob pattern is matched against __FILE__, which is\n"
  "           usually only the basename; see\n"
  "           https://lists.oblong.com/pipermail/dev/2013-January/000310.html\n"
  "\n"
  "xxxx????"
  "=on - turn on logging for the specified range of codes\n"
  "\n"
  "xxxx????"
  "=off - turn off logging for the specified range of codes\n"
  "\n"
  "xxxx????"
  "=<count> - log at most \"count\" messages for the specified\n"
  "range of codes\n"
  "\n"
  "xxxx????:<count> - log at most \"count\" instances of *each* individual\n"
  "code within the specified range of codes\n"
  "\n"
  "In the above, \"xxxx????\" indicates a string of hex digits, where a\n"
  "digit may also be replaced by a question mark.  (But question marks\n"
  "may only appear in trailing positions, not anywhere in the string.)\n"
  "This is a way of specifying a 64-bit \"code\", optionally with some\n"
  "number (although a multiple of four) of low-order bits masked out.\n"
  "Because of hierarchical nature of codes, this lets you control\n"
  "behavior for an entire module at a time.\n"
  "\n"
  "If not modified by the OB_LOG environment variable, the default is\n"
  "for log messages to include the \"code\" if one if given, or the file\n"
  "basename and line number if the \"code\" is zero.  The thread ID is\n"
  "printed for threads which are not the main thread.  The\n"
  "\"programming\" log levels (BUG and DEPRECATION) print stack traces.\n"
  "All log levels print to stderr, except DEBUG, which is disabled by\n"
  "default.\n"
  "\n"
  "Note that the DEBUG level is disabled by virtue of not going to any\n"
  "destination, but all codes are enabled for it.  Therefore, if you\n"
  "enable debug messages with something like \"DEBUG stderr\", you will\n"
  "get all debug messages!  If you only want to print a few debug\n"
  "messages, you need to use \"none\" before enabling the codes you\n"
  "want.  For example:\n"
  "\n"
  "OB_LOG=\"DEBUG stderr none 20108???"
  "=on\"\n";
// END AUTO-GENERATED

static int set_output_fd (ob_log_level *lev, int fd)
{
  int r;
  if (lev->fd <= 2)
    r = lev->fd = ob_dup_cloexec (fd);
  else
    r = ob_dup2_cloexec (fd, lev->fd);
  if (r >= 0)
    lev->flags |= OB_DST_FD;
  return r;
}

/**
 * If \a word is a recognized flag, return true and set \a set to
 * the flags bits which should be set, and \a clr to the flag bits
 * which should be cleared.
 *
 * Returns false if \a word is not a recognized flag.
 */
static bool recognize_flag (const char *word, int32 *set, int32 *clr)
{
  int32 s = 0, c = 0;

  if (0 == strcasecmp (word, "stack"))
    s = OB_FLG_STACK_TRACE;
  else if (0 == strcasecmp (word, "time"))
    s = OB_FLG_SHOW_TIME;
  else if (0 == strcasecmp (word, "full"))
    {
      s = OB_FLG_SHOW_WHERE_FULL;
      c = OB_FLG_SHOW_WHERE | OB_FLG_SHOW_CODE_OR_WHERE;
    }
  else if (0 == strcasecmp (word, "base"))
    {
      s = OB_FLG_SHOW_WHERE;
      c = OB_FLG_SHOW_WHERE_FULL | OB_FLG_SHOW_CODE_OR_WHERE;
    }
  else if (0 == strcasecmp (word, "code"))
    {
      s = OB_FLG_SHOW_CODE;
      c = OB_FLG_SHOW_CODE_OR_WHERE;
    }
  else if (0 == strcasecmp (word, "pid"))
    s = OB_FLG_SHOW_PID;
  else if (0 == strcasecmp (word, "prog"))
    s = OB_FLG_SHOW_PROG;
  else if (0 == strcasecmp (word, "tid"))
    {
      s = OB_FLG_SHOW_TID;
      c = OB_FLG_SHOW_TID_NONMAIN;
    }
  else if (0 == strcasecmp (word, "syslog"))
    s = OB_DST_SYSLOG;
  else if (0 == strcasecmp (word, "valgrind"))
    s = OB_DST_VALGRIND;
  else
    return false;

  *set = s;
  *clr = c;
  return true;
}

static inline bool is_either (char x, char c1, char c2)
{
  return (x == c1 || x == c2);
}

static void process_word (ob_log_level **levp, const char *word)
{
  ob_log_level *lev = *levp;
  int32 set, clr;

  if (0 == strcasecmp (word, "help"))
    OB_LOG_INFO_CODE (0x10060000, "%s", help);
  else if (0 == strcasecmp (word, "ERRP") || 0 == strcasecmp (word, "BUG"))
    *levp = OBLV_BUG;
  else if (0 == strcasecmp (word, "ERRU") || 0 == strcasecmp (word, "ERROR"))
    *levp = OBLV_ERROR;
  else if (0 == strcasecmp (word, "WRNP")
           || 0 == strcasecmp (word, "DEPRECATION"))
    *levp = OBLV_DEPRECATION;
  else if (0 == strcasecmp (word, "WRNU") || 0 == strcasecmp (word, "WARN"))
    *levp = OBLV_WRNU;
  else if (0 == strcasecmp (word, "INFO"))
    *levp = OBLV_INFO;
  else if (0 == strcasecmp (word, "DBUG") || 0 == strcasecmp (word, "DEBUG"))
    *levp = OBLV_DBUG;
  else if (recognize_flag (word, &set, &clr))
    lev->flags = set | (lev->flags & ~clr);
  else if (is_either (word[0], 'N', 'n') && is_either (word[1], 'O', 'o')
           && recognize_flag (2 + word, &set, &clr))
    lev->flags &= ~(set | clr);
  else if (0 == strcasecmp (word, "stderr"))
    set_output_fd (lev, 2);
  else if (0 == strcasecmp (word, "nofd"))
    lev->flags &= ~OB_DST_FD;
  else if (is_either (word[0], '+', '-'))
    ob_log_add_file_rule (lev, 1 + word, '+' == word[0]);
  else if (strchr (word, '/'))
    {
      int fd = ob_open_cloexec (word, O_WRONLY | O_APPEND | O_CREAT, 0666);
      if (fd < 0)
        OB_LOG_ERROR_CODE (0x10060001, "Can't open '%s' because '%s'\n", word,
                           strerror (errno));
      else
        {
          if (set_output_fd (lev, fd) < 0)
            OB_LOG_ERROR_CODE (0x10060003, "Can't dup '%s' because '%s'\n",
                               word, strerror (errno));
          if (close (fd) < 0)
            OB_LOG_ERROR_CODE (0x10060004, "Can't close '%s' because '%s'\n",
                               word, strerror (errno));
        }
    }
  else if (0 == strcasecmp (word, "none"))
    {
      ob_log_rule rul;
      rul.count = 0;
      rul.maxcount = 0;
      rul.code = 0;
      rul.matchbits = 0;
      rul.uniquely = false;
      rul.notify = false;
      ob_log_add_rule (lev, rul);
    }
  else
    {
      const char *p = word;
      unt64 code = 0;
      for (; isxdigit (*p); p++)
        {
          char c = *p;
          int n;
          if (isdigit (c))
            n = c - '0';
          else
            n = c - 'a' + 10;
          code <<= 4;
          code |= n;
        }
      unt8 matchbits = 64;
      for (; *p == '?'; p++)
        {
          matchbits -= 4;
          code <<= 4;
        }
      char c = *p;
      bool uniquely, notify = true;
      int64 maxcount;
      char *endp = "";
      if (c == ':')
        uniquely = true;
      else if (c == '=')
        uniquely = false;
      else
        goto bad;
      p++;
      if (0 == strcasecmp (p, "off"))
        maxcount = 0, notify = false;
      else if (0 == strcasecmp (p, "on"))
        maxcount = OB_CONST_I64 (0x7fffffffffffffff);
      else
        maxcount = strtoll (p, &endp, 10);
      if (*endp)
        {
bad:
          OB_LOG_ERROR_CODE (0x10060002, "unrecognized '%s'; try OB_LOG=help\n",
                             word);
        }
      else
        {
          ob_log_rule rul;
          rul.count = 0;
          rul.maxcount = maxcount;
          rul.code = code;
          rul.matchbits = matchbits;
          rul.uniquely = uniquely;
          rul.notify = notify;
          ob_log_add_rule (lev, rul);
        }
    }
}

static void process_env (const char *env)
{
  char *cpy = strdup (env);
  char *first = cpy;
  char *lasts = NULL;
  char *tokn;
  ob_log_level dummy;
  ob_log_level *lvl = &dummy;

  OB_CLEAR (dummy);

  while ((tokn = strtok_r (first, " ", &lasts)))
    {
      first = NULL;
      process_word (&lvl, tokn);
    }

  free (cpy);
}

ob_retort ob_log_add_file_rule (ob_log_level *lvl, const char *pattern,
                                bool print)
{
  size_t patlen = strlen (pattern);
  ob_log_file_rule *r =
    (ob_log_file_rule *) calloc (1, sizeof (ob_log_file_rule) + patlen);
  if (!r)
    return OB_NO_MEM;
  r->print = print;
  ob_safe_copy_string (r->pattern, patlen + 1, pattern);
  ob_log_file_rule **head = &lvl->frules;
  ob_log_file_rule *victim;
  do
    {
      victim = ob_atomic_pointer_ref (head);
      r->next = victim;
    }
  while (!ob_atomic_pointer_compare_and_swap (head, victim, r));
  return OB_OK;
}

static ob_log_file_rule *ob_log_find_file_rule (ob_log_level *lvl,
                                                const char *fname)
{
  ob_log_file_rule **head;
  ob_log_file_rule *victim;
  for (head = &lvl->frules; (victim = ob_atomic_pointer_ref (head));
       head = &victim->next)
    if (ob_match_glob (fname, victim->pattern))
      return victim;
  return NULL;
}

ob_retort ob_log_add_rule (ob_log_level *lvl, ob_log_rule rul)
{
  ob_log_rule *r = (ob_log_rule *) calloc (1, sizeof (ob_log_rule));
  if (!r)
    return OB_NO_MEM;
  r->count = rul.count;
  r->maxcount = rul.maxcount;
  r->code = rul.code;
  r->matchbits = rul.matchbits;
  r->uniquely = rul.uniquely;
  r->notify = rul.notify;
  ob_log_rule **head = &lvl->rules;
  ob_log_rule *victim;
  do
    {
      victim = ob_atomic_pointer_ref (head);
      if (victim && victim->matchbits > rul.matchbits)
        {
          head = &victim->next;
          continue;
        }
      r->next = victim;
    }
  while (!ob_atomic_pointer_compare_and_swap (head, victim, r));
  return OB_OK;
}

static ob_log_rule *ob_log_find_rule_internal (ob_log_rule **head, unt64 code)
{
  for (;;)
    {
      ob_log_rule *victim = ob_atomic_pointer_ref (head);
      if (!victim)
        return NULL;

      unt64 mask = 0;
      unt8 matchbits = victim->matchbits;
      if (matchbits > 0)
        {
          int64 smask = OB_CONST_I64 (1) << 63;
          // XXX: takes advantage of the technically undefined but universally
          // implemented sign-extending behavior of signed right shift in C.
          smask >>= (matchbits - 1);
          mask = smask;
          // This assertion will fail if sign extension doesn't work the way we
          // expect it to.
          assert (1 == (mask >> 63));
        }

      if ((code & mask) == (victim->code & mask))
        return victim;

      head = &victim->next;
    }
}

static ob_log_rule *ob_log_find_rule (ob_log_level *lvl, unt64 code)
{
  ob_log_rule *rul = ob_log_find_rule_internal (&lvl->rules, code);
  if (rul && rul->uniquely)
    {
      ob_log_rule *uniq = ob_log_find_rule_internal (&rul->uniques, code);
      if (!uniq)
        {
          uniq = (ob_log_rule *) calloc (1, sizeof (ob_log_rule));
          if (!uniq)
            return NULL;
          uniq->maxcount = rul->maxcount;
          uniq->code = code;
          uniq->matchbits = 64;
          uniq->notify = rul->notify;
          ob_log_rule *head;
          do
            {
              head = ob_atomic_pointer_ref (&rul->uniques);
              uniq->next = head;
            }
          while (
            !ob_atomic_pointer_compare_and_swap (&rul->uniques, head, uniq));
        }
      return uniq;
    }
  return rul;
}

typedef enum should_print_t
{
  PRINT_NO,
  PRINT_YES,
  PRINT_FINAL
} should_print_t;

static void masked_format (char buf[17], unt64 code, unt8 matchbits)
{
  unt8 i;
  for (i = 0; i < 16; i++)
    buf[i] =
      ((4 * i >= matchbits) ? '?' : "0123456789ABCDEF"[(code << 4 * i) >> 60]);
  buf[16] = 0;
}

static should_print_t should_print (ob_log_level *lvl, unt64 code,
                                    char supcode[17], int64 *maxcount_p,
                                    const char *fname)
{
  ob_log_file_rule *frul = ob_log_find_file_rule (lvl, fname);
  if (frul)
    return (frul->print ? PRINT_YES : PRINT_NO);

  ob_log_rule *rul = ob_log_find_rule (lvl, code);

  if (!rul)
    return PRINT_YES;

  const int64 newcount = ob_atomic_int64_add (&rul->count, 1);
  const int64 maxcount = ob_atomic_int64_ref (&rul->maxcount);
  if (newcount <= maxcount)
    return PRINT_YES;
  else if (newcount - 1 == maxcount && rul->notify)
    {
      *maxcount_p = maxcount;
      masked_format (supcode, rul->code, rul->matchbits);
      return PRINT_FINAL;
    }
  else
    return PRINT_NO;
}

/* same as should_print but do not advance the counters */
static should_print_t should_print_peek (ob_log_level *lvl, unt64 code,
                                         const char *fname)
{
  ob_log_file_rule *frul = ob_log_find_file_rule (lvl, fname);
  if (frul)
    return (frul->print ? PRINT_YES : PRINT_NO);

  ob_log_rule *rul = ob_log_find_rule (lvl, code);

  if (!rul)
    return PRINT_YES;

  const int64 newcount = ob_atomic_int64_ref (&rul->count);
  const int64 maxcount = ob_atomic_int64_ref (&rul->maxcount);
  if (newcount < maxcount)
    return PRINT_YES;
  else if (newcount == maxcount && rul->notify)
    return PRINT_FINAL;
  else
    return PRINT_NO;
}

/* This doesn't seem to work on Raspberry Pi for unknown reasons, so
 * skip it on ARM for now. */
#if (defined(__gnu_linux__) || defined(__APPLE__)) && !defined(__arm__)
static void read_a_line (FILE *f, char **bp, size_t *bl)
{
  size_t len = 0;
  if (feof (f))
    {
      **bp = 0;
      return;
    }
  flockfile (f);
  int c = 0;
  while ('\n' != c && EOF != (c = getc_unlocked (f)))
    {
      if (len + 2 > *bl)
        {
          char *tmp_p;
          size_t tmp_l;
          tmp_p = (char *) realloc (*bp, tmp_l = (len + 2));
          // deal with memory failure by cutting line short; not very elegant
          if (!tmp_p)
            break;
          *bp = tmp_p;
          *bl = tmp_l;
        }
      (*bp)[len++] = c;
    }
  (*bp)[len] = 0;
  funlockfile (f);
}

static void append_a_string (char **dst, const char *src)
{
  if (!*dst)
    return;

  size_t len = strlen (*dst) + strlen (src) + 1;
  char *x = (char *) realloc (*dst, len);
  if (!x)
    {
      free (*dst);
      *dst = NULL;
      return;
    }
  ob_safe_append_string (x, len, src);
  *dst = x;
}

static char *parse_backtrace (FILE *a2l, FILE *cxf)
{
  const int second_column = 20;
  const int third_column = 60;
  char spaces[80];
  size_t func_s = 32;
  size_t file_s = 32;
  size_t bt_s = 32;
  char *func_p = (char *) malloc (func_s);
  char *file_p = (char *) malloc (file_s);
  char *bt_p = (char *) malloc (bt_s);
  char *result = strdup ("");
  const bool ok = (func_p && file_p && bt_p && result);
  while (ok && (!feof (a2l) || !feof (cxf)))
    {
      char *p2;
      read_a_line (a2l, &func_p, &func_s);
      read_a_line (a2l, &file_p, &file_s);
      read_a_line (cxf, &bt_p, &bt_s);
      if (*func_p != 0 && *func_p != '?')
        {
          char *p = strrchr (bt_p, '[');
          ob_chomp (file_p);
          if (p)
            ob_chomp (func_p);
          int file_l = strlen (file_p);
          int func_l = strlen (func_p);
          int pad1 = second_column - file_l;
          if (pad1 < 1)
            pad1 = 1;
          int pad2 = third_column - (file_l + pad1 + func_l);
          if (pad2 < 1)
            pad2 = 1;
          append_a_string (&result, file_p);
          memset (spaces, ' ', sizeof (spaces));
          spaces[pad1] = 0;
          append_a_string (&result, spaces);
          append_a_string (&result, func_p);
          if (p)
            {
              memset (spaces, ' ', sizeof (spaces));
              spaces[pad2] = 0;
              append_a_string (&result, spaces);
              p++;
              if (NULL != (p2 = strrchr (p, ']')) && p2[1] == '\n')
                {
                  p2[0] = '\n';
                  p2[1] = 0;
                }
              append_a_string (&result, p);
            }
        }
      else if (*bt_p != 0)
        {
          char *p = strrchr (bt_p, '/');
          if (p)
            p++;
          else
            p = bt_p;
          char *paren = strchr (p, '(');
          char *bracket = strrchr (p, '[');
          if (paren && bracket && bracket > paren && *(bracket - 1) == ')')
            {
              *paren = 0;
              *(bracket - 1) = 0;
              int file_l = paren - p;
              int func_l = (bracket - 1) - (paren + 1);
              int pad1 = second_column - file_l;
              if (pad1 < 1)
                pad1 = 1;
              int pad2 = third_column - (file_l + pad1 + func_l);
              if (pad2 < 1)
                pad2 = 1;
              append_a_string (&result, p);
              p = paren + 1;
              memset (spaces, ' ', sizeof (spaces));
              spaces[pad1] = 0;
              append_a_string (&result, spaces);
              append_a_string (&result, p);
              p = bracket + 1;
              memset (spaces, ' ', sizeof (spaces));
              spaces[pad2] = 0;
              append_a_string (&result, spaces);
              if (NULL != (p2 = strrchr (p, ']')) && p2[1] == '\n')
                {
                  p2[0] = '\n';
                  p2[1] = 0;
                }
              append_a_string (&result, p);
            }
          else
            append_a_string (&result, p);
        }
    }
  free (func_p);
  free (file_p);
  free (bt_p);
  return result;
}

static pid_t spawn_of_satan (int sock, char *const args[])
{
  int posix_err;
  pid_t pid;
  int i;
  char buf[80];
  char *argv[4];
  char *cmd = strdup ("exec '");

  for (i = 0; args[i]; i++)
    {
      /* Quoting with single quotes handles all characters except single
       * quote.  XXX: Technically, we should quote single quotes.
       * But not a problem for what we're doing here.  Actually, we
       * probably don't need any quoting for what we're doing here. */
      if (i > 0)
        append_a_string (&cmd, "' '");
      append_a_string (&cmd, args[i]);
    }

  snprintf (buf, sizeof (buf), "' 0<&%d 1>&%d 2>&%d-", sock, sock, sock);
  append_a_string (&cmd, buf);

  if (!cmd) /* out of memory */
    return -1;

  argv[0] = "/bin/bash";
  argv[1] = "-c";
  argv[2] = cmd;
  argv[3] = NULL;

  posix_err = posix_spawn (&pid, argv[0], NULL, NULL, argv, NULL);
  free (cmd);
  if (posix_err)
    {
      // don't use ob-log, because we are already deep within it
      fprintf (stderr, "posix_spawn: %s: %s\n", args[0], strerror (posix_err));
      return -1;
    }

  return pid;
}

static void *parse_backtrace_thread (void *v)
{
  FILE **files = (FILE **) v;
  char *trace = parse_backtrace (files[0], files[1]);
  return trace;
}

#include <execinfo.h>
#define NFRAMES 50
static char *attempt_backtrace (void)
{
  void *frames[NFRAMES];
  char addrs[NFRAMES][20];
  char exe[80];
  int sz, i, stat_loc, cxxfilt_socketpair[2], addr2line_socketpair[2];

  /* We ask libc to perform a numeric backtrace, and then we shell
   * out to addr2line to convert those addresses to file and line
   * numbers. */
  sz = backtrace (frames, NFRAMES);
  for (i = 0; i < sz; i++)
    snprintf (addrs[i], sizeof (addrs[0]), "%p", frames[i]);
  snprintf (exe, sizeof (exe), "/proc/%d/exe", getpid ());

  /* const */ char *addr2line = "/usr/bin/addr2line";
  /* const */ char *args[NFRAMES + 7];
  OB_CLEAR (args);
  args[0] = addr2line;
  args[1] = "-C";
  args[2] = "-e";
  args[3] = exe;
  args[4] = "-f";
  args[5] = "-s";
  for (i = 0; i < sz; i++)
    args[6 + i] = addrs[i];

  if (0 > socketpair (AF_UNIX, SOCK_STREAM, 0, addr2line_socketpair))
    return NULL;

  /* Make our end (addr2line_socketpair[0]) close-on-exec, while the
   * other end must not be close-on-exec. */
  fcntl (addr2line_socketpair[0], F_SETFD, (long) FD_CLOEXEC);

  pid_t addr2line_pid = -1;

  /* Only run addr2line if it exists (this check has a TOCTOU problem,
   * but it's okay, because it's not a security issue, and we don't
   * expect /usr/bin/addr2line to be rapidly appearing and disappearing).
   * If it does not exist, we just get an EOF on the socketpair. */
  if (0 == access (addr2line, X_OK))
    {
      addr2line_pid = spawn_of_satan (addr2line_socketpair[1], args);
      if (addr2line_pid < 0)
        {
          close (addr2line_socketpair[0]);
          close (addr2line_socketpair[1]);
          return NULL;
        }
    }
  close (addr2line_socketpair[1]);

  /* We will ask libc to perform a symbolic
   * backtrace, and shove that text through the program c++filt,
   * which unmangles the mangled C++ names. */
  /* const */ char *cxxfilt = "/usr/bin/c++filt";
  args[0] = cxxfilt;
  args[1] = NULL;

  if (0 > socketpair (AF_UNIX, SOCK_STREAM, 0, cxxfilt_socketpair))
    return NULL;

  /* Make our end (cxxfilt_socketpair[0]) close-on-exec, while the
   * other end must not be close-on-exec. */
  fcntl (cxxfilt_socketpair[0], F_SETFD, (long) FD_CLOEXEC);

  pid_t cxxfilt_pid = -1;

  /* Only run c++filt if it exists (this check has a TOCTOU problem,
   * but it's okay, because it's not a security issue, and we don't
   * expect /usr/bin/c++filt to be rapidly appearing and disappearing).
   * If it does not exist, we just use the socketpair to talk to ourself. */
  if (0 == access (cxxfilt, X_OK))
    {
      cxxfilt_pid = spawn_of_satan (cxxfilt_socketpair[1], args);
      close (cxxfilt_socketpair[1]);
      if (cxxfilt_pid < 0)
        {
          close (cxxfilt_socketpair[0]);
          return NULL;
        }
    }

  /* Now, while those child processes are running, we parse the output
   * from both of these pipelines (numeric backtrace run through
   * addr2line, and symbolic backtrace run through c++filt), and choose
   * whichever one "looks better" for each line of the stack trace.
   * (Because both of these methods have their faults, so we try to
   * get the best of both.) */
  FILE *files[2];
  files[0] = fdopen (addr2line_socketpair[0], "r");
  files[1] = fdopen (cxxfilt_socketpair[cxxfilt_pid < 0 ? 1 : 0], "r");
  void *trace = NULL;
  pthread_t thr;
  const int pt_err = pthread_create (&thr, NULL, parse_backtrace_thread, files);
  backtrace_symbols_fd (frames, sz, cxxfilt_socketpair[0]);
  shutdown (cxxfilt_socketpair[0], SHUT_WR);
  if (0 == pt_err)
    pthread_join (thr, &trace);
  fclose (files[0]);
  fclose (files[1]);
  if (cxxfilt_pid < 0)
    close (cxxfilt_socketpair[0]);
  else
    waitpid (cxxfilt_pid, &stat_loc, 0);
  if (addr2line_pid >= 0)
    waitpid (addr2line_pid, &stat_loc, 0);
  return (char *) trace;
}
#else
static char *attempt_backtrace (void)
{
  // Don't know how to backtrace on other systems
  return NULL;
}
#endif

static ob_log_rule deprecation_do_limit_nonzero_codes = {
  0,      // count
  1,      // maxcount
  0,      // code
  0,      // matchbits
  true,   // uniquely
  false,  // notify
  NULL,   // next
  NULL    // uniques
};

static ob_log_rule deprecation_dont_limit_code_zero = {
  0,                                    // count
  OB_INT64_MAX,                         // maxcount
  0,                                    // code
  64,                                   // matchbits
  false,                                // uniquely
  false,                                // notify
  &deprecation_do_limit_nonzero_codes,  // next
  NULL                                  // uniques
};

ob_log_level oblv_bug = {
  OB_DST_FD | OB_DST_VALGRIND | OB_FLG_STACK_TRACE | OB_FLG_SHOW_CODE_OR_WHERE
    | OB_FLG_SHOW_TID_NONMAIN,
  OB_FOREGROUND_ENABLE | OB_FOREGROUND_RED | OB_FOREGROUND_BLUE, /* magenta */
  LOG_ERR,
  2,
  "programming error: ",
  NULL,
  NULL,
  NULL,
  NULL};

ob_log_level oblv_error = {OB_DST_FD | OB_FLG_SHOW_CODE_OR_WHERE
                             | OB_FLG_SHOW_TID_NONMAIN,
                           OB_FOREGROUND_ENABLE | OB_FOREGROUND_RED, /* red */
                           LOG_ERR,
                           2,
                           "error: ",
                           NULL,
                           NULL,
                           NULL,
                           NULL};

ob_log_level oblv_deprecation = {
  OB_DST_FD | OB_DST_VALGRIND | OB_FLG_STACK_TRACE | OB_FLG_SHOW_CODE_OR_WHERE
    | OB_FLG_SHOW_TID_NONMAIN,
  OB_FOREGROUND_ENABLE | OB_FOREGROUND_GREEN | OB_FOREGROUND_BLUE, /* cyan */
  LOG_WARNING,
  2,
  "deprecation: ",
  NULL,
  NULL,
  &deprecation_dont_limit_code_zero,
  NULL};

ob_log_level oblv_wrnu = {
  OB_DST_FD | OB_FLG_SHOW_CODE_OR_WHERE | OB_FLG_SHOW_TID_NONMAIN,
  OB_FOREGROUND_ENABLE | OB_FOREGROUND_RED | OB_FOREGROUND_GREEN, /* yellow */
  LOG_WARNING,
  2,
  "warning: ",
  NULL,
  NULL,
  NULL,
  NULL};

ob_log_level oblv_info = {OB_DST_FD | OB_FLG_SHOW_CODE_OR_WHERE
                            | OB_FLG_SHOW_TID_NONMAIN,
                          OB_FOREGROUND_ENABLE | OB_FOREGROUND_BLUE, /* blue */
                          LOG_INFO,
                          2,
                          "info: ",
                          NULL,
                          NULL,
                          NULL,
                          NULL};

ob_log_level oblv_dbug = {OB_FLG_SHOW_CODE_OR_WHERE | OB_FLG_SHOW_TID_NONMAIN,
                          OB_FOREGROUND_ENABLE
                            | OB_FOREGROUND_GREEN, /* green */
                          LOG_DEBUG,
                          2,
                          "dbg: ",
                          NULL,
                          NULL,
                          NULL,
                          NULL};

typedef struct time_cache
{
  struct timeval tv;
  bool got_time;
} time_cache;

static void ob_log_internal (int32 flags, time_cache *now, int64 nline,
                             int64 totlines, const char *file, int lineno,
                             ob_log_level *lvl, unt64 code, const char *msg);

static OB_UNUSED void *win_log_critical;

#define OB_DST_MASK 0x3f

void ob_log_loc_v (const char *file, int lineno, ob_log_level *lvl, unt64 code,
                   const char *fmt, va_list ap)
{
  int32 flags = lvl->flags;
  should_print_t sp;
  char supcode[17];
  int64 maxed_out = -1;
  if (PRINT_NO == (sp = should_print (lvl, code, supcode, &maxed_out, file))
      || (flags & OB_DST_MASK) == 0)
    // don't bother with potentially expensive formatting if we
    // aren't printing this anywhere
    return;

  char *msg = NULL;
  const char *p;
  time_cache now;
  now.got_time = false;

  if (PRINT_FINAL == sp)
    {
      int vasret = asprintf (
        &msg, "rule %s reached maximum count %" OB_FMT_64 "d; suppressing\n",
        supcode, maxed_out);
      if (vasret >= 0)
        {
          ob_log_internal (flags, &now, 1, 1, file, lineno, lvl, code, msg);
          free (msg);
        }
    }
  // check for format string with no formats, newline terminated but no
  // internal newlines.
  else if (!strchr (fmt, '%') && NULL != (p = strchr (fmt, '\n')) && p[1] == 0)
    // optimize (perhaps prematurely) for the case where no formatting is needed
    ob_log_internal (flags, &now, 1, 1, file, lineno, lvl, code, fmt);
  else
    {
      int vasret = vasprintf (&msg, fmt, ap);

      if (vasret < 0)
        {
          // If we can't format the message, then just printing
          // out the format string is better than nothing...
          // it may still get the point across
          ob_log_internal (flags, &now, 1, 1, file, lineno, lvl, code, fmt);
        }
      else
        {
          int64 totlines = 0;
          char c = 0;
          for (p = msg; *p; p++)
            if ('\n' == (c = *p))
              totlines++;
          if (c != '\n')
            {
              size_t len = p - msg;
              char *tmp = (char *) realloc (msg, len + 2);
              //             1 for newline + 1 for NUL ^
              if (tmp)
                {
                  tmp[len] = '\n';
                  tmp[len + 1] = 0;
                  msg = tmp;
                  totlines++;
                }
            }
          int64 nline = 1;
          char *q;
          for (q = msg; *q; nline++)
            {
              c = 0;
              char *r = strchr (q, '\n');
              if (r)
                {
                  r++;
                  c = *r;
                  *r = 0;
                }
              else
                r = q;
              ob_log_internal (flags, &now, nline, totlines, file, lineno, lvl,
                               code, q);
              *r = c;
              q = r;
            }
          free (msg);
        }
    }
}

static void ob_log_internal (int32 flags, time_cache *now, int64 nline,
                             int64 totlines, const char *file, int lineno,
                             ob_log_level *lvl, unt64 code, const char *msg)
{
  char multibuf[50];
  if (totlines > 1)
    {
      double dtotlines = (double) totlines;
      int digs = 1 + (int) floor (log10 (dtotlines));
      int z = snprintf (multibuf, sizeof (multibuf),
                        "(%*" OB_FMT_64 "d/%" OB_FMT_64 "d): ", digs, nline,
                        totlines);
      // indicate deep unhappiness if snprintf failed
      // (this could happen if digs was nonsensical, e. g. -2147483647)
      if (z < 0)
        {
          multibuf[0] = ':';
          multibuf[1] = '(';
          multibuf[2] = multibuf[3] = multibuf[4] = ' ';
          multibuf[5] = multibuf[6] = ' ';
          multibuf[7] = 0;
        }
    }

  char timebuf[80];
  if (0 != (flags & OB_FLG_SHOW_TIME))
    {
      if (!now->got_time)
        {
          gettimeofday (&now->tv, NULL);
          now->got_time = true;
        }
      ob_format_time (timebuf, sizeof (timebuf), &now->tv);
    }

  if (0 != (flags & OB_FLG_SHOW_CODE_OR_WHERE))
    {
      if (code)
        flags |= OB_FLG_SHOW_CODE;
      else
        flags |= OB_FLG_SHOW_WHERE;
    }
  else if (0 != (flags & OB_FLG_SHOW_CODE) && 0 == code)
    flags &= ~OB_FLG_SHOW_CODE;

  char codebuf[24];
  if (0 != (flags & OB_FLG_SHOW_CODE))
    snprintf (codebuf, sizeof (codebuf), "<%08" OB_FMT_64 "x> ", code);

  char tidbuf[16];
  if (0 != (flags & (OB_FLG_SHOW_TID | OB_FLG_SHOW_TID_NONMAIN)))
    {
      unt32 tid;
      bool is_main;
      ob_get_small_integer_thread_id (&tid, &is_main);
      if (0 != (flags & OB_FLG_SHOW_TID) || !is_main)
        {
          flags |= OB_FLG_SHOW_TID;
          snprintf (tidbuf, sizeof (tidbuf), "[t%u] ", tid);
        }
    }

  const char *shortfile = file;
  if (0 != (flags & OB_FLG_SHOW_WHERE))
    {
      shortfile = ob_basename (file);
      flags |= OB_FLG_SHOW_WHERE_FULL;
    }

  int fd = lvl->fd;
  bool docolor = false;
  char colorbuf[32];
  colorbuf[0] = 0;
#ifdef _MSC_VER
  CONSOLE_SCREEN_BUFFER_INFO old;
  long winconshand = -1;
  bool needlocking = false;
  if (0 != (flags & OB_DST_FD))
    {
      needlocking = true;
      if (lvl->color > 0)
        winconshand = _get_osfhandle (fd);
    }
#else
  if (0 != (flags & OB_DST_FD) && lvl->color > 0 && isatty (fd))
    {
      unt32 c = lvl->color;
      while (c)
        {
          unt8 cc = (unt8) c;
          if (cc)
            {
              char color1buf[16];
              snprintf (color1buf, sizeof (color1buf), "\033[%xm", cc);
              ob_safe_append_string (colorbuf, sizeof (colorbuf), color1buf);
              docolor = true;
            }
          c >>= 8;
        }
    }
#endif

  const char *progname = NULL;

  size_t cap = strlen (lvl->prefix);
  if (0 != (flags & OB_FLG_SHOW_PROG))
    cap += strlen (progname = ob_get_prog_name ()) + 2;
  if (0 != (flags & OB_FLG_SHOW_PID))
    cap += 12;
  if (0 != (flags & OB_FLG_SHOW_TIME))
    cap += strlen (timebuf);
  if (0 != (flags & OB_FLG_SHOW_WHERE_FULL))
    cap += strlen (shortfile) + 13;
  if (0 != (flags & OB_FLG_SHOW_CODE))
    cap += strlen (codebuf);
  if (0 != (flags & OB_FLG_SHOW_TID))
    cap += strlen (tidbuf);
  if (totlines > 1)
    cap += strlen (multibuf);
  if (docolor)
    cap += strlen (colorbuf) + 4;

  const char *final = NULL;
  char *line = NULL;
  // need to overwrite newline if using syslog; must copy in that case
  if (0 == cap && 0 == (flags & OB_DST_SYSLOG))
    // short-circuit copying the message if we aren't adding anything to it
    final = msg;
  else
    {
      cap += strlen (msg) + 1;
      line = (char *) alloca (cap);
      if (docolor)
        ob_safe_copy_string (line, cap, colorbuf);
      else
        *line = 0;
      ob_safe_append_string (line, cap, lvl->prefix);
      if (0 != (flags & OB_FLG_SHOW_PROG))
        {
          ob_safe_append_string (line, cap, progname);
          ob_safe_append_string (line, cap, ": ");
        }
      if (0 != (flags & OB_FLG_SHOW_PID))
        {
          ob_safe_append_int64 (line, cap, getpid ());
          ob_safe_append_string (line, cap, ": ");
        }
      if (0 != (flags & OB_FLG_SHOW_TIME))
        ob_safe_append_string (line, cap, timebuf);
      if (0 != (flags & OB_FLG_SHOW_WHERE_FULL))
        {
          ob_safe_append_string (line, cap, shortfile);
          ob_safe_append_string (line, cap, ":");
          ob_safe_append_int64 (line, cap, lineno);
          ob_safe_append_string (line, cap, ": ");
        }
      if (0 != (flags & OB_FLG_SHOW_CODE))
        ob_safe_append_string (line, cap, codebuf);
      if (0 != (flags & OB_FLG_SHOW_TID))
        ob_safe_append_string (line, cap, tidbuf);
      if (totlines > 1)
        ob_safe_append_string (line, cap, multibuf);
      ob_safe_append_string (line, cap, msg);
      if (docolor)
        ob_safe_append_string (line, cap, "\033[0m");
      final = line;
    }

    // On UNIX, we put everything (including the ANSI color codes)
    // together in a single string, and output it with a single
    // call to write(), so as long as it's shorter than PIPE_BUF,
    // we're guaranteed by POSIX that it will be atomic.  (Which means
    // really long messages, like the "Ctenophore turd" message,
    // might interleave, but most reasonable messages will not,
    // since PIPE_BUF appears to be 512 on OS X and 4096 on Linux.)
    // On Windows, we still put everything in one string, but the
    // color changing has to be done with separate API calls, so there's
    // a risk of interleaving between threads.  To avoid that possibility,
    // we use a critical section on Windows.

#ifdef _MSC_VER
  if (needlocking)
    EnterCriticalSection (ob_fetch_critical (&win_log_critical));
  if (winconshand > 0)
    {
      if (GetConsoleScreenBufferInfo ((HANDLE) winconshand, &old))
        {
          unt16 attr = old.wAttributes;
          if (0 != (lvl->color & OB_FOREGROUND_ENABLE))
            {
              attr &= ~0x0f;
              attr |= FOREGROUND_INTENSITY;
            }
          if (0 != (lvl->color & OB_BACKGROUND_ENABLE))
            attr &= ~0xf0;
          attr |= (unt16) lvl->color;
          SetConsoleTextAttribute ((HANDLE) winconshand, attr);
        }
      else
        winconshand = -1;
    }
#endif

  bool using_fd = false;
  while (0 != (flags & OB_DST_FD))
    {
      using_fd = true;
      ssize_t len = strlen (final);
      const char *p = final;
      ssize_t result = 1;
      while (len > 0 && result > 0)
        {
          result = write (fd, p, len);
          len -= result;
          p += result;
        }
      if (result < 0 && fd != 2)
        fd = 2;  // if we encountered an error, write again to stderr
      else
        flags &= ~OB_DST_FD;
    }

#ifdef _MSC_VER
  if (winconshand > 0)
    SetConsoleTextAttribute ((HANDLE) winconshand, old.wAttributes);
#endif

  char *stack_trace = NULL;
  if (nline == totlines && 0 != (flags & OB_FLG_STACK_TRACE))
    stack_trace = attempt_backtrace ();

  if (using_fd && stack_trace)
    {
      ssize_t len = strlen (stack_trace);
      const char *p = stack_trace;
      ssize_t result = 1;
      while (len > 0 && result > 0)
        {
          result = write (fd, p, len);
          len -= result;
          p += result;
        }
    }

#ifdef _MSC_VER
  if (needlocking)
    LeaveCriticalSection (ob_fetch_critical (&win_log_critical));
#endif

  if (docolor && 0 != (flags & OB_DST_MASK))
    {
      final += strlen (colorbuf);  // remove escape code at beginning of line
      size_t len = strlen (line);
      line[len - 4] = 0;  // remove escape code at end of line
    }

#if 0
  /* This is not supported by newer valgrinds (e. g. on Ubuntu 10.04)
     See http://old.nabble.com/valgrind:-r11032---in-trunk:-coregrind-coregrind-m_scheduler-include-td27357688.html

     Attempting to execute it causes valgrind to die with this message:

  ==22800== Valgrind: fatal error - cannot continue: use of the deprecated
  ==22800== client requests VG_USERREQ__PRINTF or VG_USERREQ__PRINTF_BACKTRACE
  ==22800== on a platform where they cannot be supported.  Please use the
  ==22800== equivalent _VALIST_BY_REF versions instead.
  ==22800==
  ==22800== This is a binary-incompatible change in Valgrind's client request
  ==22800== mechanism.  It is unfortunate, but difficult to avoid.  End-users
  ==22800== are expected to almost never see this message.  The only case in
  ==22800== which you might see this message is if your code uses the macros
  ==22800== VALGRIND_PRINTF or VALGRIND_PRINTF_BACKTRACE.  If so, you will need
  ==22800== to recompile such code, using the header files from this version of
  ==22800== Valgrind, and not any previous version.
  ==22800==
  ==22800== If you see this mesage in any other circumstances, it is probably
  ==22800== a bug in Valgrind.  In this case, please file a bug report at
  ==22800==
  ==22800==    http://www.valgrind.org/support/bug_reports.html
  ==22800==
  ==22800== Will now abort.

     So let's not use it anymore.
  */
  if (0 != (flags & OB_DST_VALGRIND))
    { VALGRIND_PRINTF_BACKTRACE ("%s", final);
    }
#endif

  if (0 != (flags & OB_DST_CALLBACK))
    {
      if (!now->got_time)
        {
          gettimeofday (&now->tv, NULL);
          now->got_time = true;
        }
      float64 t = now->tv.tv_sec + 0.000001 * now->tv.tv_usec;
      lvl->callback (lvl, code, t, nline, totlines, file, lineno, msg, final,
                     stack_trace);
    }

#ifndef _MSC_VER
  if (0 != (flags & OB_DST_SYSLOG))
    {
      char *p = strrchr (line, '\n');
      if (p)
        *p = 0;  // remove trailing newline
      syslog (lvl->sl_priority, "%s", final);
    }
#endif

  free (stack_trace);
}

void ob_log_loc (const char *file, int lineno, ob_log_level *lvl, unt64 code,
                 const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
  ob_log_loc_v (file, lineno, lvl, code, fmt, ap);
  va_end (ap);
}

void ob_log_loc_fatal (const char *file, int lineno, ob_log_level *lvl,
                       unt64 code, int exitcode, const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
  ob_log_loc_v (file, lineno, lvl, code, fmt, ap);
  va_end (ap);

  if (exitcode < 0)
    ob_abort_func ();
  else
    exit (exitcode);

  // If the user's ob_abort_func returned (which it shouldn't, but there's
  // no way we can enforce that), then do a real abort() to make sure
  // we never return, no matter what.
  abort ();
}

ob_retort ob_suppress_message (ob_log_level *lvl, unt64 code)
{
  ob_log_rule rul;
  OB_CLEAR (rul);
  rul.code = code;
  rul.matchbits = 64;
  return ob_log_add_rule (lvl, rul);
}

bool ob_log_is_enabled (const char *file, ob_log_level *lvl, unt64 code)
{
  if (!lvl)
    return true;
  int32 flags = lvl->flags;
  if (PRINT_NO == should_print_peek (lvl, code, file)
      || (flags & OB_DST_MASK) == 0)
    // don't bother with potentially expensive formatting if we
    // aren't printing this anywhere
    return false;
  return true;
}

static void summarize_suppressions (const ob_log_rule *rul, ob_log_level *dst)
{
  for (; rul != NULL; rul = ob_atomic_pointer_ref (&rul->next))
    {
      int64 count = ob_atomic_int64_ref (&rul->count);
      int64 maxcount = ob_atomic_int64_ref (&rul->maxcount);
      if (rul->notify && count > maxcount)
        {
          char buf[17];

          masked_format (buf, rul->code, rul->matchbits);
          ob_log (dst, 0,
                  "rule %s matched %" OB_FMT_64
                  "d times; suppressed after %" OB_FMT_64 "d\n",
                  buf, count, maxcount);
        }
      const ob_log_rule *uni = ob_atomic_pointer_ref (&rul->uniques);
      if (uni)
        summarize_suppressions (uni, dst);
    }
}

static ob_log_level oblv_supp = {OB_DST_FD | OB_FLG_SHOW_PROG | OB_FLG_SHOW_PID,
                                 OB_FOREGROUND_ENABLE
                                   | OB_FOREGROUND_GREEN, /* green */
                                 LOG_INFO,
                                 2,
                                 "",
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL};

/// This implements bug 767
static void ob_log_bye (void)
{
  summarize_suppressions (ob_atomic_pointer_ref (&oblv_bug.rules), &oblv_supp);
  summarize_suppressions (ob_atomic_pointer_ref (&oblv_error.rules),
                          &oblv_supp);
  summarize_suppressions (ob_atomic_pointer_ref (&oblv_deprecation.rules),
                          &oblv_supp);
  summarize_suppressions (ob_atomic_pointer_ref (&oblv_wrnu.rules), &oblv_supp);
  summarize_suppressions (ob_atomic_pointer_ref (&oblv_info.rules), &oblv_supp);
  summarize_suppressions (ob_atomic_pointer_ref (&oblv_dbug.rules), &oblv_supp);
}

// default action for exitcode -1 is to abort
ob_abort_func_t ob_abort_func = abort;

static void reset_counts (ob_log_rule *rul)
{
  // None of the accesses below need to be atomic, since we're guaranteed
  // no other threads are running when the atfork handler is called.
  for (; rul != NULL; rul = rul->next)
    {
      rul->count = 0;
      ob_log_rule *uni = rul->uniques;
      if (uni)
        reset_counts (uni);
    }
}

static void ob_log_atfork_child (void)
{
  reset_counts (ob_atomic_pointer_ref (&oblv_bug.rules));
  reset_counts (ob_atomic_pointer_ref (&oblv_error.rules));
  reset_counts (ob_atomic_pointer_ref (&oblv_deprecation.rules));
  reset_counts (ob_atomic_pointer_ref (&oblv_wrnu.rules));
  reset_counts (ob_atomic_pointer_ref (&oblv_info.rules));
  reset_counts (ob_atomic_pointer_ref (&oblv_dbug.rules));
}
