
/* (c)  oblong industries */

#ifndef LOAMXX_RETORTS_BLACKLIST
#define LOAMXX_RETORTS_BLACKLIST


#include <libLoam/c/ob-retorts.h>
#include <libLoam/c/ob-api.h>


// We have 100000 retorts to play with, from OB_RETORTS_LOAMXX_FIRST
// to OB_RETORTS_LOAMXX_LAST.

// ---------- failure codes (negative) ----------

/// Indicates a failure occurred when calling ArgParse::Parse().
/// Call ArgParse::ErrorMessage() to get details.
#define OB_ARGUMENT_PARSING_FAILED -(OB_RETORTS_LOAMXX_FIRST + 0)
/// Indicates construction of an object failed. Object should be treated
/// as invalid.
#define OB_CONSTRUCTION_FAILED     -(OB_RETORTS_LOAMXX_FIRST + 1)


// ---------- success codes (positive) ----------

/// return this as the sole action of a pseudoabstract interface definition...
#define OB_ABSTRACT_OK             (OB_RETORTS_LOAMXX_FIRST + 15)



/**
 * \cond INTERNAL
 * This is a symbol which can be referenced to force LoamxxRetorts.cpp
 * to be pulled into static libraries.  Without pulling it in, the
 * string conversion function will not be registered.
 */
OB_LOAMXX_API extern int ob_private_hack_pull_in_loamxx_retorts;
/** \endcond */


#endif /* LOAMXX_RETORTS_BLACKLIST */
