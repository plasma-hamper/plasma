
/* (c)  oblong industries */

///
/// Debugging and random helper routines for pool commands.
///
#include <ctype.h>

#include "pool_cmd.h"
#include "slaw-string.h"
#include "pool_options.h"
#include "libLoam/c/ob-dirs.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-sys.h"

// layering violation, alas
#if HAVE_OPENSSL
#include <openssl/opensslv.h>
#include <openssl/rand.h>
#endif

void pool_cmd_free_options (pool_cmd_info *cmd)
{
  Free_Protein (cmd->create_options);
}

void pool_cmd_setup_options (pool_cmd_info *cmd)
{
  // Defaults
  if (!cmd->type)
    cmd->type = "mmap";
  if (!cmd->size)
    cmd->size = POOL_MMAP_DEFAULT_SIZE;
  cmd->create_options = toc_mmap_pool_options (cmd->size, cmd->toc_capacity);
}

int pool_cmd_get_poolname (pool_cmd_info *cmd, int argc, char *argv[],
                           int my_optind)
{
  argc -= my_optind;
  argv += my_optind;
  if (argc != 1)
    return 1;
  cmd->pool_name = argv[0];
  return 0;
}

void pool_cmd_options_from_env (pool_cmd_info *cmd)
{
  OB_CLEAR (*cmd);
  const char *pool_type = getenv ("POOL_TYPE");
  const char *pool_size = getenv ("POOL_SIZE");
  const char *toc_capacity = getenv ("POOL_TOC_CAPACITY");
  const char *poolName = getenv ("TEST_POOL");
  if (pool_type)
    cmd->type = pool_type;
  if (pool_size)
    cmd->size = pool_cmd_parse_size (pool_size);
  if (toc_capacity)
    cmd->toc_capacity = pool_cmd_parse_size (toc_capacity);
  if (!poolName)
    poolName = "misc-pool-test";
  cmd->pool_name = poolName;
  pool_cmd_setup_options (cmd);
}

unt64 pool_cmd_parse_size (const char *arg)
{
  static const char suffixes[] = {'K', 'M', 'G', 'T'};
  char *units = NULL;
  const unt64 s = strtoll (arg, &units, 0);
  if (s > 0 && units && strlen (units) > 0)
    {
      const char unit = toupper (units[0]);
      int i;
      for (i = 0; i < sizeof (suffixes) / sizeof (suffixes[0]); i++)
        {
          if (unit == suffixes[i])
            return s * (1ULL << (10 * (i + 1)));
        }
    }
  return s;
}

void pool_cmd_open_pool (pool_cmd_info *cmd)
{
  ob_retort pret;

  pret = pool_participate (cmd->pool_name, &cmd->ph, NULL);
  if (pret != OB_OK)
    {
      fprintf (stderr, "bad participate: %s\n", ob_error_string (pret));
      exit (pool_cmd_retort_to_exit_code (pret));
    }
}

protein pool_cmd_create_test_protein (const char *name)
{
  // Construct a test protein to put in the pool

  slaw ingests, descrips;
  protein prot;

  descrips = slaw_list_inline_f (slaw_string_format ("descrip_%s", name), NULL);
  ingests = slaw_map_inline_ff (slaw_string_format ("key_%s", name),
                                slaw_string_format ("value_%s", name), NULL);
  prot = protein_from_ff (descrips, ingests);

  return prot;
}

ob_retort pool_cmd_add_test_protein (pool_hose ph, const char *name,
                                     protein *prot_p, int64 *idx)
{
  protein prot = pool_cmd_create_test_protein (name);
  ob_retort pret = pool_deposit (ph, prot, idx);
  if (OB_OK != pret)
    OB_LOG_ERROR_CODE (0x20102003, "no luck on the deposit: %s\n",
                       ob_error_string (pret));
  else
    *prot_p = prot;
  return pret;
}

int pool_cmd_check_protein_match (bprotein p1, bprotein p2)
{
  if (!proteins_equal (p1, p2))
    {
      fprintf (stderr, "proteins don't match\n");
      fprintf (stderr, "protein 1:\n"
                       "----------\n");
      slaw_spew_overview (p1, stderr, NULL);
      fputc ('\n', stderr);
      fprintf (stderr, "protein 2:\n"
                       "----------\n");
      slaw_spew_overview (p2, stderr, NULL);
      fputc ('\n', stderr);
      return 0;
    }
  return 1;
}

void pool_cmd_fill_pool (pool_cmd_info *cmd)
{
  protein prot;

  ob_retort pret = pool_participate (cmd->pool_name, &cmd->ph, NULL);
  if (pret != OB_OK)
    {
      OB_LOG_ERROR_CODE (0x20102000, "bad participate: %s\n",
                         ob_error_string (pret));
      exit (pool_cmd_retort_to_exit_code (pret));
    }

  slaw hosename =
    slaw_string_format ("filling %s", ob_basename (cmd->pool_name));
  OB_DIE_ON_ERR_CODE (
    0x20102008, pool_set_hose_name (cmd->ph, slaw_string_emit (hosename)));
  slaw_free (hosename);

  prot = pool_cmd_create_test_protein ("first");

  // Deposit proteins until the protein with index of 0 is
  // overwritten, meaning the pool is full.
  int64 idx = 0;
  pret = pool_oldest_index (cmd->ph, &idx);
  if (pret == POOL_NO_SUCH_PROTEIN)
    idx = 0;
  else if (pret < OB_OK)
    {
      OB_LOG_ERROR_CODE (0x20102007, "pool_oldest_index failed: %s\n",
                         ob_error_string (pret));
      exit (pool_cmd_retort_to_exit_code (pret));
    }
  while ((idx == 0) || (idx == -1))
    {
      pret = pool_deposit (cmd->ph, prot, NULL);
      if (pret != OB_OK)
        OB_FATAL_ERROR_CODE (0x20102001, "Bad deposit while filling pool\n");
      OB_DIE_ON_ERR_CODE (0x20102009, pool_oldest_index (cmd->ph, &idx));
    }

  if (cmd->verbose)
    {
      OB_DIE_ON_ERR_CODE (0x2010200a, pool_newest_index (cmd->ph, &idx));
      printf ("Deposited %" OB_FMT_64 "d proteins\n", idx);
    }

  protein_free (prot);
  pret = pool_withdraw (cmd->ph);
  if (pret < OB_OK)
    OB_FATAL_ERROR_CODE (0x20102002, "pool_withdraw: %s\n",
                         ob_error_string (pret));
}

int pool_cmd_retort_to_exit_code (ob_retort tort)
{
  switch (tort)
    {
      case OB_OK:
        return EXIT_SUCCESS;
      case POOL_NO_SUCH_POOL:
        return EXIT_NOPOOL;
      case POOL_EXISTS:
        return EXIT_EXISTS;
      case POOL_NO_SUCH_PROTEIN:
        return EXIT_NOPROTEIN;
      case POOL_POOLNAME_BADTH:
        return EXIT_BADPNAME;
      case POOL_TYPE_BADTH:
        return EXIT_BADPTYPE;
      case POOL_IN_USE:
        return EXIT_INUSE;
      default:
        return EXIT_FAILURE;
    }
}

/* Skip file descriptor leak testing on Windows (because there's no /dev/fd
 * there) and on OS X (because we get false positives on OS X, from system
 * libraries).  Testing on Linux alone should be sufficient. */
#if !defined(_MSC_VER) && !defined(__APPLE__)

typedef struct
{
  int size;
  int capacity;
  int *a;
} fdarray;

static void fdarray_construct (fdarray *fda)
{
  fda->size = 0;
  fda->capacity = 0;
  fda->a = 0;
}

static void fdarray_destroy (fdarray *fda)
{
  if (fda->a)
    {
      free (fda->a);
    }
  fdarray_construct (fda);
}

static int fdarray_expand (fdarray *fda)
{
  const int expanded_capacity = fda->capacity == 0 ? 1 : 2 * fda->capacity;
  int *const expanded_a =
    (int *) realloc (fda->a, expanded_capacity * sizeof (int));
  if (expanded_a)
    {
      fda->capacity = expanded_capacity;
      fda->a = expanded_a;
    }
  else
    {
      ob_log (OBLV_ERRU, 0x20102004,
              "could not expand capacity of fdarray to %d\n",
              expanded_capacity);
      return 1;
    }

  return 0;
}

static int fdarray_push (fdarray *fda, int x)
{
  if (fda->size == fda->capacity)
    {
      if (fdarray_expand (fda))
        {
          return 1;
        }
    }
  fda->a[fda->size++] = x;
  return 0;
}

static inline void fd_accum (int *a, int x)
{
  if (*a == 0)
    {
      *a = x;
    }
}

static int is_sss_fd (int fd)
{
#ifdef __gnu_linux__
  /* If your /etc/nsswitch.conf contains the word 'sss',
    * libnss_sss.so will leak an fd to /var/lib/sss/mc/passwd
    * after you open up a tcp pool connection
    * (possibly as a result of the call to getaddrinfo())
    * and cause tests to fail.
    * So, to allow working around that, provide this function
    * to test whether a fd is 'owned' by sss and should be ignored.
    * See https://bugs.oblong.com/show_bug.cgi?id=17977
    */
  slaw fdnamename =
    slaw_string_format ("/proc/%" OB_FMT_64 "d/fd/%d", (int64) getpid (), fd);
  char fdname[128];
  memset (fdname, 0, sizeof (fdname));
  ssize_t len =
    readlink (slaw_string_emit (fdnamename), fdname, sizeof (fdname));
  slaw_free (fdnamename);
  return ((len >= 12) && (strncmp (fdname, "/var/lib/sss", 12) == 0));
#else
  return 0;
#endif
}

/* Note: we didn't do anything special below to exclude the /dev/fd
 file descriptor from the comparison of file descriptors.  In other
 words, we were getting away with the dubious assumption that the
 same-numbered file descriptor will be used to open /dev/fd both at
 the start and at the finish.  To avoid this problem, we now
 handle it below by pushing fd onto fda only if fd != dirfd (d). */

static int get_file_descriptors (fdarray *fda)
{
  const char *devfd = "/dev/fd";
  DIR *d = opendir (devfd);
  if (!d)
    {
      ob_log (OBLV_ERRU, 0x20102005, "could not opendir %s: %s\n", devfd,
              strerror (errno));
      return 1;
    }

  int r = 0;
  const struct dirent *ent = 0;
  while ((ent = readdir (d)))
    {
      const int fd = atoi (ent->d_name);
      if (fd != dirfd (d) && !is_sss_fd (fd))
        fd_accum (&r, fdarray_push (fda, fd));
    }

  CHECK_POSIX_ERROR (closedir (d));
  return r;
}

static void report_difference (int set_id, int x)
{
  const char *const setstrings[] = {"start", "finish"};
  const char *const yes_present_at = setstrings[set_id != 0];
  const char *const not_present_at = setstrings[set_id == 0];
  fprintf (stderr, "error: file descriptor %d present at %s but not at %s\n", x,
           yes_present_at, not_present_at);
}

static int is_member (int x, const fdarray *s)
{
  // Note: linear search used below.
  // Good enough for the small set sizes we anticipate.
  int i = 0;
  for (; i < s->size; i++)
    {
      if (s->a[i] == x)
        {
          return 1;
        }
    }
  return 0;
}

static int asymmetric_set_difference (fdarray *d, const fdarray *s0,
                                      const fdarray *s1)
{
  // Note: naive n-squared algorithm used below.
  // Good enough for the small set sizes we anticipate.
  int r = 0;
  int i = 0;
  for (; i < s0->size; i++)
    {
      const int e0 = s0->a[i];  // element of set 0 under investigation
      if (!is_member (e0, s1))
        {
          fd_accum (&r, fdarray_push (d, e0));
        }
    }
  return r;
}

static int report_differences_in_one_direction (const fdarray *s0,
                                                const fdarray *s1, int set_0_id)
{
  int r = 0;

  fdarray diff;
  fdarray_construct (&diff);

  fd_accum (&r, asymmetric_set_difference (&diff, s0, s1));

  fd_accum (&r, diff.size > 0);

  int i = 0;
  for (; i < diff.size; i++)
    {
      report_difference (set_0_id, diff.a[i]);
    }

  fdarray_destroy (&diff);

  return r;
}

static int report_differences (const fdarray *s0, const fdarray *s1)
{
  int r = 0;

  fd_accum (&r, report_differences_in_one_direction (s0, s1, 0));

  fd_accum (&r, report_differences_in_one_direction (s1, s0, 1));

  return r;
}

static int get_file_descriptors_and_compare (const fdarray *fda0)
{
  int r = 0;

  fdarray fda1;
  fdarray_construct (&fda1);

  // uncomment line below for testing an fd present at finish but not at start
  // fdarray_push( &fda1, 38 );

  fd_accum (&r, get_file_descriptors (&fda1));

  fd_accum (&r, report_differences (fda0, &fda1));

  fdarray_destroy (&fda1);

  if (r)
    {
      fprintf (stderr, "some kind of error occured in "
                       "get_file_descriptors_and_compare; dumping output of "
                       "lsof to help diagnose.\n");
      slaw cmd =
        slaw_string_format ("lsof -p %" OB_FMT_64 "d", (int64) getpid ());
      if (system (slaw_string_emit (cmd)) != 0)
        ob_log (OBLV_ERRU, 0x20102006, "Error executing '%s'\n",
                slaw_string_emit (cmd));
      slaw_free (cmd);
    }

  return r;
}

int check_for_leaked_file_descriptors_scoped (mainish_function mf, int argc,
                                              char **argv)
{
  int r = 0;

  fdarray fda;
  fdarray_construct (&fda);

  // uncomment line below for testing an fd present at start but not at finish
  // fdarray_push( &fda, 37 );

  fd_accum (&r, get_file_descriptors (&fda));

  fd_accum (&r, mf (argc, argv));

#if HAVE_OPENSSL
#if OPENSSL_VERSION_NUMBER >= 0x1010100fL
  OB_LOG_DEBUG ("check_for_leaked_file_descriptors_scoped: calling "
                "RAND_keep_random_devices_open(0) to avoid test failure from "
                "benign fd leak, see issue 240\n");
  RAND_keep_random_devices_open (0);
#endif
#endif

  fd_accum (&r, get_file_descriptors_and_compare (&fda));

  fdarray_destroy (&fda);

  return r;
}

#else

int check_for_leaked_file_descriptors_scoped (mainish_function mf, int argc,
                                              char **argv)
{
  return mf (argc, argv);
}

#endif

static void modlog (ob_log_level *lvl, bool log_pids, bool log_time,
                    bool limit_logs)
{
  if (log_pids)
    lvl->flags |= OB_FLG_SHOW_PID;

  if (log_time)
    lvl->flags |= OB_FLG_SHOW_TIME;

  if (!ob_log_is_level_enabled (lvl))
    // If this log level isn't going anywhere, don't bother to limit it.
    // (Without this, we get messages on exit about suppressing debug
    // messages that we never intended to print anyway.)
    return;

  if (limit_logs)
    {
      ob_log_rule rul;
      // This rule matches everything, because matchbits is 0
      OB_CLEAR (rul);
      rul.maxcount = POOL_CMD_MAX_LOG_COUNT;
      rul.uniquely = true;
      rul.notify = true;
      OB_DIE_ON_ERR_CODE (0x2010200c, ob_log_add_rule (lvl, rul));
      // To avoid bug 1165: don't limit code 0
      OB_CLEAR (rul);
      rul.maxcount = OB_INT64_MAX;
      rul.matchbits = 64;
      OB_DIE_ON_ERR_CODE (0x2010200d, ob_log_add_rule (lvl, rul));
    }
}

void pool_cmd_modify_default_logging (bool log_pids, bool log_time,
                                      bool limit_logs)
{
  modlog (OBLV_BUG, log_pids, log_time, limit_logs);
  modlog (OBLV_ERROR, log_pids, log_time, limit_logs);
  modlog (OBLV_DEPRECATION, log_pids, log_time, limit_logs);
  modlog (OBLV_WRNU, log_pids, log_time, limit_logs);
  modlog (OBLV_INFO, log_pids, log_time, limit_logs);
  modlog (OBLV_DBUG, log_pids, log_time, limit_logs);
}

void pool_cmd_enable_debug_messages (unt64 code, unt8 matchbits)
{
  // Enable debug messages in three steps...

  // Step 1: add a rule to suppress all debug messages
  ob_log_rule rul;
  // This rule matches everything, because matchbits is 0
  // and suppresses everything, because maxcount and notify are 0
  OB_CLEAR (rul);
  OB_DIE_ON_ERR_CODE (0x2010200e, ob_log_add_rule (OBLV_DBUG, rul));

  // Step 2: add a rule to enable the desired messages
  rul.maxcount = OB_CONST_I64 (0x7fffffffffffffff);  // unlimited
  rul.code = code;
  rul.matchbits = matchbits;
  OB_DIE_ON_ERR_CODE (0x2010200f, ob_log_add_rule (OBLV_DBUG, rul));

  // Step 3: tell debug level to go to file descriptor (stderr)
  OBLV_DBUG->flags |= OB_DST_FD;
}
