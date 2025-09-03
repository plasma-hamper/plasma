
/* (c)  oblong industries */

#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-endian.h"
#include "libLoam/c/ob-sys.h"
#include "libLoam/c/ob-util.h"
#include "libLoam/c/ob-hash.h"
#include "libPlasma/c/slaw.h"
#include "libPlasma/c/protein.h"
#include "libPlasma/c/private/plasma-private.h"
#include "libPlasma/c/slaw-ordering.h"
#include "libPlasma/c/slaw-string.h"
#include "libPlasma/c/slaw-walk.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>


#define MAX_INLINE_ARGS 256

slabu *slabu_new (void)
{
  slabu *sb = (slabu *) malloc (sizeof (slabu));
  if (!sb)
    return NULL;
  sb->esses = NULL;
  sb->effs = NULL;
  sb->numEsses = sb->maxNumEsses = 0;
  sb->obeysMapInvariant = true;
  return sb;
}

int64 slabu_count (const slabu *sb)
{
  if (!sb)
    return -1;
  return sb->numEsses;
}

bslaw slabu_list_nth (const slabu *sb, int64 n)
{
  if (!sb || n < 0 || n >= sb->numEsses)
    return NULL;
  return sb->esses[n];
}

void slabu_free (slabu *sb)
{
  if (!sb)
    return;
  if (sb->esses)
    {
      slaw *essp = sb->esses + sb->numEsses - 1;
      bool *effp = sb->effs + sb->numEsses - 1;
      for (; sb->numEsses > 0; sb->numEsses--, essp--, effp--)
        if (*effp && *essp)
          slaw_free (*essp);
      free ((void *) sb->esses);
      free ((void *) sb->effs);
    }
  free ((void *) sb);
}

slabu *slabu_from_slaw (bslaw list)
{
  slabu *sb = slabu_new ();
  bslaw cole;
  ob_retort err = OB_OK;

  for (cole = slaw_list_emit_first (list); sb != NULL && cole != NULL;
       cole = slaw_list_emit_next (list, cole))
    ob_err_accum (&err, slabu_list_add_x (sb, slaw_dup (cole)));

  if (err < OB_OK)
    {
      slabu_free (sb);
      return NULL;
    }

  return sb;
}

slabu *slabu_from_slaw_f (slaw list)
{
  slabu *sb = slabu_from_slaw (list);

  if (sb)
    slaw_free (list);

  return sb;
}

slabu *slabu_dup (const slabu *sb)
{
  if (!sb)
    return NULL;

  slabu *newsb = slabu_new ();
  if (!newsb)
    return NULL;

  int64 count = slabu_count (sb);
  int64 i;
  ob_retort err = OB_OK;
  for (i = 0; i < count; i++)
    ob_err_accum (&err,
                  slabu_list_add_x (newsb, slaw_dup (slabu_list_nth (sb, i))));

  if (err < OB_OK)
    {
      slabu_free (newsb);
      return NULL;
    }

  return newsb;
}

static slabu *slabu_dup_shallow (const slabu *sb)
{
  if (!sb)
    return NULL;

  slabu *newsb = slabu_new ();
  if (!newsb)
    return NULL;

  int64 count = slabu_count (sb);
  int64 i;
  ob_retort err = OB_OK;
  for (i = 0; i < count; i++)
    ob_err_accum (&err, slabu_list_add_z (newsb, slabu_list_nth (sb, i)));

  if (err < OB_OK)
    {
      slabu_free (newsb);
      return NULL;
    }

  return newsb;
}


static bool slabu_ensure (slabu *sb, int64 capacity)
{
  if (capacity <= sb->maxNumEsses)
    return true;

  int64 maxNumEsses = capacity + 8;
  slaw *esses;
  bool *effs;

  if (!sb->esses)
    {
      esses = (slaw *) malloc ((size_t) (maxNumEsses) * sizeof (slaw));
      effs = (bool *) malloc ((size_t) (maxNumEsses) * sizeof (bool));
    }
  else
    {
      esses =
        (slaw *) realloc (sb->esses, (size_t) (maxNumEsses) * sizeof (slaw));
      effs =
        (bool *) realloc (sb->effs, (size_t) (maxNumEsses) * sizeof (bool));
    }

  bool ok = true;

  if (esses)
    sb->esses = esses;
  else
    ok = false;

  if (effs)
    sb->effs = effs;
  else
    ok = false;

  if (ok)
    sb->maxNumEsses = maxNumEsses;

  return ok;
}

static int64 slabu_list_add_internal (slabu *sb, slaw ess, bool eff)
{
  if (!sb || !ess)
    return OB_ARGUMENT_WAS_NULL;

  if (!slabu_ensure (sb, sb->numEsses + 1))
    return OB_NO_MEM;

  sb->effs[sb->numEsses] = eff;
  sb->esses[sb->numEsses] = ess;
  sb->obeysMapInvariant = false;
  return sb->numEsses++;
}

int64 slabu_list_add_x (slabu *sb, slaw ess)
{
  return slabu_list_add_internal (sb, ess, true);
}

int64 slabu_list_add_z (slabu *sb, bslaw ess)
{
  slaw castaway = (slaw) ess;
  return slabu_list_add_internal (sb, castaway, false);
}

int64 slabu_list_add_c (slabu *sb, const char *str)
{
  return slabu_list_add_x (sb, slaw_string (str));
}

int64 slabu_list_add (slabu *sb, bslaw s)
{
  return slabu_list_add_x (sb, slaw_dup (s));
}

int64 slabu_list_add_f (slabu *sb, slaw s)
{
  return slabu_list_add_x (sb, slaw_dup_f (s));
}

ob_retort slabu_list_remove (slabu *sb, bslaw ess)
{
  if (!sb || !sb->esses)
    return OB_ARGUMENT_WAS_NULL;
  int64 q, end = sb->numEsses - 1;
  // find *first* match (old version found *last* match)
  for (q = 0; q <= end; q++)
    if (slawx_equal (sb->esses[q], ess))
      return slabu_list_remove_nth (sb, q);
  return SLAW_NOT_FOUND;
}

ob_retort slabu_list_remove_f (slabu *sb, slaw s)
{
  ob_retort result = slabu_list_remove (sb, s);
  slaw_free (s);
  return result;
}

ob_retort slabu_list_remove_c (slabu *sb, const char *str)
{
  return slabu_list_remove_f (sb, slaw_string (str));
}

ob_retort slabu_list_remove_nth (slabu *sb, int64 n)
{
  if (!sb)
    return OB_ARGUMENT_WAS_NULL;
  if (n >= sb->numEsses || n < -sb->numEsses)
    return OB_BAD_INDEX;
  if (n < 0)
    n += sb->numEsses;
  int64 end = sb->numEsses - 1;
  slaw ess = sb->esses[n];
  bool eff = sb->effs[n];
  for (; n < end; n++)
    {
      sb->effs[n] = sb->effs[n + 1];
      sb->esses[n] = sb->esses[n + 1];
    }
  sb->numEsses--;
  if (eff)
    slaw_free (ess);
  return OB_OK;
}

int64 slabu_list_find (const slabu *sb, bslaw s)
{
  int64 i;

  if (!sb || !s)
    return OB_ARGUMENT_WAS_NULL;

  for (i = 0; i < sb->numEsses; i++)
    if (slawx_equal (s, sb->esses[i]))
      return i;

  return SLAW_NOT_FOUND;
}

int64 slabu_list_find_f (const slabu *sb, slaw s)
{
  int64 result = slabu_list_find (sb, s);
  slaw_free (s);
  return result;
}

int64 slabu_list_find_c (const slabu *sb, const char *str)
{
  return slabu_list_find_f (sb, slaw_string (str));
}


static int64 insrep (slabu *sb, int64 nth, slaw s, bool eff, bool replace)
{
  if (!sb || !s)
    return OB_ARGUMENT_WAS_NULL;

  if (nth == -1)
    nth = sb->numEsses;

  if (nth < 0 || nth > (sb->numEsses - replace))
    return OB_BAD_INDEX;

  if (!replace && !slabu_ensure (sb, sb->numEsses + 1))
    return OB_NO_MEM;

  if (!replace)
    {
      int64 i;
      for (i = sb->numEsses; i > nth; i--)
        {
          sb->esses[i] = sb->esses[i - 1];
          sb->effs[i] = sb->effs[i - 1];
        }
      sb->numEsses++;
    }
  else if (sb->effs[nth])
    slaw_free (sb->esses[nth]);

  sb->esses[nth] = s;
  sb->effs[nth] = eff;

  sb->obeysMapInvariant = false;

  return nth;
}

#define BODY_K(rep)                                                            \
  {                                                                            \
    return insrep (sb, nth, slaw_dup (s), true, rep);                          \
  }
#define BODY_F(rep)                                                            \
  {                                                                            \
    return insrep (sb, nth, slaw_dup_f (s), true, rep);                        \
  }
#define BODY_C(rep)                                                            \
  {                                                                            \
    return insrep (sb, nth, slaw_string (s), true, rep);                       \
  }
#define BODY_Z(rep)                                                            \
  {                                                                            \
    return insrep (sb, nth, (slaw) s, false, rep);                             \
  }
#define BODY_X(rep)                                                            \
  {                                                                            \
    return insrep (sb, nth, (slaw) s, true, rep);                              \
  }

int64 slabu_list_insert (slabu *sb, int64 nth, bslaw s) BODY_K (false);
int64 slabu_list_insert_f (slabu *sb, int64 nth, slaw s) BODY_F (false);
int64 slabu_list_insert_c (slabu *sb, int64 nth, const char *s) BODY_C (false);
int64 slabu_list_insert_z (slabu *sb, int64 nth, bslaw s) BODY_Z (false);
int64 slabu_list_insert_x (slabu *sb, int64 nth, slaw s) BODY_X (false);

int64 slabu_list_replace_nth (slabu *sb, int64 nth, bslaw s) BODY_K (true);
int64 slabu_list_replace_nth_f (slabu *sb, int64 nth, slaw s) BODY_F (true);
int64 slabu_list_replace_nth_c (slabu *sb, int64 nth, const char *s)
  BODY_C (true);
int64 slabu_list_replace_nth_z (slabu *sb, int64 nth, bslaw s) BODY_Z (true);
int64 slabu_list_replace_nth_x (slabu *sb, int64 nth, slaw s) BODY_X (true);

struct temporary_map_sorter
{
  slaw ess;
  unt64 eff : 1;
  unt64 order : 63; /* force qsort to be a stable sort */
};

static int tms_compare (const void *va, const void *vb)
{
  struct temporary_map_sorter *a = (struct temporary_map_sorter *) va;
  struct temporary_map_sorter *b = (struct temporary_map_sorter *) vb;
  bslaw acar = slaw_cons_emit_car (a->ess);
  bslaw bcar = slaw_cons_emit_car (b->ess);
  int result = slaw_semantic_compare (acar, bcar);

  if (result != 0)
    return result;

  if (a->order < b->order)
    return -1;
  else if (a->order > b->order)
    return 1;
  else
    return 0;
}

static int tms_revert (const void *va, const void *vb)
{
  struct temporary_map_sorter *a = (struct temporary_map_sorter *) va;
  struct temporary_map_sorter *b = (struct temporary_map_sorter *) vb;

  if (a->order < b->order)
    return -1;
  else if (a->order > b->order)
    return 1;
  else
    return 0;
}

// In the end, we chose to use DUP_KEEP_LAST_IN_POSITION_OF_FIRST in all cases:
// https://bugs.oblong.com/show_bug.cgi?id=28#c3
// But I'll leave this enum here in case different behavior is needed in
// the future.
typedef enum {
  DUP_KEEP_FIRST,
  DUP_KEEP_LAST,
  DUP_KEEP_LAST_IN_POSITION_OF_FIRST
} DuplicateMode;

/// eliminates duplicates and non-conses, but retains order.
/// dm determines which duplicate should be chosen, and where
/// it should go.
/// returns true if successful, false if out of memory.
static bool enforce_map_invariant (slabu *sb, DuplicateMode dm)
{
  struct temporary_map_sorter *tms;
  int64 i, j, k;
  bslaw prevcar = NULL;

  if (sb->obeysMapInvariant)
    return true; /* already obeys */

  if (sb->numEsses == 0) /* empty list satisfies invariant */
    {
      sb->obeysMapInvariant = true;
      return true;
    }

  tms = (struct temporary_map_sorter *) malloc (
    sb->numEsses * sizeof (struct temporary_map_sorter));

  if (!tms)
    return false;

  /* copy all the conses to a temporary array for sorting */

  for (i = 0, j = 0; i < sb->numEsses; i++)
    {
      slaw ess = sb->esses[i];
      bool eff = sb->effs[i];

      if (slaw_is_cons (ess))
        {
          tms[j].ess = ess;
          tms[j].eff = eff;
          tms[j].order = j;
          j++;
        }
      else /* if it's not a cons, auf wiedersehen! */
        {
          if (eff)
            slaw_free (ess);
        }
    }

  qsort (tms, j, sizeof (struct temporary_map_sorter), tms_compare);

  /* eliminate duplicates */

  for (i = 0, k = 0; i < j; i++)
    {
      slaw ess = tms[i].ess;
      bool eff = tms[i].eff;
      if (prevcar && slawx_equal (prevcar, slaw_cons_emit_car (ess)))
        {
          // it's a duplicate; bye-bye!
          if (dm == DUP_KEEP_FIRST)
            {
              if (eff)
                slaw_free (ess);
            }
          else
            {
              int64 origOrder = tms[k - 1].order;
              if (tms[k - 1].eff)
                slaw_free (tms[k - 1].ess);
              tms[k - 1] = tms[i];
              if (dm == DUP_KEEP_LAST_IN_POSITION_OF_FIRST)
                tms[k - 1].order = origOrder;
              prevcar = slaw_cons_emit_car (ess);
            }
        }
      else
        {
          if (k != i)
            tms[k] = tms[i];
          k++;
          prevcar = slaw_cons_emit_car (ess);
        }
    }

  /* re-sort so it's back in the original order! */

  qsort (tms, k, sizeof (struct temporary_map_sorter), tms_revert);

  /* copy the array back */

  for (i = 0; i < k; i++)
    {
      slaw ess = tms[i].ess;
      bool eff = tms[i].eff;
      sb->esses[i] = ess;
      sb->effs[i] = eff;
    }

  sb->numEsses = k;
  sb->obeysMapInvariant = true;

  free (tms);
  return true;
}

static int64 slabu_map_put_internal (slabu *sb, slaw entry)
{
  bslaw key;
  int64 i;

  if (!sb || !entry)
    return OB_ARGUMENT_WAS_NULL;
  if (!enforce_map_invariant (sb, DUP_KEEP_LAST_IN_POSITION_OF_FIRST))
    return OB_NO_MEM;

  key = slaw_cons_emit_car (entry);

  if (slabu_map_find (sb, key, &i))
    {
      // key found
      if (sb->effs[i])
        slaw_free (sb->esses[i]);
      sb->esses[i] = entry;
      sb->effs[i] = true;
      return i;
    }

  // key not found.
  if (!slabu_ensure (sb, sb->numEsses + 1))
    return OB_NO_MEM;

  sb->esses[sb->numEsses] = entry;
  sb->effs[sb->numEsses] = true;
  return sb->numEsses++;
}

int64 slabu_map_put (slabu *sb, bslaw key, bslaw value)
{
  return slabu_map_put_internal (sb, slaw_cons (key, value));
}

int64 slabu_map_put_ff (slabu *sb, slaw key, slaw value)
{
  if (key == value)
    value = slaw_dup (value);
  return slabu_map_put_internal (sb, slaw_cons_ff (key, value));
}

int64 slabu_map_put_lf (slabu *sb, bslaw key, slaw value)
{
  if (key == value)
    value = slaw_dup (value);
  return slabu_map_put_internal (sb, slaw_cons_lf (key, value));
}

int64 slabu_map_put_fl (slabu *sb, slaw key, bslaw value)
{
  if (key == value)
    key = slaw_dup (key);
  return slabu_map_put_internal (sb, slaw_cons_fl (key, value));
}

int64 slabu_map_put_cl (slabu *sb, const char *key, bslaw value)
{
  return slabu_map_put_internal (sb, slaw_cons_cl (key, value));
}

int64 slabu_map_put_cf (slabu *sb, const char *key, slaw value)
{
  return slabu_map_put_internal (sb, slaw_cons_cf (key, value));
}

int64 slabu_map_put_cc (slabu *sb, const char *key, const char *value)
{
  return slabu_map_put_internal (sb, slaw_cons_cc (key, value));
}


bslaw slabu_map_find (const slabu *sb, bslaw key, int64 *idx)
{
  if (!sb || !key)
    return NULL;
  else
    {
      // not sorted; use slower linear search
      int64 i;
      for (i = sb->numEsses - 1; i >= 0; i--)
        {
          if (slaw_is_cons (sb->esses[i])
              && slawx_equal (key, slaw_cons_emit_car (sb->esses[i])))
            {
              if (idx)
                *idx = i;
              return slaw_cons_emit_cdr (sb->esses[i]);
            }
        }
      // not found; leave *idx unmodified
      return NULL;
    }
}

bslaw slabu_map_find_f (const slabu *sb, slaw key, int64 *idx)
{
  bslaw result = slabu_map_find (sb, key, idx);
  slaw_free (key);
  return result;
}

bslaw slabu_map_find_c (const slabu *sb, const char *key, int64 *idx)
{
  return slabu_map_find_f (sb, slaw_string (key), idx);
}

ob_retort slabu_map_remove (slabu *sb, bslaw s)
{
  if (!sb || !s)
    return OB_ARGUMENT_WAS_NULL;
  int64 where;
  bslaw result = slabu_map_find (sb, s, &where);
  if (result)
    {
      slabu_list_remove_nth (sb, where);
      return OB_OK;
    }
  else
    return SLAW_NOT_FOUND;
}

ob_retort slabu_map_remove_f (slabu *sb, slaw s)
{
  ob_retort result = slabu_map_remove (sb, s);
  slaw_free (s);
  return result;
}

ob_retort slabu_map_remove_c (slabu *sb, const char *str)
{
  return slabu_map_remove_f (sb, slaw_string (str));
}

bool slabu_is_map (const slabu *sb)
{
  if (!sb)
    return false; /* let's say NULL is not a map */

  if (sb->obeysMapInvariant)
    return true; /* that was easy! */

  int64 i;
  for (i = 0; i < sb->numEsses; i++)
    if (!slaw_is_cons (sb->esses[i]))
      return false; /* if it contains a non-cons, not a map */

  if (sb->numEsses < 2)
    return true; /* with 0 or 1 entries, must be unique */

  /* Need to see if there are duplicates.
   * XXX: For now, do it the cheesy way. */
  slabu *sb_copy = slabu_dup_shallow (sb);
  if (!sb_copy
      || !enforce_map_invariant (sb_copy, DUP_KEEP_LAST_IN_POSITION_OF_FIRST))
    {
      OB_LOG_ERROR_CODE (0x20001000, "I ran out of memory and have no way to "
                                     "indicate it!\n");
      return false;
    }

  bool result = (sb->numEsses == sb_copy->numEsses); /* any dups removed? */
  slabu_free (sb_copy);
  return result;
}

ob_retort slabu_map_conform (slabu *sb)
{
  if (!sb)
    return OB_ARGUMENT_WAS_NULL;

  if (sb->obeysMapInvariant)
    return OB_NOTHING_TO_DO;

  int64 oldLen = sb->numEsses;

  if (!enforce_map_invariant (sb, DUP_KEEP_LAST_IN_POSITION_OF_FIRST))
    return OB_NO_MEM;

  return (sb->numEsses != oldLen ? OB_OK : OB_NOTHING_TO_DO);
}

static int ss_cmp (const void *va, const void *vb)
{
  slaw *a = (slaw *) va;
  slaw *b = (slaw *) vb;

  assert (a);
  assert (b);

  return slaw_semantic_compare (*a, *b);
}

void slabu_sort (slabu *sb)
{
  assert (sb);  // this is an internal function; no sympathy if you pass NULL

#ifndef NDEBUG
  int64 i;
  // For simplicity, since this is an internal function, require that all
  // the slawx have the same memory management (same value of eff).
  // This is reasonable since they were probably all added the same way.
  // We'd want to lift this restriction if we made this a public function.
  // (To do that, we'd need the temporary_map_sorter trick used elsewhere.)
  for (i = 0; i < sb->numEsses; i++)
    {
      assert (sb->effs[i] == sb->effs[0]);
      assert (sb->esses[i]);  // should not have any null slawx
    }
#endif

  qsort (sb->esses, sb->numEsses, sizeof (sb->esses[0]), ss_cmp);
}


//
//
//

int64 slaw_len (bslaw s)
{
  return s ? 8 * slaw_octlen (s) : -1;
}


slaw slaw_dup (bslaw s)
{
  if (!s)
    return NULL;
  unt64 scythes = slaw_octlen (s);
  slaw dupped = slaw_alloc (scythes);
  if (!dupped)
    return NULL;
/* memcpy is probably better than a loop, although the results are mixed
   * (see bug 3578) */
#if 0
  bslaw from = s + scythes;
  slaw to = dupped + scythes;
  while (from > s)
    *--to = *--from;
#else
  memcpy (dupped, s, 8 * scythes);
#endif
  return dupped;
}


// If for some reason you want to move your slaw to a new address.
// Maybe your slaw has a crazy stalker or something, or committed
// tax fraud...
slaw slaw_dup_f (slaw s)
{
  slaw result = slaw_dup (s);
  slaw_free (s);
  return result;
}


void slaw_free (slaw s)
{
  if (s)
    {
      // Trash header oct to catch use of freed memory
      s->o = OB_CONST_U64 (0xffffffffffffffff);
      free ((void *) s);
    }
}



bool slaw_is_nil (bslaw s)
{
  if (!s)
    return false;
  slaw_oct ilk = SLAW_ILK (s);
  return (ilk == SLAW_NIL_ILK);
}


bool slawx_equal (bslaw s1, bslaw s2)
{
  if (s1 == s2)
    return true;
  unt64 len = slaw_octlen (s1);
  if (slaw_octlen (s2) != len)
    return false;
#ifdef __APPLE__
  /* For reasons I can't explain, memcmp is much faster than a loop on OS X,
   * but a loop is much faster than memcmp on Linux (and a loop is slightly
   * faster than memcmp on Windows, too).  See bug 3578. */
  return (!memcmp (s1, s2, 8 * len));
#else
  for (; len > 0; len--)
    if (s1++->o != s2++->o)
      return false;
  return true;
#endif
}


bool slawx_equal_lf (bslaw s1, slaw s2)
{
  bool result = slawx_equal (s1, s2);
  slaw_free (s2);
  return result;
}


bool slawx_equal_lc (bslaw s, const char *str)
{
  if (!s || !str)
    return false;
  slaw slawstr = slaw_string (str);
  bool eql = slawx_equal (s, slawstr);
  slaw_free (slawstr);
  return eql;
}



//
//
//



slaw slaw_nil (void)
{
  slaw s = slaw_alloc ((unt64) 1);
  if (!s)
    return NULL;
  s->o = SLAW_NIL_ILK;
  return s;
}



slaw slaw_string (const char *str)
{
  if (!str)
    return NULL;
  unt64 len = strlen (str);
  return slaw_string_from_substring (str, len);
}


slaw slaw_string_from_substring (const char *str, int64 len)
{
  if (!str)
    return NULL;
  slaw result = slaw_string_raw (len);
  if (result)
    memcpy (SLAW_STRING_EMIT_WRITABLE (result), str, len);
  return result;
}


slaw slaw_string_raw (int64 len)
{
  if (len < 0)
    return NULL;

  int64 term_len = len + 1; /* add a byte for terminating NUL */

  if (len < 7)
    {
      slaw s = slaw_alloc (1);
      if (!s)
        return NULL;
      s->o = ((((slaw_oct) SLAW_NIB_WEE_STRING) << SLAW_NIBBLE_SHIFTY)
              | (term_len << SLAW_WEE_STRING_LEN_SHIFTY));
      return s;
    }

  unt64 padLen = term_len;
  if (padLen & 7)
    padLen += 8 - (padLen & 7);
  unt64 pad_bytes = padLen - term_len;
  unt64 padOctLen = padLen >> 3;
  unt64 oct_len = padOctLen + 1; /* 1 oct for header */

  slaw s = slaw_alloc (oct_len);
  if (!s)
    return NULL;

  s->o = ((((slaw_oct) SLAW_NIB_FULL_STRING) << SLAW_NIBBLE_SHIFTY)
          | (pad_bytes << SLAW_FULL_STRING_PAD_SHIFTY))
         | oct_len;

  /* initialize last word to 0 to avoid uninitialized padding problems */
  (&s->o)[oct_len - 1] = 0;

  return s;
}


bool slaw_is_string (bslaw s)
{
  return s ? SLAW_IS_STRING (s) : false;
}


const char *slaw_string_emit (bslaw s)
{
  OB_UNUSED int64 len;
  if (!s)
    return NULL;
  switch (SLAW_ILK_NIBBLE (s))
    {
      case SLAW_NIB_WEE_STRING:
        len = SLAW_WEE_STRING_LEN (s);
        return (const char *) SLAW_SPECIAL_BYTES (s, len);
      case SLAW_NIB_FULL_STRING:
        return (const char *) (s + 1);
      default:
        return NULL;
    }
}


int64 slaw_string_emit_length (bslaw s)
{
  int64 len;
  if (!s)
    return -1;
  switch (SLAW_ILK_NIBBLE (s))
    {
      case SLAW_NIB_WEE_STRING:
        len = SLAW_WEE_STRING_LEN (s);
        return len - 1; /* -1 to not count terminating NUL */
      case SLAW_NIB_FULL_STRING:
        len = SLAW_EMBEDDED_OCTLEN (s) - 1; /* -1 because of header oct */
        len <<= 3;
        len -= SLAW_FULL_STRING_PAD (s);
        if (len < 8)
          return -1;
        return len - 1; /* -1 to not count terminating NUL */
      default:
        return -1;
    }
}


static slaw slaw_container_internal (unt64 nibble, int64 numEsses,
                                     const bslaw *esses)
{
  int64 i;
  bool wee = (numEsses < SLAW_MAX_WEE_CONTAINER);
  int64 oct_len = (wee ? 1 : 2);

  for (i = 0; i < numEsses; i++)
    oct_len += slaw_octlen (esses[i]);

  slaw s = slaw_alloc (oct_len);
  if (!s)
    return NULL;

  int64 count = (wee ? numEsses : SLAW_MAX_WEE_CONTAINER);

  s->o = ((nibble << SLAW_NIBBLE_SHIFTY)
          | (count << SLAW_WEE_CONTAINER_COUNT_SHIFTY) | oct_len);

  slaw ess = s + 1;

  if (!wee)
    ess++->o = numEsses;

  for (i = 0; i < numEsses; i++)
    {
      int64 olen = slaw_octlen (esses[i]);
      slaw_copy_octs_from_to (esses[i], ess, olen);
      ess += olen;
    }

  return s;
}

static slaw slaw_container_array_internal (unt64 nibble, bslaw sess,
                                           const unt8 *bess, unt64 breadth)
{
  int64 array_olen = (breadth + 7) / 8;
  int64 buf_oct_len = 2 + array_olen; /* 1 for extra padding */
  int64 ess_oct_len = slaw_octlen (sess);
  int64 oct_len = 1 + ess_oct_len + buf_oct_len;

  slaw s = slaw_alloc (oct_len);
  if (!s)
    return NULL;

  s->o = ((nibble << SLAW_NIBBLE_SHIFTY)
          | (OB_CONST_U64 (2) << SLAW_WEE_CONTAINER_COUNT_SHIFTY) | oct_len);

  slaw ess = s + 1;

  // Copy car slaw
  slaw_copy_octs_from_to (sess, ess, ess_oct_len);
  ess += ess_oct_len;

  // Copy cdr slaw
  ess->o = (SLAW_unt8 | SLAW_NUMERIC_ILK | breadth | SLAW_NUMERIC_ARRAY_ILK);

  if (0 != (7 & breadth))
    (&ess->o)[array_olen] = 0; /* make sure padding is initialized */

  ess += 1;
  memcpy (ess, bess, breadth);

  return s;
}

// assumes ilk has already been verified
static bslaw slaw_container_first (bslaw s)
{
  int64 octlen = SLAW_EMBEDDED_OCTLEN (s);
  bslaw last = s + octlen;
  int64 count = ((SLAW_ILK (s) >> SLAW_WEE_CONTAINER_COUNT_SHIFTY)
                 & SLAW_MAX_WEE_CONTAINER);
  bslaw first = s + 1;

  if (count == 0)
    return NULL;
  else if (count == SLAW_MAX_WEE_CONTAINER)
    first++;

  if (first >= last)
    // corrupt slaw
    return NULL;

  const unt64 noctlen = slaw_octlen (first);
  if (noctlen == 0 || first + noctlen > last)
    // corrupt slaw
    return NULL;

  return first;
}

// assumes ilk has already been verified; prev must not be NULL
static bslaw slaw_container_next (bslaw s, bslaw prev)
{
  int64 octlen = SLAW_EMBEDDED_OCTLEN (s);
  bslaw last = s + octlen;
  if (prev >= last || prev <= s)
    // if either of these things happen, the caller is doing something wrong
    return NULL;
  bslaw next = prev + slaw_octlen (prev);
  if (next >= last)
    // this is a "normal" case, when you reach the end of the list
    return NULL;
  const unt64 noctlen = slaw_octlen (next);
  if (noctlen == 0 || next + noctlen > last)
    // this means the slaw is corrupt
    return NULL;
  return next;
}

OB_FLATTEN slaw slaw_cons_ff (slaw sA, slaw sB)
{
  slaw s = slaw_cons (sA, sB);

  slaw_free (sA);
  if (sB != sA)
    slaw_free (sB);

  return s;
}


slaw slaw_cons_cf (const char *str, slaw s2)
{
  return slaw_cons_ff (slaw_string (str), s2);
}


bool slaw_is_cons (bslaw s)
{
  return s ? SLAW_IS_CONS (s) : false;
}


slaw slaw_cons (bslaw sA, bslaw sB)
{
  if (!sA || !sB)
    return NULL;
  bslaw esses[2];
  esses[0] = sA;
  esses[1] = sB;
  return slaw_container_internal (SLAW_NIB_CONS, 2, esses);
}


slaw slaw_cons_lf (bslaw car, slaw cdr)
{
  slaw result = slaw_cons (car, cdr);
  slaw_free (cdr);

  return result;
}

slaw slaw_cons_fl (slaw car, bslaw cdr)
{
  slaw result = slaw_cons (car, cdr);
  slaw_free (car);

  return result;
}

slaw slaw_cons_cl (const char *car, bslaw cdr)
{
  if (!car || !cdr)
    return NULL;
  slaw result;
  slaw s = slaw_string (car);
  if (!s)
    return NULL;

  result = slaw_cons (s, cdr);
  slaw_free (s);
  return result;
}

slaw slaw_cons_lc (bslaw car, const char *cdr)
{
  if (!car || !cdr)
    return NULL;
  slaw result;
  slaw s = slaw_string (cdr);
  if (!s)
    return NULL;

  result = slaw_cons (car, s);
  slaw_free (s);
  return result;
}

slaw slaw_cons_fc (slaw car, const char *cdr)
{
  slaw result = slaw_cons_lc (car, cdr);
  slaw_free (car);

  return result;
}

slaw slaw_cons_cc (const char *car, const char *cdr)
{
  if (!car || !cdr)
    return NULL;
  slaw a = slaw_string (car);
  slaw d = slaw_string (cdr);
  slaw result = NULL;

  if (a && d)
    result = slaw_cons (a, d);

  slaw_free (a);
  slaw_free (d);

  return result;
}

slaw slaw_cons_ca (const char *car, const unt8 *cdr, int64 N)
{
  if (!car || !cdr || (N <= 0))
    return NULL;

  slaw a = slaw_string (car);
  slaw result = NULL;

  if (a)
    result = slaw_container_array_internal (SLAW_NIB_CONS, a, cdr, N);

  slaw_free (a);

  return result;
}

bslaw slaw_cons_emit_car (bslaw s)
{
  if (!s || !SLAW_IS_CONS (s))
    return NULL;
  return slaw_container_first (s);
}


bslaw slaw_cons_emit_cdr (bslaw s)
{
  if (!s || !SLAW_IS_CONS (s))
    return NULL;
  return slaw_container_next (s, slaw_container_first (s));
}



OB_FLATTEN slaw slaw_list_f (slabu *sb)
{
  slaw s = slaw_list (sb);

  if (s)
    slabu_free (sb);

  return s;
}


slaw slaw_list (const slabu *sb)
{
  if (!sb)
    return NULL;
  return slaw_container_internal (SLAW_NIB_LIST, sb->numEsses,
                                  (const bslaw *) sb->esses);
}


static void uniquify_slawx (slaw *slawx, const int count)
{
  int i, j;
  for (i = 0; i < count; ++i)
    {
      if (slawx[i])
        {
          for (j = i + 1; j < count; ++j)
            if (slawx[j] == slawx[i])
              slawx[j] = slaw_dup (slawx[i]);
        }
    }
}


slaw slaw_list_inline_f (slaw s1, ...)
{
  if (!s1)
    return slaw_list_empty ();

  slaw args[MAX_INLINE_ARGS];
  args[0] = s1;
  int n = 1;
  va_list vargies;
  va_start (vargies, s1);
  while (n < MAX_INLINE_ARGS && (args[n] = va_arg (vargies, slaw)))
    ++n;
  va_end (vargies);

  uniquify_slawx (args, n);

  slabu *sb = slabu_new ();
  ob_retort err = sb ? OB_OK : OB_NO_MEM;

  int i = 0;
  for (; i < n && OB_OK == err; ++i)
    {
      ob_err_accum (&err, slabu_list_add_x (sb, args[i]));
      if (OB_OK == err)
        args[i] = NULL;
    }

  if (OB_OK != err)
    {
      slabu_free (sb);
      for (; i < n; ++i)
        slaw_free (args[i]);
      return NULL;
    }

  return slaw_list_f (sb);
}


slaw slaw_list_inline (bslaw s1, ...)
{
  slabu *sb = slabu_new ();
  if (!sb)
    return NULL;
  if (!s1)
    return slaw_list_f (sb);

  ob_retort err = OB_OK;
  ob_err_accum (&err, slabu_list_add_z (sb, s1));
  va_list vargies;
  va_start (vargies, s1);
  slaw s;
  while (OB_OK == err && (s = va_arg (vargies, slaw)))
    ob_err_accum (&err, slabu_list_add_z (sb, s));
  va_end (vargies);

  if (err != OB_OK)
    {
      slabu_free (sb);
      return NULL;
    }

  return slaw_list_f (sb);
}


bool slaw_is_list_or_map (bslaw s)
{
  return s ? SLAW_IS_LISTORMAP (s) : false;
}

bool slaw_is_list (bslaw s)
{
  return s ? SLAW_IS_LIST (s) : false;
}

bool slaw_is_map (bslaw s)
{
  return s ? SLAW_IS_MAP (s) : false;
}


int64 slaw_list_count (bslaw s)
{
  if (!s || !SLAW_IS_LISTORMAP (s))
    return -1;
  int64 count = ((SLAW_ILK (s) >> SLAW_WEE_CONTAINER_COUNT_SHIFTY)
                 & SLAW_MAX_WEE_CONTAINER);
  if (count == SLAW_MAX_WEE_CONTAINER)
    return s[1].o;
  else
    return count;
}


bslaw slaw_list_emit_first (bslaw s)
{
  if (!s || !SLAW_IS_LISTORMAP (s))
    return NULL;
  return slaw_container_first (s);
}


bslaw slaw_list_emit_nth (bslaw s, int64 n)
{
  if (!s || n < 0 || !SLAW_IS_LISTORMAP (s))
    return NULL;
  int64 count = ((SLAW_ILK (s) >> SLAW_WEE_CONTAINER_COUNT_SHIFTY)
                 & SLAW_MAX_WEE_CONTAINER);
  if (n < count || (count == SLAW_MAX_WEE_CONTAINER && n < s[1].o))
    {
      int64 i;
      bslaw where = slaw_container_first (s);
      for (i = 0; i < n; i++)
        where = slaw_container_next (s, where);
      return where;
    }

  return NULL;
}


bslaw slaw_list_emit_next (bslaw s_list, bslaw s_prev)
{
  if (!s_prev)
    return slaw_list_emit_first (s_list);
  if (!s_list || !SLAW_IS_LISTORMAP (s_list))
    return NULL;
  return slaw_container_next (s_list, s_prev);
}



slaw slaw_list_inline_c (const char *first_str, ...)
{
  va_list vargies;
  slabu *sb = slabu_new ();
  ob_retort err = OB_OK;
  if (!sb)
    return NULL;

  if (first_str && *first_str)
    {
      ob_err_accum (&err, slabu_list_add_internal (sb, slaw_string (first_str),
                                                   true));
      const char *str;
      va_start (vargies, first_str);
      while ((str = va_arg (vargies, const char *)))
        ob_err_accum (&err,
                      slabu_list_add_internal (sb, slaw_string (str), true));
      va_end (vargies);
    }

  if (err < OB_OK)
    {
      slabu_free (sb);
      return NULL;
    }

  slaw ess = slaw_list_f (sb);
  return ess;
}


int64 slaw_list_find (bslaw s, bslaw val)
{
  bslaw cole;
  int64 i = 0;

  for (cole = slaw_list_emit_first (s); cole != NULL;
       i++, cole = slaw_list_emit_next (s, cole))
    if (slawx_equal (val, cole))
      return i;

  return -1;
}

int64 slaw_list_find_f (bslaw s, slaw val)
{
  int64 result = slaw_list_find (s, val);
  slaw_free (val);
  return result;
}

int64 slaw_list_find_c (bslaw s, const char *str)
{
  return slaw_list_find_f (s, slaw_string (str));
}


static int64 slaw_list_vararg_search_internal (bslaw s,
                                               int64 (*cont) (bslaw, slaw),
                                               va_list ap)
{
  slabu *sb = slabu_new ();
  if (!sb)
    return OB_NO_MEM;

  slaw elem;
  while ((elem = va_arg (ap, slaw)) != NULL)
    {
      if (slabu_list_add_z (sb, elem) < 0)
        {
          slabu_free (sb);
          return OB_NO_MEM;
        }
    }

  slaw lst = slaw_list_f (sb);
  return lst ? cont (s, lst) : OB_NO_MEM;
}


static int64 slaw_list_vararg_search_internal_f (bslaw s,
                                                 int64 (*cont) (bslaw, slaw),
                                                 va_list ap)
{
  slaw args[MAX_INLINE_ARGS];
  int n = 0;
  while (n < MAX_INLINE_ARGS && (args[n] = va_arg (ap, slaw)))
    ++n;
  uniquify_slawx (args, n);

  slabu *sb = slabu_new ();
  int64 result = sb ? 0 : OB_NO_MEM;

  int i = 0;
  for (; i < n && 0 == result; ++i)
    {
      if (slabu_list_add_x (sb, args[i]) < 0)
        result = OB_NO_MEM;
      else
        args[i] = 0;
    }

  slaw list = sb ? slaw_list_f (sb) : NULL;
  if (!list)
    slabu_free (sb);

  if (result != 0)
    {
      slaw_free (list);
      for (; i < n; ++i)
        slaw_free (args[i]);
      return result;
    }
  else
    return cont (s, list);
}


static int64 slaw_list_vararg_search_internal_c (bslaw s,
                                                 int64 (*cont) (bslaw, slaw),
                                                 va_list ap)
{
  slabu *sb = slabu_new ();
  if (!sb)
    return OB_NO_MEM;

  const char *str;
  while ((str = va_arg (ap, const char *)) != NULL)
    {
      slaw s2 = slaw_string (str);
      if (!s2 || slabu_list_add_x (sb, s2) < 0)
        {
          slabu_free (sb);
          return OB_NO_MEM;
        }
    }

  slaw lst = slaw_list_f (sb);
  return lst ? cont (s, lst) : OB_NO_MEM;
}


int64 slaw_list_contigsearch (bslaw s, bslaw search)
{
  bslaw haystack;
  bslaw haystack2;
  bslaw needle;
  int64 i = 0;

  if (0 == slaw_list_count (search))
    return 0;

  for (haystack = slaw_list_emit_first (s); haystack != NULL;
       i++, haystack = slaw_list_emit_next (s, haystack))
    {
      for (haystack2 = haystack, needle = slaw_list_emit_first (search);
           haystack2 != NULL && needle != NULL;
           haystack2 = slaw_list_emit_next (s, haystack2),
          needle = slaw_list_emit_next (search, needle))
        if (!slawx_equal (haystack2, needle))
          break;

      if (needle == NULL)
        return i;
    }

  return -1;
}

int64 slaw_list_contigsearch_f (bslaw s, slaw search)
{
  int64 result = slaw_list_contigsearch (s, search);
  slaw_free (search);
  return result;
}

int64 slaw_list_contigsearch_inline (bslaw s, /* slaw s1, */...)
{
  va_list ap;
  int64 result;

  va_start (ap, s);
  result = slaw_list_vararg_search_internal (s, slaw_list_contigsearch_f, ap);
  va_end (ap);

  return result;
}

int64 slaw_list_contigsearch_inline_f (bslaw s, /* slaw s1, */...)
{
  va_list ap;
  int64 result;

  va_start (ap, s);
  result = slaw_list_vararg_search_internal_f (s, slaw_list_contigsearch_f, ap);
  va_end (ap);

  return result;
}

int64 slaw_list_contigsearch_inline_c (bslaw s, /* const char *str1, */...)
{
  va_list ap;
  int64 result;

  va_start (ap, s);
  result = slaw_list_vararg_search_internal_c (s, slaw_list_contigsearch_f, ap);
  va_end (ap);

  return result;
}


int64 slaw_list_gapsearch (bslaw s, bslaw search)
{
  bslaw haystack = slaw_list_emit_first (s);
  bslaw needle = slaw_list_emit_first (search);
  int64 hit = -1;
  int64 i = 0;

  if (0 == slaw_list_count (search))
    return 0;

  while (haystack && needle)
    {
      if (slawx_equal (haystack, needle))
        {
          if (hit < 0)
            hit = i;
          needle = slaw_list_emit_next (search, needle);
        }
      i++;
      haystack = slaw_list_emit_next (s, haystack);
    }

  return (needle ? -1 : hit);
}

int64 slaw_list_gapsearch_f (bslaw s, slaw search)
{
  int64 result = slaw_list_gapsearch (s, search);
  slaw_free (search);
  return result;
}

int64 slaw_list_gapsearch_inline (bslaw s, /* slaw s1, */...)
{
  va_list ap;
  int64 result;

  va_start (ap, s);
  result = slaw_list_vararg_search_internal (s, slaw_list_gapsearch_f, ap);
  va_end (ap);

  return result;
}

int64 slaw_list_gapsearch_inline_f (bslaw s, /* slaw s1, */...)
{
  va_list ap;
  int64 result;

  va_start (ap, s);
  result = slaw_list_vararg_search_internal_f (s, slaw_list_gapsearch_f, ap);
  va_end (ap);

  return result;
}

int64 slaw_list_gapsearch_inline_c (bslaw s, /* const char *str1, */...)
{
  va_list ap;
  int64 result;

  va_start (ap, s);
  result = slaw_list_vararg_search_internal_c (s, slaw_list_gapsearch_f, ap);
  va_end (ap);

  return result;
}

slaw slaw_map_empty (void)
{
  return slaw_container_internal (SLAW_NIB_MAP, 0, NULL);
}

slaw slaw_map (const slabu *sb)
{
  if (!sb)
    return NULL;
  else if (sb->obeysMapInvariant)
    return slaw_container_internal (SLAW_NIB_MAP, sb->numEsses,
                                    (const bslaw *) sb->esses);
  else
    return slaw_map_f (slabu_dup_shallow (sb));
}

slaw slaw_map_f (slabu *sb)
{
  // We can modify the existing slabu, since we're going to free it...
  if (!sb || !enforce_map_invariant (sb, DUP_KEEP_LAST_IN_POSITION_OF_FIRST))
    return NULL;

  slaw s = slaw_container_internal (SLAW_NIB_MAP, sb->numEsses,
                                    (const bslaw *) sb->esses);

  if (s)
    slabu_free (sb);

  return s;
}

static void safely_free_slawx (slaw *slawx, const int count)
{
  int i, j;
  for (i = 0; i < count; ++i)
    {
      if (slawx[i])
        {
          for (j = i + 1; j < count; ++j)
            if (slawx[j] == slawx[i])
              slawx[j] = NULL;
          slaw_free (slawx[i]);
        }
    }
}

#define DEFINE_SLAW_MAP_INLINER(NAME, KTYPE, VTYPE, CONS, PUT)                 \
  slaw NAME (KTYPE key1, VTYPE val1, ...)                                      \
  {                                                                            \
    if (!key1 || !val1)                                                        \
      return slaw_map_empty ();                                                \
                                                                               \
    slabu *sb = slabu_new ();                                                  \
    if (!sb || slabu_list_add_x (sb, CONS (key1, val1)) < 0)                   \
      {                                                                        \
        slabu_free (sb);                                                       \
        return NULL;                                                           \
      }                                                                        \
                                                                               \
    va_list ap;                                                                \
    va_start (ap, val1);                                                       \
    KTYPE car;                                                                 \
    VTYPE cdr;                                                                 \
    while ((car = va_arg (ap, KTYPE)) != NULL                                  \
           && (cdr = va_arg (ap, VTYPE)) != NULL)                              \
      {                                                                        \
        if (PUT (sb, car, cdr) < 0)                                            \
          {                                                                    \
            slabu_free (sb);                                                   \
            va_end (ap);                                                       \
            return NULL;                                                       \
          }                                                                    \
      }                                                                        \
    va_end (ap);                                                               \
                                                                               \
    return slaw_map_f (sb);                                                    \
  }

DEFINE_SLAW_MAP_INLINER (slaw_map_inline, bslaw, bslaw, slaw_cons,
                         slabu_map_put);
DEFINE_SLAW_MAP_INLINER (slaw_map_inline_cc, const char *, const char *,
                         slaw_cons_cc, slabu_map_put_cc);
DEFINE_SLAW_MAP_INLINER (slaw_map_inline_cl, const char *, bslaw, slaw_cons_cl,
                         slabu_map_put_cl);

#undef DEFINE_SLAW_MAP_INLINER

#define DEFINE_SLAW_MAP_FINLINER(NAME, KTYPE, CONS, PUT, ARGUER)               \
  slaw NAME (KTYPE key1, slaw val1, ...)                                       \
  {                                                                            \
    slabu *sb = slabu_new ();                                                  \
    if (sb && slabu_list_add_x (sb, CONS (key1, val1)) < 0)                    \
      {                                                                        \
        slabu_free (sb);                                                       \
        if (!key1 || !val1)                                                    \
          return slaw_map_empty ();                                            \
        sb = NULL;                                                             \
      }                                                                        \
                                                                               \
    slaw args[MAX_INLINE_ARGS];                                                \
    int arg_count = 0;                                                         \
    va_list ap;                                                                \
    va_start (ap, val1);                                                       \
    KTYPE car;                                                                 \
    slaw cdr;                                                                  \
    while (arg_count < MAX_INLINE_ARGS && (car = va_arg (ap, KTYPE)) != NULL   \
           && (cdr = va_arg (ap, slaw)) != NULL)                               \
      {                                                                        \
        if (sb && PUT (sb, car, cdr) < 0)                                      \
          {                                                                    \
            slabu_free (sb);                                                   \
            sb = NULL;                                                         \
          }                                                                    \
        ARGUER ();                                                             \
      }                                                                        \
    va_end (ap);                                                               \
                                                                               \
    slaw result = sb ? slaw_map_f (sb) : NULL;                                 \
    safely_free_slawx (args, arg_count);                                       \
    return result;                                                             \
  }

#define FF_ARGUER()                                                            \
  {                                                                            \
    args[arg_count++] = car;                                                   \
    args[arg_count++] = cdr;                                                   \
  }

DEFINE_SLAW_MAP_FINLINER (slaw_map_inline_ff, slaw, slaw_cons_ff, slabu_map_put,
                          FF_ARGUER);
#undef FF_ARGUER

#define CF_ARGUER()                                                            \
  {                                                                            \
    args[arg_count++] = cdr;                                                   \
  }

DEFINE_SLAW_MAP_FINLINER (slaw_map_inline_cf, const char *, slaw_cons_cf,
                          slabu_map_put_cl, CF_ARGUER);
#undef CF_ARGUER

#undef DEFINE_SLAW_MAP_FINLINER

static slaw slaw_maps_merge_internal (bslaw map1, va_list ap)
{
  slabu *sb = slabu_new ();
  bslaw theMap = map1;
  ob_retort err = OB_OK;

  if (!sb)
    return NULL;

  do
    {
      bslaw cole;
      for (cole = slaw_list_emit_first (theMap); cole != NULL;
           cole = slaw_list_emit_next (theMap, cole))
        ob_err_accum (&err, slabu_list_add_z (sb, cole));
    }
  while ((theMap = va_arg (ap, slaw)) != NULL);

  if (err < OB_OK)
    {
      slabu_free (sb);
      return NULL;
    }

  return slaw_map_f (sb);
}

slaw slaw_maps_merge_byarray (const bslaw *maps, int64 nmaps)
{
  slabu *sb = slabu_new ();
  ob_retort err = OB_OK;
  int64 i;

  if (!sb)
    return NULL;

  for (i = 0; i < nmaps; i++)
    {
      bslaw theMap = maps[i];
      bslaw cole;
      for (cole = slaw_list_emit_first (theMap); cole != NULL;
           cole = slaw_list_emit_next (theMap, cole))
        ob_err_accum (&err, slabu_list_add_z (sb, cole));
    }

  if (err < OB_OK)
    {
      slabu_free (sb);
      return NULL;
    }

  return slaw_map_f (sb);
}

slaw slaw_maps_merge (bslaw map1, /* slaw map2, */...)
{
  va_list ap;
  slaw result;

  va_start (ap, map1);
  result = slaw_maps_merge_internal (map1, ap);
  va_end (ap);

  return result;
}

slaw slaw_maps_merge_f (slaw map1, /* slaw map2, */...)
{
  va_list ap;

  va_start (ap, map1);
  slaw result = slaw_maps_merge_internal (map1, ap);
  va_end (ap);

  slaw args[MAX_INLINE_ARGS];
  int n = 1;
  args[0] = map1;
  va_start (ap, map1);
  while (n < MAX_INLINE_ARGS && (args[n] = va_arg (ap, slaw)) != NULL)
    ++n;
  va_end (ap);

  safely_free_slawx (args, n);

  return result;
}

bslaw slaw_map_find (bslaw s, bslaw key)
{
  bslaw cole;
  bslaw result = NULL;

  if (!s || !key || !slaw_is_list_or_map (s))
    return NULL;

  for (cole = slaw_list_emit_first (s); cole != NULL;
       cole = slaw_list_emit_next (s, cole))
    {
      if (slaw_is_cons (cole))
        {
          if (slawx_equal (key, slaw_cons_emit_car (cole)))
            {
              result = slaw_cons_emit_cdr (cole);
            }
        }
    }

  return result;
}

bslaw slaw_map_find_f (bslaw s, slaw key)
{
  bslaw result = slaw_map_find (s, key);
  slaw_free (key);
  return result;
}

// Could be implemented in terms of slaw_map_find, but we have
// highly optimized slaw_map_find_c because it is the common case.
bslaw slaw_map_find_c (bslaw s, const char *key)
{
  bslaw cole, m_end;
  char firstC;
  bslaw result = NULL;

  if (!s || !key || !slaw_is_list_or_map (s))
    return NULL;

  firstC = *key;

  // XXX: the new slaw_list_emit_next is more efficient, so the
  // following argument may no longer be true.
  //
  // We have effectively inlined slaw_list_emit_next so that we
  // can pull this slaw_octlen outside the loop, which would otherwise
  // have to be executed each time through the loop.
  m_end = s + slaw_octlen (s);
  unt64 ql = 0;
  for (cole = slaw_list_emit_first (s); cole < m_end; cole += ql)
    {
      ql = slaw_octlen (cole);
      if (!ql)
        break;
      const char *str = slaw_string_emit (slaw_cons_emit_car (cole));
      if (str && *str == firstC)
        {
          if (strcmp (key, str) == 0)
            {
              result = slaw_cons_emit_cdr (cole);
            }
        }
    }

  return result;
}


slaw slaw_boolean (bool value)
{
  slaw s = slaw_alloc ((unt64) 1);
  if (!s)
    return NULL;
  value = !!value; /* canonicalize value to 0 or 1 */
  s->o = SLAW_BOOL_ILK | (unt64) value;
  return s;
}

bool slaw_is_boolean (bslaw s)
{
  if (!s)
    return false;

  return SLAW_IS_BOOL (s);
}

const bool *slaw_boolean_emit (bslaw s)
{
  static const bool values[] = {false, true};

  if (!slaw_is_boolean (s))
    return NULL;

  return values + SLAW_BOOLEAN_VALUE (s);
}


bool slaw_is_protein (bslaw s)
{
  return s ? SLAW_IS_PROTEIN (s) : false;
}

bool slaw_is_swapped_protein (bslaw s)
{
  if (!s)
    return false;
  return SLAW_IS_SWAPPED_PROTEIN (s);
}



slaw slaw_alloc (unt64 octlen)
{
  if (octlen == 0 || octlen > SLAW_OCTLEN_MASK || octlen > (MAX_SLAW_SIZE >> 3))
    return NULL;
  return (slaw) malloc ((size_t) octlen << 3);
}


unt64 slaw_octlen (bslaw s)
{
  if (!s)
    return 0;
  slaw_oct ilk = SLAW_ILK (s);
  unt64 bsize, breadth, asize;
  switch (ilk >> SLAW_NIBBLE_SHIFTY)
    {
      case SLAW_NIB_SWAPPED_PROTEIN:
        ilk = ob_swap64 (ilk);
      // fall thru
      case SLAW_NIB_PROTEIN:
        ilk &= ~SLAW_PROTEIN_ILKMASK;
        return (ilk & 0xf) | (ilk >> 4);
      case SLAW_NIB_SYMBOL:
      case SLAW_NIB_WEE_STRING:
        return 1;
      case SLAW_NIB_LIST:
      case SLAW_NIB_MAP:
      case SLAW_NIB_CONS:
      case SLAW_NIB_FULL_STRING:
        return (ilk & SLAW_OCTLEN_MASK);
      case SLAW_NIB_SINGL_SINT:
      case SLAW_NIB_SINGL_UINT:
      case SLAW_NIB_SINGL_FLOAT:
        bsize = (1 + ((ilk >> SLAW_NUMERIC_BSIZE_SHIFTY)
                      & SLAW_NUMERIC_UNIT_BSIZE_MASK));
        return (bsize <= 4 ? 1 : 1 + ((bsize + 7) / 8));
      case SLAW_NIB_ARRAY_SINT:
      case SLAW_NIB_ARRAY_UINT:
      case SLAW_NIB_ARRAY_FLOAT:
        bsize = (1 + ((ilk >> SLAW_NUMERIC_BSIZE_SHIFTY)
                      & SLAW_NUMERIC_UNIT_BSIZE_MASK));
        breadth = (ilk & SLAW_NUMERIC_ARRAY_BREADTH_MASK);
        asize = (bsize * breadth);
        return 1 + ((asize + 7) / 8);
      default:
        return 0;
    }
}


OB_FLATTEN unt64 slaw_hash (bslaw s)
{
  unt64 olen = slaw_octlen (s);

  /* Since one-oct slawx (short strings, int32, etc.) are likely
   * to be common map keys, provide a fast path for them that's
   * completely inlined here, by using a 64-bit integer hash, rather
   * than calling out to CityHash.
   */
  if (olen == 1)
    return ob_hash_unt64 (s->o);
  else
    return ob_city_hash64 (s, 8 * olen);
}


//
//
//

typedef void (*fwfunc) (void *v, const char *fmt, ...) OB_FORMAT (printf, 2, 3);

static void format_to_file (void *v, const char *fmt, ...)
{
  FILE *whither = (FILE *) v;
  va_list vargs;
  va_start (vargs, fmt);
  vfprintf (whither, fmt, vargs);
  va_end (vargs);
}

static void format_to_slabu (void *v, const char *fmt, ...)
{
  slabu *sb = (slabu *) v;
  va_list vargs;
  va_start (vargs, fmt);
  slabu_list_add_internal (sb, slaw_string_vformat (fmt, vargs), true);
  va_end (vargs);
}

typedef struct ovew_struct
{
  void *whither;
  fwfunc func;
  int64 *inc;
  int64 *csave;
  int64 c;
  int64 v;
  int64 a;
} ovew_struct;

static const char *ovew_sep (ovew_struct *vw)
{
  const char *sep = "";
  if (*(vw->inc) > 0)
    {
      if (vw->inc == &(vw->c))
        sep = " + j";
      else if (vw->inc == &(vw->v))
        sep = ", ";
      else if (vw->inc == &(vw->a))
        sep = " ; ";
    }
  (*(vw->inc))++;
  return sep;
}

static ob_retort ovew_int (void *cookie, int64 val, int bits)
{
  ovew_struct *vw = (ovew_struct *) cookie;
  vw->func (vw->whither, "%s%" OB_FMT_64 "d", ovew_sep (vw), val);
  return OB_OK;
}

static ob_retort ovew_unt (void *cookie, unt64 val, int bits)
{
  ovew_struct *vw = (ovew_struct *) cookie;
  vw->func (vw->whither, "%s%" OB_FMT_64 "u", ovew_sep (vw), val);
  return OB_OK;
}

static ob_retort ovew_float (void *cookie, float64 val, int bits)
{
  ovew_struct *vw = (ovew_struct *) cookie;
  vw->func (vw->whither, "%s%f", ovew_sep (vw), val);
  return OB_OK;
}

static ob_retort ovew_array_begin (void *cookie, int64 unused, int notused)
{
  ovew_struct *vw = (ovew_struct *) cookie;
  vw->func (vw->whither, "[");
  return OB_OK;
}

static ob_retort ovew_array_end (void *cookie)
{
  ovew_struct *vw = (ovew_struct *) cookie;
  vw->func (vw->whither, "]");
  return OB_OK;
}

static ob_retort ovew_vector_begin (void *cookie, int64 unused)
{
  ovew_struct *vw = (ovew_struct *) cookie;
  vw->func (vw->whither, "%s(", ovew_sep (vw));
  vw->v = 0;
  vw->inc = &(vw->v);
  return OB_OK;
}

static ob_retort ovew_vector_end (void *cookie)
{
  ovew_struct *vw = (ovew_struct *) cookie;
  vw->func (vw->whither, ")");
  vw->inc = &(vw->a);
  return OB_OK;
}

static ob_retort ovew_complex_begin (void *cookie)
{
  ovew_struct *vw = (ovew_struct *) cookie;
  const char *sep = ovew_sep (vw);
  if (*sep)
    vw->func (vw->whither, "%s", sep);
  vw->c = 0;
  vw->csave = vw->inc;
  vw->inc = &(vw->c);
  return OB_OK;
}

static ob_retort ovew_complex_end (void *cookie)
{
  ovew_struct *vw = (ovew_struct *) cookie;
  vw->inc = vw->csave;
  return OB_OK;
}

static ob_retort ovew_array_empty (void *cookie, int vecsize, bool isMVec,
                                   bool isComplex, bool isUnsigned,
                                   bool isFloat, int bits)
{
  ovew_struct *vw = (ovew_struct *) cookie;
  vw->func (vw->whither, "[]");
  return OB_OK;
}

static const slaw_handler ovew_handler = {NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          ovew_array_begin,
                                          ovew_array_end,
                                          ovew_vector_begin,
                                          ovew_vector_end,
                                          ovew_vector_begin,
                                          ovew_vector_end,
                                          ovew_complex_begin,
                                          ovew_complex_end,
                                          NULL,
                                          NULL,
                                          ovew_int,
                                          ovew_unt,
                                          ovew_float,
                                          ovew_array_empty,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL};

#define _FW(...) func (whither, __VA_ARGS__)

static void slaw_spew_numeric_ovewview (bslaw s, fwfunc func, void *whither)
{
  int slinc =
    slaw_is_numeric_8 (s)
      ? 1
      : (slaw_is_numeric_16 (s) ? 2 : (slaw_is_numeric_32 (s) ? 4 : 8));
  bool isV = slaw_is_numeric_vector (s);
  bool isM = slaw_is_numeric_multivector (s);
  bool isC = slaw_is_numeric_complex (s);
  bool isA = slaw_is_numeric_array (s);

  int vecWid = slaw_numeric_vector_dimension (s);
  unt64 arrWid = slaw_numeric_array_count (s);

  if (isV)
    _FW ("V%d", vecWid);
  else if (isM)
    _FW ("M%d", vecWid);
  _FW ("%s%d", (slaw_is_numeric_float (s)
                  ? "FLOAT"
                  : (slaw_is_numeric_int (s)
                       ? "INT"
                       : (slaw_is_numeric_unt (s) ? "UNT" : "<unkn>"))),
       8 * slinc);

  if (isC)
    _FW ("C");
  if (isA)
    _FW ("/A(%" OB_FMT_64 "u"
         ")",
         arrWid);
  _FW (" = ");

  ovew_struct vw;
  vw.whither = whither;
  vw.func = func;
  vw.inc = &vw.a;
  vw.csave = NULL;
  vw.c = 0;
  vw.v = 0;
  vw.a = 0;

  slaw_walk (&vw, &ovew_handler, s);
}



static void slaw_spew_internal (bslaw s, fwfunc func, void *whither,
                                const char *prolo)
{
  if (!prolo)
    prolo = "";
  if (!s)
    {
      _FW ("%s[no slaw -- NULL]", prolo);
      return;
    }
  _FW ("%sslaw[%" OB_FMT_64 "uo.%p]: ", prolo, slaw_octlen (s), s);
  if (slaw_is_string (s))
    {
      _FW ("STR(%" OB_FMT_64 "d): \"%s\"", slaw_string_emit_length (s),
           slaw_string_emit (s));
    }
  else if (slaw_is_cons (s))
    {
      size_t l = strlen (prolo);
      char *down = (char *) malloc (l + 5);
      strcpy (down, prolo);
      strcpy (down + l, " L: ");
      _FW ("CONS:\n");
      slaw_spew_internal (slaw_cons_emit_car (s), func, whither, down);
      *(down + l + 1) = 'R';
      _FW ("\n");
      slaw_spew_internal (slaw_cons_emit_cdr (s), func, whither, down);
      free (down);
    }
  else if (slaw_is_numeric (s))
    slaw_spew_numeric_ovewview (s, func, whither);
  else if (slaw_is_list_or_map (s))
    {
      unt64 q, n = slaw_list_count (s);
      _FW ("%s (%" OB_FMT_64 "u elems): {", (slaw_is_map (s) ? "MAP" : "LIST"),
           n);
      q = 1;
      bslaw ess = slaw_list_emit_first (s);
      size_t l = strlen (prolo);
      const size_t extra = 32;
      char *prefix = (char *) malloc (l + extra);
      strcpy (prefix, prolo);
      for (; ess != NULL; q++, ess = slaw_list_emit_next (s, ess))
        {
          _FW ("\n");
          snprintf (prefix + l, extra, " %" OB_FMT_64 "u: ", q);
          slaw_spew_internal (ess, func, whither, prefix);
        }
      free (prefix);
      _FW ("\n%s }", prolo);
    }
  else if (slaw_is_nil (s))
    {
      _FW ("NIL.");
    }
  else if (slaw_is_protein (s))
    {
      bslaw descrips = protein_descrips (s);
      bslaw ingests = protein_ingests (s);
      int64 rudeLen;
      const unt8 *rude = (const unt8 *) protein_rude (s, &rudeLen);
      _FW ("PROT: ((\n");
      if (descrips)
        {
          _FW ("%sdescrips:\n", prolo);
          slaw_spew_internal (descrips, func, whither, prolo);
          _FW ("\n");
        }
      if (ingests)
        {
          _FW ("%singests:\n", prolo);
          slaw_spew_internal (ingests, func, whither, prolo);
          _FW ("\n");
        }
      if (rudeLen > 0)
        {
          int64 i;
          char  rudeBuf[OB_HEX_LINE_LEN];
          _FW ("%srude data: %" OB_FMT_64 "d bytes\n", prolo, rudeLen);
          for (i = 0; i < rudeLen; i += 16)
            {
              ob_fmt_hex_line (rudeBuf, rude + i, (size_t) (rudeLen - i));
              _FW ("%s %s\n", prolo, rudeBuf);
            }
        }
      _FW ("%s ))", prolo);
    }
  else if (slaw_is_boolean (s))
    {
      _FW ("BOOLEAN: %s", (*slaw_boolean_emit (s) ? "true" : "false"));
    }
  else
    {
      _FW ("[???]");
    }
}


void slaw_spew_overview (bslaw s, FILE *whither, const char *prolo)
{
  slaw_spew_internal (s, format_to_file, (void *) whither, prolo);
}

void slaw_spew_overview_to_stderr (bslaw s)
{
  slaw_spew_overview (s, stderr, NULL);
}

slaw slaw_spew_overview_to_string (bslaw s)
{
  slabu *sb = slabu_new ();
  if (!sb)
    return NULL;
  slaw_spew_internal (s, format_to_slabu, (void *) sb, NULL);
  return slaw_strings_join_slabu_f (sb, NULL);
}


#undef _FW

slaw_type slaw_gettype (bslaw s)
{
  if (s == NULL)
    return SLAW_TYPE_NULL;

  switch (SLAW_ILK_NIBBLE (s))
    {
      case SLAW_NIB_SWAPPED_PROTEIN:
      case SLAW_NIB_PROTEIN:
        return SLAW_TYPE_PROTEIN;
      case SLAW_NIB_SYMBOL:
        return (SLAW_ILK (s) == SLAW_NIL_ILK) ? SLAW_TYPE_NIL
                                              : SLAW_TYPE_BOOLEAN;
      case SLAW_NIB_WEE_STRING:
      case SLAW_NIB_FULL_STRING:
        return SLAW_TYPE_STRING;
      case SLAW_NIB_LIST:
      case SLAW_NIB_MAP:
        return SLAW_TYPE_LIST;
      case SLAW_NIB_CONS:
        return SLAW_TYPE_CONS;
      case SLAW_NIB_SINGL_SINT:
      case SLAW_NIB_SINGL_UINT:
      case SLAW_NIB_SINGL_FLOAT:
      case SLAW_NIB_ARRAY_SINT:
      case SLAW_NIB_ARRAY_UINT:
      case SLAW_NIB_ARRAY_FLOAT:
        return SLAW_TYPE_NUMERIC;
      default:
        return SLAW_TYPE_UNKNOWN;
    }
}

// If the header oct contains a smaller integer (in the "special bytes"),
// then swapping the header oct will also swap that smaller integer and
// reposition it to the correct position for special bytes in the new
// endianness.  However, if there was more than one small integer
// (e. g. bytes of a string, or a type like v3unt8 or int16c), then
// the multiple integers will be in backwards order, so we need to
// swap the integers back in the right order.  (But in the case of
// two 16-bit integers-- reswap_special_unt16s-- note that we don't
// need to swap the bytes *within* the integers, because that was already
// done by swapping the whole oct.)
static slaw_oct reswap_special_bytes (slaw_oct ilk, unt32 n)
{
  slaw_oct jlk = 0;
  unt32 i;
  for (i = 0; i < n; i++)
    {
      jlk <<= 8;
      jlk |= (ilk & 0xff);
      ilk >>= 8;
    }
  return (ilk << (8 * n)) | jlk;
}

static slaw_oct reswap_special_unt16s (slaw_oct ilk)
{
  return ((ilk & OB_CONST_U64 (0xffffffff00000000))
          | ((ilk << 16) & OB_CONST_U64 (0xffff0000))
          | ((ilk >> 16) & OB_CONST_U64 (0xffff)));
}

ob_retort slaw_swap (slaw s, slaw stop)
{
  slaw payload = s + 1;
  bool wee;
  unt32 primBytes, bsize;
  unt64 breadth;

  // start by swapping the header
  slaw_oct ilk = s->o = ob_swap64 (s->o);
  const unt64 octocat = slaw_octlen (s);

  if (s + octocat > stop)
    return SLAW_CORRUPT_SLAW;
  stop = s + octocat;

  switch (ilk >> SLAW_NIBBLE_SHIFTY)
    {
      case SLAW_NIB_SYMBOL:      /* nil and boolean have nothing else */
      case SLAW_NIB_FULL_STRING: /* UTF-8 string is endian-neutral */
        return OB_OK;
      case SLAW_NIB_WEE_STRING:
        s->o = reswap_special_bytes (ilk, SLAW_WEE_STRING_LEN (s));
        return OB_OK;
      case SLAW_NIB_LIST:
      case SLAW_NIB_MAP:
      case SLAW_NIB_CONS:
        wee =
          (((ilk >> SLAW_WEE_CONTAINER_COUNT_SHIFTY) & SLAW_MAX_WEE_CONTAINER)
           != SLAW_MAX_WEE_CONTAINER);
        /* swap the element count */
        if (!wee)
          {
            payload->o = ob_swap64 (payload->o);
            payload++;
          }
        return slaw_swap_sequence (payload, s + octocat);
      case SLAW_NIB_SINGL_SINT:
      case SLAW_NIB_SINGL_UINT:
      case SLAW_NIB_SINGL_FLOAT:
        bsize = SLAW_NUMERIC_UNIT_BSIZE (s);
        if (bsize <= 4)
          {
            switch (3 & (ilk >> SLAW_NUMERIC_SIZE_SHIFTY))
              {
                case 0:  // 8-bit: need to unswap them
                  s->o = reswap_special_bytes (ilk, bsize);
                  return OB_OK;
                case 1:  // 16-bit
                  if (bsize
                      == 4)  // if there were two unt16s, need to unswap them
                    s->o = reswap_special_unt16s (ilk);
                  return OB_OK;
                case 2:  // 32-bit: can only be one, and it was already swapped
                  return OB_OK;
                default:  // this shouldn't happen; slaw must be bad
                  return SLAW_CORRUPT_SLAW;
              }
          }
        else  // a singleton but not a wee singleton; layout is like an array
          {
            breadth = 1;
            goto swap_numeric;
          }
      case SLAW_NIB_ARRAY_SINT:
      case SLAW_NIB_ARRAY_UINT:
      case SLAW_NIB_ARRAY_FLOAT:
        breadth = SLAW_N_ARRAY_BREADTH (s);
      swap_numeric:
        primBytes = SLAW_NUMERIC_PRIM_BYTES (s);
        if (primBytes == 1)
          return OB_OK; /* 1-byte units don't need swapping */
        {
          unt64 nprims, i;
          void *e = payload;
          unt16 *p16;
          unt32 *p32;
          unt64 *p64;

          nprims = SLAW_NUMERIC_UNIT_BSIZE (s) / primBytes;
          nprims *= breadth;

          switch (primBytes)
            {
              case 2:
                p16 = (unt16 *) e;
                for (i = 0; i < nprims; i++, p16++)
                  *p16 = ob_swap16 (*p16);
                return OB_OK;
              case 4:
                p32 = (unt32 *) e;
                for (i = 0; i < nprims; i++, p32++)
                  *p32 = ob_swap32 (*p32);
                return OB_OK;
              case 8:
                p64 = (unt64 *) e;
                for (i = 0; i < nprims; i++, p64++)
                  *p64 = ob_swap64 (*p64);
                return OB_OK;
              default:  // shouldn't happen
                return SLAW_CORRUPT_SLAW;
            }
        }
      case SLAW_NIB_PROTEIN:
        {
          int i;
          ob_retort err;
          slaw cole = payload + 1;

          slaw_oct oct2 = payload->o = ob_swap64 (payload->o);
          if (!PROTEIN_IS_VERY_RUDE (s))
            payload->o =
              reswap_special_bytes (oct2, ((oct2 & SLAW_PROTEIN_WEE_RUDE_MASK)
                                           >> SLAW_PROTEIN_WEE_RUDE_SHIFTY));

          int nThingsToSwap =
            PROTEIN_HAS_DESCRIPS (s) + PROTEIN_HAS_INGESTS (s);

          for (i = 0; i < nThingsToSwap; i++)
            {
              err = slaw_swap (cole, stop);
              if (err != OB_OK)
                return err;
              cole += slaw_octlen (cole);
            }
          return OB_OK;
        }
      default:
        return SLAW_UNIDENTIFIED_SLAW;
    }
}

ob_retort slaw_swap_sequence (slaw s, slaw stop)
{
  ob_retort err = OB_OK;

  while (err == OB_OK && s < stop)
    {
      err = slaw_swap (s, stop);
      s += slaw_octlen (s);
    }

  if (s > stop)
    return SLAW_CORRUPT_SLAW; /* "stop" was in the middle of a slaw */

  return err;
}

#define E(x)                                                                   \
  case x:                                                                      \
    return #x

static const char *plasma_error_string (ob_retort err)
{
#define E(x)                                                                   \
  case x:                                                                      \
    return #x
  switch (err)
    {
      E (POOL_ALREADY_GANG_MEMBER);
      E (POOL_AWAIT_TIMEDOUT);
      E (POOL_AWAIT_WOKEN);
      E (POOL_CONFIG_BADTH);
      E (POOL_CONF_READ_BADTH);
      E (POOL_CONF_WRITE_BADTH);
      E (POOL_CORRUPT);
      E (POOL_CREATED);
      E (POOL_EMPTY_GANG);
      E (POOL_EXISTS);
      E (POOL_FIFO_BADTH);
      E (POOL_FILE_BADTH);
      E (POOL_FROZEN);
      E (POOL_FULL);
      E (POOL_ILLEGAL_NESTING);
      E (POOL_IMPOSSIBLE_RENAME);
      E (POOL_INAPPROPRIATE_FILESYSTEM);
      E (POOL_IN_USE);
      E (POOL_INVALIDATED_BY_FORK);
      E (POOL_INVALID_SIZE);
      E (POOL_MMAP_BADTH);
      E (POOL_NO_POOLS_DIR);
      E (POOL_NO_SUCH_POOL);
      E (POOL_NO_SUCH_PROTEIN);
      E (POOL_NOT_A_GANG_MEMBER);
      E (POOL_NOT_A_GREENHOUSE_SERVER);
      E (POOL_NOT_A_PROTEIN);
      E (POOL_NOT_A_PROTEIN_OR_MAP);
      E (POOL_NO_TLS);
      E (POOL_NULL_GANG);
      E (POOL_NULL_HOSE);
      E (POOL_POOLNAME_BADTH);
      E (POOL_PROTEIN_BIGGER_THAN_POOL);
      E (POOL_PROTOCOL_ERROR);
      E (POOL_RECV_BADTH);
      E (POOL_SEMAPHORES_BADTH);
      E (POOL_SEND_BADTH);
      E (POOL_SERVER_BUSY);
      E (POOL_SERVER_UNREACH);
      E (POOL_SOCK_BADTH);
      E (POOL_TLS_ERROR);
      E (POOL_TLS_REQUIRED);
      E (POOL_TYPE_BADTH);
      E (POOL_UNEXPECTED_CLOSE);
      E (POOL_UNSUPPORTED_OPERATION);
      E (POOL_WAKEUP_NOT_ENABLED);
      E (POOL_WRONG_VERSION);
      E (SLAW_ALIAS_NOT_SUPPORTED);
      E (SLAW_BAD_TAG);
      E (SLAW_CORRUPT_PROTEIN);
      E (SLAW_CORRUPT_SLAW);
      E (SLAW_END_OF_FILE);
      E (SLAW_FABRICATOR_BADNESS);
      E (SLAW_NOT_FOUND);
      E (SLAW_NOT_NUMERIC);
      E (SLAW_NO_YAML);
      E (SLAW_PARSING_BADNESS);
      E (SLAW_RANGE_ERR);
      E (SLAW_UNIDENTIFIED_SLAW);
      E (SLAW_WRONG_FORMAT);
      E (SLAW_WRONG_LENGTH);
      E (SLAW_WRONG_VERSION);
      E (SLAW_YAML_ERR);
      default:
        return NULL;
    }
#undef E
}

// We need to arrange for libLoam to be told about our error codes...
OB_PRE_POST (ob_add_error_names (plasma_error_string), ob_nop ());
