
/* (c)  oblong industries */

/**
 * \file
 * \ingroup PlasmaPoolsGroup
 */

#ifndef POOL_CHLORINE
#define POOL_CHLORINE



#include "libLoam/c/ob-types.h"
#include "libLoam/c/ob-retorts.h"
#include "libLoam/c/ob-api.h"

#include "libPlasma/c/plasma-types.h"
#include "libPlasma/c/pool-time.h"
#include "libPlasma/c/plasma-retorts.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup PoolTypesAndConstants Types and constants
 * Constants and types representing pool connections.
 * \ingroup PlasmaPoolsGroup
 */

/**
 * Pool hoses hold all the state associated with a connection to a
 * pool.  Pool hoses are not thread-safe; each thread of execution
 * must have its own pool hose.  The pool_hose struct is private.
 * Clients should not peek at it.  This means you.
 * \ingroup PoolTypesAndConstants
 */
typedef struct pool_hose_struct *pool_hose;

/**
 * Holds "context" information that might be needed for pool operations,
 * such as credentials.  Pool contexts are not thread-safe; each thread
 * of execution must have its own pool context.  Pool contexts are opaque
 * and should only be manipulated with the functions provided.
 * \ingroup PoolTypesAndConstants
 */
typedef struct pool_context_struct *pool_context;

/**
 * \name Constants denoting timeouts
 */
//@{
/**
 * \ingroup PoolTypesAndConstants
 */
#define POOL_WAIT_FOREVER -1
#define POOL_NO_WAIT 0
//@}

/**
 * \defgroup PoolCreation Pool creation and disposal
 * Functions to create and delete pools.
 * \ingroup PlasmaPoolsGroup
 */
//@{
/**
 * Create a new pool.
 *
 * The pool_name is a URI-sh string that looks like so:
 * @code
 *     [<transport>://hostname[:port]/]<pool_name>
 * @endcode
 *
 * Pool names may contain most printable characters, including '/',
 * although '/' is special since it indicates a subdirectory, just
 * like on the filesystem.  Some other restrictions on pool name:
 *   - the total pool name must be between 1 and 100 characters in length
 *   - a pool name consists of one or more components, separated by slashes
 *   - a component may not be the empty string
 *   - a component may only contain the characters:
 *     " !#$%&'()+,-.0123456789;=@ABCDEFGHIJKLMNOPQRSTUVWXYZ"
 *     "[]^_`abcdefghijklmnopqrstuvwxyz{}~"
 *     (the double quotes are not part of the legal character set)
 *   - a component may not begin with '.', and may not end with '.', ' ',
 *     or '$'
 *   - a component may not be any of the following names, case-insensitively,
 *     nor may it begin with one of these immediately followed by a dot:
 *     CON, PRN, AUX, NUL, COM1, COM2, COM3, COM4, COM5, COM6, COM7, COM8,
 *     COM9, LPT1, LPT2, LPT3, LPT4, LPT5, LPT6, LPT7, LPT8, LPT9,
 *     or lost+found.
 *
 * Pools are uniquely identified by their names, regardless of type.
 *
 * Examples:
 *   - my_pool
 *   - pipeline/gripes
 *   - tcp://localhost/my_pool
 *   - tcp://mango:10000/my_pool
 *
 * The type string specifies what kind of pool you want to create,
 * e.g., "mmap".  This refers to the type of the pool on the host
 * machine, _not_ the transport (e.g., "tcp").  Currently, the
 * transport is specified in the pool name.
 *
 * The \a options slaw may be either a map or a protein, and
 * describes any parameters needed to create the
 * pool, which will vary by the type of pool.  The option format is a
 * protein containing multiple ingests, where the ingest key is the
 * parameter to set and the ingest value is (unsurprisingly) the
 * value of the parameter.  (Or just a slaw map with similar key/value
 * pairs.)  For example, mmap pools need to know what
 * size of pool to create, so to create a pool of size 1048576, you'd
 * pass it an option protein with the ingest "size:1048576".  If no
 * parameters are needed, the options argument is NULL.
 *
 * This function may return, besides the success indicator OB_OK,
 *  - OB_ARGUMENT_WAS_NULL if \a ret_ph is \c NULL.
 *  - OB_NO_MEM on memory allocation errors.
 *  - POOL_INVALID_SIZE if the size specified in
 *    \a options is below or above limits, or is not coercible
 *    to an integer.
 *  - POOL_POOLNAME_BADTH if \a pool_name is ill-formed.
 *  - POOL_TYPE_BADTH if \a type does not name a known
 *    pool type.
 *  - a retort encapsulating an errno in case of a system error
 *    such as a failure to acquire an OS lock.
 *  - POOL_EXISTS if a pool with this name already exists.
 *
 * The possible keys for the options map/protein are documented in
 * \ref PoolOptions
 */
OB_PLASMA_API ob_retort pool_create (const char *pool_name, const char *type,
                                     bslaw options);

/**
 * Like pool_create(), but takes a context.
 */
OB_PLASMA_API ob_retort pool_create_ctx (const char *pool_name,
                                         const char *type, bslaw options,
                                         pool_context ctx);

/**
 * Destroy a pool utterly.
 * Possible return values:
 *   - OB_ARGUMENT_WAS_NULL if \a pool_name is NULL.
 *   - POOL_NO_SUCH_POOL if a pool by that name doesn't exist.
 *   - POOL_IN_USE if there is still a hose open to this pool
 *
 */
OB_PLASMA_API ob_retort pool_dispose (const char *pool_name);

/**
 * Like pool_dispose(), but takes a context.
 */
OB_PLASMA_API ob_retort pool_dispose_ctx (const char *pool_name,
                                          pool_context ctx);

/**
 * Rename a pool.  \a old_name and \a new_name must have the same
 * protocol and hostname, if present.
 * Like pool_dispose(), returns POOL_IN_USE
 * if you call it while there are any open hoses to \a old_name.
 */
OB_PLASMA_API ob_retort pool_rename (const char *old_name,
                                     const char *new_name);

/**
 * Like pool_rename(), but takes a context.
 */
OB_PLASMA_API ob_retort pool_rename_ctx (const char *old_name,
                                         const char *new_name,
                                         pool_context ctx);

/**
 * Returns the success code OB_YES if \a pool_name exists, and
 * returns the success code OB_NO if \a pool_name does not exist.
 * (And, of course, returns an error code if an error occurs.)
 * Beware of TOCTOU!  In most cases, it would be more robust to
 * just use pool_participate(), because then if it does exist,
 * you'll have a hose to it.  With pool_exists(), it might go
 * away between now and when you participate in it.
 */
OB_PLASMA_API ob_retort pool_exists (const char *pool_name);

/**
 * Like pool_exists(), but takes a context.
 */
OB_PLASMA_API ob_retort pool_exists_ctx (const char *pool_name,
                                         pool_context ctx);

/**
 * Check that a pool name is valid.  See the above naming rules.
 * Possible return values:
 *   - OB_OK if the name is just fine.
 *   - OB_POOLNAME_BADTH if the name is no good.
 *
 * \note This only validates a local pool name (or the "pool_name"
 * or "path" component of a pool URI).  It will fail on remote
 * pool names, such as "tcp://mango:10000/my_pool".
 */
OB_PLASMA_API ob_retort pool_validate_name (const char *pool_name);


/**
 * Put a pool "to sleep", which means allowing the pool implementation
 * to free certain resources used by the pool, in the expectation that
 * it won't be used in a while.  A pool can only be put to sleep if
 * there are no open hoses to it; POOL_IN_USE will be returned if this
 * condition is not met.  The pool will automatically "wake up"
 * (reacquire the resources it needs) the next time it is participated
 * in.
 *
 * In practice, in the current implementation, "resources" means
 * "semaphores".  This function is only useful/necessary if you
 * intend to have a large number (more than 32768) of pools.
 */
OB_PLASMA_API ob_retort pool_sleep (const char *pool_name);

/**
 * Like pool_sleep(), but takes a context.
 */
OB_PLASMA_API ob_retort pool_sleep_ctx (const char *pool_name,
                                        pool_context ctx);

/**
 * If the named pool exists and there are currently no hoses open to it,
 * returns OB_OK.  If the named pool currently has one or more hoses
 * open to it, returns POOL_IN_USE.  Can also return other errors, such
 * as POOL_NO_SUCH_POOL if the pool does not exist.
 *
 * \note Beware of TOCTOU issues, though:
 * http://cwe.mitre.org/data/definitions/367.html
 */
OB_PLASMA_API ob_retort pool_check_in_use (const char *pool_name);

/**
 * Like pool_check_in_use(), but takes a context.
 */
OB_PLASMA_API ob_retort pool_check_in_use_ctx (const char *pool_name,
                                               pool_context ctx);
//@}

/**
 * \defgroup HoseConnections Connecting to pools
 * Ways of creating connections to pools.
 * Pool creation, destruction, and participation are all thread-safe:
 * multiple threads and processes can be calling these functions at
 * the same time for the same pools.  Pool hoses, on the other hand,
 * are intended for use by a single thread only.  If a second thread
 * needs access to a pool, it must connect its own pool hose.  The
 * pool_hose_clone() helper function may be useful in this case.
 * \ingroup PlasmaPoolsGroup
 */

/**
 * Create a connection to a pool - a pool hose.  The pool hose can
 * only be used by one thread at a time, very similar to a file
 * descriptor.  The \a options argument is deprecated, and should
 * be NULL.
 *
 * If the connection is successful, the associated index will be set
 * to one past its newest value (as if pool_runout() had been called),
 * and OB_OK is returned.  Possible error conditions include:
 *  - OB_ARGUMENT_WAS_NULL if \a ret_ph is \c NULL.
 *  - OB_NO_MEM on memory allocation errors.
 *  - POOL_POOLNAME_BADTH if \a pool_name is not a legal pool name
 *    (contains prohibited characters or is NULL).
 *  - a retort encapsulating an errno in case of a system error such
 *    as a failure to acquire an OS lock or a network resource.
 *  - POOL_NO_SUCH_POOL if a pool with this name does not exist.
 *  - POOL_CORRUPT, POOL_WRONG_VERSION, SLAW_WRONG_VERSION if the pool
 *    data does not have the expected format.
 *  For local pools we have also:
 *  - POOL_INVALID_SIZE when the size in an mmap pool's configuration
 *    is incorrect.
 *  - POOL_INAPPROPRIATE_FILESYSTEM when the pool is backed by a
 *    filesystem not supported by plasma (e.g. NFS).
 *  - POOL_MMAP_BADTH for errors accessing mapped memory in mmap
 *    pools.
 *  And, for remote ones:
 *  - POOL_SOCK_BADTH, POOL_SERVER_UNREACH for connectivity problems.
 *  - POOL_PROTOCOL_ERROR
 *
 * \ingroup HoseConnections
 */
OB_PLASMA_API ob_retort pool_participate (const char *pool_name,
                                          pool_hose *ret_ph, bslaw options);

/**
 * Like pool_participate(), but takes a context.
 *
 * \note The hose holds on to the context during its lifetime, so
 * you can't free \a ctx until you've withdrawn the hose.
 */
OB_PLASMA_API ob_retort pool_participate_ctx (const char *pool_name,
                                              pool_hose *ret_ph,
                                              pool_context ctx);

/**
 * Participate in a pool, creating it if it doesn't exist.  On
 * return, the pool is guaranteed to exist (modulo actual errors in
 * pool creation). Possible return values include those returned by
 * pool_create() (other than POOL_EXISTS) and pool_participate() (other
 * than POOL_NO_SUCH_POOL). This function may return two success
 * codes:
 *   - OB_OK if the pool existed and connection was successful
 *   - POOL_CREATED if the pool didn't exist but was created successfully
 *
 * \note The \a options argument contains arguments for pool creation,
 * just like for pool_create().
 */
OB_PLASMA_API ob_retort pool_participate_creatingly (const char *pool_name,
                                                     const char *type,
                                                     pool_hose *ret_ph,
                                                     bslaw options);

/**
 * Like pool_participate_creatingly(), but takes a context.
 *
 * \note The hose holds on to the context during its lifetime, so
 * you can't free \a ctx until you've withdrawn the hose.
 */
OB_PLASMA_API ob_retort pool_participate_creatingly_ctx (const char *pool_name,
                                                         const char *type,
                                                         pool_hose *ret_ph,
                                                         bslaw create_options,
                                                         pool_context ctx);

/**
 * Create a new connection exactly like the original.  The index of
 * the new pool_hose will be set to the same point as the original.
 * The cloned hose's name is set to the same as the original hose's name.
 * Possible error codes:
 *   - POOL_NULL_HOSE if \a orig_ph is NULL
 *   - OB_ARGUMENT_WAS_NULL if \a new_ph is NULL
 *   - Any of the error codes returned by pool_participate()
 * The contents of \a new_ph is modified only in case of success.
 * \ingroup HoseConnections
 */
OB_PLASMA_API ob_retort pool_hose_clone (pool_hose orig_ph, pool_hose *new_ph);

/**
 * Close your connection to the pool and free all resources associated
 * with it.  All (successful) calls to pool_participate() or
 * pool_participate_creatingly() must be followed by a
 * pool_withdraw(), eventually. We return POOL_NULL_HOSE if \a ph is
 * NULL. For remote pools, possible error codes are:
 *  - POOL_SOCK_BADTH, POOL_SERVER_UNREACH for connectivity problems.
 *  - POOL_PROTOCOL_ERROR
 * \ingroup HoseConnections
 */
OB_PLASMA_API ob_retort pool_withdraw (pool_hose ph);

/**
 * \defgroup PoolProperties Pool and hose properties
 * Functions to obtain metadata about pools and hoses.
 * \ingroup PlasmaPoolsGroup
 */

/**
 * List all the pools under a specified URI.
 * If \a uri is NULL, then lists all local pools under OB_POOLS_DIR
 * (behaves like pool_list()).  A subset of those pools, underneath
 * a specified subdirectory of OB_POOLS_DIR, can be requested with
 * a \a uri of the form "some/dir".  Pools underneath an arbitrary
 * local directory can be listed with "local:/an/absolute/dir".
 * \a uri should be a string like "tcp://chives.la923.oblong.net:1234/"
 * if you want to list pools on a remote server.
 *
 * A slaw list of strings, one for each pool name, is written to \a ret_slaw.
 *
 * \ingroup PoolProperties
 */
OB_PLASMA_API ob_retort pool_list_ex (const char *uri, slaw *ret_slaw);

/**
 * Like pool_list_ex(), but takes a context.
 */
OB_PLASMA_API ob_retort pool_list_ctx (const char *uri, slaw *ret_slaw,
                                       pool_context ctx);

/**
 * List all the pools on the local system, returning their names
 * as a list of string slawx.  The returned value belongs to the
 * caller, who is thus responsible for freeing it.
 * \ingroup PoolProperties
 */
static inline ob_retort pool_list (slaw *ret_slaw)
{
  return pool_list_ex (NULL, ret_slaw);
}

/**
 * pool_list_remote() has been renamed to pool_list_ex(), because it
 * is now capable of listing either local or remote pools.
 * This function exists for backwards compatibility.
 */
static inline ob_retort pool_list_remote (const char *server, slaw *ret_slaw)
{
  return pool_list_ex (server, ret_slaw);
}

/**
 * Sets the "oldest" index of a pool, essentially erasing any proteins
 * prior to that index.  Returns OB_OK if at least one protein was erased.
 * Returns OB_NOTHING_TO_DO if \a idx_in is older than the current
 * oldest index.  Returns POOL_NO_SUCH_PROTEIN if \a idx_in is newer than
 * the newest index.
 */
OB_PLASMA_API ob_retort pool_advance_oldest (pool_hose ph, int64 idx_in);

/**
 * Allows some of the options that were specified in pool_create()
 * to be changed after the pool is created.  \a options can be
 * either a protein or a slaw map, as in pool_create().
 *
 * The possible keys for the options map/protein are documented in
 * (FIXME: unknown?) in the install tree,
 * or libPlasma/c/doc/pool-create-options.md in the source tree.
 */
OB_PLASMA_API ob_retort pool_change_options (pool_hose ph, bslaw options);

/**
 * Position the index pointing to the protein whose timestamp is
 * closest to \a timestamp, using \a time_bound as the time comparision
 * criterium.
 */
OB_PLASMA_API ob_retort pool_seekto_time (pool_hose ph, float64 timestamp,
                                          time_comparison time_bound);

/**
 * As pool_seekto_time(), but specifying the timestamp by a delta
 * relative to the current position's timestamp.
 */
OB_PLASMA_API ob_retort pool_seekby_time (pool_hose ph, float64 relative_time,
                                          time_comparison time_bound);

/**
 * Get the name of the pool this pool hose is connected to.  The
 * returned string is valid only while the pool_hose is active (i.e.,
 * hasn't been withdrawn). You must not free the returned
 * string.  If you want to alter this string, you must make a copy of
 * it.
 * \ingroup PoolProperties
 */
OB_PLASMA_API const char *pool_name (pool_hose ph);

/**
 * Set the name of this hose.  Hose names have no effect on the
 * functioning of libPlasma, and exist only as a debugging aid.
 * Hose names may be used in various messages, so you should set it
 * to something that's meaningful to a human. Besides OB_OK
 * on success, this function can return POOL_NULL_HOSE or
 * OB_ARGUMENT_WAS_NULL on \c NULL arguments or OB_NO_MEM in
 * out of memory conditions.
 * \note The hose name should be UTF-8 encoded.
 * \ingroup PoolProperties
 */
OB_PLASMA_API ob_retort pool_set_hose_name (pool_hose ph, const char *name);

/**
 * Gets the name of this hose, which was set with pool_set_hose_name().
 * The returned string is valid only while the pool_hose is active (i.e.,
 * hasn't been withdrawn) and also becomes invalid on the next call
 * to pool_set_hose_name().  (So copy it if you want to hang onto it.)
 * \note The hose name is UTF-8 encoded.
 * \ingroup PoolProperties
 */
OB_PLASMA_API const char *pool_get_hose_name (pool_hose ph);

/**
 * Returns a protein with information about a pool.
 * Should always include an ingest "type", which is a string naming the
 * pool type, and "terminal", which is a boolean which is true if this is
 * a terminal pool type like "mmap", or false if this is a transport pool
 * type like "tcp".
 * For mmap pools, should include an ingest "size", which is an integer
 * giving the size of the pool in bytes.
 * For tcp pools, should include an ingest "host", which is a string
 * naming the host the pool is on, and "port" which is an integer
 * giving the port.
 * For other pool types, ingests with other relevant info can be included.
 * If "hops" is 0, means return information about this pool hose.
 * If "hops" is 1, means return information about the pool beyond this
 * hose (assuming this hose is a nonterminal type like TCP).  And higher
 * values of "hops" mean go further down the line, if multiple nonterminal
 * types are chained together.  If "hops" is -1, means return information
 * about the terminal pool, no matter how far it is.
 * The returned protein is newly allocated, and must be freed by the
 * caller with protein_free().
 * \ingroup PoolProperties
 */
OB_PLASMA_API ob_retort pool_get_info (pool_hose ph, int64 hops,
                                       protein *return_prot);

/**
 * \name Pool and hose indexes
 * Proteins in a pool are indexed by a monotonically increasing,
 * non-negative integer. For each pool we have a maximum and
 * minimum value for such index. On the other hand, each connection
 * (hose) keeps its own cursor or index into the pool, which can be
 * read and manipulated using the functions below.
 * All these functions will return POOL_NULL_HOSE if their first
 * argument is \c NULL, and, for those taking an output pointer,
 * OB_ARGUMENT_WAS_NULL in case the output pointer is \c NULL.
 */
//@{
/**
 * \ingroup PoolProperties
 */

/**
 * Get the index of the newest protein in this pool.  Returns
 * POOL_NO_SUCH_PROTEIN if no proteins are in the pool.
 */
OB_PLASMA_API ob_retort pool_newest_index (pool_hose ph, int64 *index_out);

/**
 * Get the index of the oldest protein in this pool.  Returns
 * POOL_NO_SUCH_PROTEIN if no proteins are in the pool.
 */
OB_PLASMA_API ob_retort pool_oldest_index (pool_hose ph, int64 *index_out);

/**
 * Determines the current position (index) of the specified
 * pool hose.  Writes the current index to \a index_out.
 */
OB_PLASMA_API ob_retort pool_index (pool_hose ph, int64 *index_out);

/**
 * Set the pool hose's index to the first available protein.
 */
OB_PLASMA_API ob_retort pool_rewind (pool_hose ph);

/**
 * Set the pool hose's index to the last available protein.
 */
OB_PLASMA_API ob_retort pool_tolast (pool_hose ph);

/**
 * Set the pool hose's index to that following the last available protein.
 */
OB_PLASMA_API ob_retort pool_runout (pool_hose ph);

/**
 * Move the pool hose's index forward by the given offset.
 */
OB_PLASMA_API ob_retort pool_frwdby (pool_hose ph, int64 indoff);

/**
 * Alias for pool_frwdby().
 */
#define pool_seekby pool_frwdby

/**
 * Move the pool hose's index back by the given offset.
 */
OB_PLASMA_API ob_retort pool_backby (pool_hose ph, int64 indoff);

/**
 * Set the pool hose's index to the given value.
 */
OB_PLASMA_API ob_retort pool_seekto (pool_hose ph, int64 idx);
//@}

// XXX Misleading names, pool_deposit is not paired with pool_withdraw

/**
 * \defgroup ReadWriteProteins Reading and writing proteins
 * \ingroup PlasmaPoolsGroup
 */

/**
 * \name Deposit (aka write) proteins in a pool.
 * Two ways of writing proteins to a pool and obtaining
 * associated metadata.
 */
//@{
/**
 * \ingroup ReadWriteProteins
 */

/**
 * Deposit a protein into this pool.  On return, \a idx is filled in
 * with the index that was assigned to the protein, provided \a idx
 * is not \c NULL, in which case it's ignored. The usual error
 * codes will mark a null hose (POOL_NULL_HOSE) or a slaw which is
 * not a protein (POOL_NOT_A_PROTEIN). Other possible error
 * conditions:
 *   - an errno encapsulated in a retort for system-level errors
 *   - POOL_SEMAPHORES_BADTH if required locks couldn't be acquired
 *   - POOL_PROTEIN_BIGGER_THAN_POOL
 *   - POOL_CORRUPT
 *  and, for remote pools, the usual suspects:
 *   - POOL_SOCK_BADTH, POOL_SERVER_UNREACH for connectivity problems.
 *   - POOL_PROTOCOL_ERROR
 */
OB_PLASMA_API ob_retort pool_deposit (pool_hose ph, bprotein p, int64 *idx);

/**
 * Deposit a protein into this pool.  On return, \a idx (if not NULL) is
 * filled in with the index that was assigned to the protein.  If \a ret_ts
 * is non-NULL, it is filled in with the timestamp that was assigned
 * to the protein. See pool_deposit() for possible retorts.
 */
OB_PLASMA_API ob_retort pool_deposit_ex (pool_hose ph, bprotein p, int64 *idx,
                                         pool_timestamp *ret_ts);
//@}

/**
 * Used as an argument to pool_fetch() to specify desired
 * portions of a protein to fetch.
 */
typedef struct pool_fetch_op
{
  // input/output (value changes on return if clamped)
  int64 idx; /*< index of protein to fetch */

  // inputs
  bool want_descrips; /*< whether descrips should be included */
  bool want_ingests;  /*< whether ingests should be included */
  int64 rude_offset;  /*< offset of rude data to start at,
                                  or -1 for no rude data */
  int64 rude_length;  /*< number of bytes of rude data to get,
                                  or -1 for "until end" */

  // outputs
  ob_retort tort;      /*< success or failure of the operation */
  float64 ts;          /*< protein's timestamp metadata from the pool */
  int64 total_bytes;   /*< total number of bytes in the protein */
  int64 descrip_bytes; /*< number of bytes in the protein's descrips */
  int64 ingest_bytes;  /*< number of bytes in the protein's ingests */
  int64 rude_bytes;    /*< number of bytes in the protein's rude data */
  int64 num_descrips;  /*< number of descrips (-1 if not list) */
  int64 num_ingests;   /*< number of ingests (-1 if not map) */
  protein p;           /*< trimmed version of requested protein */
} pool_fetch_op;

/**
 * \name Reading proteins from a pool
 * Protein retrieval functions follow.  Most of them operate in
 * relationship with the pool hose's index, and many modify the pool
 * hose's index.  In general, if the \a ret_ts, or \a ret_index
 * pointers are non-NULL, they are filled in with the timestamp,
 * index, or pool hose of the returned protein. The output parameter
 * \a ret_ph must be non-NULL, otherwise OB_ARGUMENT_WAS_NULL is
 * returned (with no operation performed on the hose). Likewise
 * for \a ph, whose nullity is saluted with POOL_NULL_HOSE. Finally,
 * remote pool access can give rise to network specific error codes:
 *  - POOL_SOCK_BADTH, POOL_SERVER_UNREACH for connectivity problems.
 *  - POOL_PROTOCOL_ERROR
 */
//@{
/**
 * \ingroup ReadWriteProteins
 */

/**
 * Retrieve the protein with the given index.  If \a ret_ts is not NULL,
 * the timestamp of the protein is returned in it.  Returns
 * POOL_NO_SUCH_PROTEIN if the index is previous to that of the
 * oldest index or if it is after that of the newest index.
 */
OB_PLASMA_API ob_retort pool_nth_protein (pool_hose ph, int64 idx,
                                          protein *ret_prot,
                                          pool_timestamp *ret_ts);

/**
 * Retrieve the next available protein at or following the pool hose's
 * index and advance the index to position following.  May skip
 * proteins since the protein immediately following the last read
 * protein may have been discarded. If no proteins are available,
 * this function returns POOL_NO_SUCH_PROTEIN.
 */
OB_PLASMA_API ob_retort pool_next (pool_hose ph, protein *ret_prot,
                                   pool_timestamp *ret_ts, int64 *ret_index);

/**
 * The same as pool_next(), but wait for the next protein if none are
 * available now.  The timeout argument specifies how long to wait
 * for a protein:
 *   - timeout == POOL_WAIT_FOREVER (-1): Wait forever
 *   - timeout == POOL_NO_WAIT (0)      : Don't wait at all
 *   - timeout  > 0                    : Return after this many seconds.
 *
 * Returns POOL_AWAIT_TIMEDOUT if no protein arrived before the
 * timeout expired.
 */
OB_PLASMA_API ob_retort pool_await_next (pool_hose ph,
                                         pool_timestamp timeout,
                                         protein *ret_prot,
                                         pool_timestamp *ret_ts,
                                         int64 *ret_index);

/**
 * Enable pool_hose_wake_up() for this hose. Calling this function
 * multiple times on a hose is the same as calling it once on a
 * hose. Wakeup is mediated by a system-dependent IPC mechanism:
 * a failure on acquiring the necessary system resources is signalled
 * by an errno encapsulated in a retort.
 */
OB_PLASMA_API ob_retort pool_hose_enable_wakeup (pool_hose ph);

/**
 * A signal-safe and thread-safe function to interrupt any call to
 * pool_await_next() on this hose. For each time that this function
 * is called, one call to await() will return with a pool_retort of
 * POOL_AWAIT_WOKEN.  (XXX That's not really true if enough wakeup
 * requests pile up-- they will eventually be eaten if no one looks
 * at them.  See bug 771.)
 *
 * It is an error to call this function without previously having
 * called pool_hose_enable_wakeup() on this hose: in that case,
 * this function will return POOL_WAKEUP_NOT_ENABLED.
 */
OB_PLASMA_API ob_retort pool_hose_wake_up (pool_hose ph);

/**
 * Retrieve the protein at the pool hose's index. If no protein
 * is available, this function returns POOL_NO_SUCH_PROTEIN.
 */
OB_PLASMA_API ob_retort pool_curr (pool_hose ph,
                                   protein *ret_prot,
                                   pool_timestamp *ret_ts);

/**
 * Retrieve the protein just previous to the pool hose's current
 * index.  Move the pool hose's index to this position. If no
 * protein before the current one is available, we return
 * POOL_NO_SUCH_PROTEIN.
 */
OB_PLASMA_API ob_retort pool_prev (pool_hose ph,
                                   protein *ret_prot,
                                   pool_timestamp *ret_ts,
                                   int64 *ret_index);

/**
 * Search forward in the pool for a protein with a descrip matching
 * that of the search argument. See pool_next for possible return
 * values.  On success (OB_OK), the hose's current index will be
 * 1 + *idx.  On failure (non-OB_OK), the hose's current index will
 * remain unchanged.
 * \note \a search is matched against candidate proteins using
 * protein_search().  Therefore, if \a search is a list, performs
 * slaw_list_gapsearch().  Otherwise, performs slaw_list_find().
 */
OB_PLASMA_API ob_retort pool_probe_frwd (pool_hose ph,
                                         bslaw search,
                                         protein *ret_prot,
                                         pool_timestamp *ret_ts,
                                         int64 *idx);

/**
 * The same as pool_probe_frwd(), but wait if necessary.  See
 * pool_await_next() comments for the meaning of the timeout
 * argument, and possible return values.
 * \note \a timeout is overall, and does not restart when a non-matching
 * protein is found.
 */
OB_PLASMA_API ob_retort pool_await_probe_frwd (pool_hose ph,
                                               bslaw search,
                                               pool_timestamp timeout,
                                               protein *ret_prot,
                                               pool_timestamp *ret_ts,
                                               int64 *idx);

/**
 * Search backward in the pool for a protein with a descrip matching
 * that of the search argument. If the beginning of the pool is
 * reached without finding a match, POOL_NO_SUCH_PROTEIN is returned.
 * On success (OB_OK), the hose's current index will be *idx.
 * On failure (non-OB_OK), the hose's current index will remain unchanged.
 */
OB_PLASMA_API ob_retort pool_probe_back (pool_hose ph,
                                         bslaw search,
                                         protein *ret_prot,
                                         pool_timestamp *ret_ts,
                                         int64 *idx);

/**
 * Fetch all or some of one or more proteins.  The \a ops array,
 * of length \a nops, describes the index of each protein to be fetched,
 * and which parts of it should be fetched.  On return, the \a ops
 * structures will be filled out with additional information, such as
 * the result (retort) of the operation, metadata about the protein,
 * and the protein itself, which, depending on the options specified,
 * may be a cut-down version of the actual protein in the pool.  (And,
 * it must be freed by the caller with protein_free().)
 *
 * As an added bonus, if non-NULL, \a oldest_idx_out and
 * \a newest_idx_out will be filled out with the oldest and newest
 * indexes, respectively.  If the pool is empty or an error
 * occurs, they will be set to a negative number-- specifically,
 * the retort indicating the error.
 */
OB_PLASMA_API void pool_fetch (pool_hose ph,
                               pool_fetch_op *ops,
                               int64 nops,
                               int64 *oldest_idx_out,
                               int64 *newest_idx_out);

/**
 * Same as pool_fetch(), but can optionally clamp the idx values to within
 * the range of valid proteins in the pool.  (i. e. if you request a protein
 * which is too old, you will get the oldest available protein, rather than
 * an error.)  If clamped, the idx member in \a ops will be changed to
 * reflect the index of the protein actually retrieved.
 */
OB_PLASMA_API void pool_fetch_ex (pool_hose ph,
                                  pool_fetch_op *ops,
                                  int64 nops,
                                  int64 *oldest_idx_out,
                                  int64 *newest_idx_out,
                                  bool clamp);
//@}

/**
 * \defgroup PoolGangs Multi-pool await support (gangs)
 * Multi-pool await allows a client to get the next available protein
 * in any of several pools.  Without this interface, a pools client
 * would have to busy-wait, checking all pools for a deposit once
 * every time interval.
 *
 * Multi-pool await operates on a pool gang.  The gang is constructed
 * by first creating an empty pool gang, then adding pool hoses to
 * the gang one by one.  Pool hoses can be removed from the gang one
 * by one, or the entire gang can be disbanded all at once.
 *
 * Functions in this group check the validity of their \a gang
 * argument, returning POOL_NULL_GANG if it's null.
 *
 * \ingroup PlasmaPoolsGroup
 */

/**
 * A collection of hoses, for multi-pool await support.
 * \see \ref PoolGangs
 * \ingroup PoolTypesAndConstants
 */
typedef struct pool_gang_struct *pool_gang;

/**
 * Create an empty pool gang.  When done with the gang, the caller must
 * call pool_disband_gang(), even if the pool gang has no gang
 * members. If \a gang is NULL, this function returns
 * OB_ARGUMENT_WAS_NULL.  OB_NO_MEM signals an error allocating memory.
 * \ingroup PoolGangs
 */
OB_PLASMA_API ob_retort pool_new_gang (pool_gang *gang);

/**
 * Add a pool hose to the gang.   A pool hose may only be a
 * member of one gang, and it may not join the
 * gang more than once: POOL_ALREADY_GANG_MEMBER is used
 * to signal attempts to re-add a hose.
 * If for some reason you want more than one
 * connection to the same pool in a pool gang, just open another
 * connection to that pool and add that to the pool gang.
 * \ingroup PoolGangs
 */
OB_PLASMA_API ob_retort pool_join_gang (pool_gang gang, pool_hose ph);

/**
 * Remove a pool hose from the gang. Returns POOL_NOT_A_GANG_MEMBER
 * if \a ph does not belong to \a gang.
 * \ingroup PoolGangs
 */
OB_PLASMA_API ob_retort pool_leave_gang (pool_gang gang, pool_hose ph);

/**
 * Disband the gang and free associated resources, with or without
 * current gang members.  If \a withdraw is \c true, each pool hose
 * will be withdrawn and freed.
 * \ingroup PoolGangs
 */
OB_PLASMA_API ob_retort pool_disband_gang (pool_gang gang, bool withdraw);

/**
 * Returns the number of members in \a gang.  Returns -1 if \a gang
 * is NULL.
 * \ingroup PoolGangs
 */
OB_PLASMA_API int64 pool_gang_count (pool_gang gang);

/**
 * Returns the \a n th hose in \a gang.  Returns NULL if \a gang is
 * NULL or \a n is out of range.
 * \ingroup PoolGangs
 */
OB_PLASMA_API pool_hose pool_gang_nth (pool_gang gang, int64 n);

/**
 * Retrieve the next protein available from one of these pools. Besides
 * the error codes listed in pool_next(), this function can return
 * POOL_EMPTY_GANG if \a gang contains no hose.
 *
 * If the ph argument is non-NULL, the pool hose of the pool the
 * protein was retrieved from will be returned in it. The only
 * output argument that must be non-null is \a ret_prot (otherwise,
 * OB_ARGUMENT_WAS_NULL is returned and \a gang and \a ph's states
 * are not modified).
 * \ingroup PoolGangs
 */
OB_PLASMA_API ob_retort pool_next_multi (pool_gang gang, pool_hose *ph,
                                         protein *ret_prot,
                                         pool_timestamp *ret_ts,
                                         int64 *ret_index);

/**
 * Retrieve the next protein available from one of these pools,
 * waiting if necessary.  See pool_await_next() for the definition of
 * timeout and possible error codes, and pool_next_multi() for
 * requirements on other parameters.
 * \ingroup PoolGangs
 */
OB_PLASMA_API ob_retort pool_await_next_multi (pool_gang gang,
                                               pool_timestamp timeout,
                                               pool_hose *ph, protein *ret_prot,
                                               pool_timestamp *ret_ts,
                                               int64 *ret_index);

/**
 * Wait until pool_await_next_multi() would have returned, but without
 * returning any information, and without advancing the index of the
 * pool hose.
 * \ingroup PoolGangs
 */
OB_PLASMA_API ob_retort pool_await_multi (pool_gang gang,
                                          pool_timestamp timeout);


/**
 * A signal-safe and thread-safe function to interrupt any call to
 * pool_await_next_multi() on this gang. For each time that this
 * function is called, one call to pool_await_next_multi() will return
 * with a pool_retort of POOL_AWAIT_WOKEN.
 *
 * Unlike bare pool hoses, all gangs have wakeup enabled. Thus there is
 * no need for an equivalent of pool_hose_enable_wakeup() for gangs.
 * \ingroup PoolGangs
 */
OB_PLASMA_API ob_retort pool_gang_wake_up (pool_gang gang);


/**
 * Allocates a new pool context and stores it in \a ctx.
 */
OB_PLASMA_API ob_retort pool_new_context (pool_context *ctx);

/**
 * Frees a pool context previously allocated with pool_new_context().
 *
 * \note You MUST NOT free a context if any hoses are still using the
 * context.
 */
OB_PLASMA_API void pool_free_context (pool_context ctx);

/**
 * Adds the options in \a opts (which is expected to be a slaw map)
 * to the context's options.  It is merged with the previous options,
 * so unchanged options do not need to be specified again.
 * \a opts is copied, so does not need to remain valid after the
 * function returns.
 */
OB_PLASMA_API ob_retort pool_ctx_set_options (pool_context ctx, bslaw opts);

/**
 * Returns the current options for \a ctx, as a slaw map.  The
 * return value is valid until pool_ctx_set_options() or pool_free_context()
 * is called on \a ctx.
 */
OB_PLASMA_API bslaw pool_ctx_get_options (pool_context ctx);

/**
 * Disables any hose-related atfork handlers that libPlasma would
 * normally set up.
 *
 * On Unix-like platforms, libPlasma defines an atfork handler to
 * clean up hose resources in the child process after a fork.  This is
 * normally safe on our supported platforms, but under some
 * circumstances, such as when multi-threaded and using the tcmalloc
 * allocator that comes with CEF, these atfork handlers are unsafe and
 * should be disabled.  However, if this handler is disabled, hoses
 * inherited from the parent process *must not* be used in the child
 * after a fork.
 */
OB_PLASMA_API void pool_disable_atfork_handlers (void);

/**
 * Enables libPlasma's hose-related atfork handlers.
 *
 * See caveat on pool_disable_atfork_handlers().
 */
OB_PLASMA_API void pool_enable_atfork_handlers (void);

#ifdef __cplusplus
}
#endif



#endif  // POOL_CHLORINE
