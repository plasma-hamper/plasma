
/* (c)  oblong industries */

#include "libLoam/c/ob-sys.h"
#include "libLoam/c/ob-pthread.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-vers.h"
#include "libLoam/c/ob-string.h"
// XXX: including a private libLoam header from libPlasma is semi-naughty
#include "libLoam/c/private/ob-syslog.h"
#include "libPlasma/c/pool-log.h"
#include "libPlasma/c/pool_cmd.h"
#include "libPlasma/c/slaw-path.h"
#include "libPlasma/c/protein.h"
#include "libPlasma/c/slaw.h"

static ob_log_level my_log = {
  OB_DST_FD | OB_FLG_SHOW_CODE_OR_WHERE | OB_FLG_SHOW_TID_NONMAIN,
  OB_FOREGROUND_ENABLE | OB_FOREGROUND_RED, /* red */
  LOG_ERR,
  2,
  "test: ",
  NULL,
  NULL,
  NULL,
  NULL};

static ob_log_level red_log = {OB_DST_FD | OB_FLG_SHOW_TID_NONMAIN,
                               OB_FOREGROUND_ENABLE
                                 | OB_FOREGROUND_RED, /* red */
                               LOG_ERR,
                               2,
                               "",
                               NULL,
                               NULL,
                               NULL,
                               NULL};

static ob_log_level yellow_log = {OB_DST_FD | OB_FLG_SHOW_TID_NONMAIN,
                                  OB_FOREGROUND_ENABLE | OB_FOREGROUND_RED
                                    | OB_FOREGROUND_GREEN, /* yellow */
                                  LOG_ERR,
                                  2,
                                  "",
                                  NULL,
                                  NULL,
                                  NULL,
                                  NULL};

static ob_log_level limited_log = {OB_DST_FD | OB_FLG_SHOW_TID_NONMAIN,
                                   0, /* no color */
                                   LOG_ERR,
                                   2,
                                   "",
                                   NULL,
                                   NULL,
                                   NULL,
                                   NULL};

typedef struct
{
  ob_log_level *lvl;
  const char *msg;
} th_arg;

typedef struct
{
  int64 tid;
  int num;
  char txt[20];
} checker;

// very primitive associative array
static checker *associate (int64 tid, checker chk[4])
{
  int i;
  for (i = 0; i < 4 && chk[i].tid != -1; i++)
    if (tid == chk[i].tid)
      return &(chk[i]);

  if (i == 4)
    error_exit ("Had %" OB_FMT_64 "d %" OB_FMT_64 "d %" OB_FMT_64
                "d %" OB_FMT_64 "d but got %" OB_FMT_64 "d\n",
                chk[0].tid, chk[1].tid, chk[2].tid, chk[3].tid, tid);

  chk[i].tid = tid;
  return &(chk[i]);
}

static void *thread_main (void *v)
{
  const th_arg *arg = (const th_arg *) v;
  int i;

  for (i = 0; i < 100; i++)
    ob_log (arg->lvl, 0, "{%d} %s\n", i + 1, arg->msg);

  return NULL;
}

static void do_thread_test (pool_hose ph, int *ret)
{
  th_arg args[4];
  pthread_t thr[4];
  checker chk[4];
  int i;

  OB_CLEAR (chk);

  args[0].lvl = &red_log;
  args[0].msg = "red leather";
  args[1].lvl = &yellow_log;
  args[1].msg = "yellow leather";
  args[2].lvl = &limited_log;
  args[2].msg = "Tastes great!";
  args[3].lvl = &limited_log;
  args[3].msg = "Less filling!";

  ob_log_rule rul;
  rul.count = 0;
  rul.maxcount = 100;
  rul.code = 0;
  rul.matchbits = 0;
  rul.uniquely = false;
  rul.notify = false;
  OB_DIE_ON_ERROR (ob_log_add_rule (&limited_log, rul));

  for (i = 0; i < 4; i++)
    {
      int erryes = pthread_create (&thr[i], NULL, thread_main, &args[i]);
      if (erryes)
        error_exit ("pthread_create[%d]: %s\n", i, strerror (erryes));
      chk[i].tid = -1;
    }

  for (i = 0; i < 300; i++)
    {
      protein p = NULL;
      ob_retort pret = pool_await_next (ph, 60, &p, NULL, NULL);
      if (pret < OB_OK)
        {
          error_report ("Got '%s' on protein %d\n", ob_error_string (pret), i);
          *ret = EXIT_FAILURE;
          break;
        }

      // check the protein!
      const char *msg = slaw_path_get_string (p, "msg", "bad");
      int64 tid = slaw_path_get_int64 (p, "thread", -1);
      if (tid < 0)
        {
          error_report ("No one expects the Spanish Inquisition, or "
                        "the integer %" OB_FMT_64 "d\n",
                        tid);
          *ret = EXIT_FAILURE;
          break;
        }
      checker *c = associate (tid, chk);
      const char *txt = strrchr (msg, '}');
      if (0 == c->txt[0])
        ob_safe_copy_string (c->txt, sizeof (c->txt), txt);
      else if (0 != strcmp (txt, c->txt))
        {
          error_report ("Expected '%s' but got '%s'\n", c->txt, txt);
          *ret = EXIT_FAILURE;
          break;
        }
      c->num++;
      int num = atoi (msg + 1);
      if (num != c->num)
        {
          error_report ("For '%s', expected %d but got %d\n", c->txt, c->num,
                        num);
          *ret = EXIT_FAILURE;
          break;
        }

      protein_free (p);
    }

  // If you can't beat 'em...
  for (i = 0; i < 4; i++)
    {
      int erryes = pthread_join (thr[i], NULL);
      if (erryes)
        error_exit ("pthread_join[%d]: %s\n", i, strerror (erryes));
    }

  // We should be at the end; should not have deposited more than 300 proteins
  int64 current, newest;
  OB_DIE_ON_ERROR (pool_index (ph, &current));
  OB_DIE_ON_ERROR (pool_index (ph, &newest));
  if (newest != current)
    {
      error_report ("current = %" OB_FMT_64 "d but newest = %" OB_FMT_64 "d\n",
                    current, newest);
      *ret = EXIT_FAILURE;
    }
}

static void usage (void)
{
  fprintf (stderr,
           "Usage: %s [-t <type>] [-s <size>] [-i <toc cap>] <poolname>\n",
           ob_get_prog_name ());
  exit (EXIT_FAILURE);
}

int main (int argc, char **argv)
{
  OB_DIE_ON_ERROR (OB_CHECK_ABI ());

  pool_cmd_info cmd;
  int c;
  int ret = EXIT_SUCCESS;
  int64 l[3];

  memset (&cmd, 0, sizeof (cmd));
  while ((c = getopt (argc, argv, "i:s:t:")) != -1)
    {
      switch (c)
        {
          case 'i':
            cmd.toc_capacity = pool_cmd_parse_size (optarg);
            break;
          case 's':
            cmd.size = pool_cmd_parse_size (optarg);
            break;
          case 't':
            cmd.type = optarg;
            break;
          default:
            usage ();
        }
    }

  pool_cmd_setup_options (&cmd);
  if (pool_cmd_get_poolname (&cmd, argc, argv, optind))
    usage ();

  // open the hose that will be used by the logging thread
  pool_hose ho;
  OB_DIE_ON_ERROR (pool_participate_creatingly (cmd.pool_name, cmd.type, &ho,
                                                cmd.create_options));

  OB_DIE_ON_ERROR (ob_log_to_pool (ho, true, &my_log, &red_log, &yellow_log,
                                   &limited_log, NULL));

  // open the hose that we will use
  pool_cmd_open_pool (&cmd);

  // log some stuff that will end up in pool
  const int64 before = __LINE__;
  ob_log (&my_log, 0, "Hello, World!\n");
  ob_log (&my_log, 0, "This is a test\non two lines.\n");
  const int64 after = __LINE__;

  protein p[3];
  OB_CLEAR (p);
  for (c = 0; c < 3; c++)
    {
      ob_retort pret = pool_await_next (cmd.ph, 60, &p[c], NULL, NULL);
      if (pret < OB_OK)
        {
          error_report ("Got '%s' on protein %d\n", ob_error_string (pret), c);
          ret = EXIT_FAILURE;
          goto die;
        }
    }

  {
    // First check that messages are correct
    const char *actual = slaw_path_get_string (p[0], "msg", "bad");
    const char *expected = "Hello, World!\n";
    if (0 != strcmp (actual, expected))
      {
        error_report ("Expected: %sbut got: %s", expected, actual);
        ret = EXIT_FAILURE;
      }

    actual = slaw_path_get_string (p[1], "msg", "bad");
    expected = "This is a test\n";
    if (0 != strcmp (actual, expected))
      {
        error_report ("Expected: %sbut got: %s", expected, actual);
        ret = EXIT_FAILURE;
      }

    actual = slaw_path_get_string (p[2], "msg", "bad");
    expected = "on two lines.\n";
    if (0 != strcmp (actual, expected))
      {
        error_report ("Expected: %sbut got: %s", expected, actual);
        ret = EXIT_FAILURE;
      }

    // All three should be: in main thread, in current pid,
    // in file "pool-log-test.c", in program "pool-log-test".
    for (c = 0; c < 3; c++)
      {
        actual = slaw_path_get_string (p[c], "thread", "bad");
        expected = "main";
        if (0 != strcmp (actual, expected))
          {
            error_report ("Expected '%s' but got '%s'\n", expected, actual);
            ret = EXIT_FAILURE;
          }

        int64 actual_pid = slaw_path_get_int64 (p[c], "pid", -1);
        int64 expected_pid = getpid ();
        if (actual_pid != expected_pid)
          {
            error_report ("Expected %" OB_FMT_64 "d but got %" OB_FMT_64 "d\n",
                          expected_pid, actual_pid);
            ret = EXIT_FAILURE;
          }

        actual = slaw_path_get_string (p[c], "file", "bad");
        /* On Mac and Windows, the suffix on __FILE__ may be uppercase, so skip it */
        expected = "pool-log-test.";
        if (!strstr (actual, expected))
          {
            error_report ("'%s' did not contain '%s'\n", actual, expected);
            ret = EXIT_FAILURE;
          }

        actual = slaw_path_get_string (p[c], "prog", "bad");
        expected = "pool-log-test";
        if (!strstr (actual, expected))
          {
            error_report ("'%s' did not contain '%s'\n", actual, expected);
            ret = EXIT_FAILURE;
          }

        l[c] = slaw_path_get_int64 (p[c], "line", -1);
      }

    // Check line numbers
    int64 expected_line = before + 1;
    if (l[0] != expected_line)
      {
        error_report ("Expected %" OB_FMT_64 "d but got %" OB_FMT_64 "d\n",
                      expected_line, l[0]);
        ret = EXIT_FAILURE;
      }

    expected_line = after - 1;
    for (c = 1; c < 3; c++)
      if (l[c] != expected_line)
        {
          error_report ("{%d} Expected %" OB_FMT_64 "d but got %" OB_FMT_64
                        "d\n",
                        c, expected_line, l[c]);
          ret = EXIT_FAILURE;
        }

    // The two lines of a multiline message should have identical timestamps
    float64 t1 = slaw_path_get_float64 (p[1], "time", 111.111);
    float64 t2 = slaw_path_get_float64 (p[2], "time", 222.222);

    if (t1 != t2)
      {
        error_report ("multiline message has multiple timestamps\n");
        ret = EXIT_FAILURE;
      }

    do_thread_test (cmd.ph, &ret);
  }

die:
  OB_DIE_ON_ERROR (pool_withdraw (cmd.ph));
  pool_cmd_free_options (&cmd);
  for (c = 0; c < 3; c++)
    protein_free (p[c]);
  return ret;
}
