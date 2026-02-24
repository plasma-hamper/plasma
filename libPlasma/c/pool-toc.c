
/* (c)  oblong industries */

#include "private/pool-toc.h"
#include "libLoam/c/ob-log.h"

#include <stddef.h>
#include <assert.h>

// Private types and constants

typedef struct
{
  unt64 offset;
  pool_timestamp stamp;
} index_entry;

struct pool_toc_s
{
  unt64 signature;        /* magic number */
  unt64 capacity;         /* maximum number of entries */
  unt64 count;            /* current number of entries */
  unt64 start;            /* first entry position */
  unt64 step;             /* idx diff between entries */
  int64 first;            /* idx of the first entry */
  int64 last;             /* idx of the last added protein */
  index_entry entries[1]; /* start of the circular buffer */
};

#define EMPTY_ENTRY_GUTS {POOL_TOC_UNKNOWN_OFFSET, POOL_TOC_UNKNOWN_TIME}

static const index_entry EMPTY_ENTRY = EMPTY_ENTRY_GUTS;

#define SIGNATURE OB_CONST_U64 (0x00BEEF00FEED0011)

/* ---------------------------------------------------------------------- */
// Private functions

// we always use an even capacity
static inline unt64 normalize_capacity (unt64 c)
{
  return (c & OB_CONST_U64 (1)) ? c + 1 : c;
}


// physical index from logical n in [0, pi->capacity)
static inline unt64 nth_pos (const pool_toc_t *pi, unt64 n)
{
  return (pi->start + n) % pi->capacity;
}


// actual entry from logical n in [0, pi->count)
static inline index_entry *nth_entry (const pool_toc_t *pi, unt64 n)
{
  return (index_entry *) (pi->entries + nth_pos (pi, n));
}


// logical position for protein index
static inline unt64 n_for_idx (const pool_toc_t *pi, int64 idx)
{
  return (idx < pi->first) ? 0 : (idx - pi->first) / pi->step;
}


static inline int64 idx_from_n (const pool_toc_t *pi, unt64 n)
{
  return pi->first + n * pi->step;
}


pool_timestamp nth_stamp (const pool_toc_t *pi, unt64 n)
{
  if (!pi || n >= pi->count)
    return POOL_TOC_UNKNOWN_TIME;
  index_entry *entry = nth_entry (pi, n);
  return entry ? entry->stamp : POOL_TOC_UNKNOWN_TIME;
}


// next non-empty entry
static inline index_entry *next_entry (const pool_toc_t *pi, unt64 *n)
{
  unt64 i = *n + 1;
  if (i >= pi->count)
    return NULL;
  index_entry *entry = nth_entry (pi, i);
  while (pi->count > i && entry->offset == POOL_TOC_UNKNOWN_OFFSET)
    entry = nth_entry (pi, ++i);
  if (i >= pi->count || entry->offset == POOL_TOC_UNKNOWN_OFFSET)
    return NULL;
  *n = i;
  return entry;
}


// entry for protein index
static bool entries_for_idx (const pool_toc_t *pi, int64 *di, index_entry **de,
                             int64 *ui, index_entry **ue)
{
  assert (pi);
  if (*di < pi->first)
    return false;

  unt64 no = *di < pi->last ? n_for_idx (pi, *di) : pi->count - 1;
  if (no >= pi->count)
    return false;
  *de = nth_entry (pi, no);
  if (*de)
    *di = idx_from_n (pi, no);
  *ue = next_entry (pi, &no);
  if (*ue)
    *ui = idx_from_n (pi, no);

  return true;
}


static bool entries_for_stamp (const pool_toc_t *pi, pool_timestamp ts,
                               int64 *di, index_entry **de, int64 *ui,
                               index_entry **ue)
{
  if (0 == pi->count)
    return false;

  if (ts < nth_stamp (pi, 0))
    {
      *de = NULL;
      *ui = idx_from_n (pi, 0);
      *ue = nth_entry (pi, 0);
      return true;
    }

  unt64 first = 0, mid = 0;
  unt64 last = pi->count;
  index_entry *entry = NULL;
  while (first < last)
    {
      mid = (last + first) / 2;
      entry = nth_entry (pi, mid);
      assert (entry);
      if (entry->stamp == ts)
        break;
      if (entry->stamp < ts)
        first = mid + 1;
      else
        last = mid;
    }

  while (entry->stamp > ts && mid > 0)
    entry = nth_entry (pi, --mid);

  *di = idx_from_n (pi, mid);
  *de = entry;
  *ue = next_entry (pi, &mid);
  if (*ue)
    *ui = idx_from_n (pi, mid);

  return true;
}


static inline bool fill_result (int64 idx, const index_entry *ie,
                                pool_toc_entry *result)
{
  if (result)
    {
      result->idx = ie ? idx : POOL_TOC_UNKNOWN_IDX;
      result->stamp = ie ? ie->stamp : POOL_TOC_UNKNOWN_TIME;
      result->offset = ie ? ie->offset : POOL_TOC_UNKNOWN_OFFSET;
    }
  return ie != NULL;
}


static bool check_result (const pool_toc_t *pi, const pool_toc_entry *e)
{
  if (!e || POOL_TOC_UNKNOWN_IDX == e->idx)
    return true;
  index_entry *ie = nth_entry (pi, n_for_idx (pi, e->idx));
  return e->offset == ie->offset && e->stamp == ie->stamp;
}


/// If the index is full, remove any index entries which are
/// no longer in the pool.  This is done "lazily" (i. e. only
/// when we are out of space) because that results in fewer
/// times that we have to update our three variables (which
/// happens non-atomically, so it could result in an inconsistency
/// which will cause the reader to retry).
/// oldest_offset is the offset of the oldest protein that's still
/// in the pool (i. e. all offsets less than it are no longer in
/// the pool) and is the value returned by get_oldest_entry() in
/// pool_mmap.c.
static void garbage_collect (pool_toc_t *pi, unt64 oldest_offset)
{
  if (pi->count > pi->capacity)
    OB_LOG_ERROR_CODE (0x20103000,
                       "pi->count (%" OB_FMT_64 "u) is larger than "
                       "pi->capacity (%" OB_FMT_64 "u)\n",
                       pi->count, pi->capacity);
  else if (pi->count == pi->capacity)
    {
      unt64 num_to_kill = 0;
      while (num_to_kill < pi->count
             && nth_entry (pi, num_to_kill)->offset < oldest_offset)
        num_to_kill++;
      // Would be nice if these three updates could happen atomically,
      // but they can't.  So, sometimes readers will get an inconsistent
      // view.  However, I believe that the worst thing that can happen
      // (although please think long and hard about this and see if
      // I'm wrong) is that the reader will see that he got an
      // inconsistent entry and retry later.
      pi->count -= num_to_kill;
      pi->start += num_to_kill;
      pi->first += num_to_kill * pi->step;
      // Hmmm... aren't start and first going to always advance together?
      // Maybe we can take advantage of that.
    }
}


static void compact_index (pool_toc_t *pi)
{
  if (pi->count >= pi->capacity)
    {
      pi->count /= 2;
      pi->step *= 2;
      unt64 i = 1;
      for (; i < pi->count; i++)
        {
          index_entry *to = nth_entry (pi, i);
          index_entry *from = nth_entry (pi, 2 * i);
          assert (to);
          assert (from);
          *to = *from;
        }
    }
}


static void add_entry (pool_toc_t *pi, const pool_toc_entry *e,
                       unt64 oldest_offset)
{
  if (0 == pi->count)
    pi->first = e->idx;

  unt64 n = n_for_idx (pi, e->idx);
  if (n >= pi->count)
    {
      while (n > pi->count)
        {
          index_entry *entry = nth_entry (pi, pi->count);
          assert (entry);
          *entry = EMPTY_ENTRY;
          pi->count++;
        }
      // two different ways to try to get the space we need...
      garbage_collect (pi, oldest_offset);
      compact_index (pi);
      index_entry *entry = nth_entry (pi, pi->count);
      assert (entry);
      entry->offset = e->offset;
      entry->stamp = e->stamp;
      pi->count++;
    }
  pi->last = e->idx;
}

/* ---------------------------------------------------------------------- */
// Public interface
const pool_toc_entry POOL_TOC_NULL_ENTRY = {
  POOL_TOC_UNKNOWN_IDX, POOL_TOC_UNKNOWN_OFFSET, POOL_TOC_UNKNOWN_TIME};


pool_toc_t *pool_toc_read (const byte *buffer)
{
  if (!buffer)
    return NULL;
  pool_toc_t *idx = (pool_toc_t *) buffer;
  if (SIGNATURE != idx->signature)
    {
      ob_log (OBLV_DBUG, 0x20103001, "Wrong signature: %" OB_FMT_64 "u\n",
              idx->signature);
      return NULL;
    }
  return idx;
}


pool_toc_t *pool_toc_init (byte *buffer, unt64 capacity)
{
  static const struct pool_toc_s EMPTY_INDEX = {
    /*.signature =*/SIGNATURE,
    0,  //need capacity
    /*.count =*/0,
    /*.start =*/0,
    /*.step =*/1,
    /*.first =*/0,
    /*.last =*/0,
    {EMPTY_ENTRY_GUTS}  //need entries
  };

  if (!buffer || capacity == 0)
    return NULL;
  pool_toc_t *idx = (pool_toc_t *) buffer;
  memcpy ((void *) idx, &EMPTY_INDEX, sizeof (EMPTY_INDEX));
  idx->capacity = normalize_capacity (capacity);
  memcpy ((void *) &idx->entries[0], &EMPTY_ENTRY, sizeof (EMPTY_ENTRY));
  assert (pool_toc_read (buffer) == idx);
  return idx;
}


unt64 pool_toc_room (unt64 capacity)
{
  return sizeof (pool_toc_t)
         + (normalize_capacity (capacity) - 1) * sizeof (index_entry);
}


unt64 pool_toc_capacity (const pool_toc_t *pi)
{
  return pi ? pi->capacity : 0;
}


unt64 pool_toc_count (const pool_toc_t *pi)
{
  return pi ? pi->count : 0;
}


unt64 pool_toc_step (const pool_toc_t *pi)
{
  return pi ? pi->step : 0;
}


bool pool_toc_find_idx (const pool_toc_t *pi, int64 idx, pool_toc_entry *lower,
                        pool_toc_entry *upper)
{
  if (!pi || idx < pi->first)
    return false;

  do
    {
      int64 iu = idx;
      index_entry *ed = NULL, *eu = NULL;
      if (!entries_for_idx (pi, &idx, &ed, &iu, &eu))
        return false;
      fill_result (idx, ed, lower);
      fill_result (iu, eu, upper);
    }
  while (!check_result (pi, lower) || !check_result (pi, upper));

  return true;
}


bool pool_toc_find_timestamp (const pool_toc_t *pi, pool_timestamp ts,
                              pool_toc_entry *lower, pool_toc_entry *upper)
{
  if (!pi)
    return false;

  do
    {
      int64 id = -1, iu = -1;
      index_entry *ed = NULL, *eu = NULL;
      if (!entries_for_stamp (pi, ts, &id, &ed, &iu, &eu))
        return false;
      fill_result (id, ed, lower);
      fill_result (iu, eu, upper);
    }
  while (!check_result (pi, lower) || !check_result (pi, upper));

  return true;
}


pool_timestamp pool_toc_min_timestamp (const pool_toc_t *pi)
{
  return nth_stamp (pi, 0);
}


pool_timestamp pool_toc_max_timestamp (const pool_toc_t *pi)
{
  return pi ? nth_stamp (pi, pi->count - 1) : POOL_TOC_UNKNOWN_TIME;
}


bool pool_toc_append (pool_toc_t *pi, pool_toc_entry e, unt64 oldest_offset)
{
  if (!pi || e.idx < 0 || e.stamp < 0)
    return false;
  add_entry (pi, &e, oldest_offset);
  return true;
}


#if 0 /* unused */
void
pool_toc_dump (const pool_toc_t *pi, FILE *out)
{ if (! pi  ||  ! out)
    return;
  unt64 i = 0, last = pi->count;
  for (  ;  i < last  ;  i++)
    { index_entry *entry = nth_entry (pi, i);
      assert (entry);
      fprintf (out,
               "%" OB_FMT_64 "d. (%" OB_FMT_64 "d, %lf) @ %" OB_FMT_64 "u\n",
               i, pi->first + pi->step * i, entry->stamp, entry->offset);
    }
}
#endif
