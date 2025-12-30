
/* (c)  oblong industries */

#include <private/pool-toc.h>

#include <gtest/gtest.h>
#include <string.h>

// Helpers
::std::ostream &operator<< (::std::ostream &o, const pool_toc_entry &e)
{
  return o << "{" << e.idx << ", " << e.offset << ", " << e.stamp << "}";
}

bool operator== (const pool_toc_entry &l, const pool_toc_entry &r)
{
  return l.idx == r.idx && l.offset == r.offset && l.stamp == r.stamp;
}

bool operator> (const pool_toc_entry &l, const pool_toc_entry &r)
{
  if (l.stamp == POOL_TOC_UNKNOWN_TIME)
    return r.stamp != POOL_TOC_UNKNOWN_TIME;
  return l.idx > r.idx && l.stamp > r.stamp && l.offset > r.offset;
}

static pool_toc_entry check_entry (const pool_toc_t *pi, pool_timestamp ts,
                                   const pool_toc_entry &low)
{
  pool_toc_entry l = POOL_TOC_NULL_ENTRY, u = POOL_TOC_NULL_ENTRY;
  EXPECT_TRUE (pool_toc_find_timestamp (pi, ts, &l, &u))
    << "Seeking timestamp " << ts;
  EXPECT_EQ (low, l);

  if (!POOL_TOC_ENTRY_NULL_P (l))
    EXPECT_GT (u, l);
  else
    EXPECT_FALSE (POOL_TOC_ENTRY_NULL_P (u));

  return u;
}

static pool_toc_entry check_entry (const pool_toc_t *pi, int64 idx,
                                   pool_timestamp ts, const pool_toc_entry &low)
{
  pool_toc_entry l = POOL_TOC_NULL_ENTRY, u = POOL_TOC_NULL_ENTRY;
  EXPECT_TRUE (pool_toc_find_idx (pi, idx, &l, &u)) << "Seeking index " << idx;
  EXPECT_EQ (low, l);
  EXPECT_GT (u, l);
  return check_entry (pi, ts, low);
}

static void check_entry (const pool_toc_t *pi, const pool_toc_entry &entry)
{
  check_entry (pi, entry.idx, entry.stamp, entry);
}

// Fixtures

pool_toc_entry test_data[] = {
  {0, 1, 0.11},       {1, 19, 0.2},        {2, 21, 0.33},
  {3, 31, 50.40002},  {4, 32, 284.6},      {5, 33, 3231.0216},
  {6, 42, 3233},      {7, 543, 3233.001},  {8, 552, 3240.2},
  {9, 6000, 10000.1}, {10, 6001, 10000.2}, {11, 6002, 10001.1}};

static const unt64 TESTDN = sizeof (test_data) / sizeof (test_data[0]);

static void fill_pool_toc (pool_toc_t *pi, unt64 no = 0)
{
  if (0 == no)
    no = pool_toc_capacity (pi);
  for (unt64 i = 0; i < no; ++i)
    {
      EXPECT_TRUE (pool_toc_append (pi, test_data[i], 0));
      EXPECT_EQ (i + 1, pool_toc_count (pi));
      check_entry (pi, test_data[i]);
    }
  EXPECT_EQ (test_data[0].stamp, pool_toc_min_timestamp (pi));
  EXPECT_EQ (test_data[no - 1].stamp, pool_toc_max_timestamp (pi));
}

static pool_toc_t *make_index (unt64 c)
{
  return pool_toc_init ((byte *) malloc (pool_toc_room (c)), c);
}

static pool_toc_t *make_full_index (unt64 c = TESTDN)
{
  pool_toc_t *pi = make_index (c);
  fill_pool_toc (pi);
  return pi;
}

/* ---------------------------------------------------------------------- */
// Tests

TEST (TocUnitTest, EmptyTableOfContents)
{
  EXPECT_GT (pool_toc_room (0), 0UL);
  EXPECT_GT (pool_toc_room (5), pool_toc_room (4));

  EXPECT_TRUE (NULL == pool_toc_read (NULL));
  EXPECT_TRUE (NULL == pool_toc_init (NULL, 3));
  EXPECT_EQ (0UL, pool_toc_capacity (NULL));
  EXPECT_EQ (0UL, pool_toc_count (NULL));
  EXPECT_FALSE (pool_toc_find_idx (NULL, 1, NULL, NULL));
  pool_toc_entry l = POOL_TOC_NULL_ENTRY, u = POOL_TOC_NULL_ENTRY;
  EXPECT_FALSE (pool_toc_find_timestamp (NULL, 1.2, &l, &u));
  EXPECT_EQ (POOL_TOC_NULL_ENTRY, l);
  EXPECT_EQ (POOL_TOC_NULL_ENTRY, u);

  byte *buffer = (byte *) malloc (pool_toc_room (20));
  memset (buffer, 0, pool_toc_room (20));
  EXPECT_TRUE (NULL == pool_toc_read (buffer));

  pool_toc_t *idx = pool_toc_init (buffer, 5);
  EXPECT_TRUE (idx != NULL);
  EXPECT_EQ (6UL, pool_toc_capacity (idx));
  EXPECT_EQ (0UL, pool_toc_count (idx));

  idx = pool_toc_init (buffer, 20);
  EXPECT_TRUE (idx != NULL);
  EXPECT_EQ (20UL, pool_toc_capacity (idx));
  EXPECT_EQ (0UL, pool_toc_count (idx));

  EXPECT_FALSE (pool_toc_find_idx (idx, 0, NULL, NULL));
  EXPECT_FALSE (pool_toc_find_idx (idx, 2, NULL, NULL));

  EXPECT_FALSE (pool_toc_find_timestamp (idx, -1.2, &l, &u));
  EXPECT_EQ (POOL_TOC_NULL_ENTRY, l);
  EXPECT_EQ (POOL_TOC_NULL_ENTRY, u);

  free (buffer);
}

TEST (TocUnitTest, Monotonic)
{
  pool_toc_t *pi = make_full_index ();
  for (unt64 i = 0; i < TESTDN; ++i)
    {
      check_entry (pi, test_data[i]);

      static const float64 SHIFTS[] = {0.3, 0.5, 0.77};
      static const int K = sizeof SHIFTS / sizeof SHIFTS[0];

      pool_timestamp delta =
        (i < TESTDN - 1) ? test_data[i + 1].stamp - test_data[i].stamp : 1;
      for (int k = 0; k < K; ++k)
        {
          pool_toc_entry e = check_entry (
            pi, test_data[i].stamp + SHIFTS[k] * delta, test_data[i]);
          if (i < TESTDN - 1)
            EXPECT_EQ (e, test_data[i + 1]);
          else
            EXPECT_TRUE (POOL_TOC_ENTRY_NULL_P (e));
        }

      delta = (i > 0) ? test_data[i].stamp - test_data[i - 1].stamp : 1;
      for (int k = 0; k < K; ++k)
        {
          if (i > 0)
            {
              pool_toc_entry e = check_entry (
                pi, test_data[i].stamp - SHIFTS[k] * delta, test_data[i - 1]);
              EXPECT_EQ (e, test_data[i]);
            }
          else
            {
              pool_toc_entry e = check_entry (
                pi, test_data[0].stamp - SHIFTS[k], POOL_TOC_NULL_ENTRY);
              EXPECT_EQ (e, test_data[0]);
            }
        }
    }
  free ((void *) pi);  // cast away "volatile" to satisfy compiler
}

TEST (TocUnitTest, CapacityWrap)
{
  const unt64 CAP = TESTDN / 2;
  pool_toc_t *pi = make_full_index (CAP);

  for (unt64 i = CAP; i < TESTDN; ++i)
    EXPECT_TRUE (pool_toc_append (pi, test_data[i], 0));

  // Same number of entries, but spaced by two
  EXPECT_EQ (CAP, pool_toc_count (pi));
  EXPECT_EQ (pool_toc_capacity (pi), pool_toc_count (pi));

  // Only the even entries of the first batch have survived (the odd
  // ones are merged into the even ones):
  for (unt64 i = 0; i < TESTDN; i += 2)
    {
      check_entry (pi, test_data[i]);
      pool_toc_entry e = check_entry (pi, test_data[i + 1].idx,
                                      test_data[i + 1].stamp, test_data[i]);
      if (i + 2 < TESTDN)
        EXPECT_EQ (test_data[i + 2], e);
      else
        EXPECT_TRUE (POOL_TOC_ENTRY_NULL_P (e));
    }

  free ((void *) pi);  // cast away "volatile" to satisfy compiler
}
