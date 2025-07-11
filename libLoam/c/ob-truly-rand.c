
/* (c)  oblong industries */

/* A cryptographically secure pseudo-random number generator:
 *
 * <s>
 * On OS X, use arc4random(), which is a user-space CSPRNG seeded from
 * /dev/urandom, but we have to work around two problems with it:
 *  - its state is duplicated by fork(), so we must force a reseed on fork
 *  - it never reseeds automatically, so we reseed it every 20 minutes
 * </s>
 *
 * Don't use arc4random on OS X anymore, because:
 *  - RC4 is considered to be not-very-secure
 *  - arc4random never fails; i. e. it uses an insecure seed (which is
 *    very bad) if it can't read from /dev/urandom
 *
 * So for now, use ISAAC-64 on OS X as well as Linux.  Eventually, we
 * should switch to something like libottery, once it becomes mature.
 *
 * On Linux, there is no built-in equivalent of arc4random (which is
 * somewhat ironic, since the Linux manual page for /dev/urandom is
 * much more stern than the OS X one about the fact that you should
 * be using the device to seed a CSPRNG, not getting all your random
 * numbers directly from the device), so we write our own equivalent
 * which is in the same spirit as arc4random, except that we use
 * the ISAAC-64 CSPRNG instead of ARC4.  (ISAAC-64 is faster than ARC4,
 * and might be of higher quality.)
 *
 * On Windows, we use CryptGenRandom directly, without having our own
 * CSPRNG, since my understanding is that CryptGenRandom is already
 * a userspace CSPRNG seeded from kernel entropy, much like arc4random.
 * (However, this is based on MS's scanty documentation.)
 *
 * On all three OSes, we have some amount of global state, so we use
 * a mutex to make things thread-safe.
 *
 * Note: for both arc4random and ISAAC-64, these algorithms do not
 * offer backtracking resistance.  So, although they are cryptographically
 * secure, they may not be satisfactory to the most paranoid users.
 * Since the Windows implementation is opaque, it's less clear whether
 * they offer backtracking resistance.  Some sources suggest that newer
 * versions (Windows 7) do, but older versions (Windows XP) don't.
 *
 * Oh, one more thing: starting with Ivy Bridge (spring 2012), Intel
 * will have a new instruction, RdRand (code-named Bull Mountain),
 * which generates random numbers in hardware, which should be faster
 * and higher quality than any of the above methods.  This file currently
 * has stubs showing where Bull Mountain support should go, but it's
 * waiting to be fleshed out once we actually have hardware to test on. */

#include "libLoam/c/ob-file.h"
#include "libLoam/c/ob-rand.h"
#include "libLoam/c/ob-sys.h"
#include "libLoam/c/ob-time.h"
#include "libLoam/c/ob-pthread.h"
#include "libLoam/c/ob-util.h"

#include <stdlib.h>
#include <string.h>

static ob_retort update_reseed_time (void);

typedef enum {
  RAND_NEED_INITIALIZATION = 0,
  RAND_NEED_RESEED,
  RAND_DOING_FINE
} rand_initialized_enum;

#ifdef _MSC_VER

static HCRYPTPROV hp = NULL;
static void *rand_mutex;

static ob_retort software_random (unt8 *dst, size_t len)
{
  CRITICAL_SECTION *cs = ob_fetch_critical (&rand_mutex);
  if (!cs)
    return OB_NO_MEM;

  EnterCriticalSection (cs);

  ob_retort tort = OB_OK;

  if ((hp == NULL
       && !CryptAcquireContext (&hp, NULL, NULL, PROV_RSA_FULL,
                                CRYPT_VERIFYCONTEXT | CRYPT_SILENT))
      || !CryptGenRandom (hp, len, (BYTE *) dst))
    tort = ob_win32err_to_retort (GetLastError ());

  LeaveCriticalSection (cs);

  return tort;
}

#else

/* ISAAC64 cryptographic pseudorandom number generator
 * from http://burtleburtle.net/bob/rand/isaacafa.html
 * By Bob Jenkins, 1996.  Public Domain. */

#define RANDSIZL (8)
#define RANDSIZ (1 << RANDSIZL)

static unt64 randrsl[RANDSIZ];
static size_t result_pos = 0;

static unt64 mm[RANDSIZ];
static unt64 aa = 0, bb = 0, cc = 0;

// This definition of ind is simpler and much cleaner.  Bob seemed to be
// smoking something in his original definition of ind.
#define ind(mm, x) (mm[((x) >> 3) & (RANDSIZ - 1)])
#define rngstep(mix, a, b, mm, m, m2, r, x)                                    \
  {                                                                            \
    x = *m;                                                                    \
    a = (mix) + *(m2++);                                                       \
    *(m++) = y = ind (mm, x) + a + b;                                          \
    *(r++) = b = ind (mm, y >> RANDSIZL) + x;                                  \
  }

static void isaac64 (void)
{
  unt64 a, b, x, y, *m, *m2, *r, *mend;
  m = mm;
  r = randrsl;
  a = aa;
  b = bb + (++cc);
  for (m = mm, mend = m2 = m + (RANDSIZ / 2); m < mend;)
    {
      rngstep (~(a ^ (a << 21)), a, b, mm, m, m2, r, x);
      rngstep (a ^ (a >> 5), a, b, mm, m, m2, r, x);
      rngstep (a ^ (a << 12), a, b, mm, m, m2, r, x);
      rngstep (a ^ (a >> 33), a, b, mm, m, m2, r, x);
    }
  for (m2 = mm; m2 < mend;)
    {
      rngstep (~(a ^ (a << 21)), a, b, mm, m, m2, r, x);
      rngstep (a ^ (a >> 5), a, b, mm, m, m2, r, x);
      rngstep (a ^ (a << 12), a, b, mm, m, m2, r, x);
      rngstep (a ^ (a >> 33), a, b, mm, m, m2, r, x);
    }
  bb = b;
  aa = a;

  result_pos = 0;
}

#define mix(a, b, c, d, e, f, g, h)                                            \
  {                                                                            \
    a -= e;                                                                    \
    f ^= h >> 9;                                                               \
    h += a;                                                                    \
    b -= f;                                                                    \
    g ^= a << 9;                                                               \
    a += b;                                                                    \
    c -= g;                                                                    \
    h ^= b >> 23;                                                              \
    b += c;                                                                    \
    d -= h;                                                                    \
    a ^= c << 15;                                                              \
    c += d;                                                                    \
    e -= a;                                                                    \
    b ^= d >> 14;                                                              \
    d += e;                                                                    \
    f -= b;                                                                    \
    c ^= e << 20;                                                              \
    e += f;                                                                    \
    g -= c;                                                                    \
    d ^= f >> 17;                                                              \
    f += g;                                                                    \
    h -= d;                                                                    \
    e ^= g << 14;                                                              \
    g += h;                                                                    \
  }

static void randinit (void)
{
  int i;
  unt64 a, b, c, d, e, f, g, h;
  aa = bb = cc = OB_CONST_U64 (0);
  a = b = c = d = e = f = g = h =
    OB_CONST_U64 (0x9e3779b97f4a7c13); /* the golden ratio */

  for (i = 0; i < 4; ++i) /* scramble it */
    {
      mix (a, b, c, d, e, f, g, h);
    }

  for (i = 0; i < RANDSIZ; i += 8) /* fill in mm[] with messy stuff */
    {
      if (true) /* use all the information in the seed */
        {
          a += randrsl[i];
          b += randrsl[i + 1];
          c += randrsl[i + 2];
          d += randrsl[i + 3];
          e += randrsl[i + 4];
          f += randrsl[i + 5];
          g += randrsl[i + 6];
          h += randrsl[i + 7];
        }
      mix (a, b, c, d, e, f, g, h);
      mm[i] = a;
      mm[i + 1] = b;
      mm[i + 2] = c;
      mm[i + 3] = d;
      mm[i + 4] = e;
      mm[i + 5] = f;
      mm[i + 6] = g;
      mm[i + 7] = h;
    }

  if (true)
    { /* do a second pass to make all of the seed affect all of mm */
      for (i = 0; i < RANDSIZ; i += 8)
        {
          a += mm[i];
          b += mm[i + 1];
          c += mm[i + 2];
          d += mm[i + 3];
          e += mm[i + 4];
          f += mm[i + 5];
          g += mm[i + 6];
          h += mm[i + 7];
          mix (a, b, c, d, e, f, g, h);
          mm[i] = a;
          mm[i + 1] = b;
          mm[i + 2] = c;
          mm[i + 3] = d;
          mm[i + 4] = e;
          mm[i + 5] = f;
          mm[i + 6] = g;
          mm[i + 7] = h;
        }
    }

  isaac64 (); /* fill in the first set of results */
}

static void do_software_random (unt8 *dst, size_t len)
{
  /* This uses the randrsl buffer from front to back, while Jenkins'
   * original code uses it from back to front.  Shouldn't matter, though,
   * since after all, it's random! */
  while (len > 0)
    {
      const size_t bytes_left = sizeof (randrsl) - result_pos;
      const size_t n = (len > bytes_left ? bytes_left : len);
      const unt8 *src = result_pos + (const unt8 *) randrsl;
      memcpy (dst, src, n);
      dst += n;
      len -= n;
      result_pos += n;
      if (result_pos >= sizeof (randrsl))
        isaac64 ();
    }
}

static ob_retort do_reseed (rand_initialized_enum inited)
{
  unt8 seed[sizeof (randrsl)];

  /* Some entropy to supplement our primary source, /dev/urandom */
  struct
  {
    unt64 t0;
    void *p0;
    unt64 t1;
    float64 t2;
    void *p1;
    int p2;
    unt64 t3;
  } seedinfo;

  OB_CLEAR (seedinfo);

  seedinfo.t0 = ob_monotonic_time ();
  seedinfo.p0 = malloc (8);
  free (seedinfo.p0);
  seedinfo.t1 = ob_monotonic_time ();
  seedinfo.t2 = ob_current_time ();
  seedinfo.p1 = &seedinfo;
  seedinfo.p2 = getpid ();

  OB_CLEAR (seed);

  unt8 *dst = seed + sizeof (seedinfo);
  size_t bytes_left = sizeof (seed) - sizeof (seedinfo);

#ifdef __APPLE__
  /* more than we need, but might as well */
  const size_t urandom_len = 256;
#else
  /* "man 4 urandom" recommends not using more than 32 bytes */
  const size_t urandom_len = 32;
#endif

  size_t red = 0;

  int fd = ob_open_cloexec ("/dev/urandom", O_RDONLY, 0);
  if (fd < 0)
    return ob_errno_to_retort (errno);
  while (red < urandom_len)
    {
      ssize_t res = read (fd, dst, urandom_len);
      const int erryes = errno;
      if (res < 0 && erryes == EINTR)
        continue;
      if (res <= 0)
        {
          close (fd);
          return (res == 0 ? OB_UNKNOWN_ERR : ob_errno_to_retort (erryes));
        }
      red += res;
    }
  if (close (fd) < 0)
    return ob_errno_to_retort (errno);

  dst += urandom_len;
  bytes_left -= urandom_len;

  if (inited == RAND_NEED_INITIALIZATION)
    {
#ifndef __APPLE__
      /* If you're building a snap package, and pool creation fails because
      * opening /proc/interrupt fails,
      * it's because snapd has access locked down six ways from sunday.
      * Workaround is as follows: in snapcraft.yaml, do one of the following:
      *
      * # 1. Either tell libLoam to not read entropy from /proc/interrupts:
      * apps:
      *   shiny-app-name:
      *     environment:
      *       OB_DISABLE_PROC_INTERRUPTS: 'true'
      *
      * # 2. or open up access to it
      * apps:
      *   shiny-app-name:
      *     plugs:
      *       hardware-observe
      * If you use #2, you'll also have to do
      *   sudo snap connect shiny-app-name:hardware-observe core:hardware-observe
      * after install.  Clearly, #1 is easier if it's enough for you.
      */
      if (!getenv ("OB_DISABLE_PROC_INTERRUPTS"))
        {
          /* use interrupt counters as another supplementary source of entropy */
          fd = ob_open_cloexec ("/proc/interrupts", O_RDONLY, 0);
          if (fd < 0)
            return ob_errno_to_retort (errno);
          if (read (fd, dst, bytes_left) < 0)
            {
              const int erryes = errno;
              close (fd);
              return ob_errno_to_retort (erryes);
            }
          if (close (fd) < 0)
            return ob_errno_to_retort (errno);
        }
#endif
    }
  else
    {
      /* hang onto some of the old seed material if this is a reseed */
      do_software_random (dst, bytes_left);
    }

  seedinfo.t3 = ob_monotonic_time ();
  memcpy (seed, &seedinfo, sizeof (seedinfo));

  memcpy (randrsl, seed, sizeof (randrsl));
  randinit ();
  return update_reseed_time ();
}

#define PARENT_ACTION()
#define CHILD_ACTION() rand_initialized = RAND_NEED_RESEED

#define STUFF_FOR_UNIX

#endif

/* code shared by both Linux and OS X */
#ifdef STUFF_FOR_UNIX

static rand_initialized_enum rand_initialized;

static unt64 last_reseed_time;
static unt8 call_count;

static const unt64 reseed_interval_nanos =
  OB_CONST_U64 (1000000000) /* nanoseconds per second */
  * 60                      /* seconds per minute */
  * 20;                     /* reseed every 20 minutes */

static pthread_mutex_t rand_mutex = PTHREAD_MUTEX_INITIALIZER;

static ob_retort rand_mutex_lock (void)
{
  const int erryes = pthread_mutex_lock (&rand_mutex);
  if (erryes == 0)
    return OB_OK;
  else
    return ob_errno_to_retort (erryes);
}

static ob_retort rand_mutex_unlock (void)
{
  const int erryes = pthread_mutex_unlock (&rand_mutex);
  if (erryes == 0)
    return OB_OK;
  else
    return ob_errno_to_retort (erryes);
}

static ob_retort update_reseed_time (void)
{
  last_reseed_time = ob_monotonic_time ();
  rand_initialized = RAND_DOING_FINE;
  return OB_OK;
}

static void atfork_prepare (void)
{
  rand_mutex_lock ();
}

static void atfork_parent (void)
{
  PARENT_ACTION ();
  rand_mutex_unlock ();
}

static void atfork_child (void)
{
  CHILD_ACTION ();
  rand_mutex_unlock ();
}

static ob_retort install_fork_handler (void)
{
  const int erryes =
    pthread_atfork (atfork_prepare, atfork_parent, atfork_child);

  if (erryes == 0)
    return OB_OK;
  else
    return ob_errno_to_retort (erryes);
}

/* a wrapper for do_software_random(), which handles locking,
 * initialization, and reseeding. */
static ob_retort software_random (unt8 *dst, size_t len)
{
  ob_retort tort = rand_mutex_lock ();
  if (tort < OB_OK)
    return tort;

  if (rand_initialized == RAND_DOING_FINE && 0 == call_count++
      && ob_monotonic_time () - last_reseed_time > reseed_interval_nanos)
    rand_initialized = RAND_NEED_RESEED;

  if (rand_initialized == RAND_NEED_INITIALIZATION)
    tort = install_fork_handler ();

  if (tort >= OB_OK && rand_initialized != RAND_DOING_FINE)
    tort = do_reseed (rand_initialized);

  if (tort >= OB_OK)
    do_software_random (dst, len);
  ob_err_accum (&tort, rand_mutex_unlock ());
  return tort;
}

#endif /* STUFF_FOR_UNIX */

/* Once Ivy Bridge chips are available, and gcc supports the _rdrand64_step
 * intrinsic (which I think will be gcc 4.7), use the result of CPUID
 * to determine which of these two functions to use. */
#if 0
static bool try_rdrand_unt64_ivy (unt64 *result)
{ return _rdrand64_step (result); }
#endif

static bool try_rdrand_unt64_dummy (OB_UNUSED unt64 *unused)
{
  return false;
}

static bool (*try_rdrand_unt64) (unt64 *r) = try_rdrand_unt64_dummy;

ob_retort ob_truly_random (void *dst, size_t len)
{
  unt8 *d = (unt8 *) dst;
  size_t l = len;
  unt64 tmp;

  while (l > 0 && try_rdrand_unt64 (&tmp))
    {
      const size_t n = (l > sizeof (tmp) ? sizeof (tmp) : l);
      memcpy (d, &tmp, n);
      d += n;
      l -= n;
    }

  if (l > 0)
    return software_random (d, l);
  else
    return OB_OK;
}
