
/* (c)  oblong industries */

// Test of ob-log functionality.

#include "libLoam/c/ob-sys.h"
#include "libLoam/c/ob-vers.h"
#include "libLoam/c/ob-dirs.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-pthread.h"
#include "libLoam/c/ob-util.h"
#include "libLoam/c/ob-time.h"
#include "libLoam/c/ob-string.h"
#include "libLoam/c/ob-file.h"
#include "libLoam/c/private/ob-syslog.h"
#include <stdlib.h>
#include <stdio.h>

extern int test_logging_last_line;

static bool yeah_i_got_called = false;

static void cback_func (const struct ob_log_level *lvl, unt64 code, float64 t,
                        int64 lnum, int64 lcount, const char *file, int line,
                        const char *msg, const char *fmtdmsg,
                        OB_UNUSED const char *stack)
{
  if (code != 0xc0de)
    error_exit ("code was %" OB_FMT_64 "x\n", code);

  if (0 != strcmp ((const char *) lvl->cookie, "monster"))
    error_exit ("cookie was %p\n", lvl->cookie);

  if (0 != strcmp (msg, "And this is in green\n"))
    error_exit ("msg was '%s'\n", msg);

  if (0 != strcmp (fmtdmsg, "<0000c0de> And this is in green\n"))
    error_exit ("fmtdmsg was '%s'\n", fmtdmsg);

  float64 oldest_possible = 1276556109;
  float64 newest_possible = ob_current_time ();

  if (t < oldest_possible || t > newest_possible)
    error_exit ("%f was not between %f and %f\n", t, oldest_possible,
                newest_possible);

  int oldest_line = __LINE__;
  int newest_line = test_logging_last_line;

  if (line < oldest_line || line > newest_line)
    error_exit ("%d was not between %d and %d\n", line, oldest_line,
                newest_line);

  /* On Mac and Windows, the suffix on __FILE__ may be uppercase, so skip it */
  if (!strstr (file, "test-logging."))
    error_exit ("'%s' did not contain 'test-logging.'\n", file);

  if (lnum != 1 || lcount != 1)
    error_exit ("lnum = %" OB_FMT_64 "d; lcount = %" OB_FMT_64
                "d; expected 1\n",
                lnum, lcount);

  yeah_i_got_called = true;
}

static ob_log_level blue_plain = {
  OB_DST_FD,     OB_FOREGROUND_ENABLE | OB_FOREGROUND_BLUE, /* blue */
  LOG_INFO,      1,
  "blue text: ", NULL,
  NULL,          NULL,
  NULL};

static ob_log_level totally_plain = {OB_DST_FD, 0, /* no color */
                                     LOG_INFO,  1, "", NULL, NULL, NULL, NULL};

static ob_log_level red_error = {OB_DST_FD | OB_FLG_SHOW_CODE_OR_WHERE,
                                 OB_FOREGROUND_ENABLE
                                   | OB_FOREGROUND_RED, /* red */
                                 LOG_ERR,
                                 1,
                                 "",
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL};

static ob_log_level green_prog = {
  OB_DST_FD | OB_FLG_SHOW_PROG | OB_FLG_SHOW_PID,
  OB_FOREGROUND_ENABLE | OB_FOREGROUND_GREEN, /* green */
  LOG_INFO,
  1,
  "",
  NULL,
  NULL,
  NULL,
  NULL};

static ob_log_level stack_trace = {OB_DST_FD | OB_FLG_SHOW_TIME
                                     | OB_FLG_SHOW_WHERE | OB_FLG_STACK_TRACE,
                                   0, /* no color */
                                   LOG_INFO,
                                   1,
                                   "",
                                   NULL,
                                   NULL,
                                   NULL,
                                   NULL};

static ob_log_level cyan_syslog = {OB_DST_FD | OB_DST_SYSLOG,
                                   OB_FOREGROUND_ENABLE | OB_FOREGROUND_GREEN
                                     | OB_FOREGROUND_BLUE, /* cyan */
                                   LOG_INFO,
                                   1,
                                   "this is only a test: ",
                                   NULL,
                                   NULL,
                                   NULL,
                                   NULL};

static ob_log_level limited = {
  OB_DST_FD, OB_FOREGROUND_ENABLE | OB_FOREGROUND_GREEN, /* green */
  LOG_INFO,  1,
  "",        NULL,
  NULL,      NULL,
  NULL};

static ob_log_level limited2 = {OB_DST_FD, 0, /* no color */
                                LOG_INFO,  1, "foo: ", NULL, NULL, NULL, NULL};

static ob_log_level callback = {OB_DST_FD | OB_DST_CALLBACK | OB_FLG_SHOW_CODE,
                                OB_FOREGROUND_ENABLE
                                  | OB_FOREGROUND_GREEN, /* green */
                                LOG_INFO,
                                1,
                                "",
                                cback_func,
                                (void *) "monster",
                                NULL,
                                NULL};

#define LINE(f, b)                                                             \
  errno = 0;                                                                   \
  if (!fgets (b, sizeof (b), f))                                               \
    error_exit ("fgets: %s\n", strerror (errno));                              \
  else                                                                         \
    ob_chomp (b)

static void *check_log (void *v)
{
  char buf[4096];
  const char *expected;
  FILE *f = (FILE *) v;
  int i;

  bool OB_UNUSED valgrind = false;
  if (ob_running_under_valgrind ())
    valgrind = true;

  LINE (f, buf);
  expected =
    "This is a truly evil string, because it has a gr8 number of "
    "arguments 2 format, so if there R problems with not having va_copy on "
    "Windows, we should hopefully C them!";
  if (0 != strcmp (buf, expected))
    error_exit ("Got '%s' but expected '%s'\n", buf, expected);

  LINE (f, buf);
  expected =
    ": (1/2): "
    "Parser error: while parsing a block mapping at line 9259457, column 5";
  if (!strstr (buf, expected))
    error_exit ("Expected '%s' to contain '%s' but it did not\n", buf,
                expected);

  LINE (f, buf);
  expected = ": (2/2): did not find expected key at line 9260367, column 5";
  if (!strstr (buf, expected))
    error_exit ("Expected '%s' to contain '%s' but it did not\n", buf,
                expected);

  LINE (f, buf);
  expected = "<0000beef> This should indicate location code 0xbeef";
  if (0 != strcmp (buf, expected))
    error_exit ("Got '%s' but expected '%s'\n", buf, expected);

  // TODO: compare program name is correct
  LINE (f, buf);
  expected = ": This should show the pid and program name (";
  if (!strstr (buf, expected))
    error_exit ("Expected '%s' to contain '%s' but it did not\n", buf,
                expected);

  LINE (f, buf);
  expected = ": And this should be accompanied by a stack trace";
  if (!strstr (buf, expected))
    error_exit ("Expected '%s' to contain '%s' but it did not\n", buf,
                expected);

// stack trace not yet implemented on Windows
// and doesn't work properly for position-independent-executable
// or on Raspberry Pi
#if !defined(_MSC_VER) && !defined(__PIC__) && !defined(__PIE__)               \
  && !defined(__arm__)
  // under valgrind, symbols will be missing from stack trace
  if (!valgrind)
    {
      expected = "ob_log_loc";
      do
        {
          LINE (f, buf);
        }
      while (!strstr (buf, expected));

      expected = "deep_stack";
      do
        {
          LINE (f, buf);
        }
      while (!strstr (buf, expected));
    }
#endif

  expected = "this is only a test: Writing to syslog as a test";
  do
    {
      LINE (f, buf);
    }
  while (0 != strcmp (buf, expected));

  for (i = 0; i < 3; i++)
    {
      LINE (f, buf);
      expected = "This should only appear three times";
      if (0 != strcmp (buf, expected))
        error_exit ("Got '%s' but expected '%s'\n", buf, expected);
    }

  for (i = 0; i < 3; i++)
    {
      LINE (f, buf);
      expected = "And this should also appear three times";
      if (0 != strcmp (buf, expected))
        error_exit ("Got '%s' but expected '%s'\n", buf, expected);
    }

  LINE (f, buf);
  expected = "If you cannot afford a newline, one will be provided for you.";
  if (0 != strcmp (buf, expected))
    error_exit ("Got '%s' but expected '%s'\n", buf, expected);

  LINE (f, buf);
  expected = "(1/2): Even if there is one";
  if (0 != strcmp (buf, expected))
    error_exit ("Got '%s' but expected '%s'\n", buf, expected);

  LINE (f, buf);
  expected = "(2/2): in the middle but not at the end";
  if (0 != strcmp (buf, expected))
    error_exit ("Got '%s' but expected '%s'\n", buf, expected);

  for (i = 0; i < 3; i++)
    {
      LINE (f, buf);
      expected = "foo: (1/4): a multiline";
      if (0 != strcmp (buf, expected))
        error_exit ("Got '%s' but expected '%s'\n", buf, expected);
      LINE (f, buf);
      expected = "foo: (2/4): log message";
      if (0 != strcmp (buf, expected))
        error_exit ("Got '%s' but expected '%s'\n", buf, expected);
      LINE (f, buf);
      expected = "foo: (3/4): still just";
      if (0 != strcmp (buf, expected))
        error_exit ("Got '%s' but expected '%s'\n", buf, expected);
      LINE (f, buf);
      expected = "foo: (4/4): counts as one message";
      if (0 != strcmp (buf, expected))
        error_exit ("Got '%s' but expected '%s'\n", buf, expected);
    }

  LINE (f, buf);
  expected = "foo: rule 0000000010FFFFF? reached maximum count 3; suppressing";
  if (0 != strcmp (buf, expected))
    error_exit ("Got '%s' but expected '%s'\n", buf, expected);

  for (i = 1; i <= 10; i++)
    {
      char spected[100];
      LINE (f, buf);
      snprintf (spected, sizeof (spected), "(%2d/10): %d", i, i);
      if (0 != strcmp (buf, spected))
        error_exit ("Got '%s' but expected '%s'\n", buf, spected);
    }

  for (i = 1; i <= 100; i++)
    {
      char spected[100];
      LINE (f, buf);
      snprintf (spected, sizeof (spected), "(%3d/100): %d", i, i);
      if (0 != strcmp (buf, spected))
        error_exit ("Got '%s' but expected '%s'\n", buf, spected);
    }

  LINE (f, buf);
  expected = "but this should be printed";
  if (0 != strcmp (buf, expected))
    error_exit ("Got '%s' but expected '%s'\n", buf, expected);

  return NULL;
}

void deep_stack (const int *depth)
{
  int *deeper = (int *) malloc (sizeof (int));
  *deeper = (*depth) - 1;
  if ((*deeper) <= 0)
    ob_log (&stack_trace, 0,
            "And this should be accompanied by a stack trace\n");
  else
    deep_stack (deeper);
  free (deeper);
}

#if defined(__OPTIMIZE__)
static const char optiness[] = "OPTIMIZED";
#else
static const char optiness[] = "NOT optimized";
#endif

static int fork_handler_called;

static void fork_handler (void)
{
  fork_handler_called++;
}

// Usage: "test-logging" (no args) to self-check
//        "test-logging foo" (any dummy arg) to print test to terminal
int main (int argc, char **argv)
{
  int fildes[2];
  pthread_t thr;
  FILE *f = NULL;
  char *piler;

  if (argc == 1)
    {
      OB_DIE_ON_ERROR (ob_pipe (fildes));
      totally_plain.fd = fildes[1];
      red_error.fd = fildes[1];
      green_prog.fd = fildes[1];
      stack_trace.fd = fildes[1];
      cyan_syslog.fd = fildes[1];
      limited.fd = fildes[1];
      limited2.fd = fildes[1];
      f = fdopen (fildes[0], "rb");
      CHECK_PTHREAD_ERROR (pthread_create (&thr, NULL, check_log, f));
    }

  piler = ob_get_version (OB_VERSION_OF_COMPILER);
  ob_log (&blue_plain, 0, "##### %s, %" OB_FMT_SIZE "d-bit, %s #####\n",
          optiness, 8 * sizeof (void *), piler);
  free (piler);
  ob_log (&totally_plain, 0,
          "%s is %x truly %c%c%c%c string, because it has %x%c"
          "gr%d number of arguments %d %s, so if there %c "
          "problems with not having %s on %s, we should %s "
          "%" OB_FMT_64 "X them%c\n",
          "This", 10, 'e', 'v', 'i', 'l', 10, ' ', 8, 2, "format", 'R',
          "va_copy", "Windows", "hopefully", OB_CONST_I64 (12), '!');
  ob_log (&red_error, 0,
          "Parser error: %s at line %" OB_FMT_SIZE "d, column %" OB_FMT_SIZE
          "d\n"
          "%s at line %" OB_FMT_SIZE "d, column %" OB_FMT_SIZE "d\n",
          "while parsing a block mapping", (size_t) 9259457, (size_t) 5,
          "did not find expected key", (size_t) 9260367, (size_t) 5);
  ob_log (&red_error, 0xbeef, "This should indicate location code 0xbeef\n");
  ob_log (&green_prog, 0, "This should show the pid and program name (%s)\n",
          ob_basename (argv[0]));

#ifndef _MSC_VER
  /* verify that printing a stack trace does not cause the atfork
   * handlers to be called.  (bug 4953) */
  pthread_atfork (fork_handler, fork_handler, fork_handler);
#endif
  int depth = 300;
  deep_stack (&depth);
  ob_log (&cyan_syslog, 0, "Writing to syslog as a test\n");
  if (fork_handler_called)
    error_exit ("fork handler was called!  (bug 4953)\n");

  ob_log_rule rul;
  rul.count = 0;
  rul.maxcount = 3;
  rul.code = 0x10fffff0;
  rul.matchbits = 60;
  rul.uniquely = true;
  rul.notify = false;
  OB_DIE_ON_ERROR (ob_log_add_rule (&limited, rul));
  rul.count = 0;
  rul.maxcount = 3;
  rul.code = 0x10fffff0;
  rul.matchbits = 60;
  rul.uniquely = false;
  rul.notify = true;
  OB_DIE_ON_ERROR (ob_log_add_rule (&limited2, rul));

  int i;
  for (i = 0; i < 10; i++)
    ob_log (&limited, 0x10fffff0, "This should only appear three times\n");

  for (i = 0; i < 10; i++)
    ob_log (&limited, 0x10fffff1, "And this should also appear three times\n");

  ob_log (&callback, 0xc0de, "And this is in green\n");

  ob_log (&totally_plain, 0,
          "If you cannot afford a newline, one will be provided for you.");
  ob_log (&totally_plain, 0,
          "Even if there is one\nin the middle but not at the end");

  for (i = 0; i < 10; i++)
    ob_log (&limited2, 0x10fffff0,
            "a multiline\nlog message\nstill just\ncounts as one message\n");

  ob_log (&totally_plain, 0, "%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n", 1, 2,
          3, 4, 5, 6, 7, 8, 9, 10);

  ob_log (&totally_plain, 0,
          "%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n"
          "%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n"
          "%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n"
          "%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n"
          "%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n"
          "%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n"
          "%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n"
          "%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n"
          "%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n"
          "%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n",
          1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
          21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37,
          38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54,
          55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71,
          72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88,
          89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100);

  OB_DIE_ON_ERROR (ob_log_add_file_rule (&totally_plain, "Death*", false));
  ob_log_loc ("DeathlyHallows.cpp", 123, &totally_plain, 0,
              "this should not be printed\n");
  ob_log_loc ("C:\\Documents and Settings and Other Random Crap\\DeathStar.cpp",
              123, &totally_plain, 0, "but this should be printed\n");

  if (argc == 1)
    {
      CHECK_POSIX_ERROR (close (fildes[1]));
      CHECK_PTHREAD_ERROR (pthread_join (thr, NULL));
      CHECK_POSIX_ERROR (fclose (f));
    }
  else
    {
      // Make sure there's no funny business with the variadic macros.
      // We always test the compilation, which is the most important
      // part here, but we only run these if we are in non-self-checking
      // mode.
      OB_LOG_ERROR ("I have no arguments");
      OB_LOG_WARNING ("I have %d argument", 1);
      OB_LOG_INFO ("I have %d argument%s", 2, "s");
      OB_LOG_DEBUG ("I have %d %xrgument%s", 3, 10, "s");
      // deprecation with a code should only print once
      OB_LOG_DEPRECATION_CODE (0x10fffff2, "I am deprecated");
      OB_LOG_DEPRECATION_CODE (0x10fffff2, "I am deprecated");
      // no such luxury for deprecation without a code
      OB_LOG_DEPRECATION ("I am deprecated");
      OB_LOG_DEPRECATION ("I am deprecated");
    }

  if (!yeah_i_got_called)
    error_exit ("Callback function was not called\n");

  OBSERT (1 + 1 == 2);
  OBSERT_CODE (0x10fffff3, 2 + 2 == 4);

  return EXIT_SUCCESS;
}

int test_logging_last_line = __LINE__;
