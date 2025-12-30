
/* (c)  oblong industries */

///
/// Test that a pool hose can't join more than one gang, or join the
/// same gang twice.  Test that two pool hoses to the same pool in the
/// same gang do work.  Named pool must already exist.
///
#include "pool_cmd.h"
#include "libLoam/c/ob-sys.h"
#include "libLoam/c/ob-log.h"

static void usage (void)
{
  fprintf (stderr, "Usage: doppelganger <poolname>\n");
  exit (1);
}

int mainish (int argc, char *argv[])
{
  pool_cmd_info cmd;
  ob_retort pret;

  memset (&cmd, 0, sizeof (cmd));
  // ob_log_to_file ("/dev/tty");

  if (pool_cmd_get_poolname (&cmd, argc, argv, optind))
    usage ();

  ob_log (OBLV_DBUG, 0x20403000, "Creating crips\n");
  pool_gang crips;
  pret = pool_new_gang (&crips);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x20403001,
                         "New gang creation failed: %" OB_FMT_RETORT "d"
                         "\n",
                         pret);

  ob_log (OBLV_DBUG, 0x20403002, "Creating bloods\n");
  pool_gang bloods;
  pret = pool_new_gang (&bloods);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x20403003,
                         "New gang creation failed: %" OB_FMT_RETORT "d"
                         "\n",
                         pret);

  ob_log (OBLV_DBUG, 0x20403004, "Opening pool\n");
  pool_cmd_open_pool (&cmd);

  // Join a gang
  ob_log (OBLV_DBUG, 0x20403005, "Join a gang\n");
  pret = pool_join_gang (crips, cmd.ph);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x20403006,
                         "Pool %s failed to join gang: %" OB_FMT_RETORT "d"
                         "\n",
                         cmd.pool_name, pret);

  // Try to rejoin a gang
  ob_log (OBLV_DBUG, 0x20403007, "Try to rejoin a gang\n");
  pret = pool_join_gang (crips, cmd.ph);
  if (pret != POOL_ALREADY_GANG_MEMBER)
    OB_FATAL_ERROR_CODE (0x20403008, "Joined the same gang twice succeeded "
                                     "(should have failed)\n");

  // Try to join a different gang
  ob_log (OBLV_DBUG, 0x20403009, "Try to join a different gang\n");
  pret = pool_join_gang (bloods, cmd.ph);
  if (pret != POOL_ALREADY_GANG_MEMBER)
    OB_FATAL_ERROR_CODE (0x2040300a,
                         "Joining two gangs succeeded (should have failed)\n");

  // Leave the gang and see if we can come back.
  ob_log (OBLV_DBUG, 0x2040300b,
          "Leave the gang and see if we can come back.\n");
  OB_DIE_ON_ERROR (pool_leave_gang (crips, cmd.ph));
  pret = pool_join_gang (crips, cmd.ph);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x2040300c,
                         "Pool %s failed to rejoin gang: %" OB_FMT_RETORT "d"
                         "\n",
                         cmd.pool_name, pret);

  // See if we can have two hoses to the same pool in the same gang
  // and do an await.
  ob_log (OBLV_DBUG, 0x2040300d,
          "See if we can have two hoses to the same pool in the same gang "
          "and do an await.\n");
  pool_cmd_info cmd2 = cmd;
  cmd2.ph = NULL;
  ob_log (OBLV_DBUG, 0x2040300e,
          "Opening second hose ...\n(if this hangs, "
          "perhaps your pool_tcp_server is single-threaded?)\n");
  pool_cmd_open_pool (&cmd2);
  ob_log (OBLV_DBUG, 0x2040300f, "Joining gang with second hose...\n");
  pret = pool_join_gang (crips, cmd2.ph);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x20403010,
                         "Second pool hose for  %s failed to join "
                         "gang: %" OB_FMT_RETORT "d"
                         "\n",
                         cmd2.pool_name, pret);

  protein prot;
  // Give a short timeout so we go into await but return quickly
  ob_log (OBLV_DBUG, 0x20403011,
          "Give a short timeout so we go into await but return quickly\n");
  pret = pool_await_next_multi (crips, 0.1, NULL, &prot, NULL, NULL);
  if (pret != POOL_AWAIT_TIMEDOUT)
    OB_FATAL_ERROR_CODE (0x20403012,
                         "Await for two hoses to same pool failed %s\n",
                         ob_error_string (pret));

  ob_log (OBLV_DBUG, 0x20403013, "disband gangs\n");
  OB_DIE_ON_ERROR (pool_disband_gang (crips, true));
  OB_DIE_ON_ERROR (pool_disband_gang (bloods, true));

  pool_cmd_free_options (&cmd);

  return 0;
}


int main (int argc, char **argv)
{
  return check_for_leaked_file_descriptors_scoped (mainish, argc, argv);
}
