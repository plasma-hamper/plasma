
/* (c)  oblong industries */

/**
 * \file
 * \ingroup PlasmaSlawGroup
 */

#ifndef SLAW_UPRISINGS
#define SLAW_UPRISINGS

#include "libLoam/c/ob-attrs.h"
#include "libLoam/c/ob-types.h"
#include "libLoam/c/ob-retorts.h"
#include "libLoam/c/ob-api.h"
#include "libPlasma/c/plasma-types.h"
#include "libPlasma/c/plasma-retorts.h"

#include <stdio.h>


#ifdef __cplusplus
extern "C" {
#endif

/**
 * \name Slaw proteins
 */
//@{
/**
 * \ingroup SlawBasicAPI
 * Protein with correct byte order
 */
OB_PLASMA_API bool slaw_is_protein (bslaw s);
/**
 * \ingroup SlawBasicAPI
 * Protein with incorrect byte order
 */
OB_PLASMA_API bool slaw_is_swapped_protein (bslaw s);
//@}

/**
 * Creates a new slabu, which you can subsequently use
 * to incrementally build a list or map.
 * \ingroup Slabu
 */
OB_PLASMA_API slabu *slabu_new (void);

/**
 * Return the number of elements currently in the slabu, or -1 if
 * \a sb is \c NULL.
 * \ingroup Slabu
 */
OB_PLASMA_API int64 slabu_count (const slabu *sb);

/**
 * Return the nth element of the slabu. If \a sb is \c NULL or
 * \a n is out of bounds (no negative values are allowed),
 * this function returns \c NULL.
 * \ingroup Slabu
 */
OB_PLASMA_API bslaw slabu_list_nth (const slabu *sb, int64 n);

/**
 * Free the slabu itself, and any non-_z elements it contains.
 * This is a no-op if \a sb is \c NULL.
 * \ingroup Slabu
 */
OB_PLASMA_API void slabu_free (slabu *sb);

/**
 * Convenience macro freeing a slabu and setting it to \c NULL
 * in one shot.
 * \ingroup Slabu
 */
#define Free_Slabu(sb)                                                         \
  do                                                                           \
    {                                                                          \
      slabu_free (sb);                                                         \
      (sb) = NULL;                                                             \
    }                                                                          \
  while (0)

/**
 * Creates a new slabu that contains the contents of an
 * existing slaw list. If \a list is \c NULL, a new empty
 * slabu is returned.
 * \ingroup Slabu
 */
OB_PLASMA_API slabu *slabu_from_slaw (bslaw list);

/**
 * Creates a new slabu that contains the contents of an
 * existing slaw list, and frees the existing list.
 * If \a list is \c NULL, returns an empty slabu.
 * \ingroup Slabu
 */
OB_PLASMA_API slabu *slabu_from_slaw_f (slaw list);

/**
 * Creates a "deep copy" of the slabu (i. e. all the elements
 * in the new slabu are newly-allocated copies of whatever
 * elements were in the old slabu).
 * Returns \c NULL when \a sb is \c NULL.
 * \ingroup Slabu
 */
OB_PLASMA_API slabu *slabu_dup (const slabu *sb);

/**
 * \name List construction using slabus
 * slabu_list_add() takes a slabu on which to
 * operate as its first argument and a slaw to add to the list as
 * its second. Returns the index of the newly-added slaw on
 * success, or OB_NO_MEM on failure.
 *
 * Any slaw may be added to a list, including the nil slaw.
 *
 * The basic slabu_list_add() functions dups its slaw argument,
 * so that a copy of the slaw is added to the list. The _f
 * instance dups, then frees, its slaw argument. The _c instance
 * constructs a slaw from a const char*.
 *
 * Two specialized suffixes, _z and _x, allow one to sidestep the
 * dup performed by the basic and _f implementations. Think "zero
 * copy" for _z: the slabu holds onto the passed slaw (as a
 * reference) until the slaw_list is created. The _x instance
 * does the same thing, but frees the slaw upon list
 * creation. Note that these two methods are dangerous. If the
 * slaw is freed after being passed to the slabu but before the
 * list is created, Bad Things happen. If some other piece of
 * code retains a reference to a slaw passed to the _x instance,
 * that reference will stay valid until the list is created, but
 * become toxic then, as the slaw is freed. It is also an error
 * to use the same slaw in multiple calls to slabu_list_add_x:
 * in would be deleted multiple times.
 *
 * In all cases, both arguments must be non-null;
 * otherwise, OB_ARGUMENT_WAS_NULL is returned. A return value
 * greater than or equal to zero indicates success.
 */
//@{
/**
 * \ingroup Slabu
 */
OB_PLASMA_API int64 slabu_list_add (slabu *sb, bslaw s);
OB_PLASMA_API int64 slabu_list_add_f (slabu *sb, slaw s);
OB_PLASMA_API int64 slabu_list_add_c (slabu *sb, const char *str);
OB_PLASMA_API int64 slabu_list_add_z (slabu *sb, bslaw s);
OB_PLASMA_API int64 slabu_list_add_x (slabu *sb, slaw s);
//@}

/**
 * \name List element removal in slabus
 * Remove an item from the slabu list. The slaw argument is
 * compared against the slawx in the list with slawx_equal(),
 * starting at index zero; the first list item slawx_equal to the
 * argument is removed. Returns OB_OK if a match was found (and an
 * item removed), SLAW_NOT_FOUND otherwise.
 */
//
/**
 * The slaw that is removed from the list is freed internally,
 * unless it was added using a _z method.
 *
 * The _f and _c instances have their normal connotations: _f
 * frees the slaw after it is used by the method; _c constructs a
 * slaw from a const char*.
 *
 * In all cases, both arguments must be non-null;
 * otherwise, OB_ARGUMENT_WAS_NULL is returned.
 */
//@{
/**
 * \ingroup Slabu
 */
OB_PLASMA_API ob_retort slabu_list_remove (slabu *sb, bslaw s);
OB_PLASMA_API ob_retort slabu_list_remove_f (slabu *sb, slaw s);
OB_PLASMA_API ob_retort slabu_list_remove_c (slabu *sb, const char *str);

/**
 * Removes an item from the slabu list given its position.
 * The int64 argument is an
 * index into the list (with negative values wrapping backwards);
 * the slaw in that position is removed. Returns OB_OK on
 * success, OB_BAD_INDEX if the int64 argument is greater than
 * the length of the list or less than the negated length of the
 * list, minus one. \a sb must be non-null; otherwise,
 * OB_ARGUMENT_WAS_NULL is returned.
 *
 * The slaw that is removed from the list is freed internally,
 * unless it was added using a _z method.
 */
OB_PLASMA_API ob_retort slabu_list_remove_nth (slabu *sb, int64 nth);
//@}

/**
 * \name Slabu list element lookup
 * Look for a slaw in a list. The slaw argument is compared
 * against the slawx in the list with slawx_equal(), starting at
 * index zero. Returns the index of the first list item
 * slawx_equal to the argument, or SLAW_NOT_FOUND. (A return
 * value greater than or equal to zero indicates that a matching
 * slaw was found in the list.)
 *
 * In all cases, both arguments must be non-null;
 * otherwise, OB_ARGUMENT_WAS_NULL is returned.
 */
//@{
/**
 * \ingroup Slabu
 */
OB_PLASMA_API int64 slabu_list_find (const slabu *sb, bslaw s);
OB_PLASMA_API int64 slabu_list_find_f (const slabu *sb, slaw s);
OB_PLASMA_API int64 slabu_list_find_c (const slabu *sb, const char *str);
//@}

/**
 * \name Slabu list element insertion
 * Insert a slaw into the middle of a list. Behaves as
 * slabu_list_add() except that the new item is inserted into
 * position \a nth, pushing all subsequent entries back one
 * index. Given an index equal to the length of the list, or a
 * negative one, this method is functionally equivalent to
 * slabu_list_add(). Returns the (positive) index of the
 * newly-added slaw on success, or OB_NO_MEM or OB_BAD_INDEX on
 * failure. (A return value greater than or equal to zero
 * indicates success.)
 *
 * In all cases, both \a sb and \a s must be non-null;
 * otherwise, OB_ARGUMENT_WAS_NULL is returned.
 */
//@{
/**
 * \ingroup Slabu
 */
OB_PLASMA_API int64 slabu_list_insert (slabu *sb, int64 nth, bslaw s);
OB_PLASMA_API int64 slabu_list_insert_f (slabu *sb, int64 nth, slaw s);
OB_PLASMA_API int64 slabu_list_insert_c (slabu *sb, int64 nth, const char *s);
OB_PLASMA_API int64 slabu_list_insert_z (slabu *sb, int64 nth, bslaw s);
OB_PLASMA_API int64 slabu_list_insert_x (slabu *sb, int64 nth, slaw s);
//@}

/**
 * \name Slabu list element replacement
 * Replace a slaw at a particular position in the list. Behaves
 * as slabu_list_remove_nth() followed by
 * slabu_list_insert(). Returns the (positive) index of the
 * newly-added slaw on success, or OB_NO_MEM or OB_BAD_INDEX on
 * failure. (A return value greater than or equal to zero
 * indicates success.)
 *
 * In all cases, both \a sb and \a s must be non-null;
 * otherwise, OB_ARGUMENT_WAS_NULL is returned.
 */
//@{
/**
 * \ingroup Slabu
 */
OB_PLASMA_API int64 slabu_list_replace_nth (slabu *sb, int64 nth, bslaw s);
OB_PLASMA_API int64 slabu_list_replace_nth_f (slabu *sb, int64 nth, slaw s);
OB_PLASMA_API int64 slabu_list_replace_nth_c (slabu *sb, int64 nth,
                                              const char *s);
OB_PLASMA_API int64 slabu_list_replace_nth_z (slabu *sb, int64 nth, bslaw s);
OB_PLASMA_API int64 slabu_list_replace_nth_x (slabu *sb, int64 nth, slaw s);
//@}

/**
 * Checks the mappishness of a slabu. Returns true if all of the
 * slabu's elements are conses and each cons has a unique car (as
 * determined by slawx_equal()). A \c NULL \a sb is not a map.
 * \ingroup Slabu
 */
OB_PLASMA_API bool slabu_is_map (const slabu *sb);

/**
 * Transforms a slabu into a valid map.  Iterates across the slabu,
 * discarding any elements that are not conses and any conses with
 * duplicate cars.  (If there are duplicates, the *value* of the last
 * duplicate is retained, in the *position* of the first duplicate.
 * This matches the result you would have gotten if you'd been using
 * slabu_map_put all along.)  Returns OB_NOTHING_TO_DO if the slabu
 * was a valid map on entry (if no transformation was required).  If
 * the slabu was not a valid map but was successfully converted to
 * one, returns OB_OK.  Returns OB_ARGUMENT_WAS_NULL if \a sb was
 * \c NULL.
 * \ingroup Slabu
 */
OB_PLASMA_API ob_retort slabu_map_conform (slabu *sb);

/**
 * \name Map key/value insertion.
 * Insert a key/value pair into the list. This operation is
 * equivalent to a put or set operation on a hash/map/dictionary
 * data structure: if an entry with a matching key (compared
 * using slawx_equal()) exists in the list, that entry is
 * replaced, otherwise a new entry is added. The key and value
 * that are passed in are consed together to form the new
 * entry. Returns the index in the list of the new entry on
 * success, OB_NO_MEM on failure trying to allocate memory, or
 * OB_ARGUMENT_WAS_NULL if any of the passed arguments is \c NULL. (A return
 * value greater than or equal to zero indicates success.)
 *
 * The l, f, and c suffixes have their normal connotations, are
 * used two at a time, and apply to the key and value
 * arguments.  l indicates that the argument slaw is used but not
 * modified, f frees the slaw after it is used by the method; c
 * constructs a slaw from a const char*.
 *
 * It is safe to use the same slaw as the key and the value.
 */
//@{
/**
 * \ingroup Slabu
 */
OB_PLASMA_API int64 slabu_map_put (slabu *sb, bslaw key, bslaw value);
OB_PLASMA_API int64 slabu_map_put_ff (slabu *sb, slaw key, slaw value);
OB_PLASMA_API int64 slabu_map_put_lf (slabu *sb, bslaw key, slaw value);
OB_PLASMA_API int64 slabu_map_put_fl (slabu *sb, slaw key, bslaw value);
OB_PLASMA_API int64 slabu_map_put_cl (slabu *sb, const char *key, bslaw value);
OB_PLASMA_API int64 slabu_map_put_cf (slabu *sb, const char *key, slaw value);
OB_PLASMA_API int64 slabu_map_put_cc (slabu *sb, const char *key,
                                      const char *value);
//@}

/**
 * \name Slabu map lookups.
 * Find an entry in a map. Compares the key (using slawx_equal())
 * to the car of list entries that are conses, starting from the highest
 * index, working down to zero.  If a matching key is found, its cdr is
 * returned. Otherwise the return value is \c NULL. If the index_out
 * argument is non-NULL and a match is found, *index_out is set
 * to the index of the matching cons, otherwise *index_out is not
 * modified. Note that slabu_map_find() is happy to operate on a
 * non-map slabu; the method simply ignores elements that are not
 * cons cells, (and returns the highest-index match, though there
 * may be duplicate keys).
 *
 * If any of \a sb or \a key is \c NULL, the function returns \c NULL.
 */
//@{
/**
 * \ingroup Slabu
 */
OB_PLASMA_API bslaw slabu_map_find (const slabu *sb, bslaw key,
                                    int64 *index_out);
OB_PLASMA_API bslaw slabu_map_find_f (const slabu *sb, slaw key,
                                      int64 *index_out);
OB_PLASMA_API bslaw slabu_map_find_c (const slabu *sb, const char *key,
                                      int64 *index_out);
//@}

/**
 * \name Slabu map element removal.
 * Remove an entry from a map. Searches for a key match as in
 * slabu_map_find(), and if one is found, removes the matching
 * entry. Returns OB_OK if a match was found (and an entry
 * removed), SLAW_NOT_FOUND otherwise (unless either \a sb or \a key
 * is \c NULL, in which case OB_ARGUMENT_WAS_NULL is returned).
 */
//@{
/**
 * \ingroup Slabu
 */
OB_PLASMA_API ob_retort slabu_map_remove (slabu *sb, bslaw key);
OB_PLASMA_API ob_retort slabu_map_remove_f (slabu *sb, slaw key);
OB_PLASMA_API ob_retort slabu_map_remove_c (slabu *sb, const char *key);
//@}

/**
 * \name Copying and freeing slawx
 */
//@{
/**
 * \ingroup SlawBasicAPI
 * Creates an exact copy of an existing slaw.
 */
OB_PLASMA_API slaw slaw_dup (bslaw s);

/**
 * \ingroup SlawBasicAPI
 * Creates an exact copy of an existing slaw, and frees the original.
 */
OB_PLASMA_API slaw slaw_dup_f (slaw s);

/**
 * \ingroup SlawBasicAPI
 * Frees a slaw. It is safe to pass a \c NULL \a s.
 */
OB_PLASMA_API void slaw_free (slaw s);

/**
 * \ingroup SlawBasicAPI
 * Frees \a s and sets it to \c NULL.
 */
#define Free_Slaw(s)                                                           \
  do                                                                           \
    {                                                                          \
      slaw_free (s);                                                           \
      (s) = NULL;                                                              \
    }                                                                          \
  while (0)
//@}

/**
 * \defgroup SlawBooleanNil Boolean and nil slawx
 * Truth and being
 * \ingroup PlasmaSlawGroup
 */

/**
 * \name Boolean slawx
 */
//@{
/**
 * \ingroup SlawBooleanNil
 */
OB_PLASMA_API slaw slaw_boolean (bool value);
OB_PLASMA_API bool slaw_is_boolean (bslaw s);
OB_PLASMA_API const bool *slaw_boolean_emit (bslaw s);
//@}

/**
 * \name Nil slawx
 * This type has a single value, denoting, well, a nil.
 */
//@{
/**
 * \ingroup SlawBooleanNil
 * Creates a new slaw of type "nil" (which is a distinct type,
 * but has no associated value).
 */
OB_PLASMA_API slaw slaw_nil (void);
/**
 * Returns true if the slaw is of type "nil"; i. e. was created
 * by the function slaw_nil(). Note that a \c NULL slaw is not nil.
 * \sa slaw_nil
 */
OB_PLASMA_API bool slaw_is_nil (bslaw s);
//@}

/**
 * Returns the length of a slaw, in bytes. If \a s is \c NULL, we return
 * -1.
 * \ingroup SlawBasicAPI
 */
OB_PLASMA_API int64 slaw_len (bslaw s);

/**
 * Returns a 64-bit hash code for \a s.
 */
OB_PLASMA_API unt64 slaw_hash (bslaw s);

/**
 * \name Slaw equality
 * Functions to compare slawx
 */
//@{
/**
 * \ingroup SlawBasicAPI
 * Returns true if two slawx are exactly, byte-for-byte equal,
 * or if both s1 and s2 are \c NULL.
 */
OB_PLASMA_API bool slawx_equal (bslaw s1, bslaw s2);

/**
 * Returns true if two slawx are exactly, byte-for-byte equal,
 * and frees \a s2 in any case.
 */
OB_PLASMA_API bool slawx_equal_lf (bslaw s1, slaw s2);

/**
 * Returns true if \a s is a slaw string whose string is equal
 * to \a str.
 */
OB_PLASMA_API bool slawx_equal_lc (bslaw s, const char *str);
//@}

/**
 * \defgroup SlawStrings Slaw strings
 * NULL-terminated strings represented as an atomic slaw.
 * \ingroup PlasmaSlawGroup
 */

/**
 * Creates a new slaw of type "string", containing the specified
 * string, which is expected to be a NUL-terminated, UTF-8
 * encoded string. Returns \c NULL if \a str is \c NULL.
 * \note For non-NUL terminated strings, try
 * slaw_string_from_substring() instead.
 * \ingroup SlawStrings
 */
OB_PLASMA_API slaw slaw_string (const char *str);

/**
 * Produces a slaw string, given a pointer to a UTF-8
 * string and a length in bytes.
 *  \param[in] str points to the start of the string to copy
 *  \param[in] len is the number of bytes (not code points!) to copy
 * \return newly allocated slaw string, or \c NULL if \a str is \c NULL or
 *         \a len is less than zero.
 * \ingroup SlawStrings
 */
OB_PLASMA_API slaw slaw_string_from_substring (const char *str, int64 len);

/**
 * Returns true if the slaw is of type "string".
 * \ingroup SlawStrings
 */
OB_PLASMA_API bool slaw_is_string (bslaw s);

/**
 * If the specified slaw is of type "string", returns the
 * NUL-terminated, UTF-8 encoded string inside it.  If the
 * slaw is not of type "string", returns \c NULL.
 * \note The returned string is a pointer to inside the
 * slaw, so its lifetime will be for as long as the slaw
 * exists.
 * \ingroup SlawStrings
 */
OB_PLASMA_API const char *slaw_string_emit (bslaw s);

/**
 * If the specified slaw is of type "string", returns the
 * length of the UTF-8 encoded string inside it, not including
 * the terminating NUL.  If the slaw is not of type "string",
 * returns -1.
 * \ingroup SlawStrings
 */
OB_PLASMA_API int64 slaw_string_emit_length (bslaw s);

/**
 * \name String concatenation
 * These functions concatenate all their string arguments
 * up to encountering \c NULL value. Non-string slawx are ignored
 * but do not mark the end of the list: they're just skipped. If
 * no slaw is a string, or the very first argument is \c NULL,
 * a slaw representing string ("\0") is returned. All slaw
 * arguments are freed in the _f variants (regardless of the result),
 * but you can pass duplicate arguments without causing a double deallocation.
 */
//@{
/**
 * \ingroup SlawStrings
 */
OB_PLASMA_API slaw slaw_strings_concat_f (slaw s1, ...) OB_SENTINEL;
OB_PLASMA_API slaw slaw_strings_concat (bslaw s1, ...) OB_SENTINEL;
OB_PLASMA_API slaw slaw_strings_concat_c (const char *str1, ...) OB_SENTINEL;


// XXX: OB_SENTINEL requires two NULLs in the case where
// someone is concatenating no strings (i. e. str1 is NULL and there
// are no variable arguments) like this:
//   slaw_string_concat_cstrings_f (slaw_dup (s3), NULL, NULL)
// instead of this:
//   slaw_string_concat_cstrings_f (slaw_dup (s3), NULL)
// not sure what the proper fix for this is.
// It looks like this is a known problem; maybe gcc will fix it:
// http://marc.info/?l=gcc&m=115022350304807&w=2
OB_PLASMA_API slaw slaw_string_concat_cstrings_f (slaw s, const char *str1,
                                                  ...) OB_SENTINEL;
OB_PLASMA_API slaw slaw_string_concat_cstrings (bslaw s, const char *str1,
                                                ...) OB_SENTINEL;
//@}

/**
 * \defgroup SlawCons Slaw conses
 * A cons contains exactly two elements, of any type.
 * \ingroup PlasmaSlawGroup
 */

/**
 * \name Slaw cons checks and accessors
 */
//@{
/**
 * \ingroup SlawCons
 */
OB_PLASMA_API bool slaw_is_cons (bslaw s);
OB_PLASMA_API bslaw slaw_cons_emit_car (bslaw s);
OB_PLASMA_API bslaw slaw_cons_emit_cdr (bslaw s);
//@}

/**
 * \name Slaw Cons constructors
 * Both \a car and \a cdr must be non-null; otherwise, a \c NULL slaw
 * will be returned. Non-NULL slaw arguments are always freed in _f
 * variants, regardless of whether the construction succeeds or not.
 */
//@{
/**
 * \ingroup SlawCons
 */
OB_PLASMA_API slaw slaw_cons (bslaw car, bslaw cdr);
OB_PLASMA_API slaw slaw_cons_ff (slaw car, slaw cdr);
OB_PLASMA_API slaw slaw_cons_lf (bslaw car, slaw cdr);
OB_PLASMA_API slaw slaw_cons_fl (slaw car, bslaw cdr);
OB_PLASMA_API slaw slaw_cons_cl (const char *car, bslaw cdr);
OB_PLASMA_API slaw slaw_cons_cf (const char *car, slaw cdr);
OB_PLASMA_API slaw slaw_cons_lc (bslaw car, const char *cdr);
OB_PLASMA_API slaw slaw_cons_fc (slaw car, const char *cdr);
OB_PLASMA_API slaw slaw_cons_cc (const char *car, const char *cdr);
OB_PLASMA_API slaw slaw_cons_ca (const char *car, const unt8 *cdr, int64 N);
//@}

/**
 * \defgroup SlawList Slaw lists
 * A list contains 0 or more slawx, of any type. It's a typical
 * linear structure with O(n) access time.
 * \ingroup PlasmaSlawGroup
 */

/**
 * \name Slaw list constructors
 */
//@{
/**
 * \ingroup SlawList
 * Creates a list from the given builder, unless \a sb is \c NULL
 * (in which case \c NULL is returned). Afterwards, the builder is
 * freed.
 */
OB_PLASMA_API slaw slaw_list_f (slabu *sb);

/**
 * Creates a list from the given builder, unless \a sb is \c NULL
 * (in which case \c NULL is returned). The builder is not modified.
 */
OB_PLASMA_API slaw slaw_list (const slabu *sb);

/**
 * Creates a list from the given explicitly provided slawx, which
 * are subsequently freed. The first \c NULL argument marks the
 * end of the list. Therefore, if \a s1 is \c NULL, you'll get an
 * empty list (not a \c NULL). If an error occurs during list creation
 * (e.g. out of memory), the return value will be \c NULL, but
 * all passed slawx will be deleted anyway. This behaviour allows
 * safe usage of temporary slawx, as shown in the following snippet:
 * \code
 *    slaw list = slaw_slaw_list_inline_f (slaw_string ("foo"),
 *                                         slaw_int32 (12),
 *                                         slaw_boolean (false),
 *                                         NULL);
 *    if (!list) handle_error (); // no leak anyway
 * \endcode
 * But you can pass duplicate arguments without causing a double free.
 */
OB_PLASMA_API slaw slaw_list_inline_f (slaw s1, ...) OB_SENTINEL;

/**
 * Works as slaw_list_inline_f(), but the passed slawx are not freed.
 */
OB_PLASMA_API slaw slaw_list_inline (bslaw s1, ...) OB_SENTINEL;

/**
 * Works as slaw_list_inline(), creating string slawx from the
 * given ones.
 */
OB_PLASMA_API slaw slaw_list_inline_c (const char *first_str, ...) OB_SENTINEL;

/**
 * Convenience macro to create an empty list.
 */
#define slaw_list_empty() slaw_list_inline (NULL, NULL)
//@}

/**
 * Returns \c true if the slaw is of type "list".  Unless you
 * really care deeply about the type, use slaw_is_list_or_map() instead.
 * \ingroup SlawList
 */
OB_PLASMA_API bool slaw_is_list (bslaw s);

/**
 * Returns \c true if either slaw_is_list() or slaw_is_map() would
 * return \c true.  Since all slaw list functions can be used on slaw
 * maps (treating it as a list of conses of key/value pairs), and
 * all slaw map functions can be used on slaw lists if the list
 * is a list of conses, this is generally the function you want
 * to use when testing for lists or maps.
 * \ingroup SlawList
 */
OB_PLASMA_API bool slaw_is_list_or_map (bslaw s);

/**
 * Returns the number of elements in a slaw list or a slaw map.
 * If \a s is not a list (e.g. it's a cons, or \c NULL), this
 * function returns -1.
 * \ingroup SlawList
 */
OB_PLASMA_API int64 slaw_list_count (bslaw s);

/**
 * Returns the first (well, 0th if you want to be that way)
 * element of a slaw list or map.  (Where in a map, the element
 * will be a cons of key and value.)  Returns \c NULL if s
 * is not a list or map, or if there is no first element (i. e.
 * it is empty or \c NULL).
 * \ingroup SlawList
 */
OB_PLASMA_API OB_WARN_UNUSED_RESULT bslaw slaw_list_emit_first (bslaw s);

/**
 * Returns the element in \a s_list which immediately follows
 * \a s_prev, which must have been returned by slaw_list_emit_first()
 * or a previous call to slaw_list_emit_next().  This is the
 * most efficient way to iterate over a slaw list or map.
 * If \a s_prev is NULL, behaves like slaw_list_emit_first().
 * Returns \c NULL if there are no more elements.
 * \ingroup SlawList
 */
OB_PLASMA_API OB_WARN_UNUSED_RESULT bslaw slaw_list_emit_next (bslaw s_list,
                                                               bslaw s_prev);

/**
 * Returns the nth element of a list, where n=0 is the same
 * as the element returned by slaw_list_emit_first().
 * Note that a single call to slaw_list_emit_nth() is O(n),
 * because it must start at the beginning of the list
 * each time.  Thus, if you use this function to iterate
 * over a slaw list, your loop will be O(n**2).  For
 * more efficient O(n) iteration, use slaw_list_emit_first()
 * and slaw_list_emit_next() instead.
 * \ingroup SlawList
 */
OB_PLASMA_API OB_WARN_UNUSED_RESULT bslaw slaw_list_emit_nth (bslaw s, int64 n);

/**
 * \name Slaw list concatenation
 * These functions concatenate the passed lists. The first \c NULL
 * argument marks the end of the lists to concatenate. Non-null arguments
 * which are not lists are ignored. Thus, if none of the arguments
 * is a list, or the first one is NULL, an empty list is returned:
 * you only get a NULL when there's an error allocating memory for
 * the new list. In the _f variety, all arguments up to the first
 * NULL are always freed, but you can pass duplicate arguments
 * without causing a double deallocation.
 */
//@{
/**
 * \ingroup SlawList
 */
OB_PLASMA_API slaw slaw_lists_concat_f (slaw s1, ...) OB_SENTINEL;
OB_PLASMA_API slaw slaw_lists_concat (bslaw s1, ...) OB_SENTINEL;
//@}

/**
 * \name Find an element in a list
 * These functions look up \a val in the list \a s.  If \a s is not
 * a list or map, or if the value is not found, they return -1.
 * Otherwise, the index of \a val (or \a str converted to a slaw)
 * in \a s is returned.
 * Value equality is checked using slawx_equal().
 */
//@{
/**
 * \ingroup SlawList
 */
OB_PLASMA_API int64 slaw_list_find (bslaw s, bslaw val);
OB_PLASMA_API int64 slaw_list_find_f (bslaw s, slaw val);
OB_PLASMA_API int64 slaw_list_find_c (bslaw s, const char *str);
//@}

/**
 * \name Finding contiguous sublists
 * These functions look for the slaw list \a search in the
 * slaw list \a s. That is, they return a non-negative index
 * when all the sub-slawx of \a search appear, in the same
 * order and without intervening gaps, in \a s, starting at the
 * returned value. All plain slaw arguments are freed after the
 * search, regardless of its result, but you can pass duplicate
 * arguments without causing a double deallocation.
 * In the inline variant, the list of slawx to be sought for
 * is provided explicitly, instead of wrapped in a slaw list.
 */
//@{
/**
 * \ingroup SlawList
 */
OB_PLASMA_API int64 slaw_list_contigsearch (bslaw s, bslaw search);
OB_PLASMA_API int64 slaw_list_contigsearch_f (bslaw s, slaw search);
OB_PLASMA_API int64 slaw_list_contigsearch_inline (bslaw s,
                                                   /* bslaw s1, */
                                                   ...) OB_SENTINEL;
OB_PLASMA_API int64 slaw_list_contigsearch_inline_f (bslaw s,
                                                     /* slaw s1, */
                                                     ...) OB_SENTINEL;
OB_PLASMA_API int64 slaw_list_contigsearch_inline_c (bslaw s,
                                                     /* const char *str1, */
                                                     ...) OB_SENTINEL;
//@}

/**
 * \name Finding non-contiguous sublists
 * These functions work as the slaw_list_contigsearch() family
 * with the difference that the sought for slawx don't need
 * to be contiguous, only to appear in \a s in the same order
 * (but, possibly, with other slawx in between).
 * Non-NULL slaw arguments are always freed in _f
 * variants, regardless of whether the construction succeeds or not,
 * but duplicate arguments are allowed (they won't cause a double free).
 */
//@{
/**
 * \ingroup SlawList
 */
OB_PLASMA_API int64 slaw_list_gapsearch (bslaw s, bslaw search);
OB_PLASMA_API int64 slaw_list_gapsearch_f (bslaw s, slaw search);
OB_PLASMA_API int64 slaw_list_gapsearch_inline (bslaw s,
                                                /* bslaw s1, */
                                                ...) OB_SENTINEL;
OB_PLASMA_API int64 slaw_list_gapsearch_inline_f (bslaw s,
                                                  /* slaw s1, */
                                                  ...) OB_SENTINEL;
OB_PLASMA_API int64 slaw_list_gapsearch_inline_c (bslaw s,
                                                  /* const char *str1, */
                                                  ...) OB_SENTINEL;
//@}

/**
 * \defgroup SlawMap Slaw maps
 * A list of cons cells, car of each cons unique. This container
 * behaves as a list, i.e., it provides O(n) lookups.
 * \ingroup PlasmaSlawGroup
 */

/**
 * Returns true if the slaw is of type "map".  Unless you
 * really care deeply about the type, use
 * slaw_is_list_or_map() instead.
 * \ingroup SlawMap
 */
OB_PLASMA_API bool slaw_is_map (bslaw s);

/**
 * \name Map construction
 * Make the map: check mappiness & conform if necessary.
 * The semantics are equivalent to calling slabu_map_conform(), in that
 * any non-cons elements are discarded, and duplicate keys are resolved by
 * retaining the value of the last duplicate, in the position of the
 * first duplicate.
 * As usual, a \c NULL \a sb produces a \c NULL slaw as a result, instead
 * of a map.
 */
//@{
/**
 * \ingroup SlawMap
 */
OB_PLASMA_API slaw slaw_map (const slabu *sb);
OB_PLASMA_API slaw slaw_map_f (slabu *sb);
/**
 * Creates a non-NULL slaw that is a map but has no key/value pairs.
 */
OB_PLASMA_API slaw slaw_map_empty (void);
//@}

/**
 * \name Inline map construction
 * These functions create new maps from the provided key/value
 * slawx. A \c NULL key marks the end of the sequence. An even number
 * of non-NULL arguments is expected, otherwise an empty map is returned.
 * As usual, the \c _f variants free all their slaw arguments, even
 * in the presence of errors. It is safe to pass duplicate arguments.
 */
//@{
/**
 * \ingroup SlawMap
 */
OB_PLASMA_API slaw slaw_map_inline (bslaw key1, bslaw val1, ...) OB_SENTINEL;
OB_PLASMA_API slaw slaw_map_inline_ff (slaw key1, slaw val1, ...) OB_SENTINEL;
OB_PLASMA_API slaw slaw_map_inline_cc (const char *key1, const char *val1,
                                       ...) OB_SENTINEL;
OB_PLASMA_API slaw slaw_map_inline_cl (const char *key1, bslaw val1,
                                       ...) OB_SENTINEL;
OB_PLASMA_API slaw slaw_map_inline_cf (const char *key1, slaw val1,
                                       ...) OB_SENTINEL;
//@}

/**
 * Returns a new map which is the result of merging all the specified maps.
 * (Which may be any number of maps, with a \c NULL terminating the varargs.)
 * The result is equivalent to starting with an empty map, and adding
 * each entry from each specified map, in order.  Thus, if there are
 * duplicates, either between maps or within a map (if a specified "map"
 * is actually a slaw list with duplicates and not a true map), the
 * *value* of the last duplicate will be retained, in the *position* of
 * the first duplicate.  Like slabu_map_conform(), any non-cons elements
 * (in lists which are not actually maps) are discarded.
 * It is safe to pass duplicate arguments, even to the _f variant.
 */
//@{
/**
 * \ingroup SlawMap
 */
OB_PLASMA_API slaw slaw_maps_merge (bslaw map1,
                                    /* bslaw map2, */...) OB_SENTINEL;
OB_PLASMA_API slaw slaw_maps_merge_f (slaw map1,
                                      /* slaw map2, */...) OB_SENTINEL;
//@}

/**
 * Same as slaw_maps_merge(), but takes its maps in an array, rather than
 * via varargs.
 * \ingroup SlawMap
 */
OB_PLASMA_API slaw slaw_maps_merge_byarray (const bslaw *maps, int64 nmaps);

/**
 * Number of key/value pairs in the given map. This is just
 * slaw_list_count (remember that a map is a list), which see.
 * \ingroup SlawMap
 */
#define slaw_map_count slaw_list_count

/**
 * \name Slaw map lookups
 * Find an entry in a map.  Compares the key (using slawx_equal())
 * to the car of list entries that are conses, starting from the highest
 * index, working down to zero.  If a matching key is found, its cdr is
 * returned.  Otherwise the return value is \c NULL.
 * Note that slaw_map_find() is happy to operate on a
 * non-map slaw list; the method simply ignores elements that are not
 * cons cells, (and returns the highest-index match, though there
 * may be duplicate keys).
 */
//@{
/**
 * \ingroup SlawMap
 */
OB_PLASMA_API bslaw slaw_map_find (bslaw s, bslaw key);
OB_PLASMA_API bslaw slaw_map_find_f (bslaw s, slaw key);
OB_PLASMA_API bslaw slaw_map_find_c (bslaw s, const char *key);
//@}

#include "libPlasma/c/slaw-numeric-ilk-rumpus.h"

/**
 * \defgroup SlawNumericAPI Numeric types
 * Support for atomic numeric types and homogeneous
 * numeric arrays.
 * \ingroup PlasmaSlawGroup
 */

/**
 * \name Predicates for numeric slawx
 */
//@{
/**
 * \ingroup SlawNumericAPI
 */
OB_PLASMA_API bool slaw_is_numeric (bslaw s);
OB_PLASMA_API bool slaw_is_numeric_8 (bslaw s);
OB_PLASMA_API bool slaw_is_numeric_16 (bslaw s);
OB_PLASMA_API bool slaw_is_numeric_32 (bslaw s);
OB_PLASMA_API bool slaw_is_numeric_64 (bslaw s);
OB_PLASMA_API bool slaw_is_numeric_int (bslaw s);
OB_PLASMA_API bool slaw_is_numeric_unt (bslaw s);
OB_PLASMA_API bool slaw_is_numeric_float (bslaw s);
OB_PLASMA_API bool slaw_is_numeric_complex (bslaw s);
OB_PLASMA_API bool slaw_is_numeric_vector (bslaw s);
OB_PLASMA_API bool slaw_is_numeric_multivector (bslaw s);
OB_PLASMA_API bool slaw_is_numeric_array (bslaw s);
//@}

/**
 * \name Numeric arrays and vectors
 */
//@{
/**
 * \ingroup SlawNumericAPI
 * Returns the number of elements in a slaw array, or -1 if \a s is not an
 * array.
 */
OB_PLASMA_API int64 slaw_numeric_array_count (bslaw s);
/**
 * Emits a pointer to the native numeric array contained in \a s,
 * or NULL when its argument is not a numeric array.
 */
OB_PLASMA_API const void *slaw_numeric_array_emit (bslaw s);
/**
 * Returns the dimension of a vector (2, 3, 4) \b or a multivector
 * (2, 3, 4, 5). When \a s is not a vector or multivector, this
 * function returns 0.
 */
OB_PLASMA_API int slaw_numeric_vector_dimension (bslaw s);
/**
 * For a numeric array, returns the size in bytes of a single array
 * element.  For a numeric singleton, returns the size in bytes of the
 * singleton (not including the slaw header and padding).  In other words,
 * it is the sizeof the struct or typedef from ob-types.h which is used
 * to store the numeric type.
 * If \a s is not numeric or NULL, 0 is returned.
 */
OB_PLASMA_API int slaw_numeric_unit_bsize (bslaw s);
//@}

/**
 * \name Slaw printing
 */
//@{
/**
 * \ingroup SlawBasicAPI
 * Print a human-readable representation of the specified slaw to the
 * stream \a whither.  Each line is prefixed with \a prolo.  (A NULL
 * \a prolo is treated the same as the empty string.)
 */
OB_PLASMA_API void slaw_spew_overview (bslaw s, FILE *whither,
                                       const char *prolo);

/**
 * Same as slaw_spew_overview_to_string(), but with an additional
 * \a flags argument.
 *
 * \a flags can be a combination of SLAW_SPEW_FLAG constants, "or"-ed
 * together.
 */
OB_PLASMA_API void slaw_spew_overview_ex (bslaw       s,
                                          FILE       *whither,
                                          unt32       flags,
                                          const char *prolo);

/**
 * Print a human-readable representation of the specified slaw to stderr.
 */
OB_PLASMA_API void slaw_spew_overview_to_stderr (bslaw s);

/**
 * Return a slaw string which contains the "spew" representation of the
 * given slaw, as would be printed by slaw_spew_overview_to_stderr().
 */
OB_PLASMA_API slaw slaw_spew_overview_to_string (bslaw s);

/**
 * Same as slaw_spew_overview_to_string(), but with two additional
 * arguments.
 *
 * \a flags can be a combination of SLAW_SPEW_FLAG constants, "or"-ed
 * together.
 *
 * If \a prolo is non-NULL, then every line of the spew is
 * prefixed with \a prolo.  (NULL is the default used by the
 * slaw_spew functions that don't take a \a prolo argument.)
 */
OB_PLASMA_API slaw slaw_spew_overview_to_string_ex (bslaw       s,
                                                    unt32       flags,
                                                    const char *prolo);

/**
 * User-supplied function which can be passed to
 * slaw_spew_overview_to_func().
 *
 * The function is given a NUL-terminated string, and also the
 * length of that string (not counting the NUL-terminator).
 *
 * If the function returns a failure retort, the function will not
 * be called anymore, and the retort will be returned by
 * slaw_spew_overview_to_func().
 */
typedef ob_retort (*slaw_spew_func) (void       *cookie,
                                     const char *str,
                                     size_t      len);

/**
 * Same as slaw_spew_overview_to_string_ex(), but instead of returning
 * a string, it calls \a func on chunks of output.  The \a cookie
 * argument is passed to \a func, as a way to keep track of whatever
 * data the function wants to.
 *
 * \a size_hint is a hint about the size of chunks that \func would
 * like to be called with.  However, this is only a hint, and \a func
 * may be called with larger or smaller chunks, so \a func must be
 * prepared to handle chunks of any size.  To use a default chunk
 * size, specify 0.
 */
OB_PLASMA_API ob_retort slaw_spew_overview_to_func (bslaw          s,
                                                    slaw_spew_func func,
                                                    void          *cookie,
                                                    size_t         size_hint,
                                                    unt32          flags,
                                                    const char    *prolo);

/**
 * Annotates subslawx with their byte offset relative to the top-level
 * slaw, rather than the absolute address.
 */
#define SLAW_SPEW_FLAG_REL_OFF        0x0001

/**
 * When displaying rude data, include ASCII in addition to hex bytes,
 * similar to the "hd" command.  Default is to display hex bytes only.
 */
#define SLAW_SPEW_FLAG_RUDE_ASCII     0x0002

/**
 * When displaying slaw strings, show escape sequences for unprintable
 * characters.
 */
#define SLAW_SPEW_FLAG_ESCAPE_STRINGS 0x0004
//@}

#ifdef __cplusplus
}
#endif


#endif
