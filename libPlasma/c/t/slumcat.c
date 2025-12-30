
/* (c)  oblong industries */

#include <libPlasma/c/slaw.h>
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-vers.h"


#define NUMLUMP(mp, vt, bt, ct)                                                \
                                                                               \
  vt##bt##ct od1_##vt##bt##ct[11];                                             \
  vt##bt##ct od2_##vt##bt##ct[11];                                             \
  vt##bt##ct od3_##vt##bt##ct[11];                                             \
  vt##bt##ct od4_##vt##bt##ct[11];                                             \
  bt val##vt##bt##ct = (bt) initial;                                           \
  bt *ptr##vt##bt##ct;                                                         \
                                                                               \
  ptr##vt##bt##ct = (bt *) ((void *) &od1_##vt##bt##ct[0]);                    \
  for (q = 0; q < 3 * mp; q++)                                                 \
    {                                                                          \
      *ptr##vt##bt##ct = val##vt##bt##ct;                                      \
      val##vt##bt##ct += 2;                                                    \
      ptr##vt##bt##ct += 1;                                                    \
    }                                                                          \
                                                                               \
  ptr##vt##bt##ct = (bt *) ((void *) &od2_##vt##bt##ct[0]);                    \
  for (q = 0; q < 1 * mp; q++)                                                 \
    {                                                                          \
      *ptr##vt##bt##ct = val##vt##bt##ct;                                      \
      val##vt##bt##ct += 2;                                                    \
      ptr##vt##bt##ct += 1;                                                    \
    }                                                                          \
                                                                               \
  ptr##vt##bt##ct = (bt *) ((void *) &od3_##vt##bt##ct[0]);                    \
  for (q = 0; q < 0 * mp; q++)                                                 \
    {                                                                          \
      *ptr##vt##bt##ct = val##vt##bt##ct;                                      \
      val##vt##bt##ct += 2;                                                    \
      ptr##vt##bt##ct += 1;                                                    \
    }                                                                          \
                                                                               \
  ptr##vt##bt##ct = (bt *) ((void *) &od4_##vt##bt##ct[0]);                    \
  for (q = 0; q < 5 * mp; q++)                                                 \
    {                                                                          \
      *ptr##vt##bt##ct = val##vt##bt##ct;                                      \
      val##vt##bt##ct += 2;                                                    \
      ptr##vt##bt##ct += 1;                                                    \
    }                                                                          \
                                                                               \
  sa1 = slaw_##vt##bt##ct##_array (od1_##vt##bt##ct, 3);                       \
  sa2 = slaw_##vt##bt##ct##_array (od2_##vt##bt##ct, 1);                       \
  sa3 = slaw_##vt##bt##ct##_array (od3_##vt##bt##ct, 0);                       \
  sa4 = slaw_##vt##bt##ct##_array (od4_##vt##bt##ct, 5);                       \
                                                                               \
  OBSERT (NULL != sa1);                                                        \
  OBSERT (NULL != sa2);                                                        \
  OBSERT (NULL != sa3);                                                        \
  OBSERT (NULL != sa4);                                                        \
  snumcat = slaw_##vt##bt##ct##_arrays_concat_f (                              \
    slaw_dup (sa1), slaw_dup (sa2), slaw_dup (sa3), slaw_dup (sa4), NULL);     \
  ptr##vt##bt##ct =                                                            \
    (bt *) ((void *) slaw_##vt##bt##ct##_array_emit (snumcat));                \
                                                                               \
  val##vt##bt##ct = (bt) initial;                                              \
  for (q = 0; q < 9 * mp; q++)                                                 \
    {                                                                          \
      OBSERT (val##vt##bt##ct == *ptr##vt##bt##ct);                            \
      val##vt##bt##ct += 2;                                                    \
      ptr##vt##bt##ct += 1;                                                    \
    }                                                                          \
                                                                               \
  Free_Slaw (sa1);                                                             \
  Free_Slaw (sa2);                                                             \
  Free_Slaw (sa3);                                                             \
  Free_Slaw (sa4);                                                             \
  Free_Slaw (snumcat);

void lumps1 (void)
{
  int q;
  slaw sa1, sa2, sa3, sa4;
  slaw snumcat;

  /* This needs to start negative in the int8 case in order to avoid
   * overflowing.  Signed overflow is undefined in C:
   *
   * http://blog.llvm.org/2011/05/what-every-c-programmer-should-know.html
   *
   * And, gcc 4.7 chooses to use the flexibility of "undefined behavior"
   * to generate an infinite loop when signed overflow occurs, at least in
   * some cases.  In fact, if you set "initial" to 1 here, and compile with
   * gcc 4.7 with -O3, this program will not terminate.  (Yes, this took
   * an entire evening to figure out.  Why can't the !@#$%^ gcc people
   * emit a trap instruction for undefined behavior, like clang does,
   * rather than an infinite loop, which is much harder to debug?) */
  int initial = -127;

  NUMLUMP (1, , int8, );
  NUMLUMP (2, , int8, c);
  NUMLUMP (2, v2, int8, );
  NUMLUMP (4, v2, int8, c);
  NUMLUMP (3, v3, int8, );
  NUMLUMP (6, v3, int8, c);
  NUMLUMP (4, v4, int8, );
  NUMLUMP (8, v4, int8, c);

  initial = 1;

  NUMLUMP (1, , unt8, );
  NUMLUMP (2, , unt8, c);
  NUMLUMP (2, v2, unt8, );
  NUMLUMP (4, v2, unt8, c);
  NUMLUMP (3, v3, unt8, );
  NUMLUMP (6, v3, unt8, c);
  NUMLUMP (4, v4, unt8, );
  NUMLUMP (8, v4, unt8, c);


  NUMLUMP (1, , int16, );
  NUMLUMP (2, , int16, c);
  NUMLUMP (2, v2, int16, );
  NUMLUMP (4, v2, int16, c);
  NUMLUMP (3, v3, int16, );
  NUMLUMP (6, v3, int16, c);
  NUMLUMP (4, v4, int16, );
  NUMLUMP (8, v4, int16, c);

  NUMLUMP (1, , unt16, );
  NUMLUMP (2, , unt16, c);
  NUMLUMP (2, v2, unt16, );
  NUMLUMP (4, v2, unt16, c);
  NUMLUMP (3, v3, unt16, );
  NUMLUMP (6, v3, unt16, c);
  NUMLUMP (4, v4, unt16, );
  NUMLUMP (8, v4, unt16, c);


  NUMLUMP (1, , int32, );
  NUMLUMP (2, , int32, c);
  NUMLUMP (2, v2, int32, );
  NUMLUMP (4, v2, int32, c);
  NUMLUMP (3, v3, int32, );
  NUMLUMP (6, v3, int32, c);
  NUMLUMP (4, v4, int32, );
  NUMLUMP (8, v4, int32, c);

  NUMLUMP (1, , unt32, );
  NUMLUMP (2, , unt32, c);
  NUMLUMP (2, v2, unt32, );
  NUMLUMP (4, v2, unt32, c);
  NUMLUMP (3, v3, unt32, );
  NUMLUMP (6, v3, unt32, c);
  NUMLUMP (4, v4, unt32, );
  NUMLUMP (8, v4, unt32, c);
}

void lumps2 (void)
{
  int q;
  slaw sa1, sa2, sa3, sa4;
  slaw snumcat;
  const int initial = 1;

  NUMLUMP (1, , int64, );
  NUMLUMP (2, , int64, c);
  NUMLUMP (2, v2, int64, );
  NUMLUMP (4, v2, int64, c);
  NUMLUMP (3, v3, int64, );
  NUMLUMP (6, v3, int64, c);
  NUMLUMP (4, v4, int64, );
  NUMLUMP (8, v4, int64, c);

  NUMLUMP (1, , unt64, );
  NUMLUMP (2, , unt64, c);
  NUMLUMP (2, v2, unt64, );
  NUMLUMP (4, v2, unt64, c);
  NUMLUMP (3, v3, unt64, );
  NUMLUMP (6, v3, unt64, c);
  NUMLUMP (4, v4, unt64, );
  NUMLUMP (8, v4, unt64, c);
}

int main (int ac, char **av)
{
  OB_DIE_ON_ERROR (OB_CHECK_ABI ());
  //
  // numbers, Dirk.
  //
  lumps1 ();
  lumps2 ();

  return 0;
}
