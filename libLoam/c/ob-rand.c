
/* (c)  oblong industries */

#include "libLoam/c/ob-atomic.h"
#include "libLoam/c/ob-attrs.h"
#include "libLoam/c/ob-file.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-rand.h"
#include "libLoam/c/ob-sys.h"
#include "libLoam/c/ob-time.h"
#include "libLoam/c/ob-util.h"
#include <math.h>
#include <stdlib.h>

#include "libLoam/c/private/dSFMT.h"


static ob_rand_t *ob_global_rand_state = NULL;

void ob_rand_seed_int32 (int32 seedval)
{
  // Originally, ob_global_rand_state was just an ob_rand_t rather
  // than a pointer to one.  But, when generating 32-bit code, Red Hat
  // gcc 4.1 refused to 16-byte align it, causing segmentation faults.

  if (ob_global_rand_state)
    ob_rand_state_seed_int32 (ob_global_rand_state, seedval);
  else
    ob_global_rand_state = ob_rand_allocate_state (seedval);

  if (!ob_global_rand_state)
    ob_log_fatal (OBLV_ERRU, 0x10050002, EXIT_FAILURE,
                  "Out of memory.  Fatal!\n");
}

ob_rand_t *ob_rand_allocate_state (int32 seedval)
{
  ob_retort tort = OB_NO_MEM;
  ob_rand_t *ret = NULL;

  // The posix_memalign call is not available on Mac.  But, we can use
  // the Mac's plain malloc instead, since it is guaranteed to be
  // aligned for any type including SSE-related types.

  /* And on Windows, we use VirtualAlloc(), which will give us
   page-aligned memory, which will be more than enough alignment!
   (It's a bit wasteful, but since there will only be a few of these
   allocated in the whole program, it doesn't matter.) */

  /* It is unclear whether posix_memalign will zero its pseudo-return
   value (ob_global_rand_state) on error, so we do it below. Plus,
   this has the advantage of showing the compiler that we are at
   least paying some attention to the return value (err), which is
   good since otherwise the Ubuntu 9.04 optimized build will fail
   with a "werror". ( The posix_memalign function is annotated with
   warn_unused_result. )*/

#ifdef __APPLE__
  ret = (ob_rand_t *) malloc (sizeof (ob_rand_t));
  if (!ret)
    goto badness;
#elif defined(_MSC_VER)
  ret = (ob_rand_t *) VirtualAlloc (NULL, sizeof (ob_rand_t),
                                    MEM_COMMIT | MEM_RESERVE | MEM_TOP_DOWN,
                                    PAGE_READWRITE);
  if (!ret)
    {
      DWORD lasterr = GetLastError ();
      tort = ob_win32err_to_retort (lasterr);
      goto badness;
    }
#else
  int err = posix_memalign ((void **) &ret, 16, sizeof (ob_rand_t));
  if (err)
    {
      ret = NULL;
      tort = ob_errno_to_retort (err);
      goto badness;
    }
#endif

  ob_rand_state_seed_int32 (ret, seedval);
  return ret;

badness:
  ob_log (OBLV_ERRU, 0x10050001, "Can't allocate memory for ob_rand_t: %s\n",
          ob_error_string (tort));
  return NULL;
}

void ob_rand_free_state (ob_rand_t *rand_state)
{
#ifdef _MSC_VER
  if (!VirtualFree (rand_state, 0, MEM_RELEASE))
    {
      DWORD lasterr = GetLastError ();
      ob_retort tort = ob_win32err_to_retort (lasterr);
      ob_log (OBLV_ERRU, 0x10050003, "Can't free %p because '%s'\n", rand_state,
              ob_error_string (tort));
    }
#else
  free (rand_state);
#endif
}

typedef struct
{
  float64 t;
  int32 p;
  int32 c;
} fallback_seed_t;

typedef union
{
  fallback_seed_t fallback;
  unt32 words[4];
} ob_seed_t;

static int32 counter;

static ob_seed_t completely_random_seed (void)
{
  ob_seed_t seed;
  ob_retort tort = ob_truly_random (&seed, sizeof (seed));
  if (tort < OB_OK)
    {
      // Back-up seed, better than not random at all.
      seed.fallback.t = ob_current_time ();
      seed.fallback.p = getpid ();
      seed.fallback.c = ob_atomic_int32_add (&counter, 1);
      if (seed.fallback.c == 1)
        OB_LOG_WARNING_CODE (0x10050000,
                             "Using suboptimal entropy source\n"
                             "because '%s'\n",
                             ob_error_string (tort));
    }
  return seed;
}

//////////////////////////////////////////////////////////////////////
// start of obRand functions based on user-supplied dSFMT state
//////////////////////////////////////////////////////////////////////

void ob_rand_state_seed_int32 (ob_rand_t *rand_state, int32 seedval)
{
  assert (rand_state != NULL);
  if (seedval == OB_RAND_COMPLETELY_RANDOM_PLEASE)
    {
      ob_seed_t seed = completely_random_seed ();
      dsfmt_init_by_array (rand_state, seed.words,
                           sizeof (seed.words) / sizeof (seed.words[0]));
    }
  else
    dsfmt_init_gen_rand (rand_state, seedval);
}



float64 ob_rand_state_float64 (float64 low, float64 high, ob_rand_t *rand_state)
{
  assert (rand_state != NULL);
  // TODO what should happen if high <= low?
  assert (high > low);
  return low + (high - low) * dsfmt_genrand_close_open (rand_state);
}



int32 ob_rand_state_int32 (int32 low, int32 high, ob_rand_t *rand_state)
{
  float64 v;
  assert (rand_state != NULL);
  // TODO what should happen if high <= low?
  assert (high > low);

  v = (float64) low
      + (((float64) ((int64) high - (int64) low))
         * dsfmt_genrand_close_open (rand_state));

  // TODO prefer a faster method for negative case; problem: the
  // contract is low <= x < high, but an int cast truncates (moves
  // toward zero), which is wrong.

  // TODO prefer a faster method overall -- int casts are slow due to
  // need to set cpu rounding state; see, e.g.,
  // http://ldesoras.free.fr/doc/articles/rounding_en.pdf (Section 1)

  if (v >= 0)
    return (int32) v;
  else
    return (int32) floor (v);
}


unt32 ob_rand_state_unt32 (ob_rand_t *rand_state)
{
  return dsfmt_genrand_uint32 (rand_state);
}

unt64 ob_rand_state_unt64 (ob_rand_t *rand_state)
{
  unt32 lo = dsfmt_genrand_uint32 (rand_state);
  unt64 hi = dsfmt_genrand_uint32 (rand_state);
  hi <<= 32;
  return (lo ^ hi);
}


//////////////////////////////////////////////////////////////////////
// start of obRand functions based on implicit state
//////////////////////////////////////////////////////////////////////

static inline void INIT (void)
{
  if (!ob_global_rand_state)
    ob_global_rand_state =
      ob_rand_allocate_state (OB_RAND_COMPLETELY_RANDOM_PLEASE);
}

float64 ob_rand_float64 (float64 low, float64 high)
{
  INIT ();
  return ob_rand_state_float64 (low, high, ob_global_rand_state);
}

int32 ob_rand_int32 (int32 low, int32 high)
{
  INIT ();
  return ob_rand_state_int32 (low, high, ob_global_rand_state);
}

unt32 ob_rand_unt32 (void)
{
  INIT ();
  return ob_rand_state_unt32 (ob_global_rand_state);
}

unt64 ob_rand_unt64 (void)
{
  INIT ();
  return ob_rand_state_unt64 (ob_global_rand_state);
}

float64 ob_rand_normal (float64 *second)
{
  INIT ();
  return ob_rand_normal_state (ob_global_rand_state, second);
}


////////////////////////////////////////////////////////////
// end of obRand functions
////////////////////////////////////////////////////////////

float64 ob_rand_normal_state (ob_rand_t *rand_state, float64 *second)
{
  // Marsaglia polar method -- better methods do exist (e.g.,
  // ziggeraut), but this one is a reasonable choice unless/until we
  // find that the speed of generating normal variates is slowing down
  // the whole system

  float64 x, y, s;
  do
    {
      x = ob_rand_state_float64 (0.0, 1.0, rand_state) * 2.0 - 1.0;
      y = ob_rand_state_float64 (0.0, 1.0, rand_state) * 2.0 - 1.0;
      s = x * x + y * y;
    }
  while (s >= 1.0);

  float64 tmp = sqrt (-2.0 * log (s) / s);
  if (second)
    *second = y * tmp;
  return x * tmp;
}

void ob_random_bytes (unt8 *uu, size_t n)
{
  size_t idx = 0;
  for (; n - idx > 4; idx += 4)
    {
      union
      {
        unt8 bytes[4];
        int32 quad;
      } data;
      data.quad = ob_rand_int32 (OB_INT32_MIN, OB_INT32_MAX);
      memcpy (uu + idx, data.bytes, 4);
    }
  for (; idx < n; idx++)
    uu[idx] = (unt8) ob_rand_int32 (0, 256);
}

void ob_random_bytes_state (ob_rand_t *rand_state, unt8 *uu, size_t n)
{
  size_t i;
  unt64 u = 0;
  for (i = 0; i < n; i++)
    {
      if (0 == (i & 7))
        u = ob_rand_state_unt64 (rand_state);
      else
        u >>= 8;
      uu[i] = (u & 0xff);
    }
}
