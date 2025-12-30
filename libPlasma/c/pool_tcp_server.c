
/* (c)  oblong industries */

//
// Server to implement TCP network access to pools.
//

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#include "libLoam/c/ob-sys.h"
#include "libLoam/c/ob-vers.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-string.h"
#include "libLoam/c/ob-dirs.h"
#include "libLoam/c/ob-file.h"

#include "libPlasma/c/protein.h"
#include "libPlasma/c/pool.h"
#include "libPlasma/c/private/pool_impl.h"
#include "libPlasma/c/private/pool_tcp.h"
#include "libPlasma/c/private/pool_multi.h"
#include "libPlasma/c/pool_cmd.h"
#include "libPlasma/c/pool_options.h"
#include "libPlasma/c/pool-log.h"
#include "libPlasma/c/private/pool-tls.h"
#include "libPlasma/c/private/pool-portable.h"

#include "libPlasma/c/slaw-string.h"
#include "libPlasma/c/slaw-interop.h"
#include "libPlasma/c/slaw-path.h"

#ifdef __gnu_linux__
#include <sys/prctl.h>
#endif

#define EXIT_IN_USE 9

// Set bits n, m, ... to cause leaks at locations n, m, ...
// E.g. 0 for no leaks,
//      2 for a leak at location 2,
//      6 for leaks at locations 2 and 3, etc.
//static int leak_mask = 0;

// Command line option to keep process in the foreground and not fork
static int debug;
static bool no_fork;

// true means don't print a message about the port being in use
static bool silent_port_in_use = false;

// true means set process name to contain status information (Linux only)
static bool top_info = false;

// true means set process name to contain client information (Linux only)
static bool top_client_info = false;

// file name to write port and PID to
static const char *pidfile;

// the port that was chosen
static int actual_port;

// signal handler sets this when SIGINT or SIGTERM is received
static volatile sig_atomic_t want_to_exit = false;

// file name to write statistics to on exit
static const char *cleanupfile;

// if >=0, number of seconds before killing children
static int cleanup_seconds = -1;

// Keep track of children
static slabu *children_alive;

static bool want_cmd_count = false;

// Name of pool to log to
static const char *log_pool;

// Log each server to a different pool
static bool log_dir;

static ob_retort tls_available = POOL_NO_TLS;

static bool allow_tls = true;
static bool require_tls = false;
static bool client_auth = false;

// an upper bound on the value of a network op
#define NUM_CMDS 128

static void add_child (pid_t pid)
{
  if (!children_alive)
    children_alive = slabu_new ();
  slabu_list_add_x (children_alive, slaw_int64 (pid));
}

static bool children_not_empty (void)
{
  return (children_alive && slabu_count (children_alive) > 0);
}

static ob_retort remove_child (pid_t pid)
{
  if (children_not_empty ())
    return slabu_list_remove_f (children_alive, slaw_int64 (pid));
  return SLAW_NOT_FOUND;
}

static slaw list_children (void)
{
  slabu *sb = slabu_new ();
  int64 i;
  for (i = 0; children_alive && i < slabu_count (children_alive); i++)
    slabu_list_add_x (sb, slaw_string_format ("%" OB_FMT_64 "d",
                                              *slaw_int64_emit (slabu_list_nth (
                                                children_alive, i))));
  return slaw_strings_join_slabu_f (sb, ",");
}

static void set_my_name (const char *name)
{
#ifdef __gnu_linux__
  if (top_info && name)
    {
      char buf[17];
      snprintf (buf, sizeof (buf), "=%s", name);
      prctl (PR_SET_NAME, (long) buf, 0, 0, 0);
    }
#endif
}

static void set_client_info (const char *name)
{
#ifdef __gnu_linux__
  if (top_client_info && name)
    {
      char buf[17];
      snprintf (buf, sizeof (buf), "=%s", name);
      prctl (PR_SET_NAME, (long) buf, 0, 0, 0);
    }
#endif
}

static void initialize_logging (void)
{
  if (log_pool)
    {
      pool_hose ph = NULL;
      const char *pname;
      slaw foo = NULL;
      protein opt;
      if (log_dir)
        {
          opt = mmap_pool_options (64 * 1024);
          int p = getpid ();
          foo = slaw_string_format ("%s/%d", log_pool, p);
          pname = slaw_string_emit (foo);
        }
      else
        {
          opt = large_mmap_pool_options ();
          pname = log_pool;
        }
      ob_retort tort = pool_participate_creatingly (pname, "mmap", &ph, opt);
      Free_Protein (opt);
      if (tort < OB_OK)
        {
          OB_LOG_ERROR_CODE (0x2010903e, "Can't log to '%s' because '%s'\n",
                             pname, ob_error_string (tort));
          return;
        }
      tort = ob_log_to_pool (
        ph, true, OBLV_BUG, OBLV_ERROR, OBLV_DEPRECATION, OBLV_WRNU, OBLV_INFO,
        ob_log_is_level_enabled (OBLV_DBUG) ? OBLV_DBUG : NULL, NULL);
      if (tort < OB_OK)
        OB_LOG_ERROR_CODE (0x2010903f, "Can't log to '%s' because '%s'\n",
                           pname, ob_error_string (tort));
      Free_Slaw (foo);
    }
}

static void reap_children (void);

static void print_pid (int pid)
{
  OB_LOG_DEBUG_CODE (0x2010902c,
                     "pid = %d, port = %d\n"
                     "pools dir = '%s'\n",
                     pid, actual_port, ob_get_standard_path (ob_pools_dir));

  if (!pidfile)
    return;

  OB_LOG_DEBUG_CODE (0x2010902d, "writing pid and port to '%s'\n", pidfile);
  FILE *f = fopen (pidfile, "w");
  if (!f)
    {
      OB_PERROR_CODE (0x2010904c, pidfile);
      return;
    }

  fprintf (f, "port: %d\n", actual_port);
  fprintf (f, "pid: %d\n", pid);
  OB_CHECK_POSIX_CODE (0x20109046, fclose (f));
}

static void print_op_stats (const char *name, const char *process,
                            const int64 *counts)
{
  if (want_cmd_count)
    printf ("^ %s: %s: ", process, name);

  int64 total = 0;
  int i;
  for (i = 0; i < NUM_CMDS; i++)
    if (counts[i])
      {
        if (want_cmd_count)
          printf ("%d => %" OB_FMT_64 "d, ", i, counts[i]);
        total += counts[i];
      }

  if (want_cmd_count)
    printf ("total => %" OB_FMT_64 "d\n", total);

  OB_LOG_DEBUG_CODE (0x2010902e,
                     "process = '%s', name = '%s', total = %" OB_FMT_64 "d\n",
                     process, name, total);
}

/// Send the final phase of the handshake when negotiating a
/// new protocol version.  This includes sending what
/// commands we support.
static ob_retort welcome_new_version (pool_net_data *net,
                                      bool advertise_starttls,
                                      bool advertise_everything_else)
{
  // Use the latest version we support, or the latest version the
  // client supports, whichever is lesser.
  if (net->net_version > POOL_TCP_VERSION_CURRENT)
    net->net_version = POOL_TCP_VERSION_CURRENT;
  if (net->slaw_version > SLAW_VERSION_CURRENT)
    net->slaw_version = SLAW_VERSION_CURRENT;

  unt64 cmds = 0;

  if (advertise_everything_else)
    {
      cmds |= (OB_CONST_U64 (1) << POOL_CMD_CREATE);
      cmds |= (OB_CONST_U64 (1) << POOL_CMD_DISPOSE);
      cmds |= (OB_CONST_U64 (1) << POOL_CMD_PARTICIPATE);
      cmds |= (OB_CONST_U64 (1) << POOL_CMD_PARTICIPATE_CREATINGLY);
      cmds |= (OB_CONST_U64 (1) << POOL_CMD_WITHDRAW);
      cmds |= (OB_CONST_U64 (1) << POOL_CMD_DEPOSIT);
      cmds |= (OB_CONST_U64 (1) << POOL_CMD_NTH_PROTEIN);
      cmds |= (OB_CONST_U64 (1) << POOL_CMD_NEXT);
      cmds |= (OB_CONST_U64 (1) << POOL_CMD_PROBE_FRWD);
      cmds |= (OB_CONST_U64 (1) << POOL_CMD_NEWEST_INDEX);
      cmds |= (OB_CONST_U64 (1) << POOL_CMD_OLDEST_INDEX);
      cmds |= (OB_CONST_U64 (1) << POOL_CMD_AWAIT_NEXT_SINGLE);
      cmds |= (OB_CONST_U64 (1) << POOL_CMD_MULTI_ADD_AWAITER);
      cmds |= (OB_CONST_U64 (1) << POOL_CMD_INFO);
      cmds |= (OB_CONST_U64 (1) << POOL_CMD_LIST);
      cmds |= (OB_CONST_U64 (1) << POOL_CMD_INDEX_LOOKUP);
      cmds |= (OB_CONST_U64 (1) << POOL_CMD_PROBE_BACK);
      cmds |= (OB_CONST_U64 (1) << POOL_CMD_PREV);
      cmds |= (OB_CONST_U64 (1) << POOL_CMD_FANCY_ADD_AWAITER);
      cmds |= (OB_CONST_U64 (1) << POOL_CMD_SET_HOSE_NAME);
      cmds |= (OB_CONST_U64 (1) << POOL_CMD_SUB_FETCH);
      cmds |= (OB_CONST_U64 (1) << POOL_CMD_RENAME);
      cmds |= (OB_CONST_U64 (1) << POOL_CMD_ADVANCE_OLDEST);
      cmds |= (OB_CONST_U64 (1) << POOL_CMD_SLEEP);
      cmds |= (OB_CONST_U64 (1) << POOL_CMD_CHANGE_OPTIONS);
      cmds |= (OB_CONST_U64 (1) << POOL_CMD_LIST_EX);
      cmds |= (OB_CONST_U64 (1) << POOL_CMD_SUB_FETCH_EX);
#ifdef GREENHOUSE
      cmds |= (OB_CONST_U64 (1) << POOL_CMD_GREENHOUSE);
#endif
    }

  if (advertise_starttls)
    cmds |= (OB_CONST_U64 (1) << POOL_CMD_STARTTLS);

  OB_LOG_DEBUG_CODE (0x2010902f,
                     "net version = %d, slaw version = %d, cmd "
                     "mask = 0x%" OB_FMT_64 "x\n",
                     net->net_version, net->slaw_version, cmds);

  byte welcome[12];
  welcome[0] = net->net_version;
  welcome[1] = net->slaw_version;

  unt8 i = 0;
  while (cmds != 0)
    {
      welcome[3 + i++] = (byte) cmds;
      cmds >>= 8;
    }

  welcome[2] = i;

  const ob_handle_t nullWaitObject = OB_NULL_HANDLE;

  return net->send_nbytes (net->connfd, welcome, 3 + i, nullWaitObject);
}

/* Like above, but also receives the two version bytes directly.
 * We use this variant when re-handshaking after initiating TLS. */
static ob_retort welcome_new_version_tls (pool_net_data *net)
{
  const ob_handle_t null_wait = OB_NULL_HANDLE;
  byte buf[2];

  ob_retort tort = net->recv_nbytes (net->connfd, buf, sizeof (buf), null_wait);

  if (tort < OB_OK)
    return tort;

  net->net_version = buf[0];
  net->slaw_version = buf[1];

  return welcome_new_version (net, false, true);
}

#define YE_OLDE_POOL_EMPTY -200010
#define YE_OLDE_DISCARDED_PROTEIN -200610
#define YE_OLDE_FUTURE_PROTEIN -200620

/// Satisfy old clients who expect POOL_NO_SUCH_PROTEIN to be
/// disambiguated into a multitude of different retorts.
/// Fixes bug 420 comment 8.
static ob_retort ye_olde_no_protein_retorte (pool_hose ph, int64 idx)
{
  int64 oldest;
  ob_retort pret = pool_oldest_index (ph, &oldest);
  if (pret < OB_OK)
    return YE_OLDE_POOL_EMPTY;
  else if (idx < oldest)
    return YE_OLDE_DISCARDED_PROTEIN;
  else
    return YE_OLDE_FUTURE_PROTEIN;
}

static int64 always_return_0 (bslaw s, bslaw surch)
{
  return 0;
}

#define Free_Ptr(ptr)                                                          \
  do                                                                           \
    {                                                                          \
      if (ptr)                                                                 \
        {                                                                      \
          free (ptr);                                                          \
          (ptr) = NULL;                                                        \
        }                                                                      \
    }                                                                          \
  while (0)

static pool_fetch_op convert_slaw_to_fetch_op (bslaw s)
{
  pool_fetch_op op;
  OB_CLEAR (op);
  op.idx = slaw_path_get_int64 (s, "idx", -1);
  op.want_descrips = slaw_path_get_bool (s, "des", false);
  op.want_ingests = slaw_path_get_bool (s, "ing", false);
  op.rude_offset = slaw_path_get_int64 (s, "roff", -1);
  op.rude_length = slaw_path_get_int64 (s, "rbytes", -1);
  return op;
}

static void convert_slaw_to_fetch_ops (bslaw s, pool_fetch_op *ops, int64 nops)
{
  int64 i;
  bslaw el = NULL;
  for (i = 0; i < nops; i++)
    ops[i] = convert_slaw_to_fetch_op (el = slaw_list_emit_next (s, el));
}

static slaw convert_fetch_op_to_slaw (pool_fetch_op ops)
{
  // XXX: make some of these conditional as described in pool-tcp-protocol.txt?
  return slaw_map_inline_cf (
    "idx", slaw_int64 (ops.idx), "retort", slaw_int64 (ops.tort), "time",
    slaw_float64 (ops.ts), "tbytes", slaw_int64 (ops.total_bytes), "dbytes",
    slaw_int64 (ops.descrip_bytes), "ibytes", slaw_int64 (ops.ingest_bytes),
    "rbytes", slaw_int64 (ops.rude_bytes), "ndes",
    slaw_int64 (ops.num_descrips), "ning", slaw_int64 (ops.num_ingests), "prot",
    ops.p, NULL);
}

static slaw convert_fetch_ops_to_slaw (const pool_fetch_op *ops, int64 nops)
{
  int64 i;
  slabu *sb = slabu_new ();
  for (i = 0; sb && i < nops; i++)
    slabu_list_add_x (sb, convert_fetch_op_to_slaw (ops[i]));
  return slaw_list_f (sb);
}

#define SUPREME_BADNESS(what, err, e)                                          \
  do                                                                           \
    {                                                                          \
      grumpy = (what);                                                         \
      badness_pret = err;                                                      \
      badness_file = __FILE__;                                                 \
      badness_line = __LINE__;                                                 \
      badness_errno = e;                                                       \
      goto badness;                                                            \
    }                                                                          \
  while (0)

/// Once a connection has been established, process commands coming
/// from the client.
///
/// The command processing loop in the server is a very simple state
/// machine.  The first state is one of create, participate, or
/// dispose.  For create and dispose, the connection ends immediately
/// after the operation completes.  For a participate, we go into a
/// loop processing "on-going" commands.  We exit out of the loop when
/// we get a withdraw command or the connection breaks down.

// We attempt to be somewhat resilient against bad things happening,
// but we really need some sort of safe protein sanity-check before
// unpacking the operation if we want to be safe against malicious
// clients.  In particular, if we get an unknown or unexpected
// command, we have to terminate the connection because we don't know
// what the remote end is expecting to get back as a result.  It's
// possible to write the recv_result so that it can deal with the case
// of unexpected return values but it doesn't currently.

static void tend_pool_hose (pool_net_data *net, const char *remote_host)
{
  /// All operations involve two ob_retort values: one recording the
  /// result of the actual operation, and one recording the result of
  /// the attempt to send or receive data.  We must check both.
  ob_retort pret;
  ob_retort send_pret;
  ob_retort recv_pret;

  int op_num;
  protein op_protein;
  char *poolName = NULL;
  pool_hose ph = NULL;
  slaw hose_name = NULL;

  // Variables needed for incoming requests
  protein create_options;
  protein participate_options;
  protein deposit;
  slaw search;
  int64 idx;
  pool_timestamp timeout;
  char *type;
  // Variables needed for returning the results
  int64 ret_index;
  protein ret_prot;
  pool_timestamp ret_ts;

  // Variables used by SUPREME_BADNESS macro
  const char *grumpy;      // description of where error occurred
  ob_retort badness_pret;  // error that occurred
  int badness_errno;       // copy of errno if we need it
  const char *badness_file;
  int badness_line;
  const char *msg;

  // Variables used in the main while loop:
  bool exit_cmd_loop;
  int count;

  // Has STARTTLS been executed yet?
  bool enabled_tls = false;

  // Variables used for statistics
  char *remote_hname = NULL;
  char *remote_process = NULL;
  int64 remote_pid = -1;
  int64 op_count[NUM_CMDS];
  OB_CLEAR (op_count);

  ob_log (OBLV_DBUG, 0x20109000, "reading first command\n");

#ifdef DROP_SUPPORT_FOR_SLAW_V1
  // Don't even try to negotiate.
  // This is inflexible, but useful during creduce.
  net->net_version = POOL_TCP_VERSION_CURRENT;
  net->slaw_version = SLAW_VERSION_CURRENT;
#else
  // Start out the connection using protocol verion 0 (which
  // implies slaw version 1) and then possibly negotiate a
  // higher version from there.
  net->net_version = 0;
  net->slaw_version = 1;
#endif

first_command:
#ifndef _MSC_VER /* for now I don't want to mess with this on Windows */
  ob_nop ();     /* can't have declaration immediately after label :( */
  /* This is the hack mentioned in bug 10657 comment 8 */
  ob_select2_t os2;
  pret =
    ob_select2_prepare (&os2, OB_SEL2_RECEIVE, net->connfd, OB_NULL_HANDLE);
  if (pret < OB_OK)
    SUPREME_BADNESS ("ob_select2_prepare", pret, errno);
  pret = ob_select2 (&os2, 15.0, false);
  const int foo = errno;
  ob_select2_finish (&os2);
  if (pret < OB_OK)
    SUPREME_BADNESS ("ob_select2", pret, foo);
#endif
  pret = pool_net_recv_op (net, &op_num, &op_protein);
  if (pret != OB_OK)
    {
      int e = errno;
      OB_LOG_ERROR_CODE (0x20109028, "bad first read\n");
      SUPREME_BADNESS ("pool_net_recv_op", pret, e);
    }

  // The first command has to be create, dispose, rename, list,
  // starttls, sleep, or participate - all others are not allowed.
  if (op_num == POOL_CMD_STARTTLS)
    {
      slaw ignore_slaw, empty_map;
      OB_LOG_DEBUG_CODE (0x20109061, "starttls\n");
      pret =
        pool_net_unpack_op_f (op_protein, net->net_version, "x", &ignore_slaw);
      const int e1 = errno;
      Free_Slaw (ignore_slaw);
      if (pret < OB_OK)
        SUPREME_BADNESS ("POOL_CMD_STARTTLS: pool_net_unpack_op", pret, e1);
      empty_map = slaw_map_f (slabu_new ());
      send_pret = pool_net_send_result (net, "rx", tls_available, empty_map);
      const int e2 = errno;
      Free_Slaw (empty_map);
      if (send_pret < OB_OK)
        SUPREME_BADNESS ("POOL_CMD_STARTTLS: pool_net_send_result", send_pret,
                         e2);
      if (tls_available >= OB_OK)
        {
          int pair[2];
          OB_DIE_ON_ERROR (
            ob_socketpair_cloexec (OB_SP_DOMAIN, SOCK_STREAM, 0, pair));
          OB_DIE_ON_ERROR (ob_nosigpipe_sockopt_x2 (pair));
          OB_DIE_ON_ERROR (ob_tls_server_launch_thread (
            pair[0], net->connfd, &net->tls_thread, !require_tls, client_auth));
          net->connfd = pair[1];
          send_pret = welcome_new_version_tls (net);
          const int e = errno;
          if (send_pret < OB_OK)
            SUPREME_BADNESS ("re-negotiating protocol version", send_pret, e);
          enabled_tls = true;
        }
      goto first_command;
    }

  if (op_num == POOL_CMD_DISPOSE)
    {
      pret = pool_net_unpack_op (op_protein, net->net_version, "s", &poolName);
      if (pret != OB_OK)
        SUPREME_BADNESS ("POOL_CMD_DISPOSE: pool_net_unpack_op", pret, errno);
      const unt8 *rude_ptr;
      int64 rude_len;
      if (net->net_version == 0 && net->slaw_version == 1
          && strcmp (poolName, "^/^/^/^") == 0
          && (rude_ptr = (const unt8 *) protein_rude (op_protein, &rude_len))
               != NULL
          && rude_len == 12)
        {
          // This is the special "escape hatch" for negotiating a
          // different protocol version.  You try to delete a pool
          // named "^/^/^/^" (which is illegal) and include the new
          // version in the rude data.
          net->net_version = rude_ptr[0];
          net->slaw_version = rude_ptr[1];
          protein_free (op_protein);
          send_pret = welcome_new_version (
            net, (allow_tls && (tls_available >= OB_OK)), !require_tls);
          int e = errno;
          if (send_pret != OB_OK)
            SUPREME_BADNESS ("negotiating protocol version", send_pret, e);
          Free_Ptr (poolName);
          // Don't close connection; allow them to set the version and then
          // do something else on same hose.
          goto first_command;
        }
      else if (require_tls && !enabled_tls)
        goto bad_start;
      protein_free (op_protein);
      ob_log (OBLV_DBUG, 0x20109002, "%s: dispose\n", poolName);
      pret = pool_dispose (poolName);
      send_pret = pool_net_send_result (net, "r", pret);
      int e = errno;
      if (send_pret != OB_OK)
        SUPREME_BADNESS ("POOL_CMD_DISPOSE: pool_net_send_result", send_pret,
                         e);
      Free_Ptr (poolName);
      return;
    }

  if (require_tls && !enabled_tls)
    goto bad_start;

  if (op_num == POOL_CMD_CREATE)
    {
      pret = pool_net_unpack_op_f (op_protein, net->net_version, "ssp",
                                   &poolName, &type, &create_options);
      if (pret != OB_OK)
        SUPREME_BADNESS ("POOL_CMD_CREATE: pool_net_unpack_op", pret, errno);
      ob_log (OBLV_DBUG, 0x20109001, "%s: create\n", poolName);
      pret = pool_create (poolName, type, create_options);
      send_pret = pool_net_send_result (net, "r", pret);
      int e = errno;
      free (type);
      protein_free (create_options);
      // Terminate the connection by returning from the function
      if (send_pret != OB_OK)
        SUPREME_BADNESS ("POOL_CMD_CREATE: pool_net_send_result", send_pret, e);
      Free_Ptr (poolName);
      return;
    }

  if (op_num == POOL_CMD_SLEEP)
    {
      pret = pool_net_unpack_op (op_protein, net->net_version, "s", &poolName);
      if (pret != OB_OK)
        SUPREME_BADNESS ("POOL_CMD_SLEEP: pool_net_unpack_op", pret, errno);
      Free_Protein (op_protein);
      OB_LOG_DEBUG_CODE (0x20109055, "%s: dispose\n", poolName);
      pret = pool_sleep (poolName);
      send_pret = pool_net_send_result (net, "r", pret);
      const int e = errno;
      if (send_pret != OB_OK)
        SUPREME_BADNESS ("POOL_CMD_SLEEP: pool_net_send_result", send_pret, e);
      Free_Ptr (poolName);
      return;
    }

  if (op_num == POOL_CMD_RENAME)
    {
      char *new_name = NULL;
      pret = pool_net_unpack_op_f (op_protein, net->net_version, "ss",
                                   &poolName, &new_name);
      if (pret != OB_OK)
        SUPREME_BADNESS ("POOL_CMD_RENAME: pool_net_unpack_op", pret, errno);
      OB_LOG_DEBUG_CODE (0x2010904b, "%s to %s: rename\n", poolName, new_name);
      pret = pool_rename (poolName, new_name);
      send_pret = pool_net_send_result (net, "r", pret);
      const int e = errno;
      Free_Ptr (new_name);
      // Terminate the connection by returning from the function
      if (send_pret != OB_OK)
        SUPREME_BADNESS ("POOL_CMD_RENAME: pool_net_send_result", send_pret, e);
      Free_Ptr (poolName);
      return;
    }

  if (op_num == POOL_CMD_LIST)
    {
      ob_log (OBLV_DBUG, 0x20109003, "list\n");
      protein_free (op_protein);
      slaw lst = NULL;
      pret = pool_list (&lst);
      send_pret = pool_net_send_result (net, "rx", pret, lst);
      int e = errno;
      slaw_free (lst);
      if (send_pret != OB_OK)
        SUPREME_BADNESS ("POOL_CMD_LIST: pool_net_send_result", send_pret, e);
      return;
    }

  if (op_num == POOL_CMD_LIST_EX)
    {
      slaw lst = NULL;
      pret =
        pool_net_unpack_op_f (op_protein, net->net_version, "s", &poolName);
      if (pret < OB_OK)
        SUPREME_BADNESS ("POOL_CMD_LIST_EX: pool_net_unpack_op", pret, errno);
      OB_LOG_DEBUG_CODE (0x2010905d, "%s: list_ex\n", poolName);
      pret = pool_list_ex (poolName, &lst);
      send_pret = pool_net_send_result (net, "rx", pret, lst);
      int e = errno;
      slaw_free (lst);
      if (send_pret != OB_OK)
        SUPREME_BADNESS ("POOL_CMD_LIST_EX: pool_net_send_result", send_pret,
                         e);
      Free_Ptr (poolName);
      return;
    }

  // Everything from here on out starts a connection and goes into the
  // command processing loop.
  if (op_num == POOL_CMD_PARTICIPATE)
    {
      // Starting a new connection!  How fun!
      pret = pool_net_unpack_op_f (op_protein, net->net_version, "sp",
                                   &poolName, &participate_options);
      if (pret != OB_OK)
        SUPREME_BADNESS ("POOL_CMD_PARTICIPATE: pool_net_unpack_op", pret,
                         errno);
      ob_log (OBLV_DBUG, 0x20109004, "%s: participate\n", poolName);
      pret = pool_participate (poolName, &ph, participate_options);
      int e1 = errno;
      send_pret = pool_net_send_result (net, "r", pret);
      int e2 = errno;
      protein_free (participate_options);
      if (pret != OB_OK)
        {
          SUPREME_BADNESS ("POOL_CMD_PARTICIPATE: pool_participate", pret, e1);
        }
      if (send_pret != OB_OK)
        {
          pool_withdraw (ph);
          SUPREME_BADNESS ("POOL_CMD_PARTICIPATE: pool_net_send_result",
                           send_pret, e2);
        }
    }
  else if (op_num == POOL_CMD_PARTICIPATE_CREATINGLY)
    {
      pret =
        pool_net_unpack_op_f (op_protein, net->net_version, "sspp", &poolName,
                              &type, &create_options, &participate_options);
      // (participate_options are vestigial and are ignored)
      if (pret != OB_OK)
        SUPREME_BADNESS ("POOL_CMD_PARTICIPATE_CREATINGLY: "
                         "pool_net_unpack_op",
                         pret, errno);
      ob_log (OBLV_DBUG, 0x20109005, "%s: participate_creatingly\n", poolName);
      ob_retort cret =
        pool_participate_creatingly (poolName, type, &ph, create_options);
      pret = cret;
      if (net->net_version < POOL_TCP_VERSION_WITH_NEW_PCREATINGLY_CODES)
        {
          if (pret == OB_OK)
            pret = POOL_EXISTS;
          else if (pret == POOL_CREATED)
            pret = OB_OK;
        }
      int e1 = errno;
      send_pret = pool_net_send_result (net, "r", pret);
      int e2 = errno;
      free (type);
      protein_free (participate_options);
      protein_free (create_options);
      if (cret != OB_OK && cret != POOL_CREATED)
        {
          SUPREME_BADNESS ("POOL_CMD_PARTICIPATE_CREATINGLY: "
                           "pool_participate_creatingly",
                           cret, e1);
        }
      if (send_pret != OB_OK)
        {
          pool_withdraw (ph);
          SUPREME_BADNESS ("POOL_CMD_PARTICIPATE_CREATINGLY: "
                           "pool_net_send_result",
                           send_pret, e2);
        }
    }
  else
    {
// No other commands are valid.
bad_start:
      OB_LOG_ERROR_CODE (0x20109029, "bad starting op %d\n", op_num);
      protein_free (op_protein);
      return;
    }

  ob_log (OBLV_DBUG, 0x20109006, "%s: command processing\n", poolName);

  hose_name = slaw_string_format ("%s@%s", poolName, remote_host);
  pool_set_hose_name (ph, slaw_string_emit (hose_name));

  // Now that we have a connection and a valid pool struct, handle
  // on-going commands.
  exit_cmd_loop = false;
  count = 0;
  while (OB_LOG_DEBUG_CODE (0x2010903d, "waiting - %s\n",
                            slaw_string_emit (hose_name)),
         set_my_name (slaw_string_emit (hose_name)),
         (recv_pret = pool_net_recv_op (net, &op_num, &op_protein)) == OB_OK)
    {
      ret_prot = NULL;
      ret_index = -1;
      ret_ts = -1;
      op_count[op_num]++;
      switch (op_num)
        {
          case POOL_CMD_SET_HOSE_NAME:
            set_my_name ("SET_HOSE_NAME");
            ob_log (OBLV_DBUG, 0x20109007, "%s: set hose name\n", poolName);
            Free_Ptr (remote_hname);
            Free_Ptr (remote_process);
            Free_Slaw (hose_name);
            pret = pool_net_unpack_op_f (op_protein, net->net_version, "ssi",
                                         &remote_hname, &remote_process,
                                         &remote_pid);
            if (pret >= OB_OK)
              {
                hose_name =
                  slaw_string_format ("%s@%s/%" OB_FMT_64 "d/%s", remote_hname,
                                      remote_process, remote_pid, remote_host);
                pool_set_hose_name (ph, slaw_string_emit (hose_name));
                slaw tmp = slaw_string_format ("%" OB_FMT_64 "d/%s", remote_pid,
                                               remote_host);
                set_client_info (slaw_string_emit (tmp));
                slaw_free (tmp);
              }
            // Note: there is no response to this command; none is needed.
            break;
          case POOL_CMD_DEPOSIT:
            set_my_name ("DEPOSIT");
            ob_log (OBLV_DBUG, 0x20109008, "%s: deposit %d\n", poolName,
                    count++);
            pret = pool_net_unpack_op_f (op_protein, net->net_version, "p",
                                         &deposit);
            if (pret < OB_OK)
              deposit = NULL;
            else
              pret = pool_deposit_ex (ph, deposit, &ret_index, &ret_ts);
            send_pret =
              pool_net_send_result (net, "irt", ret_index, pret, ret_ts);
            protein_free (deposit);
            break;
          case POOL_CMD_NTH_PROTEIN:
            set_my_name ("NTH_PROTEIN");
            ob_log (OBLV_DBUG, 0x20109009, "%s: nth protein\n", poolName);
            pret =
              pool_net_unpack_op_f (op_protein, net->net_version, "i", &idx);
            if (pret >= OB_OK)
              pret = pool_nth_protein (ph, idx, &ret_prot, &ret_ts);
            if (net->net_version < POOL_TCP_VERSION_WITH_NEW_PCREATINGLY_CODES
                && pret == POOL_NO_SUCH_PROTEIN)
              send_pret =
                pool_net_send_result (net, "ptR", ret_prot, ret_ts,
                                      ye_olde_no_protein_retorte (ph, idx));
            else
              send_pret =
                pool_net_send_result (net, "ptr", ret_prot, ret_ts, pret);
            protein_free (ret_prot);
            break;
          case POOL_CMD_NEXT:
            set_my_name ("NEXT");
            pret =
              pool_net_unpack_op_f (op_protein, net->net_version, "i", &idx);
            ob_log (OBLV_DBUG, 0x2010900a, "%s: next\n", poolName);
            if (pret < OB_OK)
              search = NULL;
            else
              pret = pool_seekto (ph, idx);
            if (pret >= OB_OK)
              pret = pool_next (ph, &ret_prot, &ret_ts, &ret_index);
            if (net->net_version < POOL_TCP_VERSION_WITH_NEW_PCREATINGLY_CODES
                && pret == POOL_NO_SUCH_PROTEIN)
              send_pret =
                pool_net_send_result (net, "ptiR", ret_prot, ret_ts, ret_index,
                                      ye_olde_no_protein_retorte (ph, idx));
            else
              send_pret = pool_net_send_result (net, "ptir", ret_prot, ret_ts,
                                                ret_index, pret);
            protein_free (ret_prot);
            break;
          case POOL_CMD_PREV:
            set_my_name ("PREV");
            pret =
              pool_net_unpack_op_f (op_protein, net->net_version, "i", &idx);
            ob_log (OBLV_DBUG, 0x2010900b, "%s: prev\n", poolName);
            if (pret < OB_OK)
              search = NULL;
            else
              pret = pool_seekto (ph, idx);
            if (pret >= OB_OK)
              pret = pool_prev (ph, &ret_prot, &ret_ts, &ret_index);
            // XXX: Technically, this can't happen, since POOL_CMD_PREV was
            // added after the retorts were changed.  But, theoretically,
            // the protocol version and the supported commands are
            // orthogonal.
            if (net->net_version < POOL_TCP_VERSION_WITH_NEW_PCREATINGLY_CODES
                && pret == POOL_NO_SUCH_PROTEIN)
              send_pret =
                pool_net_send_result (net, "ptiR", ret_prot, ret_ts, ret_index,
                                      ye_olde_no_protein_retorte (ph, idx));
            else
              send_pret = pool_net_send_result (net, "ptir", ret_prot, ret_ts,
                                                ret_index, pret);
            protein_free (ret_prot);
            break;
          case POOL_CMD_PROBE_FRWD:
            set_my_name ("PROBE_FRWD");
            pret = pool_net_unpack_op_f (op_protein, net->net_version, "ix",
                                         &idx, &search);
            ob_log (OBLV_DBUG, 0x2010900c, "%s: probe_frwd\n", poolName);
            if (pret < OB_OK)
              search = NULL;
            else
              pret = pool_seekto (ph, idx);
            if (pret >= OB_OK)
              pret =
                pool_probe_frwd (ph, search, &ret_prot, &ret_ts, &ret_index);
            slaw_free (search);
            send_pret = pool_net_send_result (net, "ptir", ret_prot, ret_ts,
                                              ret_index, pret);
            protein_free (ret_prot);
            break;
          case POOL_CMD_PROBE_BACK:
            set_my_name ("PROBE_BACK");
            pret = pool_net_unpack_op_f (op_protein, net->net_version, "ix",
                                         &idx, &search);
            ob_log (OBLV_DBUG, 0x2010900d, "%s: probe_back\n", poolName);
            if (pret < OB_OK)
              search = NULL;
            else
              pret = pool_seekto (ph, idx);
            if (pret >= OB_OK)
              pret =
                pool_probe_back (ph, search, &ret_prot, &ret_ts, &ret_index);
            slaw_free (search);
            send_pret = pool_net_send_result (net, "ptir", ret_prot, ret_ts,
                                              ret_index, pret);
            protein_free (ret_prot);
            break;
          case POOL_CMD_NEWEST_INDEX:
            set_my_name ("NEWEST_INDEX");
            ob_log (OBLV_DBUG, 0x2010900e, "%s: newest index\n", poolName);
            // No need to unpack the op - we already know everything - but
            // do need to free it (do this after we reply for efficiency).
            pret = pool_newest_index (ph, &ret_index);
            send_pret = pool_net_send_result (net, "ir", ret_index, pret);
            protein_free (op_protein);
            break;
          case POOL_CMD_OLDEST_INDEX:
            set_my_name ("OLDEST_INDEX");
            ob_log (OBLV_DBUG, 0x2010900f, "%s: oldest index\n", poolName);
            pret = pool_oldest_index (ph, &ret_index);
            send_pret = pool_net_send_result (net, "ir", ret_index, pret);
            protein_free (op_protein);
            break;
          case POOL_CMD_INDEX_LOOKUP:
            set_my_name ("INDEX_LOOKUP");
            ob_log (OBLV_DBUG, 0x20109010, "%s: index lookup\n", poolName);
            {
              int64 cmp = -1;
              int64 rel = 0;
              pret = pool_net_unpack_op_f (op_protein, net->net_version, "tii",
                                           &ret_ts, &rel, &cmp);
              if (pret < OB_OK)
                ob_nop ();  // do nothing and reply with bad pret below
              else if (cmp < OB_CLOSEST || cmp > OB_CLOSEST_HIGHER)
                pret = POOL_UNSUPPORTED_OPERATION;
              else
                {
                  if (rel < 0)
                    pret = pool_seekto_time (ph, ret_ts, (time_comparison) cmp);
                  else
                    {
                      pret = pool_seekto (ph, rel);
                      if (OB_OK == pret)
                        pret =
                          pool_seekby_time (ph, ret_ts, (time_comparison) cmp);
                    }
                  if (OB_OK == pret)
                    pret = pool_index (ph, &ret_index);
                }
              send_pret = pool_net_send_result (net, "ir", ret_index, pret);
            }
            break;
          case POOL_CMD_AWAIT_NEXT_SINGLE:
            set_my_name ("AWAIT_NEXT_SINGLE");
            ob_log (OBLV_DBUG, 0x20109011, "%s: await next\n", poolName);
            pret = pool_net_unpack_op_f (op_protein, net->net_version, "t",
                                         &timeout);
            if (pret >= OB_OK)
              {
                pool_net_adjust_timeout_value_for_version (&timeout,
                                                           net->net_version);
                pret =
                  pool_await_next (ph, timeout, &ret_prot, &ret_ts, &ret_index);
              }
            send_pret = pool_net_send_result (net, "rpti", pret, ret_prot,
                                              ret_ts, ret_index);
            protein_free (ret_prot);
            break;
          case POOL_CMD_MULTI_ADD_AWAITER:
            set_my_name ("MULTI_ADD_AWAITER");
            ob_log (OBLV_DBUG, 0x20109012, "%s: add_awaiter\n", poolName);
            pret = pool_net_server_await (ph, net, POOL_WAIT_FOREVER, &ret_prot,
                                          &ret_ts, &ret_index);
            send_pret = pool_net_send_result (net, "rpti", pret, ret_prot,
                                              ret_ts, ret_index);
            protein_free (ret_prot);
            protein_free (op_protein);
            break;
          case POOL_CMD_FANCY_ADD_AWAITER:
            set_my_name ("FANCY_ADD_AWAITER");
            pret = pool_net_unpack_op_f (op_protein, net->net_version, "ix",
                                         &idx, &search);
            ob_log (OBLV_DBUG, 0x2010902b,
                    "%s: fancy_add_awaiter (%" OB_FMT_64 "d)\n", poolName, idx);
            if (pret < OB_OK)
              send_pret = pool_net_send_op (net, POOL_CMD_FANCY_RESULT_1, "rti",
                                            pret, ret_ts, ret_index);
            else
              {
                int64 newest = -1;
                pool_seekto (ph, idx);
                // _pool_net_unpack_op() converts nil to NULL, which IMO is a
                // terrible idea (bug 480), but to be safest I'll just check
                // for either one.
                bool unconditional = (!search || slaw_is_nil (search));
                pret = pool_newest_index (ph, &newest);
                if (pret < OB_OK)
                  newest = idx;
                if (unconditional)
                  pret = pool_next (ph, &ret_prot, &ret_ts, &ret_index);
                else
                  pret = pool_probe_frwd (ph, search, &ret_prot, &ret_ts,
                                          &ret_index);
                send_pret = pool_net_send_op (net, POOL_CMD_FANCY_RESULT_1,
                                              "rti", pret, ret_ts, ret_index);
                OB_LOG_DEBUG_CODE (0x20109041,
                                   "POOL_CMD_FANCY_RESULT_1 (%s, "
                                   "%f, %" OB_FMT_64 "d)\n",
                                   ob_error_string (pret), ret_ts, ret_index);
                if (pret >= OB_OK && send_pret >= OB_OK)
                  {
                    send_pret =
                      pool_net_send_op (net, POOL_CMD_FANCY_RESULT_3, "tip",
                                        ret_ts, ret_index, ret_prot);
                    OB_LOG_DEBUG_CODE (0x20109042,
                                       "POOL_CMD_FANCY_RESULT_3 "
                                       "(%f, %" OB_FMT_64 "d)\n",
                                       ret_ts, ret_index);
                    protein_free (ret_prot);
                  }
                else if (pret == POOL_NO_SUCH_PROTEIN && send_pret >= OB_OK)
                  {
                    int64 (*func) (bslaw s, bslaw surch);
                    int64 thingy;
                    func = (unconditional ? always_return_0 : protein_search);
                    pool_seekto (ph, (thingy = (idx > newest ? idx : newest)));
                    OB_LOG_DEBUG_CODE (
                      0x20109045, "set index to %" OB_FMT_64 "d\n", thingy);
                    do
                      {
                        protein_free (ret_prot);
                        OB_LOG_DEBUG_CODE (0x2010903b, "starting await\n");
                        pret = pool_net_server_await (
                          ph, net, POOL_WAIT_FOREVER, &ret_prot, &ret_ts,
                          &ret_index);
                        OB_LOG_DEBUG_CODE (0x2010903c, "finished await\n");
                      }
                    while (pret == OB_OK && func (ret_prot, search) < 0);
                    send_pret =
                      pool_net_send_op (net, POOL_CMD_FANCY_RESULT_2, "rti",
                                        pret, ret_ts, ret_index);
                    OB_LOG_DEBUG_CODE (0x20109043,
                                       "POOL_CMD_FANCY_RESULT_2 (%s, %f, "
                                       "%" OB_FMT_64 "d)\n",
                                       ob_error_string (pret), ret_ts,
                                       ret_index);
                    if (pret >= OB_OK && send_pret >= OB_OK)
                      {
                        send_pret =
                          pool_net_send_op (net, POOL_CMD_FANCY_RESULT_3, "tip",
                                            ret_ts, ret_index, ret_prot);
                        OB_LOG_DEBUG_CODE (0x20109044,
                                           "POOL_CMD_FANCY_RESULT_3 (%f, "
                                           "%" OB_FMT_64 "d)\n",
                                           ret_ts, ret_index);
                      }
                    protein_free (ret_prot);
                  }
              }
            Free_Slaw (search);
            break;
          case POOL_CMD_WITHDRAW:
            set_my_name ("WITHDRAW");
            ob_log (OBLV_DBUG, 0x20109013, "%s: withdraw\n", poolName);
            protein_free (op_protein);
            exit_cmd_loop = true;
            break;
          case POOL_CMD_INFO:
            set_my_name ("INFO");
            {
              int64 hops;
              ob_log (OBLV_DBUG, 0x20109014, "%s: info\n", poolName);
              pret =
                pool_net_unpack_op_f (op_protein, net->net_version, "i", &hops);
              if (pret == OB_OK)
                pret = pool_get_info (ph, hops, &ret_prot);
              send_pret = pool_net_send_result (net, "rp", pret, ret_prot);
              protein_free (ret_prot);
              break;
            }
          case POOL_CMD_SUB_FETCH:
            set_my_name ("SUB_FETCH");
            ob_log (OBLV_DBUG, 0x20109040, "%s: sub fetch\n", poolName);
            {
              slaw s = NULL;
              pret =
                pool_net_unpack_op_f (op_protein, net->net_version, "x", &s);
              int64 oldest = pret, newest = pret, nops;
              if (pret >= OB_OK && (nops = slaw_list_count (s)) >= 0)
                {
                  pool_fetch_op *ops =
                    (pool_fetch_op *) calloc (nops, sizeof (pool_fetch_op));
                  convert_slaw_to_fetch_ops (s, ops, nops);
                  Free_Slaw (s);
                  pool_fetch (ph, ops, nops, &oldest, &newest);
                  s = convert_fetch_ops_to_slaw (ops, nops);
                  Free_Ptr (ops);
                }
              else
                {
                  ob_err_accum (&oldest, OB_UNKNOWN_ERR);
                  ob_err_accum (&newest, OB_UNKNOWN_ERR);
                }
              send_pret = pool_net_send_result (net, "xii", s, oldest, newest);
              Free_Slaw (s);
            }
            break;
          case POOL_CMD_SUB_FETCH_EX:
            set_my_name ("SUB_FETCH_EX");
            ob_log (OBLV_DBUG, 0x20109060, "%s: sub fetch ex\n", poolName);
            {
              slaw s = NULL;
              int64 clamp64;
              pret = pool_net_unpack_op_f (op_protein, net->net_version, "xi",
                                           &s, &clamp64);
              int64 oldest = pret, newest = pret, nops;
              if (pret >= OB_OK && (nops = slaw_list_count (s)) >= 0)
                {
                  pool_fetch_op *ops =
                    (pool_fetch_op *) calloc (nops, sizeof (pool_fetch_op));
                  convert_slaw_to_fetch_ops (s, ops, nops);
                  Free_Slaw (s);
                  bool clamp = (clamp64 != 0);
                  pool_fetch_ex (ph, ops, nops, &oldest, &newest, clamp);
                  s = convert_fetch_ops_to_slaw (ops, nops);
                  Free_Ptr (ops);
                }
              else
                {
                  ob_err_accum (&oldest, OB_UNKNOWN_ERR);
                  ob_err_accum (&newest, OB_UNKNOWN_ERR);
                }
              send_pret = pool_net_send_result (net, "xii", s, oldest, newest);
              Free_Slaw (s);
            }
            break;
          case POOL_CMD_ADVANCE_OLDEST:
            set_my_name ("ADVANCE_OLDEST");
            OB_LOG_DEBUG_CODE (0x20109054, "%s: advance oldest\n", poolName);
            pret =
              pool_net_unpack_op_f (op_protein, net->net_version, "i", &idx);
            if (pret >= OB_OK)
              pret = pool_advance_oldest (ph, idx);
            send_pret = pool_net_send_result (net, "r", pret);
            break;
          case POOL_CMD_CHANGE_OPTIONS:
            {
              protein opts = NULL;
              set_my_name ("CHANGE_OPTIONS");
              OB_LOG_DEBUG_CODE (0x20109056, "%s: change_options\n", poolName);
              pret =
                pool_net_unpack_op_f (op_protein, net->net_version, "p", &opts);
              if (pret >= OB_OK)
                pret = pool_change_options (ph, opts);
              send_pret = pool_net_send_result (net, "r", pret);
              Free_Protein (opts);
            }
            break;
          default:
            // Unknown command.  We can't send a result because we don't
            // know what the remote end is expecting, so close the
            // connection - it'll get the message across.
            OB_LOG_ERROR_CODE (0x2010902a, "Unknown command %d\n", op_num);
            protein_free (op_protein);
            Free_Ptr (poolName);
            Free_Slaw (hose_name);
            return;
        }
      // Do we want to close the connection?
      if ((exit_cmd_loop) || (send_pret != OB_OK))
        break;
    }

  if (recv_pret != OB_OK)
    {
      const int erryes = errno;
      pool_withdraw (ph);
      SUPREME_BADNESS ("pool_net_recv_op", recv_pret, erryes);
    }

  print_op_stats (remote_hname ? remote_hname : poolName,
                  remote_process ? remote_process : remote_host, op_count);

  Free_Ptr (remote_hname);
  Free_Ptr (remote_process);

  if (send_pret != OB_OK)
    ob_log (OBLV_DBUG, 0x20109015, "%s: send_pret %s\n", poolName,
            ob_error_string (send_pret));
  // At this point, only withdraw is allowed in normal operation.  A
  // connection ending without a withdraw happens in particular if a
  // process forgets to withdraw from a pool or is killed before it
  // can withdraw.
  if (op_num != POOL_CMD_WITHDRAW)
    ob_log (OBLV_DBUG, 0x20109016, "Connection didn't end with a withdraw!\n");
  pret = pool_withdraw (ph);
  // Only send the result if the remote end is expecting it
  if (op_num == POOL_CMD_WITHDRAW)
    ob_ignore_retort (pool_net_send_result (net, "r", pret));
  // Terminate the connection by returning from the function
  ob_log (OBLV_DBUG, 0x20109017, "%s: exiting\n", poolName);
  Free_Ptr (poolName);
  Free_Slaw (hose_name);
  return;

badness:
  errno = badness_errno;
  msg = ob_error_string (badness_pret);
  slaw extra;
  if (badness_pret == POOL_RECV_BADTH)
    // POOL_RECV_BADTH was probably caused by a system error,
    // so tack on errno for extra information.
    // XXX: but sometimes this is misleading.
    extra = slaw_string_format (" (%s)", strerror (badness_errno));
  else if (badness_pret == ob_errno_to_retort (EACCES))
    extra = slaw_string_format (", when OB_POOLS_DIR=%s",
                                ob_get_standard_path (ob_pools_dir));
  else
    // Otherwise, there is no extra information.
    extra = slaw_string ("");
  ob_log_loc (badness_file, badness_line, OBLV_ERROR, 0,
              "[pool %s] %s encountered %s%s\n",
              (poolName ? poolName : "<unknown>"), grumpy, msg,
              slaw_string_emit (extra));
  Free_Ptr (poolName);
  slaw_free (extra);
  Free_Ptr (remote_hname);
  Free_Ptr (remote_process);
  Free_Slaw (hose_name);
}

/* Little wrapper function to ensure cleanup, since tend_pool_hose()
 * has many return paths. */
static void tend_pool_hose_tls (pool_net_data net, const char *remote_host)
{
  tend_pool_hose (&net, remote_host);
#ifndef _MSC_VER
  OB_CHECK_POSIX_CODE (0x20109048, close (net.connfd));
#endif
  if (net.tls_thread)
    // XXX: assumes 0 is not a valid pthread_t (which is true on Linux
    // and OS X, but may not be true in the general case)
    OB_DIE_ON_ERROR (ob_tls_server_join_thread (net.tls_thread));
}

#ifdef _MSC_VER

// The Windows pool server does not fork-off child processes, it
// loops waiting for new connections, then when one is accepted, it
// spawns a thread to handle the connection. When that connection is
// completed, the thread exits.
//

//connection_thread_entry is where the spawned threads begin execution

//args is an int that contains the freshly connected socket descriptor
unsigned int __stdcall connection_thread_entry (void *args)
{
  int connfd = (int) args;

  pool_net_data net;
  memset (&net, 0, sizeof (net));
  net.connfd = connfd;
  net.send_nbytes = pool_tcp_send_nbytes;
  net.recv_nbytes = pool_tcp_recv_nbytes;

  HANDLE null_handle = NULL;
  net.wakeup_handle_loc = &null_handle;

  // XXX: TODO: get remote hostname on Windows
  tend_pool_hose_tls (net, "unknown");

  //close the socket
  ob_log (OBLV_DBUG, 0x20109018, "closing connfd\n");

  shutdown (connfd, SD_RECEIVE | SD_SEND);
  closesocket (connfd);

  return 0;
}

/// Loop waiting for connections from clients.
static void wait_for_connection (int listenfd)
{
  // XXX backlog depends on likely number of concurrent pool
  // openers... perhaps should retry in client?  Set to more than 100
  // right now since our stress tests start up to 100 clients at once.
  if (listen (listenfd, 110) < 0)
    {
      OB_PERROR_CODE (0x2010904d, "listen failed");
      OB_LOG_DEBUG_CODE (0x20109030, "calling exit (1);\n");
      exit (1);
    }

  // Now we can finally accept incoming requests!
  while (1)
    {
      // XXX May care about source address for security reasons eventually
      ob_log (OBLV_DBUG, 0x20109019, "accepting...\n");
      int connfd = accept (listenfd, NULL, NULL);
      ob_log (OBLV_DBUG, 0x2010901a, "accepted\n");

      if (connfd == INVALID_SOCKET)
        {
          ob_log (OBLV_DBUG, 0x2010901b,
                  "accept() returned INVALID_SOCKET, continuing\n");
          continue;
        }

      //spawn a new thread to handle the connection
      HANDLE new_handle = (HANDLE) _beginthreadex (
        NULL, 0, connection_thread_entry, (void *) connfd, 0, NULL);
      if (new_handle != 0)
        {
          //thread creation was successful

          //Our reference (new_handle) to the thread must be closed
          //eventually to allow the thread resources to free up.
          //Better to just do that now. This does not end the thread,
          //it just releases the handle returned from _beginthreadex.
          //Once the thread exits, its resources will be freed by the os.
          CloseHandle (new_handle);
        }
      else
        ob_log (OBLV_DBUG, 0x2010901c,
                "failed to spawn thread for connection %d\n", connfd);
    }
}

#else

/// Create a new process to handle operations coming in over this
/// connection.  connfd is the socket attached to the client, listenfd
/// is the socket we listen() on, passed in solely that the child
/// process may close it and gain one more available file descriptor.
///
/// In debug mode, we do not create a separate child process.
/// Instead, the parent handles this connection until it closes.  The
/// server can then only handle one connection at a time.

static void connect_pool_hose (int connfd, int listenfd,
                               const char *remote_host)
{
  // Set up our connection info
  pool_net_data net;
  int negative_one = -1;
  memset (&net, 0, sizeof (net));
  net.connfd = connfd;
  net.send_nbytes = pool_tcp_send_nbytes;
  net.recv_nbytes = pool_tcp_recv_nbytes;
  net.wakeup_handle_loc = &negative_one;
  //net.outstanding_await and awaiter_added are client side only

  // Fork after we set up the connection info so we can send an error
  // back if necessary.
  if (!debug)
    {
      ob_log (OBLV_DBUG, 0x2010901d, "forking...\n");
      pid_t pid = fork ();
      ob_log (OBLV_DBUG, 0x2010901e, "forked (fork returned %d)\n", pid);
      if (pid < 0)
        {
          OB_PERROR_CODE (0x2010904e, "fork");
          // Shut down the connection politely
          // The joy of passing 64-bit values through varargs...
          // stuff POOL_SERVER_BUSY in a variable to make sure
          // the width is right.
          ob_retort busy = POOL_SERVER_BUSY;
          ob_ignore_retort (pool_net_send_result (&net, "r", busy));
          // Remember, we are still the parent here
          return;
        }
      else if (pid == 0)
        {
          // Child - handle requests from remote participator
          OB_CHECK_POSIX_CODE (0x20109047, close (listenfd));
          // We only want to handle SIGINT and SIGTERM in the parent,
          // so restore to default in the child.
          struct sigaction act;
          memset (&act, 0, sizeof (act));
          act.sa_handler = SIG_DFL;
          sigaction (SIGINT, &act, NULL);
          sigaction (SIGTERM, &act, NULL);
          // We must initialize logging *after* we fork, because
          // the logging thread won't live through the fork.
          initialize_logging ();
          tend_pool_hose_tls (net, remote_host);
          //if ( leak_mask & 4 ) { malloc (4); }
          OB_LOG_DEBUG_CODE (0x20109031, "calling exit (0);\n");
          exit (0);
        }
      else
        {
          // Parent
          add_child (pid);
          OB_LOG_DEBUG_CODE (0x20109026, "closing connfd\n");
          OB_CHECK_POSIX_CODE (0x2010904a, close (connfd));
          return;
        }
    }
  else
    {
      // Debug mode, parent handles connection
      tend_pool_hose_tls (net, remote_host);
      return;
    }
}

/// Invoked (indirectly) upon SIGINT or SIGTERM
static void graceful_exit (void)
{
  // First, turn off all our signal handlers
  struct sigaction act;
  memset (&act, 0, sizeof (act));
  act.sa_handler = SIG_DFL;
  sigaction (SIGCHLD, &act, NULL);
  sigaction (SIGPIPE, &act, NULL);
  sigaction (SIGINT, &act, NULL);
  sigaction (SIGTERM, &act, NULL);

  int i;
  for (i = 0; i < cleanup_seconds && children_not_empty (); i++)
    {
      slaw pids = list_children ();
      ob_log (OBLV_INFO, 0x2010901f, "Waiting for children: %s\n",
              slaw_string_emit (pids));
      slaw_free (pids);
      sleep (1);
      reap_children ();
    }

  slaw pids = list_children ();

  if (cleanupfile)
    {
      FILE *f = fopen (cleanupfile, "w");
      if (f)
        {
          fprintf (f, "%s\n", slaw_string_emit (pids));
          fclose (f);
        }
    }

  if (cleanup_seconds >= 0 && children_not_empty ())
    {
      ob_log (OBLV_WRNU, 0x20109020, "Killing children: %s\n",
              slaw_string_emit (pids));
      for (i = 0; i < slabu_count (children_alive); i++)
        {
          pid_t pid = *slaw_int64_emit (slabu_list_nth (children_alive, i));
          OB_CHECK_POSIX_CODE (0x20109049, kill (pid, SIGKILL));
        }
    }

  slaw_free (pids);
  OB_LOG_DEBUG_CODE (0x20109032, "calling exit (EXIT_SUCCESS);\n");
  exit (EXIT_SUCCESS);
}

/// Loop waiting for connections from clients.
static void wait_for_connection (int listenfd)
{
  int connfd;
  //if ( leak_mask & 2 ) { malloc (2); }
  // XXX backlog depends on likely number of concurrent pool
  // openers... perhaps should retry in client?  Set to more than 100
  // right now since our stress tests start up to 100 clients at once.
  if (listen (listenfd, 110) < 0)
    {
      OB_PERROR_CODE (0x2010904f, "listen failed");
      OB_LOG_DEBUG_CODE (0x20109033, "calling exit (1);\n");
      exit (1);
    }
  // Now we can finally accept incoming requests!
  while (1)
    {
      // XXX May care about source address for security reasons eventually
      ob_log (OBLV_DBUG, 0x20109021, "accepting...\n");
      char hostname[1024];
      ob_sox sox;
      socklen_t sock_size = sizeof (sox);
      connfd = accept (listenfd, &sox.sa, &sock_size);
      int erryes = errno;  // save because reap_children will overwrite errno
      reap_children ();
      if (want_to_exit)  // signal handler sets this, and we wake for EINTR
        graceful_exit ();
      if (connfd < 0)
        {
          if (erryes != EINTR)  // EINTR is fine, don't complain
            OB_LOG_ERROR_CODE (0x20109022, "accept: %s\n", strerror (erryes));
          continue;
        }

      ob_log (OBLV_DBUG, 0x20109023, "accepted\n");

      int flags = 0;
      // Don't do reverse DNS lookup, because it can take a long time.
      // (bug 2143)
      flags |= NI_NUMERICHOST;
      int gai = getnameinfo (&sox.sa, sock_size, hostname, sizeof (hostname),
                             NULL, 0, flags);
      if (gai != 0)
        {
          if (gai == EAI_SYSTEM)
            OB_LOG_ERROR_CODE (0x20109024, "getnameinfo: %s\n",
                               strerror (errno));
          else
            OB_LOG_ERROR_CODE (0x20109025, "getnameinfo: %s\n",
                               gai_strerror (gai));
          hostname[0] = hostname[1] = hostname[2] = '?';
          hostname[3] = 0;
        }

      connect_pool_hose (connfd, listenfd, hostname);
      // If we're here, it's because we're the parent or we're in debug mode
    }
}

/// Signal handler for SIGCHLD.

static void handle_sigchld (int signo)
{
  // Nothing happens here.  The only point of handling this
  // signal is so that the main thread gets woken up by EINTR,
  // and it will call reap_children then.
}

static void reap_children (void)
{
  // I care about the exit status, because we had a bug where
  // all the children were segfaulting, but it was completely
  // silent, so no one knew!
  pid_t pid;
  int status;
  while ((pid = waitpid (-1, &status, WNOHANG)) > 0)
    {
      if (SLAW_NOT_FOUND == remove_child (pid))
        OB_LOG_ERROR_CODE (0x20109027, "bogus child %u\n", pid);

      // Unless the child exited with success, let the world know!
      if (!(WIFEXITED (status) && WEXITSTATUS (status) == 0))
        {
          // Stdio is not safe to use in a signal handler, but luckily
          // plain old write() is, so let's just write directly to
          // stderr the hard way...
          // XXX: This used to be a signal handler, and now it's not,
          // so we no longer need to be so careful about which library
          // functions we call.  But since this code works, we'll keep
          // it for now.
          char buf[100];
          buf[0] = 0;
          ob_safe_append_string (buf, sizeof (buf), "pool_tcp_server: child ");
          ob_safe_append_int64 (buf, sizeof (buf), (int64) pid);
          if (WIFEXITED (status))
            {
              ob_safe_append_string (buf, sizeof (buf), " exited with status ");
              ob_safe_append_int64 (buf, sizeof (buf),
                                    (int64) WEXITSTATUS (status));
            }
          else if (WIFSIGNALED (status))
            {
              ob_safe_append_string (buf, sizeof (buf), " killed by signal ");
              ob_safe_append_int64 (buf, sizeof (buf),
                                    (int64) WTERMSIG (status));
            }
          else
            {
              ob_safe_append_string (buf, sizeof (buf),
                                     " died of unknown causes");
            }
          size_t len = ob_safe_append_string (buf, sizeof (buf), "\n");
          ob_ignore (write (2, buf, len));
        }
    }
}

/// Signal handler for SIGTERM/SIGINT (see bug 383)

static void handle_sigterm (int signo)
{
  int saved_errno = errno;
  want_to_exit = true;
  ob_ignore (write (2, "Got SIGTERM or SIGINT\n", 22));
  errno = saved_errno;
  // The real magic happens when accept() gets woken up by EINTR.
}

/// Daemonize the server so it doesn't work in the foreground.  If
/// debugging is turned on, stay in the foreground.
/// (Windows server does not do this, it keeps foreground)

static void fork_off (int listenfd)
{
  // Establish our signal handler to reap child processes
  struct sigaction act;
  memset (&act, 0, sizeof (act));
  act.sa_handler = handle_sigchld;
  sigaction (SIGCHLD, &act, NULL);
  // Ignore SIGPIPE, just get the error back on write()
  memset (&act, 0, sizeof (act));
  act.sa_handler = SIG_IGN;
  sigaction (SIGPIPE, &act, NULL);
  // Signal handlers for SIGTERM, SIGINT
  memset (&act, 0, sizeof (act));
  act.sa_handler = handle_sigterm;
  sigaction (SIGINT, &act, NULL);
  sigaction (SIGTERM, &act, NULL);

  pid_t pid;
  // Don't fork if we're debugging
  if (debug || no_fork)
    {
      pid = 0;
      print_pid (getpid ()); /* we are the server, so print our own pid */
    }
  else
    pid = fork ();

  // Child - becomes background daemon
  if (0 == pid)
    // Below never returns
    wait_for_connection (listenfd);
  else
    print_pid (pid); /* print the pid of the server we forked */

  // Don't need the parent any more
  OB_LOG_DEBUG_CODE (0x20109034, "calling exit (0);\n");
  exit (0);
}

#endif

/// Bind to the specified listening port and exit if we can't.

static int bind_port (int port, bool autoPort)
{
  int listenfd;
  int err;
  int af;
  size_t sock_size;
  ob_sox addr;
  OB_CLEAR (addr);

  // Make a socket
  listenfd = socket ((af = AF_INET6), SOCK_STREAM, IPPROTO_TCP);

  if (listenfd < 0
#ifdef _MSC_VER
      && WSAGetLastError () == WSAEAFNOSUPPORT
#else
      && errno == EAFNOSUPPORT
#endif
      ) /* fall back to IPv4-only if IPv6 not supported */
    listenfd = socket ((af = AF_INET), SOCK_STREAM, IPPROTO_TCP);

  if (listenfd < 0)
    {
      OB_LOG_ERROR_CODE (0x20109050,
                         "socket creation failed for family %d: '%s'\n", af,
                         ob_sockmsg ());
      OB_LOG_DEBUG_CODE (0x20109035, "calling exit (1);\n");
      exit (1);
    }

  if (AF_INET6 == af)
    {
      // We want our AF_INET6 socket to listen to IPv4 as well as IPv6.
      int off = 0;
      if (setsockopt (listenfd, IPPROTO_IPV6, IPV6_V6ONLY,
                      EVIL_SOCKOPT_CAST (&off), sizeof (off))
          != 0)
        {
          OB_LOG_ERROR_CODE (0x2010905e, "setsockopt failed: '%s'\n",
                             ob_sockmsg ());
          OB_LOG_DEBUG_CODE (0x2010905f, "calling exit (1);\n");
          exit (1);
        }
    }

  // We want to restart the server a lot, so let us reuse the address
  // (unless we're auto-choosing a port, and then don't)
  int on = 1;
  if (!autoPort)
    {
      err = setsockopt (listenfd, SOL_SOCKET, SO_REUSEADDR,
                        EVIL_SOCKOPT_CAST (&on), sizeof (on));
      if (err != 0)
        {
          OB_LOG_ERROR_CODE (0x20109051,
                             "setsockopt failed for family %d: '%s'\n", af,
                             ob_sockmsg ());
          OB_LOG_DEBUG_CODE (0x20109036, "calling exit (1);\n");
          exit (1);
        }
    }

  // Set keepalive (Bug 14568)
  on = 1;
  err = setsockopt (listenfd, SOL_SOCKET, SO_KEEPALIVE, EVIL_SOCKOPT_CAST (&on),
                    sizeof (on));
  if (err != 0)
    {
      OB_LOG_ERROR_CODE (0x20109052, "setsockout failed for family %d: '%s'\n",
                         af, ob_sockmsg ());
      OB_LOG_DEBUG_CODE (0x20109066, "calling exit (1);\n");
      exit (1);
    }

  // Set Nagle and DSCP
  if (ob_common_sockopts (listenfd) < OB_OK)
    {
      OB_LOG_DEBUG_CODE (0x20109037, "calling exit (1);\n");
      exit (1);
    }

// Assign it to our port
again:
  if (AF_INET6 == af)
    {
      addr.i6.sin6_family = AF_INET6;
      addr.i6.sin6_addr = in6addr_any;
      addr.i6.sin6_port = htons (port);
      sock_size = sizeof (addr.i6);
    }
  else
    {
      addr.in.sin_family = AF_INET;
      addr.in.sin_addr.s_addr = htonl (INADDR_ANY);
      addr.in.sin_port = htons (port);
      sock_size = sizeof (addr.in);
    }

  err = bind (listenfd, &addr.sa, sock_size);

#ifdef _MSC_VER
  //these values are what should be checked according to MSDN docs,
  //previously I had #defined EADDRINUSE to be equivalent but this is
  //a safer and more explicit check, I *think* in vs2010 there actually
  //are definitions for EADDRINUSE that have different numerical values
  //than WSAEADDRINUSE.. I checked somewhat hastily and figured this
  //was the right thing to do according to the docs so here it is -Trey
  int wsa = WSAGetLastError ();
  bool errOK = (wsa == WSAEADDRINUSE || wsa == WSAEADDRNOTAVAIL);
#else
  bool errOK = (errno == EADDRINUSE || errno == EADDRNOTAVAIL);
#endif

  if (err != 0 && autoPort && errOK && port > 1024)
    {
      port--;
      goto again;
    }
  if (err != 0)
    {
      int erryes = errno;
      // XXX: shouldn't Windows use WSAGetLastError() instead of errno?
      if (!(silent_port_in_use && erryes == EADDRINUSE))
        {
          OB_PERROR_CODE (0x20109053, "bind failed");
          if (erryes == EADDRINUSE)
            fprintf (stderr,
                     "Try \"lsof -i tcp:%d\" to see what is using that port\n",
                     port);
        }
      const int tixe = (erryes == EADDRINUSE ? EXIT_IN_USE : EXIT_FAILURE);
      OB_LOG_DEBUG_CODE (0x20109038, "calling exit (%d);\n", tixe);
      exit (tixe);
    }
  actual_port = port;
  return listenfd;
}

// parse port number and detect user error (bug 2403)

static int parse_port (const char *port_str)
{
  long int port;
  char *endptr = NULL;

  errno = 0;
  port = strtol (port_str, &endptr, 0);
  if (*port_str == 0)
    OB_LOG_DEBUG_CODE (0x20109057, "port was empty string\n");
  else if (*endptr != 0)
    OB_LOG_DEBUG_CODE (0x20109058,
                       "invalid characters '%s' provided "
                       "after port number\n",
                       endptr);
  else if (errno != 0)
    OB_LOG_DEBUG_CODE (0x20109059, "invalid port: strtol results in '%s'\n",
                       strerror (errno));
  else if (port < 0)
    OB_LOG_DEBUG_CODE (0x2010905a, "port %ld was negative\n", port);
  else if (port > 0xffff)
    OB_LOG_DEBUG_CODE (0x2010905b, "port %ld does not fit in 16 bits\n", port);
  else
    return (int) port;
  fprintf (stderr, "invalid port provided.\n");
  OB_LOG_DEBUG_CODE (0x2010905c, "calling exit (EXIT_FAILURE);\n");
  exit (EXIT_FAILURE);
}

static const char secure_msg1[] =
  "If secure connections are desired, the server will look for the two\n"
  "mandatory files 'server-certificate-chain.pem' and "
  "'server-private-key.pem',\n"
  "and optional file 'dh-params.pem' in the ob_etc_path, which is:\n";

static const char secure_msg2[] =
  "\nand can be set with the OB_ETC_PATH environment variable.\n";

/// Print a usage message for the server.

static void print_usage (FILE *fp)
{
  const bool have_tls = (OB_OK == ob_tls_banner (fp));
  fputc ('\n', fp);
  fprintf (fp, "usage: pool_tcp_server [-cdnvqtTSIC] [-p <port>]\n");
  fprintf (fp,
           "                       [-P <file>] [-s <seconds>] [-X <file>]\n");
  fprintf (fp, "                       [-L <pool>] [-l <pool prefix>]\n");
  fprintf (fp, "       -c count network operations\n");
  fprintf (fp,
           "       -d don't ever fork (initially or to service requests)\n");
  fprintf (fp,
           "       -n don't fork initially (but fork to service requests)\n");
  fprintf (fp, "       -v verbose\n");
  fprintf (fp, "       -q don't print a message if port is in use\n");
  fprintf (fp, "          (but still exit with code %d)\n", EXIT_IN_USE);
  fprintf (fp, "       -p specify port to listen on (default %u)\n",
           POOL_TCP_PORT);
  fprintf (fp, "       -P choose an unused port automatically, and print\n");
  fprintf (fp, "          the port and PID to the specified file\n");
  fprintf (fp, "       -s number of seconds to wait (on exit) for child\n");
  fprintf (fp, "          processes to die before killing them\n");
  fprintf (fp, "          (default is to not kill children)\n");
  fprintf (fp, "       -X file to print information to on exit\n");
  fprintf (fp, "       -l name of pool directory to log messages to\n");
  fprintf (fp, "          (especially useful in conjunction with -v)\n");
  fprintf (fp, "       -L name of pool to log messages to\n");
  fprintf (fp, "          (especially useful in conjunction with -v)\n");
#ifdef __gnu_linux__
  fprintf (fp, "       -t display client information in \"top\"\n");
  fprintf (fp, "          (at the expense of being able to use killall)\n");
  fprintf (fp, "       -T display status information in \"top\"\n");
  fprintf (fp, "          (at the expense of being able to use killall)\n");
#else
  fprintf (fp, "       -t (option not supported on this platform)\n");
  fprintf (fp, "       -T (option not supported on this platform)\n");
#endif
  if (have_tls)
    {
      fprintf (fp, "       -S only allow secure connections (using TLS)\n");
      fprintf (fp, "       -I only allow insecure connections (forbid TLS)\n");
      fprintf (fp, "          (default is to allow both secure and "
                   "insecure connections)\n");
      fprintf (fp, "       -C require client authentication (implies -S)\n");
      fprintf (fp, "\n%s%s%s\n", secure_msg1,
               ob_get_standard_path (ob_etc_path), secure_msg2);
      fprintf (fp, "Clients will look for the file "
                   "'certificate-authorities.pem', to\n");
      fprintf (fp, "authenticate the server's certificate.\n");
      fprintf (fp, "\n");
      fprintf (fp, "If -C is specified, then the client also needs "
                   "'client-certificate-chain.pem'\n");
      fprintf (fp, "and 'client-private-key.pem', and the server needs\n");
      fprintf (fp, "'certificate-authorities.pem'.\n");
    }
  else
    {
      fprintf (fp, "       -I only allow insecure connections\n");
      fprintf (fp, "          (already the default, and thus is a nop in "
                   "this build)\n");
      fprintf (fp, "       no TLS support in this build (-S and -C not "
                   "supported)\n");
    }
  fprintf (fp, "\n");
  fprintf (fp, "exit codes:\n");
  fprintf (fp, "    %d success\n", EXIT_SUCCESS);
  fprintf (fp, "    %d other failure\n", EXIT_FAILURE);
  fprintf (fp, "    %d port is already in use by another program\n",
           EXIT_IN_USE);
}

static void usage (void)
{
  print_usage (stderr);
  OB_LOG_DEBUG_CODE (0x20109039, "calling exit (EXIT_FAILURE);\n");
  exit (EXIT_FAILURE);
}

/// Process command line options, bind to our port, and daemonize.

int main (int argc, char *argv[])
{
  OB_CHECK_ABI ();

  int port = POOL_TCP_PORT;
  bool autoPort = false;
  int listenfd;
  int c;

  // log pids (because we fork), and log time and limit logs (because
  // we are a daemon)
  pool_cmd_modify_default_logging (true, true, true);

  //if ( leak_mask & 1 ) { malloc (1); }

  while ((c = getopt (argc, argv, "hcdnp:qvl:L:P:s:tTX:SIC")) != -1)
    {
      switch (c)
        {
          case 'c':
            want_cmd_count = true;
            break;
          case 'd':
            debug++;
            break;
          case 'n':
            no_fork = true;
            break;
          case 'p':
            port = parse_port (optarg);
            break;
          case 'v':
            pool_cmd_enable_debug_messages (0x20109000, 64 - 12);
            break;
          case 'q':
            silent_port_in_use = true;
            break;
          case 'l':
            log_pool = strdup (optarg);
            log_dir = true;
            break;
          case 'L':
            log_pool = strdup (optarg);
            break;
          case 'P':
            pidfile = strdup (optarg);
            autoPort = true;
            port =
              POOL_TCP_PORT - 1; /* start below standard port and go downward */
            break;
          case 's':
            cleanup_seconds = atoi (optarg);
            break;
          case 't':
            top_client_info = true;
            break;
          case 'T':
            top_info = true;
            break;
          case 'X':
            cleanupfile = strdup (optarg);
            break;
          case 'C':
            client_auth = true;
          /* fall through to 'S', since 'C' implies 'S' */
          case 'S':
            require_tls = true;
            break;
          case 'I':
            allow_tls = false;
            break;
          case 'h':
            print_usage (stdout);
            exit (0);
            break;
          default:
            fprintf (stderr, "Unknown argument %c\n", optopt);
            usage ();
        }
    }

  if (argc != optind)
    {
      fprintf (stderr,
               "pool_tcp_server does not accept non-option arguments.\n"
               "Non-option argument \"%s\" was provided.\n",
               argv[optind]);
      usage ();
    }

  if (require_tls && !allow_tls)
    {
      fprintf (stderr, "The specified options forbid both secure and insecure "
                       "connections,\n leaving nothing left.\n\n");
      usage ();
    }

  if (allow_tls)
    {
      // initialize TLS (including prompting for passphrase if needed)
      tls_available = ob_tls_server_available ();
      if (tls_available < OB_OK)
        {
          if (require_tls)
            OB_FATAL_ERROR_CODE (0x20109062,
                                 "Cannot operate in secure mode because: %s\n",
                                 ob_error_string (tls_available));
          else
            OB_LOG_WARNING_CODE (0x20109063,
                                 "Secure connections are not "
                                 "available because: %s\n"
                                 "So only insecure connections "
                                 "will be allowed.\n",
                                 ob_error_string (tls_available));
        }
      else if (tls_available == POOL_ANONYMOUS_ONLY)
        {
          const char *secure_msg0 =
            "Could not load certificate and/or private key.\n"
            "(See above for error details, and below for\n"
            "locations checked.)\n";

          if (require_tls)
            OB_FATAL_ERROR_CODE (
              0x20109064, "%sCannot operate securely.\n\n%s%s%s", secure_msg0,
              secure_msg1, ob_get_standard_path (ob_etc_path), secure_msg2);
          else
            OB_LOG_WARNING_CODE (0x20109065,
                                 "%sTherefore, only anonymous ciphersuites\n"
                                 "will be available (vulnerable to\n"
                                 "woman-in-the-middle-attack)\n\n%s%s%s",
                                 secure_msg0, secure_msg1,
                                 ob_get_standard_path (ob_etc_path),
                                 secure_msg2);
        }
    }

#ifdef _MSC_VER
  // Since we are threading instead of forking, we can initialize
  // logging once, now
  initialize_logging ();

  //initialize windows sockets
  winsock_init ();

  // bind the port
  listenfd = bind_port (port, autoPort);

  // on UNIX this would be called in fork_off
  print_pid (getpid ());

  // wait for connections
  wait_for_connection (listenfd);

  //uninit windows sockets
  winsock_shutdown ();

#else
  // bind the port
  listenfd = bind_port (port, autoPort);

  // Fork off and do exciting listening stuff
  fork_off (listenfd);
#endif

  OB_LOG_DEBUG_CODE (0x2010903a, "ending main() with return 0;\n");
  return 0;
}
