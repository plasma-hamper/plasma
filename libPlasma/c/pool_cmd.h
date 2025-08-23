
/* (c)  oblong industries */

#ifndef POOL_CMD_CHAIN
#define POOL_CMD_CHAIN

/**
 * Common helper routines for pool commands.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "libLoam/c/ob-api.h"
#include "libLoam/c/ob-sys.h"

#include "pool.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The pool_cmd_info struct bundles together all the information we
 * may want to grab from the command line.
 */

typedef struct
{
  /**
   * Pool type - e.g., "mmap"
   */
  const char *type;
  /**
   * Pool size
   */
  unt64 size;
  /**
   * Number of proteins in the table of contents
   */
  unt64 toc_capacity;
  /**
   * Verbosity level for the program itself (not libPlasma)
   */
  int verbose;
  pool_hose ph;
  const char *pool_name;
  /**
   * Below two are filled in by pool_cmd_setup_options().
   * Options to pass to pool_create()
   */
  protein create_options;
} pool_cmd_info;

/**
 * Given a fully or partially filled-out pool_cmd_info struct, set
 * defaults for any unset fields and assemble the options for create
 * and participate.  Must be followed by a call to
 * pool_cmd_free_options(), which will leave all the user-set fields
 * intact but free the options proteins.
 */

OB_PLASMA_API void pool_cmd_setup_options (pool_cmd_info *cmd);

/**
 * Free options proteins created by pool_cmd_setup_options().
 */

OB_PLASMA_API void pool_cmd_free_options (pool_cmd_info *cmd);

/**
 * Get the pool name from the command line and stuff it into the
 * pool_cmd_info struct.  argc and argv are the unaltered arguments
 * to main().  optind is the value of optind post-getopt(); if
 * getopt() isn't called, should be zero.  It's safe to pass optind
 * to this function if getopt() is never called.
 *
 * This function expects exactly one non-option argument (the pool
 * name).  Returns 0 on success (found a pool name and nothing else)
 * and 1 on failure (more or less than one non-option argument).
 */

OB_PLASMA_API int pool_cmd_get_poolname (pool_cmd_info *cmd, int argc,
                                         char *argv[], int my_optind);

/**
 * This short-circuits what happens in our C tests, where a
 * shell script puts environment variables into command line
 * options, and getopt gets them out and puts them in pool_cmd_info.
 * Here we just do that in one step.
 */
OB_PLASMA_API void pool_cmd_options_from_env (pool_cmd_info *cmd);

/**
 * Default pool size for mmap pools.
 */

#define POOL_MMAP_DEFAULT_SIZE (1024ULL * 1024ULL)

/**
 * Parse a size string taking into account (optional) unit suffixes.
 * E.g. 100M, 2Gb, 323k.
 */
OB_PLASMA_API unt64 pool_cmd_parse_size (const char *arg);

/**
 * Open the pool or exit with an error.
 */

OB_PLASMA_API void pool_cmd_open_pool (pool_cmd_info *cmd);

/**
 * Create a protein that looks like so:
 *
 * descrip_<name>, key_<name>:value_<name>
 */

OB_PLASMA_API protein pool_cmd_create_test_protein (const char *name);

/**
 * Create a protein of the same form as pool_cmd_create_test_protein()
 * and deposit it.
 */

OB_PLASMA_API ob_retort pool_cmd_add_test_protein (pool_hose ph,
                                                   const char *name,
                                                   protein *prot_p, int64 *idx);

/**
 * Check for matching proteins and fail informatively if they don't
 * match.
 */

OB_PLASMA_API int pool_cmd_check_protein_match (bprotein p1, bprotein p2);

/**
 * Fill a pool.
 */

OB_PLASMA_API void pool_cmd_fill_pool (pool_cmd_info *cmd);

// Fails if any file descriptors are open, other than stdin, stdout,
// and stderr.  (Although valgrind will detect leaks of fopen/fclose,
// because fopen allocates memory, valgrind will not detect leaks
// of open/close, because open does not allocate any memory in
// userspace.)

void check_for_leaked_file_descriptors (void);

typedef int (*mainish_function) (int argc, char **argv);

OB_PLASMA_API int check_for_leaked_file_descriptors_scoped (mainish_function,
                                                            int argc,
                                                            char **argv);

// Distinguished status codes for calling exit() with.  More exciting
// (and useful to scripts) than just EXIT_SUCCESS(0) and EXIT_FAILURE(1).
#define EXIT_NOPOOL    10  // POOL_NO_SUCH_POOL
#define EXIT_EXISTS    12  // POOL_EXISTS
#define EXIT_NOPROTEIN 14  // POOL_NO_SUCH_PROTEIN
#define EXIT_BADPNAME  16  // POOL_POOLNAME_BADTH
#define EXIT_BADPTYPE  18  // POOL_TYPE_BADTH
#define EXIT_INUSE     20  // POOL_IN_USE

// Convert a retort to an exit code suitable for returning from main().
OB_PLASMA_API int pool_cmd_retort_to_exit_code (ob_retort tort);

/**
 * This is a convenience function for changing some of the default
 * logging settings.  It's not very general, which is why it's here
 * instead of in ob-log.h.  You can enable printing the pid with
 * all log messages (useful for programs that fork), you can enable
 * the printing of the time with the log message (useful for daemons
 * that log to a file), and you can enable limiting the number of
 * times a given message can be printed (also useful for daemons).
 */
OB_PLASMA_API void
pool_cmd_modify_default_logging (bool log_pids, bool log_time, bool limit_logs);

/**
 * If \a limit_logs is set to true in pool_cmd_modify_default_logging(),
 * this is the maximum number of times one message will be printed.
 */
#define POOL_CMD_MAX_LOG_COUNT 50

/**
 * Another convenience function for logging.  Enables some debug messages,
 * those that match the first \a matchbits of \a code.
 */
OB_PLASMA_API void pool_cmd_enable_debug_messages (unt64 code, unt8 matchbits);

#ifdef __cplusplus
}  //extern C
#endif

#endif /* POOL_CMD_CHAIN */
