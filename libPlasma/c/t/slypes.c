
/* (c)  oblong industries */

#include <libPlasma/c/protein.h>
#include <libPlasma/c/slaw.h>
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-vers.h"


static int gantlet_secular (slaw s, int pnil, int pstring, int pcons, int plist,
                            int pnum, int pprot)
{
  int misses = 0;

  misses += ((slaw_is_nil (s) ? 1 : 0) == (pnil ? 1 : 0)) ? 0 : 1;
  misses += ((slaw_is_string (s) ? 1 : 0) == (pstring ? 1 : 0)) ? 0 : 1;
  misses += ((slaw_is_cons (s) ? 1 : 0) == (pcons ? 1 : 0)) ? 0 : 1;
  misses += ((slaw_is_list (s) ? 1 : 0) == (plist ? 1 : 0)) ? 0 : 1;
  misses += ((slaw_is_numeric (s) ? 1 : 0) == (pnum ? 1 : 0)) ? 0 : 1;
  misses += ((slaw_is_protein (s) ? 1 : 0) == (pprot ? 1 : 0)) ? 0 : 1;

  return misses;
}


static int gantlet_ecclesiastical (slaw s, int s_8bit, int s_16bit, int s_32bit,
                                   int s_64bit, int s_integer, int s_float,
                                   int s_signed, int s_unsigned, int s_complex,
                                   int s_real, int s_vector, int s_scalar,
                                   int s_2vector, int s_3vector, int s_4vector,
                                   int s_array, int s_singleton)
{
  int misses = 0;

  misses += ((slaw_is_numeric_8 (s) ? 1 : 0) == (s_8bit ? 1 : 0)) ? 0 : 1;
  misses += ((slaw_is_numeric_16 (s) ? 1 : 0) == (s_16bit ? 1 : 0)) ? 0 : 1;
  misses += ((slaw_is_numeric_32 (s) ? 1 : 0) == (s_32bit ? 1 : 0)) ? 0 : 1;
  misses += ((slaw_is_numeric_64 (s) ? 1 : 0) == (s_64bit ? 1 : 0)) ? 0 : 1;
  misses += (((slaw_is_numeric_int (s) || slaw_is_numeric_unt (s)) ? 1 : 0)
             == (s_integer ? 1 : 0))
              ? 0
              : 1;
  misses += (((slaw_is_numeric (s) && !slaw_is_numeric_complex (s)) ? 1 : 0)
             == (s_real ? 1 : 0))
              ? 0
              : 1;
  misses += (((slaw_is_numeric_int (s) || slaw_is_numeric_float (s)) ? 1 : 0)
             == (s_signed ? 1 : 0))
              ? 0
              : 1;
  misses += ((slaw_is_numeric_unt (s) ? 1 : 0) == (s_unsigned ? 1 : 0)) ? 0 : 1;
  misses +=
    ((slaw_is_numeric_complex (s) ? 1 : 0) == (s_complex ? 1 : 0)) ? 0 : 1;
  misses +=
    ((slaw_is_numeric_vector (s) ? 1 : 0) == (s_vector ? 1 : 0)) ? 0 : 1;
  misses += (((slaw_is_numeric (s) && !slaw_is_numeric_vector (s)) ? 1 : 0)
             == (s_scalar ? 1 : 0))
              ? 0
              : 1;
  misses +=
    (((2 == slaw_numeric_vector_dimension (s)) ? 1 : 0) == (s_2vector ? 1 : 0))
      ? 0
      : 1;
  misses +=
    (((3 == slaw_numeric_vector_dimension (s)) ? 1 : 0) == (s_3vector ? 1 : 0))
      ? 0
      : 1;
  misses +=
    (((4 == slaw_numeric_vector_dimension (s)) ? 1 : 0) == (s_4vector ? 1 : 0))
      ? 0
      : 1;
  misses += ((slaw_is_numeric_array (s) ? 1 : 0) == (s_array ? 1 : 0)) ? 0 : 1;
  misses += (((slaw_is_numeric (s) && !slaw_is_numeric_array (s)) ? 1 : 0)
             == (s_singleton ? 1 : 0))
              ? 0
              : 1;

  return misses;
}



int main (int ac, char **av)
{
  OB_DIE_ON_ERROR (OB_CHECK_ABI ());

  slaw snil = slaw_nil ();
  slaw sstr = slaw_string ("bleeb!");
  slaw scns = slaw_cons (sstr, snil);
  slaw snum = slaw_float64 (34.5);
  slaw slst = slaw_list_inline (snum, scns, snil, sstr, NULL);
  protein p =
    protein_from_ff (slaw_list_inline_c ("owah", "tagu", "siam", NULL),
                     slaw_map_inline_cc ("dum", "dee", "bra", "ket", NULL));
  slaw spro = slaw_dup (p);

  OBSERT (0 == gantlet_secular (snil, 1, 0, 0, 0, 0, 0));
  OBSERT (0 == gantlet_secular (sstr, 0, 1, 0, 0, 0, 0));
  OBSERT (0 == gantlet_secular (scns, 0, 0, 1, 0, 0, 0));
  OBSERT (0 == gantlet_secular (slst, 0, 0, 0, 1, 0, 0));
  OBSERT (0 == gantlet_secular (snum, 0, 0, 0, 0, 1, 0));
  OBSERT (0 == gantlet_secular (spro, 0, 0, 0, 0, 0, 1));

  slaw sn, sna;

  slaw snull = NULL;
  OBSERT (0 == gantlet_secular (snull, 0, 0, 0, 0, 0, 0));
  OBSERT (0
          == gantlet_ecclesiastical (snull, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                     0, 0, 0, 0, 0));

#define NUMPY(typ, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o)                \
  typ n_##typ;                                                                 \
  memset (&n_##typ, 0, sizeof (typ));                                          \
  sn = slaw_##typ (n_##typ);                                                   \
  sna = slaw_##typ##_array_raw ((unt64) 39, NULL);                             \
  OBSERT (0 == gantlet_secular (sn, 0, 0, 0, 0, 1, 0));                        \
  OBSERT (0 == gantlet_secular (sna, 0, 0, 0, 0, 1, 0));                       \
  OBSERT (slaw_is_##typ (sn));                                                 \
  OBSERT (slaw_is_##typ##_array (sna));                                        \
  OBSERT (!slaw_is_##typ (snull));                                             \
  OBSERT (0                                                                    \
          == gantlet_ecclesiastical (sn, a, b, c, d, e, f, g, h, i, j, k, l,   \
                                     m, n, o, 0, 1));                          \
  OBSERT (0                                                                    \
          == gantlet_ecclesiastical (sna, a, b, c, d, e, f, g, h, i, j, k, l,  \
                                     m, n, o, 1, 0));                          \
  Free_Slaw (sn);                                                              \
  Free_Slaw (sna);

  NUMPY (int8, 1, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0);
  NUMPY (unt8, 1, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0);
  NUMPY (int16, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0);
  NUMPY (unt16, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0);
  NUMPY (int32, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0);
  NUMPY (unt32, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0);
  NUMPY (int64, 0, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0);
  NUMPY (unt64, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0);
  NUMPY (float32, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0, 0, 0);
  NUMPY (float64, 0, 0, 0, 1, 0, 1, 1, 0, 0, 1, 0, 1, 0, 0, 0);

  NUMPY (int8c, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0);
  NUMPY (unt8c, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0);
  NUMPY (int16c, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0);
  NUMPY (unt16c, 0, 1, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0);
  NUMPY (int32c, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0);
  NUMPY (unt32c, 0, 0, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0);
  NUMPY (int64c, 0, 0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0);
  NUMPY (unt64c, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0);
  NUMPY (float32c, 0, 0, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0);
  NUMPY (float64c, 0, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0);

  NUMPY (v2int8, 1, 0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 0, 1, 0, 0);
  NUMPY (v2unt8, 1, 0, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0);
  NUMPY (v2int16, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 1, 0, 1, 0, 0);
  NUMPY (v2unt16, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0);
  NUMPY (v2int32, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1, 0, 1, 0, 0);
  NUMPY (v2unt32, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0);
  NUMPY (v2int64, 0, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 1, 0, 0);
  NUMPY (v2unt64, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0);
  NUMPY (v2float32, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0);
  NUMPY (v2float64, 0, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0);

  NUMPY (v3int8, 1, 0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0);
  NUMPY (v3unt8, 1, 0, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 0);
  NUMPY (v3int16, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0);
  NUMPY (v3unt16, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 0);
  NUMPY (v3int32, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0);
  NUMPY (v3unt32, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 0);
  NUMPY (v3int64, 0, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0);
  NUMPY (v3unt64, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 0);
  NUMPY (v3float32, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 0);
  NUMPY (v3float64, 0, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 0);

  NUMPY (v4int8, 1, 0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1);
  NUMPY (v4unt8, 1, 0, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 0, 1);
  NUMPY (v4int16, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1);
  NUMPY (v4unt16, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 0, 1);
  NUMPY (v4int32, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1);
  NUMPY (v4unt32, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 0, 1);
  NUMPY (v4int64, 0, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1);
  NUMPY (v4unt64, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 0, 1);
  NUMPY (v4float32, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1);
  NUMPY (v4float64, 0, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1);

  NUMPY (v2int8c, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0);
  NUMPY (v2unt8c, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 0, 1, 0, 0);
  NUMPY (v2int16c, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0);
  NUMPY (v2unt16c, 0, 1, 0, 0, 1, 0, 0, 1, 1, 0, 1, 0, 1, 0, 0);
  NUMPY (v2int32c, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0);
  NUMPY (v2unt32c, 0, 0, 1, 0, 1, 0, 0, 1, 1, 0, 1, 0, 1, 0, 0);
  NUMPY (v2int64c, 0, 0, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0);
  NUMPY (v2unt64c, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1, 0, 0);
  NUMPY (v2float32c, 0, 0, 1, 0, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0);
  NUMPY (v2float64c, 0, 0, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0);

  NUMPY (v3int8c, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0);
  NUMPY (v3unt8c, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0);
  NUMPY (v3int16c, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0);
  NUMPY (v3unt16c, 0, 1, 0, 0, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0);
  NUMPY (v3int32c, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0);
  NUMPY (v3unt32c, 0, 0, 1, 0, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0);
  NUMPY (v3int64c, 0, 0, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0);
  NUMPY (v3unt64c, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0);
  NUMPY (v3float32c, 0, 0, 1, 0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 1, 0);
  NUMPY (v3float64c, 0, 0, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 0, 1, 0);

  NUMPY (v4int8c, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1);
  NUMPY (v4unt8c, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1);
  NUMPY (v4int16c, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1);
  NUMPY (v4unt16c, 0, 1, 0, 0, 1, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1);
  NUMPY (v4int32c, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1);
  NUMPY (v4unt32c, 0, 0, 1, 0, 1, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1);
  NUMPY (v4int64c, 0, 0, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1);
  NUMPY (v4unt64c, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1);
  NUMPY (v4float32c, 0, 0, 1, 0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 0, 1);
  NUMPY (v4float64c, 0, 0, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 0, 0, 1);

  // free stuff to please valgrind
  slaw_free (snil);
  slaw_free (sstr);
  slaw_free (scns);
  slaw_free (snum);
  slaw_free (slst);
  protein_free (p);
  slaw_free (spro);

  return 0;
}
