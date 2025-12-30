
/* (c)  oblong industries */

#include <libPlasma/c/slaw.h>
#include <libPlasma/c/protein.h>
#include "libLoam/c/ob-log.h"


#define NUMSSEMBLE(vecnm, basicnm, sizenm, complexnm)                          \
  vecnm##basicnm##sizenm##complexnm

#define NILK_TEST(vecpart, basicpart, sizepart, complexpart, arrayizer)        \
  (NULL                                                                        \
   != slaw_##vecpart##basicpart##sizepart##complexpart##arrayizer##_emit (s))



#define YELP(...) return t;
#define GUTS
#define HURL NILK_TEST

#define FORE t++,


#define NUMBLE()                                                               \
  if (FORE HURL (, int, 8, , ))                                                \
    YELP (GUTS)                                                                \
  if (FORE HURL (, unt, 8, , ))                                                \
    YELP (GUTS)                                                                \
  if (FORE HURL (, int, 16, , ))                                               \
    YELP (GUTS)                                                                \
  if (FORE HURL (, unt, 16, , ))                                               \
    YELP (GUTS)                                                                \
  if (FORE HURL (, int, 32, , ))                                               \
    YELP (GUTS)                                                                \
  if (FORE HURL (, unt, 32, , ))                                               \
    YELP (GUTS)                                                                \
  if (FORE HURL (, int, 64, , ))                                               \
    YELP (GUTS)                                                                \
  if (FORE HURL (, unt, 64, , ))                                               \
    YELP (GUTS)                                                                \
  if (FORE HURL (, float, 32, , ))                                             \
    YELP (GUTS)                                                                \
  if (FORE HURL (, float, 64, , ))                                             \
    YELP (GUTS)                                                                \
                                                                               \
  if (FORE HURL (v2, int, 8, , ))                                              \
    YELP (GUTS)                                                                \
  if (FORE HURL (v2, unt, 8, , ))                                              \
    YELP (GUTS)                                                                \
  if (FORE HURL (v2, int, 16, , ))                                             \
    YELP (GUTS)                                                                \
  if (FORE HURL (v2, unt, 16, , ))                                             \
    YELP (GUTS)                                                                \
  if (FORE HURL (v2, int, 32, , ))                                             \
    YELP (GUTS)                                                                \
  if (FORE HURL (v2, unt, 32, , ))                                             \
    YELP (GUTS)                                                                \
  if (FORE HURL (v2, int, 64, , ))                                             \
    YELP (GUTS)                                                                \
  if (FORE HURL (v2, unt, 64, , ))                                             \
    YELP (GUTS)                                                                \
  if (FORE HURL (v2, float, 32, , ))                                           \
    YELP (GUTS)                                                                \
  if (FORE HURL (v2, float, 64, , ))                                           \
    YELP (GUTS)                                                                \
                                                                               \
  if (FORE HURL (v3, int, 8, , ))                                              \
    YELP (GUTS)                                                                \
  if (FORE HURL (v3, unt, 8, , ))                                              \
    YELP (GUTS)                                                                \
  if (FORE HURL (v3, int, 16, , ))                                             \
    YELP (GUTS)                                                                \
  if (FORE HURL (v3, unt, 16, , ))                                             \
    YELP (GUTS)                                                                \
  if (FORE HURL (v3, int, 32, , ))                                             \
    YELP (GUTS)                                                                \
  if (FORE HURL (v3, unt, 32, , ))                                             \
    YELP (GUTS)                                                                \
  if (FORE HURL (v3, int, 64, , ))                                             \
    YELP (GUTS)                                                                \
  if (FORE HURL (v3, unt, 64, , ))                                             \
    YELP (GUTS)                                                                \
  if (FORE HURL (v3, float, 32, , ))                                           \
    YELP (GUTS)                                                                \
  if (FORE HURL (v3, float, 64, , ))                                           \
    YELP (GUTS)                                                                \
                                                                               \
  if (FORE HURL (v4, int, 8, , ))                                              \
    YELP (GUTS)                                                                \
  if (FORE HURL (v4, unt, 8, , ))                                              \
    YELP (GUTS)                                                                \
  if (FORE HURL (v4, int, 16, , ))                                             \
    YELP (GUTS)                                                                \
  if (FORE HURL (v4, unt, 16, , ))                                             \
    YELP (GUTS)                                                                \
  if (FORE HURL (v4, int, 32, , ))                                             \
    YELP (GUTS)                                                                \
  if (FORE HURL (v4, unt, 32, , ))                                             \
    YELP (GUTS)                                                                \
  if (FORE HURL (v4, int, 64, , ))                                             \
    YELP (GUTS)                                                                \
  if (FORE HURL (v4, unt, 64, , ))                                             \
    YELP (GUTS)                                                                \
  if (FORE HURL (v4, float, 32, , ))                                           \
    YELP (GUTS)                                                                \
  if (FORE HURL (v4, float, 64, , ))                                           \
    YELP (GUTS)                                                                \
                                                                               \
  if (FORE HURL (, int, 8, c, ))                                               \
    YELP (GUTS)                                                                \
  if (FORE HURL (, unt, 8, c, ))                                               \
    YELP (GUTS)                                                                \
  if (FORE HURL (, int, 16, c, ))                                              \
    YELP (GUTS)                                                                \
  if (FORE HURL (, unt, 16, c, ))                                              \
    YELP (GUTS)                                                                \
  if (FORE HURL (, int, 32, c, ))                                              \
    YELP (GUTS)                                                                \
  if (FORE HURL (, unt, 32, c, ))                                              \
    YELP (GUTS)                                                                \
  if (FORE HURL (, int, 64, c, ))                                              \
    YELP (GUTS)                                                                \
  if (FORE HURL (, unt, 64, c, ))                                              \
    YELP (GUTS)                                                                \
  if (FORE HURL (, float, 32, c, ))                                            \
    YELP (GUTS)                                                                \
  if (FORE HURL (, float, 64, c, ))                                            \
    YELP (GUTS)                                                                \
                                                                               \
  if (FORE HURL (v2, int, 8, c, ))                                             \
    YELP (GUTS)                                                                \
  if (FORE HURL (v2, unt, 8, c, ))                                             \
    YELP (GUTS)                                                                \
  if (FORE HURL (v2, int, 16, c, ))                                            \
    YELP (GUTS)                                                                \
  if (FORE HURL (v2, unt, 16, c, ))                                            \
    YELP (GUTS)                                                                \
  if (FORE HURL (v2, int, 32, c, ))                                            \
    YELP (GUTS)                                                                \
  if (FORE HURL (v2, unt, 32, c, ))                                            \
    YELP (GUTS)                                                                \
  if (FORE HURL (v2, int, 64, c, ))                                            \
    YELP (GUTS)                                                                \
  if (FORE HURL (v2, unt, 64, c, ))                                            \
    YELP (GUTS)                                                                \
  if (FORE HURL (v2, float, 32, c, ))                                          \
    YELP (GUTS)                                                                \
  if (FORE HURL (v2, float, 64, c, ))                                          \
    YELP (GUTS)                                                                \
                                                                               \
  if (FORE HURL (v3, int, 8, c, ))                                             \
    YELP (GUTS)                                                                \
  if (FORE HURL (v3, unt, 8, c, ))                                             \
    YELP (GUTS)                                                                \
  if (FORE HURL (v3, int, 16, c, ))                                            \
    YELP (GUTS)                                                                \
  if (FORE HURL (v3, unt, 16, c, ))                                            \
    YELP (GUTS)                                                                \
  if (FORE HURL (v3, int, 32, c, ))                                            \
    YELP (GUTS)                                                                \
  if (FORE HURL (v3, unt, 32, c, ))                                            \
    YELP (GUTS)                                                                \
  if (FORE HURL (v3, int, 64, c, ))                                            \
    YELP (GUTS)                                                                \
  if (FORE HURL (v3, unt, 64, c, ))                                            \
    YELP (GUTS)                                                                \
  if (FORE HURL (v3, float, 32, c, ))                                          \
    YELP (GUTS)                                                                \
  if (FORE HURL (v3, float, 64, c, ))                                          \
    YELP (GUTS)                                                                \
                                                                               \
  if (FORE HURL (v4, int, 8, c, ))                                             \
    YELP (GUTS)                                                                \
  if (FORE HURL (v4, unt, 8, c, ))                                             \
    YELP (GUTS)                                                                \
  if (FORE HURL (v4, int, 16, c, ))                                            \
    YELP (GUTS)                                                                \
  if (FORE HURL (v4, unt, 16, c, ))                                            \
    YELP (GUTS)                                                                \
  if (FORE HURL (v4, int, 32, c, ))                                            \
    YELP (GUTS)                                                                \
  if (FORE HURL (v4, unt, 32, c, ))                                            \
    YELP (GUTS)                                                                \
  if (FORE HURL (v4, int, 64, c, ))                                            \
    YELP (GUTS)                                                                \
  if (FORE HURL (v4, unt, 64, c, ))                                            \
    YELP (GUTS)                                                                \
  if (FORE HURL (v4, float, 32, c, ))                                          \
    YELP (GUTS)                                                                \
  if (FORE HURL (v4, float, 64, c, ))                                          \
    YELP (GUTS)                                                                \
                                                                               \
  if (FORE HURL (, int, 8, , _array))                                          \
    YELP (GUTS)                                                                \
  if (FORE HURL (, unt, 8, , _array))                                          \
    YELP (GUTS)                                                                \
  if (FORE HURL (, int, 16, , _array))                                         \
    YELP (GUTS)                                                                \
  if (FORE HURL (, unt, 16, , _array))                                         \
    YELP (GUTS)                                                                \
  if (FORE HURL (, int, 32, , _array))                                         \
    YELP (GUTS)                                                                \
  if (FORE HURL (, unt, 32, , _array))                                         \
    YELP (GUTS)                                                                \
  if (FORE HURL (, int, 64, , _array))                                         \
    YELP (GUTS)                                                                \
  if (FORE HURL (, unt, 64, , _array))                                         \
    YELP (GUTS)                                                                \
  if (FORE HURL (, float, 32, , _array))                                       \
    YELP (GUTS)                                                                \
  if (FORE HURL (, float, 64, , _array))                                       \
    YELP (GUTS)                                                                \
                                                                               \
  if (FORE HURL (v2, int, 8, , _array))                                        \
    YELP (GUTS)                                                                \
  if (FORE HURL (v2, unt, 8, , _array))                                        \
    YELP (GUTS)                                                                \
  if (FORE HURL (v2, int, 16, , _array))                                       \
    YELP (GUTS)                                                                \
  if (FORE HURL (v2, unt, 16, , _array))                                       \
    YELP (GUTS)                                                                \
  if (FORE HURL (v2, int, 32, , _array))                                       \
    YELP (GUTS)                                                                \
  if (FORE HURL (v2, unt, 32, , _array))                                       \
    YELP (GUTS)                                                                \
  if (FORE HURL (v2, int, 64, , _array))                                       \
    YELP (GUTS)                                                                \
  if (FORE HURL (v2, unt, 64, , _array))                                       \
    YELP (GUTS)                                                                \
  if (FORE HURL (v2, float, 32, , _array))                                     \
    YELP (GUTS)                                                                \
  if (FORE HURL (v2, float, 64, , _array))                                     \
    YELP (GUTS)                                                                \
                                                                               \
  if (FORE HURL (v3, int, 8, , _array))                                        \
    YELP (GUTS)                                                                \
  if (FORE HURL (v3, unt, 8, , _array))                                        \
    YELP (GUTS)                                                                \
  if (FORE HURL (v3, int, 16, , _array))                                       \
    YELP (GUTS)                                                                \
  if (FORE HURL (v3, unt, 16, , _array))                                       \
    YELP (GUTS)                                                                \
  if (FORE HURL (v3, int, 32, , _array))                                       \
    YELP (GUTS)                                                                \
  if (FORE HURL (v3, unt, 32, , _array))                                       \
    YELP (GUTS)                                                                \
  if (FORE HURL (v3, int, 64, , _array))                                       \
    YELP (GUTS)                                                                \
  if (FORE HURL (v3, unt, 64, , _array))                                       \
    YELP (GUTS)                                                                \
  if (FORE HURL (v3, float, 32, , _array))                                     \
    YELP (GUTS)                                                                \
  if (FORE HURL (v3, float, 64, , _array))                                     \
    YELP (GUTS)                                                                \
                                                                               \
  if (FORE HURL (v4, int, 8, , _array))                                        \
    YELP (GUTS)                                                                \
  if (FORE HURL (v4, unt, 8, , _array))                                        \
    YELP (GUTS)                                                                \
  if (FORE HURL (v4, int, 16, , _array))                                       \
    YELP (GUTS)                                                                \
  if (FORE HURL (v4, unt, 16, , _array))                                       \
    YELP (GUTS)                                                                \
  if (FORE HURL (v4, int, 32, , _array))                                       \
    YELP (GUTS)                                                                \
  if (FORE HURL (v4, unt, 32, , _array))                                       \
    YELP (GUTS)                                                                \
  if (FORE HURL (v4, int, 64, , _array))                                       \
    YELP (GUTS)                                                                \
  if (FORE HURL (v4, unt, 64, , _array))                                       \
    YELP (GUTS)                                                                \
  if (FORE HURL (v4, float, 32, , _array))                                     \
    YELP (GUTS)                                                                \
  if (FORE HURL (v4, float, 64, , _array))                                     \
    YELP (GUTS)                                                                \
                                                                               \
  if (FORE HURL (, int, 8, c, _array))                                         \
    YELP (GUTS)                                                                \
  if (FORE HURL (, unt, 8, c, _array))                                         \
    YELP (GUTS)                                                                \
  if (FORE HURL (, int, 16, c, _array))                                        \
    YELP (GUTS)                                                                \
  if (FORE HURL (, unt, 16, c, _array))                                        \
    YELP (GUTS)                                                                \
  if (FORE HURL (, int, 32, c, _array))                                        \
    YELP (GUTS)                                                                \
  if (FORE HURL (, unt, 32, c, _array))                                        \
    YELP (GUTS)                                                                \
  if (FORE HURL (, int, 64, c, _array))                                        \
    YELP (GUTS)                                                                \
  if (FORE HURL (, unt, 64, c, _array))                                        \
    YELP (GUTS)                                                                \
  if (FORE HURL (, float, 32, c, _array))                                      \
    YELP (GUTS)                                                                \
  if (FORE HURL (, float, 64, c, _array))                                      \
    YELP (GUTS)                                                                \
                                                                               \
  if (FORE HURL (v2, int, 8, c, _array))                                       \
    YELP (GUTS)                                                                \
  if (FORE HURL (v2, unt, 8, c, _array))                                       \
    YELP (GUTS)                                                                \
  if (FORE HURL (v2, int, 16, c, _array))                                      \
    YELP (GUTS)                                                                \
  if (FORE HURL (v2, unt, 16, c, _array))                                      \
    YELP (GUTS)                                                                \
  if (FORE HURL (v2, int, 32, c, _array))                                      \
    YELP (GUTS)                                                                \
  if (FORE HURL (v2, unt, 32, c, _array))                                      \
    YELP (GUTS)                                                                \
  if (FORE HURL (v2, int, 64, c, _array))                                      \
    YELP (GUTS)                                                                \
  if (FORE HURL (v2, unt, 64, c, _array))                                      \
    YELP (GUTS)                                                                \
  if (FORE HURL (v2, float, 32, c, _array))                                    \
    YELP (GUTS)                                                                \
  if (FORE HURL (v2, float, 64, c, _array))                                    \
    YELP (GUTS)                                                                \
                                                                               \
  if (FORE HURL (v3, int, 8, c, _array))                                       \
    YELP (GUTS)                                                                \
  if (FORE HURL (v3, unt, 8, c, _array))                                       \
    YELP (GUTS)                                                                \
  if (FORE HURL (v3, int, 16, c, _array))                                      \
    YELP (GUTS)                                                                \
  if (FORE HURL (v3, unt, 16, c, _array))                                      \
    YELP (GUTS)                                                                \
  if (FORE HURL (v3, int, 32, c, _array))                                      \
    YELP (GUTS)                                                                \
  if (FORE HURL (v3, unt, 32, c, _array))                                      \
    YELP (GUTS)                                                                \
  if (FORE HURL (v3, int, 64, c, _array))                                      \
    YELP (GUTS)                                                                \
  if (FORE HURL (v3, unt, 64, c, _array))                                      \
    YELP (GUTS)                                                                \
  if (FORE HURL (v3, float, 32, c, _array))                                    \
    YELP (GUTS)                                                                \
  if (FORE HURL (v3, float, 64, c, _array))                                    \
    YELP (GUTS)                                                                \
                                                                               \
  if (FORE HURL (v4, int, 8, c, _array))                                       \
    YELP (GUTS)                                                                \
  if (FORE HURL (v4, unt, 8, c, _array))                                       \
    YELP (GUTS)                                                                \
  if (FORE HURL (v4, int, 16, c, _array))                                      \
    YELP (GUTS)                                                                \
  if (FORE HURL (v4, unt, 16, c, _array))                                      \
    YELP (GUTS)                                                                \
  if (FORE HURL (v4, int, 32, c, _array))                                      \
    YELP (GUTS)                                                                \
  if (FORE HURL (v4, unt, 32, c, _array))                                      \
    YELP (GUTS)                                                                \
  if (FORE HURL (v4, int, 64, c, _array))                                      \
    YELP (GUTS)                                                                \
  if (FORE HURL (v4, unt, 64, c, _array))                                      \
    YELP (GUTS)                                                                \
  if (FORE HURL (v4, float, 32, c, _array))                                    \
    YELP (GUTS)                                                                \
  if (FORE HURL (v4, float, 64, c, _array))                                    \
  YELP (GUTS)


static int numeric_cataract (slaw s)
{
  int t = 0;

  NUMBLE ();

  return 0;
}



int main (int ac, char **av)
{
  fprintf (stderr, "roughing up slaw-nil...\n");
  slaw s0 = slaw_nil ();
  OBSERT (NULL != s0);
  OBSERT (true == slaw_is_nil (s0));
  OBSERT (NULL == slaw_string_emit (s0));
  OBSERT (NULL == slaw_cons_emit_car (s0));
  OBSERT (NULL == slaw_cons_emit_cdr (s0));
  OBSERT (0 > slaw_list_count (s0));
  OBSERT (NULL == slaw_list_emit_first (s0));
  OBSERT (NULL == slaw_list_emit_nth (s0, 1));
  OBSERT (!slaw_is_protein (s0));
  OBSERT (0 == numeric_cataract (s0));
  OBSERT (slawx_equal (s0, s0));

  fprintf (stderr, "picking on slaw-string...\n");
  slaw s1 = slaw_string ("borple!");
  OBSERT (NULL != s1);
  OBSERT (0 == slaw_is_nil (s1));
  OBSERT (NULL != slaw_string_emit (s1));
  OBSERT (0 == strcmp ("borple!", slaw_string_emit (s1)));
  OBSERT (0 != strcmp ("berple!", slaw_string_emit (s1)));
  OBSERT (0 != strcmp ("borple! ", slaw_string_emit (s1)));
  OBSERT (0 != strcmp ("borple", slaw_string_emit (s1)));
  OBSERT (0 != strcmp ("", slaw_string_emit (s1)));
  OBSERT (1 == slawx_equal_lc (s1, "borple!"));
  OBSERT (1 != slawx_equal_lc (s1, "tainted cheese"));
  OBSERT (NULL == slaw_cons_emit_car (s1));
  OBSERT (NULL == slaw_cons_emit_cdr (s1));
  OBSERT (0 > slaw_list_count (s1));
  OBSERT (NULL == slaw_list_emit_first (s1));
  OBSERT (NULL == slaw_list_emit_nth (s1, 1));
  OBSERT (!slaw_is_protein (s1));
  OBSERT (0 == numeric_cataract (s1));
  OBSERT (!slawx_equal (s0, s1));
  OBSERT (slawx_equal (s1, s1));

  slaw s2 = slaw_dup (s1);
  OBSERT (s1 != s2);
  OBSERT (slawx_equal (s1, s2));

  slaw s3 = slaw_string ("jeezy");

  fprintf (stderr, "rolling slaw-cons for loose change...\n");
  slaw s4 = slaw_cons (s2, s3);
  OBSERT (NULL != s4);
  OBSERT (0 == slaw_is_nil (s4));
  OBSERT (NULL == slaw_string_emit (s4));
  OBSERT (NULL != slaw_cons_emit_car (s4));
  OBSERT (NULL != slaw_cons_emit_cdr (s4));
  OBSERT (slawx_equal (s2, slaw_cons_emit_car (s4)));
  OBSERT (!slawx_equal (s2, slaw_cons_emit_cdr (s4)));
  OBSERT (!slawx_equal (s3, slaw_cons_emit_car (s4)));
  OBSERT (slawx_equal (s3, slaw_cons_emit_cdr (s4)));
  OBSERT (0 > slaw_list_count (s4));
  OBSERT (NULL == slaw_list_emit_first (s4));
  OBSERT (NULL == slaw_list_emit_nth (s4, 1));
  OBSERT (!slaw_is_protein (s4));
  OBSERT (0 == numeric_cataract (s4));
  OBSERT (!slawx_equal (s3, s4));
  OBSERT (slawx_equal (s4, s4));

  fprintf (stderr, "spanking slaw-list mightily...\n");
  slaw s5 =
    slaw_list_inline_c ("shadrach", "meshach", "abednego", "borple!", NULL);
  OBSERT (NULL != s5);
  OBSERT (0 == slaw_is_nil (s5));
  OBSERT (NULL == slaw_string_emit (s5));
  OBSERT (NULL == slaw_cons_emit_car (s5));
  OBSERT (NULL == slaw_cons_emit_cdr (s5));
  OBSERT (4 == slaw_list_count (s5));
  OBSERT (NULL != slaw_list_emit_first (s5));
  OBSERT (NULL != slaw_list_emit_nth (s5, 0));
  OBSERT (NULL != slaw_list_emit_nth (s5, 1));
  OBSERT (NULL != slaw_list_emit_nth (s5, 2));
  OBSERT (NULL != slaw_list_emit_nth (s5, 3));
  OBSERT (NULL == slaw_list_emit_nth (s5, 4));
  OBSERT (slawx_equal (s1, slaw_list_emit_nth (s5, 3)));
  OBSERT (slawx_equal (slaw_list_emit_nth (s5, 1), slaw_list_emit_nth (s5, 1)));
  OBSERT (
    !slawx_equal (slaw_list_emit_nth (s5, 1), slaw_list_emit_nth (s5, 2)));
  OBSERT (slawx_equal_lc (slaw_list_emit_nth (s5, 0), "shadrach"));
  OBSERT (!slaw_is_protein (s5));
  OBSERT (0 == numeric_cataract (s5));
  OBSERT (!slawx_equal (s3, s5));
  OBSERT (slawx_equal (s5, s5));

  slabu *sb = slabu_new ();
  OB_DIE_ON_ERROR (slabu_list_add_x (sb, slaw_dup (s3)));
  OB_DIE_ON_ERROR (slabu_list_add_x (sb, slaw_dup (s0)));
  OB_DIE_ON_ERROR (slabu_list_add_x (sb, slaw_dup (s2)));
  fprintf (stderr, "playing chin music on slabu-based slaw-list...\n");
  slaw s6 = slaw_list_f (sb);
  OBSERT (NULL != s6);
  OBSERT (0 == slaw_is_nil (s6));
  OBSERT (NULL == slaw_string_emit (s6));
  OBSERT (NULL == slaw_cons_emit_car (s6));
  OBSERT (NULL == slaw_cons_emit_cdr (s6));
  OBSERT (3 == slaw_list_count (s6));
  OBSERT (NULL != slaw_list_emit_first (s6));
  OBSERT (NULL != slaw_list_emit_nth (s6, 0));
  OBSERT (NULL != slaw_list_emit_nth (s6, 1));
  OBSERT (NULL != slaw_list_emit_nth (s6, 2));
  OBSERT (NULL == slaw_list_emit_nth (s6, 3));
  OBSERT (NULL == slaw_list_emit_nth (s6, 4));
  OBSERT (slawx_equal (s3, slaw_list_emit_nth (s6, 0)));
  OBSERT (slawx_equal (s0, slaw_list_emit_nth (s6, 1)));
  OBSERT (slaw_is_nil (slaw_list_emit_nth (s6, 1)));
  OBSERT (slawx_equal (s2, slaw_list_emit_nth (s6, 2)));
  OBSERT (!slawx_equal_lc (slaw_list_emit_nth (s6, 0), "shadrach"));
  OBSERT (!slaw_is_protein (s6));
  OBSERT (0 == numeric_cataract (s6));
  OBSERT (!slawx_equal (s5, s6));
  OBSERT (slawx_equal (s6, s6));


  protein p = protein_from_ff (
    slaw_list_inline_c ("scribblers", "major", NULL),
    slaw_map_inline_cc ("crank", "William Gaddis", "philosopher",
                        "William Gass", "savant", "David Foster Wallace",
                        "mindman", "Nicholas Mosley", "fabulist",
                        "Harry Mathews", "gnarled", "Samuel Beckett", NULL));
  OBSERT (NULL != p);
  fprintf (stderr, "molesting protein sans mercy...\n");
  slaw s7 = slaw_dup (p);
  OBSERT (NULL != s7);
  OBSERT (0 == slaw_is_nil (s7));
  OBSERT (NULL == slaw_string_emit (s7));
  OBSERT (NULL == slaw_cons_emit_car (s7));
  OBSERT (NULL == slaw_cons_emit_cdr (s7));
  OBSERT (0 > slaw_list_count (s7));
  OBSERT (slaw_is_protein (s7));
  OBSERT (proteins_equal (p, s7));
  OBSERT (0 == numeric_cataract (s7));
  OBSERT (!slawx_equal (s6, s7));
  OBSERT (slawx_equal (s7, s7));
  //slaw_spew_overview_to_stderr (s7);

  fprintf (stderr, "... flying colors, is the thing.\n");

  // valgrind happiness (puzzled why this didn't show up before...)
  slaw_free (s0);
  slaw_free (s1);
  slaw_free (s2);
  slaw_free (s3);
  slaw_free (s4);
  slaw_free (s5);
  slaw_free (s6);
  slaw_free (s7);
  protein_free (p);

  return 0;
}
