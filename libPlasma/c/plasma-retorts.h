
/* (c)  oblong industries */

#ifndef PLASMA_RETORTS_TOKAMAK
#define PLASMA_RETORTS_TOKAMAK

#include "libLoam/c/ob-retorts.h"

/* some subdivisions within the plasma space */
#define OB_RETORTS_PLASMA_POOLS (OB_RETORTS_PLASMA_FIRST)
#define OB_RETORTS_PLASMA_SLAW  (OB_RETORTS_PLASMA_FIRST + 10000)
#define OB_RETORTS_PLASMA_IO    (OB_RETORTS_PLASMA_FIRST + 20000)

/**
 * \name Slaw error codes
 * According to our general conventions, all these codes are
 * negative.
 */
//@{
/**
 * \ingroup SlawBasicAPI
 */
/** Protein is invalid/corrupt */
#define SLAW_CORRUPT_PROTEIN     -(OB_RETORTS_PLASMA_SLAW + 0)
/** Slaw is invalid/corrupt */
#define SLAW_CORRUPT_SLAW        -(OB_RETORTS_PLASMA_SLAW + 1)
/** Internal error (I think?) */
#define SLAW_FABRICATOR_BADNESS  -(OB_RETORTS_PLASMA_SLAW + 2)
/** Attempted to coerce a non-numeric slaw to a number */
#define SLAW_NOT_NUMERIC         -(OB_RETORTS_PLASMA_SLAW + 3)
/** Value is outside the range supported by the requested type */
#define SLAW_RANGE_ERR           -(OB_RETORTS_PLASMA_SLAW + 4)
/** An unknown/unsupported slaw was encountered */
#define SLAW_UNIDENTIFIED_SLAW   -(OB_RETORTS_PLASMA_SLAW + 5)
/** Incorrect length when coercing slaw to vector */
#define SLAW_WRONG_LENGTH        -(OB_RETORTS_PLASMA_SLAW + 6)
/** Requested item was not found */
#define SLAW_NOT_FOUND           -(OB_RETORTS_PLASMA_SLAW + 7)
/** YAML aliases are not allowed in slaw files */
#define SLAW_ALIAS_NOT_SUPPORTED -(OB_RETORTS_PLASMA_IO + 0)
/** Unexpected tag when parsing slaw from YAML file */
#define SLAW_BAD_TAG             -(OB_RETORTS_PLASMA_IO + 1)
/** End of file was encountered */
#define SLAW_END_OF_FILE         -(OB_RETORTS_PLASMA_IO + 2)
/** Error parsing slaw from YAML file */
#define SLAW_PARSING_BADNESS     -(OB_RETORTS_PLASMA_IO + 3)
/** File does not seem to be a binary slaw file */
#define SLAW_WRONG_FORMAT        -(OB_RETORTS_PLASMA_IO + 4)
/** Binary file is not a supported version */
#define SLAW_WRONG_VERSION       -(OB_RETORTS_PLASMA_IO + 5)
/** File was not valid YAML */
#define SLAW_YAML_ERR            -(OB_RETORTS_PLASMA_IO + 6)
/** libPlasma was built without YAML support */
#define SLAW_NO_YAML             -(OB_RETORTS_PLASMA_IO + 7)
//@}

// Error codes between
//     -(OB_RETORTS_PLASMA_POOLS + 2000)
// and
//     -(OB_RETORTS_PLASMA_POOLS + 3000)
// are reserved for internal use, and should not be seen by the user,
// and ditto for success codes in the equivalent positive range.
// See "internal ob_retorts" section of pool_impl.h.

/**
 * \name Pools-specific error codes
 */
//@{
/**
 * \ingroup PoolTypesAndConstants
 */
/** Couldn't find a directory to put pools in */
#define POOL_NO_POOLS_DIR             -(OB_RETORTS_PLASMA_POOLS + 400)
/** Some file-related op failed */
#define POOL_FILE_BADTH               -(OB_RETORTS_PLASMA_POOLS + 500)
/** pool_hose passed was NULL */
#define POOL_NULL_HOSE                -(OB_RETORTS_PLASMA_POOLS + 505)
/** Problem with semaphores */
#define POOL_SEMAPHORES_BADTH         -(OB_RETORTS_PLASMA_POOLS + 510)
/** mmap didn't work */
#define POOL_MMAP_BADTH               -(OB_RETORTS_PLASMA_POOLS + 520)
/** User tried to create an mmap pool on NFS */
#define POOL_INAPPROPRIATE_FILESYSTEM -(OB_RETORTS_PLASMA_POOLS + 525)
/** Tried to delete (or rename) a pool that was still in use */
#define POOL_IN_USE                   -(OB_RETORTS_PLASMA_POOLS + 530)
/** Unknown pool type */
#define POOL_TYPE_BADTH               -(OB_RETORTS_PLASMA_POOLS + 540)
/** Pool config file problem */
#define POOL_CONFIG_BADTH             -(OB_RETORTS_PLASMA_POOLS + 545)
/** Unexpected pool-version in config file */
#define POOL_WRONG_VERSION            -(OB_RETORTS_PLASMA_POOLS + 547)
/** Something about the pool itself is bad/invalid */
#define POOL_CORRUPT                  -(OB_RETORTS_PLASMA_POOLS + 548)
/** Invalid pool name */
#define POOL_POOLNAME_BADTH           -(OB_RETORTS_PLASMA_POOLS + 550)
/** Trying to rename a local pool to a network pool, or similar nonsense. */
#define POOL_IMPOSSIBLE_RENAME        -(OB_RETORTS_PLASMA_POOLS + 551)
/** Problem with fifos */
#define POOL_FIFO_BADTH               -(OB_RETORTS_PLASMA_POOLS + 555)
/** The size specified for a pool was not a number or beyond bounds */
#define POOL_INVALID_SIZE             -(OB_RETORTS_PLASMA_POOLS + 560)
/** No pool with this name */
#define POOL_NO_SUCH_POOL             -(OB_RETORTS_PLASMA_POOLS + 570)
/** Attempted to create existing pool. */
#define POOL_EXISTS                   -(OB_RETORTS_PLASMA_POOLS + 575)
/** Attempted to create pool "foo/bar" when pool "foo" exists, or vice versa. */
#define POOL_ILLEGAL_NESTING          -(OB_RETORTS_PLASMA_POOLS + 576)
/** Something unexpected happened in the network pool protocol. */
#define POOL_PROTOCOL_ERROR           -(OB_RETORTS_PLASMA_POOLS + 580)
/** The requested protein was not available */
#define POOL_NO_SUCH_PROTEIN          -(OB_RETORTS_PLASMA_POOLS + 635)
/** Await period expired */
#define POOL_AWAIT_TIMEDOUT           -(OB_RETORTS_PLASMA_POOLS + 640)
/** Await cancelled by wake() */
#define POOL_AWAIT_WOKEN              -(OB_RETORTS_PLASMA_POOLS + 650)
/** Attempted to wake a hose without having previously enabled wakeup. */
#define POOL_WAKEUP_NOT_ENABLED       -(OB_RETORTS_PLASMA_POOLS + 660)
/** Protein bigger than pool */
#define POOL_PROTEIN_BIGGER_THAN_POOL -(OB_RETORTS_PLASMA_POOLS + 700)
/** Tried to deposit to a "frozen" pool */
#define POOL_FROZEN                   -(OB_RETORTS_PLASMA_POOLS + 710)
/** Tried to deposit to full pool that does not allow wrapping */
#define POOL_FULL                     -(OB_RETORTS_PLASMA_POOLS + 720)
/** Tried to deposit a non-protein slaw */
#define POOL_NOT_A_PROTEIN            -(OB_RETORTS_PLASMA_POOLS + 800)
/** The options slaw was not a protein or map */
#define POOL_NOT_A_PROTEIN_OR_MAP     -(OB_RETORTS_PLASMA_POOLS + 810)
/** Writing config file failed */
#define POOL_CONF_WRITE_BADTH         -(OB_RETORTS_PLASMA_POOLS + 900)
/** Reading config file failed */
#define POOL_CONF_READ_BADTH          -(OB_RETORTS_PLASMA_POOLS + 910)
/** Problem sending over network */
#define POOL_SEND_BADTH               -(OB_RETORTS_PLASMA_POOLS + 1000)
/** Problem reading over network */
#define POOL_RECV_BADTH               -(OB_RETORTS_PLASMA_POOLS + 1010)
/** Remote end closed socket unexpectedly */
#define POOL_UNEXPECTED_CLOSE         -(OB_RETORTS_PLASMA_POOLS + 1015)
/** Problem making network socket */
#define POOL_SOCK_BADTH               -(OB_RETORTS_PLASMA_POOLS + 1020)
/** Network pool server busy */
#define POOL_SERVER_BUSY              -(OB_RETORTS_PLASMA_POOLS + 1030)
/** Network pool server unreachable */
#define POOL_SERVER_UNREACH           -(OB_RETORTS_PLASMA_POOLS + 1040)
/** Pool hose already part of a gang */
#define POOL_ALREADY_GANG_MEMBER      -(OB_RETORTS_PLASMA_POOLS + 1050)
/** Pool hose is not a member of a given gang */
#define POOL_NOT_A_GANG_MEMBER        -(OB_RETORTS_PLASMA_POOLS + 1055)
/** pool_next_multi() called on an empty gang */
#define POOL_EMPTY_GANG               -(OB_RETORTS_PLASMA_POOLS + 1060)
/** A NULL gang was passed to any of the gang functions */
#define POOL_NULL_GANG                -(OB_RETORTS_PLASMA_POOLS + 1070)
/** The pool type does not support what you want to do to it. */
#define POOL_UNSUPPORTED_OPERATION    -(OB_RETORTS_PLASMA_POOLS + 1100)
/** A hose created before a fork is no longer valid in the child. */
#define POOL_INVALIDATED_BY_FORK      -(OB_RETORTS_PLASMA_POOLS + 1110)
/** libPlasma was built without TLS support, or server does not support it */
#define POOL_NO_TLS                   -(OB_RETORTS_PLASMA_POOLS + 1500)
/** client does not want to use TLS, but server requires it */
#define POOL_TLS_REQUIRED             -(OB_RETORTS_PLASMA_POOLS + 1505)
/** Something went wrong with TLS... not very specific */
#define POOL_TLS_ERROR                -(OB_RETORTS_PLASMA_POOLS + 1510)
/** Greenhouse-enabled client tried to connect to non-Greenhouse server */
#define POOL_NOT_A_GREENHOUSE_SERVER  -(OB_RETORTS_PLASMA_POOLS + 1600)
//@}

/**
 * \name Success codes
 */
//@{
/**
 * \ingroup PoolTypesAndConstants
 * A pool was successfully created
 */
#define POOL_CREATED                  (OB_RETORTS_PLASMA_POOLS + 10)
//@}

#endif /* PLASMA_RETORTS_TOKAMAK */
