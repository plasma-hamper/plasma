
/* (c)  oblong industries */

// test ob_strptime () && ob_format_time_f

#include "libLoam/c/ob-sys.h"
#include "libLoam/c/ob-time.h"
#include "libLoam/c/ob-util.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-retorts.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <math.h>

static int num_failures = 0;

typedef struct for_a_good_time_call
{
  char good_time[32];
  char expected[32];
} for_a_good_time_call;

static const for_a_good_time_call good_times[] = {
  {"Dec 20, 2024 13:30:53.63 ", "Dec 20, 2024 13:30:53.63 "},
  {"Dec 20, 2024 13:30:53.63", "Dec 20, 2024 13:30:53.63 "},
  {"Dec 20, 2024 13:30:53.631 ", "Dec 20, 2024 13:30:53.63 "},
  {"Dec 20, 2024 13:30:53.631", "Dec 20, 2024 13:30:53.63 "},
  {"Dec 20, 2024 13:30:53.50 ", "Dec 20, 2024 13:30:53.50 "},
  {"Dec 20, 2024 13:30:53.50", "Dec 20, 2024 13:30:53.50 "},
  {"Dec 20, 2024 13:30:53.5 ", "Dec 20, 2024 13:30:53.50 "},
  {"Dec 20, 2024 13:30:53.5", "Dec 20, 2024 13:30:53.50 "},
  {"Dec 20, 2024 13:30:53 ", "Dec 20, 2024 13:30:53.00 "},
  {"Dec 20, 2024 13:30:53", "Dec 20, 2024 13:30:53.00 "},
};

typedef struct for_a_bad_time_call
{
  char bad_time[32];
  ob_retort expected;
} for_a_bad_time_call;

static const for_a_bad_time_call bad_times[] = {
  {"Dec 32, 2024 13:30:53.63", OB_PARSE_ERROR},
  {"Dec 20, 2024 25:30:53.63", OB_PARSE_ERROR},
  {"Dec 20, 2024 13:30:xx.63", OB_PARSE_ERROR},
  {"Dec 20, 2024 13:30:53.xx", OB_PARSE_ERROR},
  {"Dec 20, 867-5309 13:30:53.63", OB_PARSE_ERROR},
  {"Blob 20, 2024 13:30:53.63", OB_PARSE_ERROR},
  {".63", OB_PARSE_ERROR},
  {"", OB_PARSE_ERROR},
};

/* oddly, ob_format_time outputs a single trailing space. */
static const char beg_of_time_s[] = "Jan 1, 1970 00:00:00.00 ";
static const float64 beg_of_time_sec = 0.0;
static struct timeval beg_of_time_tv (void)
{
  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  return tv;
}

static const char rand_test_time_s[] = "Oct 31, 2016 16:20:42.09 ";
static const float64 rand_test_time_sec = 1477930842.099525;
static struct timeval rand_test_time_tv (void)
{
  struct timeval tv;
  tv.tv_sec = 1477930842;
  tv.tv_usec = 99525;
  return tv;
}

static void test_ob_format_time (void)
{
  char buf[80];
  size_t expected_len = 24;

  /* Test the beginning of time */
  struct timeval tv = beg_of_time_tv ();
  memset (buf, 1, sizeof (buf));
  ob_format_time (buf, sizeof (buf), &tv);
  if (strncmp (buf, beg_of_time_s, sizeof (beg_of_time_s)))
    error_exit ("test_ob_format_time (beginning of time): '%s' != '%s'\n", buf,
                beg_of_time_s);
  if (strlen (buf) != expected_len)
    error_exit ("test_ob_format_time (beginning of time): strlen('%s') != %zu",
                buf, expected_len);

  /* Test a specific non-zero time */
  expected_len = 25;
  tv = rand_test_time_tv ();
  memset (buf, 1, sizeof (buf));
  ob_format_time (buf, sizeof (buf), &tv);
  if (strncmp (buf, rand_test_time_s, sizeof (rand_test_time_s)))
    error_exit ("test_ob_format_time (non-zero of time): '%s' != '%s'\n", buf,
                rand_test_time_s);
  if (strlen (buf) != expected_len)
    error_exit ("test_ob_format_time (non-zero of time): strlen('%s') != %zu",
                buf, expected_len);

  /* Test a buffer JUST big enough to fit strftime + ms ("%02d ") plus one */
  memset (buf, 1, sizeof (buf));
  ob_format_time (buf, expected_len + 2, &tv);
  if (strncmp (buf, rand_test_time_s, sizeof (rand_test_time_s)))
    error_exit ("test_ob_format_time (exact buffer size): '%s' != '%s'\n", buf,
                rand_test_time_s);
  if (strlen (buf) != expected_len)
    error_exit ("test_ob_format_time (exact buffer size): strlen('%s') != %zu",
                buf, expected_len);

#if 0
  /* FIXME: ob_format_time behavior for short buffers needs to be
   * better defined before we can test it
   */

  /* Test a buffer big enough to fit strftime, but not ms ("%02d ") */
  memset(buf, 1, sizeof(buf));
  ob_format_time (buf, expected_len, &tv);
  if (strncmp(buf, rand_test_time_s, sizeof(rand_test_time_s)))
    error_exit ("test_ob_format_time (mid buffer size): '%s' != '%s'\n",
                buf, rand_test_time_s);
  if (strlen(buf) != expected_len)
    error_exit ("test_ob_format_time (mid buffer size): strlen('%s') != %zu",
                buf, expected_len);

  /* Test a buffer insufficient to even fit strftime */
  memset(buf, 1, sizeof(buf));
  ob_format_time (buf, expected_len - 4, &tv);
  if (buf != NULL)
    error_exit ("test_ob_format_time (insufficient buffer): '%s' != '%s'\n",
                buf, rand_test_time_s);
  if (strlen(buf) != expected_len)
    error_exit ("test_ob_format_time (insufficient buffer): strlen('%s') != %zu",
                buf, expected_len);
#endif
}

static void test_ob_format_time_f (void)
{
  char buf[80];
  size_t expected_len = 24;

  /* Test the beginning of time */
  memset (buf, 1, sizeof (buf));
  ob_format_time_f (buf, sizeof (buf), beg_of_time_sec);
  if (strncmp (buf, beg_of_time_s, sizeof (beg_of_time_s)))
    error_exit ("test_ob_format_time_f (beginning of time): '%s' != '%s'\n",
                buf, beg_of_time_s);
  if (strlen (buf) != expected_len)
    error_exit (
      "test_ob_format_time_f (beginning of time): strlen('%s') != %zu", buf,
      expected_len);

  /* Test a specific non-zero time */
  expected_len = 25;
  memset (buf, 1, sizeof (buf));
  ob_format_time_f (buf, sizeof (buf), rand_test_time_sec);
  if (strncmp (buf, rand_test_time_s, sizeof (rand_test_time_s)))
    error_exit ("test_ob_format_time_f (non-zero of time): '%s' != '%s'\n", buf,
                rand_test_time_s);
  if (strlen (buf) != expected_len)
    error_exit ("test_ob_format_time_f (non-zero of time): strlen('%s') != %zu",
                buf, expected_len);
}

static void print_passfail (bool good)
{
  if (good)
    {
      printf ("\033[32m\342\234\224\033[0m");
    }
  else
    {
      printf ("\033[91m\360\237\227\264\033[0m");
      num_failures++;
    }
}

static void print_test_string (const char *str)
{
  char buf[80];

  snprintf (buf, sizeof (buf), "\"%.75s\"", str);
  printf ("%-30s -> ", buf);
}

static void test_ob_strptime (void)
{
  char ctime1[80];
  float64 cur_time, echo_time;

  cur_time = ob_current_time ();
  ob_format_time_f (ctime1, sizeof (ctime1), cur_time);
  printf ("test_ob_strptime: cur_time = %f = %s\n", cur_time, ctime1);

  OB_DIE_ON_ERROR (ob_strptime (ctime1, &echo_time));

  if (fabs (cur_time - echo_time) > 0.01)
    error_exit ("test_ob_strptime: %f != %f\n", cur_time, echo_time);

  /* Test whether some known-good strings round-trip as expected */
  size_t i;

  for (i = 0; i < sizeof (good_times) / sizeof (good_times[0]); i++)
    {
      float64 secs = -1.0;
      const char *good = good_times[i].good_time;

      print_test_string (good);
      ob_retort tort = ob_strptime (good, &secs);

      if (tort == OB_OK)
        {
          char buf[80];

          ob_format_time_f (buf, sizeof (buf), secs);

          print_passfail (0 == strcmp (buf, good_times[i].expected));
          printf (" \"%s\"\n", buf);
        }
      else
        {
          const char *msg = ob_error_string (tort);
          print_passfail (false);
          printf (" %s\n", msg);
        }
    }

  /* Test whether some known-bad strings fail as expected */
  for (i = 0; i < sizeof (bad_times) / sizeof (bad_times[0]); i++)
    {
      float64 secs = -1.0;
      const char *bad = bad_times[i].bad_time;

      print_test_string (bad);

      ob_retort tort = ob_strptime (bad, &secs);
      print_passfail (tort == bad_times[i].expected);

      const char *msg = ob_error_string (tort);
      printf (" %s\n", msg);
    }
}

int main (int argc, char **argv)
{
  /* Set timezone for reproducibility */
  ob_setenv ("TZ", "UTC");
  tzset ();

  test_ob_format_time ();
  test_ob_format_time_f ();
  test_ob_strptime ();

  return num_failures;
}
