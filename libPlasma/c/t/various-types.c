
/* (c)  oblong industries */

// Tests putting a variety of different types into maps and lists.

#include "libLoam/c/ob-rand.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-vers.h"
#include "libLoam/c/ob-sys.h"
#include "libPlasma/c/slaw.h"
#include "libPlasma/c/protein.h"
#include "libPlasma/c/pool.h"
#include "libPlasma/c/slaw-ordering.h"
#include "libPlasma/c/slaw-walk.h"

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>

#include <errno.h>

#define AS_LIST 0
#define AS_LIST_OF_CONS 1
#define AS_MAP 2
#define VARIETY 3

// This is just to make valgrind happy!
static slaw to_be_freed[50000];
static int free_counter = 0;

static slaw f (slaw s)
{
  to_be_freed[free_counter++] = s;
  return s;
}

static void add_something (slabu *sb, int what, int how)
{
  slaw s;
  slaw x[5];
  byte b;
  v3float32c v;
  slabu *foo;
  int i;
  m2int32 m2;
  m5float64 m5;

  switch (what)
    {
      case 0:
        s = slaw_nil ();
        break;
      case 1:
        s = slaw_string ("aluminum");
        break;
      case 2:
        s = slaw_cons_cf ("pid", slaw_int64 (getpid ()));
        break;
      case 3:
        memset (&m2, 0, sizeof (m2));
        m2.u0 = -1000000;
        m2.u1[0] = -768;
        m2.u1[1] = 1993;
        m2.u2 = 12345678;
        s = slaw_m2int32 (m2);
        break;
      case 4:
        x[0] = slaw_string (strerror (EINVAL));
        x[1] = slaw_string (strerror (ERANGE));
        x[2] = slaw_string (strerror (EEXIST));
        x[3] = slaw_string (strerror (ENOENT));
        x[4] = slaw_string (strerror (ENOSPC));
        s = slaw_map_inline_ff (
          slaw_string ("EINVAL"), x[0], slaw_string ("ERANGE"), x[1],
          slaw_string ("EEXIST"), x[2], slaw_string ("ENOENT"), x[3],
          slaw_string ("ENOSPC"), x[4], NULL);
        break;
      case 5:
        s = protein_from_ff (slaw_string ("descrips is not a list!"),
                             slaw_string ("ingests is not a list!"));
        break;
      case 6:
        s = slaw_boolean (0);
        break;
      case 7:
        s = slaw_boolean (1);
        break;
      case 8:
        s = slaw_int8 (5);
        break;
      case 9:
        s = slaw_unt8 (5);
        break;
      case 10:
        s = slaw_int16 (5);
        break;
      case 11:
        s = slaw_unt16 (5);
        break;
      case 12:
        s = slaw_int32 (5);
        break;
      case 13:
        s = slaw_unt32 (5);
        break;
      case 14:
        s = slaw_int64 (5);
        break;
      case 15:
        s = slaw_unt64 (5);
        break;
      case 16:
        s = slaw_float32 (5);
        break;
      case 17:
        s = slaw_float64 (5);
        break;
      case 18:
        s = slaw_int8 (6);
        break;
      case 19:
        s = slaw_unt8 (6);
        break;
      case 20:
        s = slaw_int16 (6);
        break;
      case 21:
        s = slaw_unt16 (6);
        break;
      case 22:
        s = slaw_int32 (6);
        break;
      case 23:
        s = slaw_unt32 (6);
        break;
      case 24:
        s = slaw_int64 (6);
        break;
      case 25:
        s = slaw_unt64 (6);
        break;
      case 26:
        s = slaw_float32 (6);
        break;
      case 27:
        s = slaw_float64 (6);
        break;
      case 28:
        s =
          slaw_list_inline_c ("another", "list", "of", "five", "things", NULL);
        break;
      case 29:
        memset (&m5, 0, sizeof (m5));
        m5.u0 = 100;
        m5.u1[0] = 50;
        m5.u1[4] = 25;
        m5.u2[0] = 12.5;
        m5.u2[9] = 6.25;
        m5.u3[0] = 1;
        m5.u3[9] = 0.5;
        m5.u4[0] = 0.25;
        m5.u4[4] = 0.125;
        m5.u5 = -1000000;
        s = slaw_m5float64 (m5);
        break;
      case 30:
        s = slaw_int32_array_filled (5, 5);
        break;
      case 31:
        s = slaw_int32_array_filled (5, 6);
        break;
      case 32:
        s = slaw_int32_array_filled (6, 5);
        break;
      case 33:
        s = slaw_int32_array_filled (6, 6);
        break;
      case 34:
        s = slaw_float64_array_filled (1, 3.14159);
        break;
      case 35:
        s = protein_from_ff (
          NULL, slaw_list_inline_c ("descrips", "are", "NULL", NULL));
        break;
      case 36:
        s = protein_from_ff (
          slaw_list_inline_c ("ingests", "are", "NULL", NULL), NULL);
        break;
      case 37:
        b = 10;
        s = protein_from_llr (NULL, NULL, &b, 1);
        break;
      case 38:
        b = 11;
        s = protein_from_llr (NULL, NULL, &b, 1);
        break;
      case 39:
        v.x.re = 4.5;
        v.x.im = -1.25;
        v.y.re = 12;
        v.y.im = 10000;
        v.z.re = 0;
        v.z.im = -100.75;
        s = slaw_v3float32c (v);
        break;
      case 40:
        foo = slabu_new ();
        for (i = 0; i < 40; i++)
          add_something (foo, i, AS_LIST);
        s = slaw_list_f (foo);
        break;
      case 41:
        foo = slabu_new ();
        for (i = 39; i >= 0; i--)
          add_something (foo, i, AS_LIST);
        s = slaw_list_f (foo);
        break;
      case 42:
        foo = slabu_new ();
        for (i = 0; i < 42; i++)
          add_something (foo, i, AS_LIST);
        s =
          protein_from_ff (slaw_list_inline_c ("foo", NULL), slaw_list_f (foo));
        break;
      case 43:
        foo = slabu_new ();
        for (i = 0; i < 42; i++)
          add_something (foo, i, AS_LIST);
        s =
          protein_from_ff (slaw_list_inline_c ("bar", NULL), slaw_list_f (foo));
        break;
      case 44:
        foo = slabu_new ();
        for (i = 0; i < 42; i++)
          add_something (foo, i, AS_LIST);
        s = protein_from_ff (slaw_list_f (foo),
                             slaw_map_inline_cc ("x", "foo", NULL));
        break;
      case 45:
        foo = slabu_new ();
        for (i = 0; i < 42; i++)
          add_something (foo, i, AS_LIST);
        s = protein_from_ff (slaw_list_f (foo),
                             slaw_map_inline_cc ("x", "bar", NULL));
        break;
      default:
        OB_FATAL_ERROR_CODE (0x20317000, "what is %d\n", what);
        s = NULL; /* not reached; make compiler happy */
    }

  if (how == VARIETY)
    how = (ob_rand_int32 (0, 2) ? AS_MAP : AS_LIST_OF_CONS);

  if (how == AS_LIST_OF_CONS)
    {
      s = slaw_cons_ff (s, slaw_nil ());
      how = AS_LIST;
    }

  switch (how)
    {
      case AS_LIST:
        if (ob_rand_int32 (0, 2))
          OB_DIE_ON_ERROR (slabu_list_add_z (sb, f (s)));
        else
          OB_DIE_ON_ERROR (slabu_list_add_x (sb, s));
        break;
      case AS_MAP:
        OB_DIE_ON_ERROR (slabu_map_put_ff (sb, s, slaw_nil ()));
        break;
      default:
        OB_FATAL_ERROR_CODE (0x20317001, "don't know how\n");
    }
}

int main (int argc, char **argv)
{
  OB_DIE_ON_ERROR (OB_CHECK_ABI ());

  ob_rand_seed_int32 (24601);

  const int num_things = 46;
  int i, j;
  slabu *sb;
  slaw s1, s2, s3;
  slaw_fabricator *sf;
  ob_retort err;

  sb = slabu_new ();
  for (i = 0; i < num_things; i++)
    add_something (sb, i, AS_MAP);
  s1 = slaw_map_f (sb);

  if (slaw_list_count (s1) != num_things || slaw_map_count (s1) != num_things)
    {
      fprintf (stderr, "====================\n");
      slaw_spew_overview_to_stderr (s1);
      fprintf (stderr, "\n====================\n");
      OB_FATAL_ERROR_CODE (
        0x20317002, "expected %" OB_FMT_64 "d things and got %" OB_FMT_64 "d\n",
        (int64) num_things, (int64) slaw_list_count (s1));
    }

  sb = slabu_new ();
  for (i = num_things; i > 0; i--)
    add_something (sb, i - 1, AS_MAP);
  s3 = slaw_map_f (sb);

  sb = slabu_new ();
  for (i = 0; i < num_things; i++)
    add_something (sb, i, AS_LIST_OF_CONS);
  s2 = slaw_map_f (sb);

  if (!slawx_equal (s1, s2))
    {
      const char *s1_file = "scratch/s1.spew";
      const char *s2_file = "scratch/s2.spew";
      FILE *g = fopen (s1_file, "w");
      if (!g)
        OB_FATAL_ERROR_CODE (0x20317003, "couldn't open %s\n", s1_file);
      slaw_spew_overview (s1, g, NULL);
      fclose (g);
      g = fopen (s2_file, "w");
      if (!g)
        OB_FATAL_ERROR_CODE (0x20317004, "couldn't open %s\n", s2_file);
      slaw_spew_overview (s2, g, NULL);
      fclose (g);
      OB_FATAL_ERROR_CODE (0x20317005, "not equal: see files %s %s\n", s1_file,
                           s2_file);
    }
  if (slaw_semantic_compare (s1, s2) != 0)
    OB_FATAL_ERROR_CODE (0x20317006, "not equal\n");
  slaw_free (s2);

  sb = slabu_new ();
  for (i = num_things; i > 0; i--)
    add_something (sb, i - 1, AS_LIST_OF_CONS);
  s2 = slaw_map_f (sb);

  if (!slawx_equal (s3, s2))
    OB_FATAL_ERROR_CODE (0x20317007, "not equal\n");
  if (slaw_semantic_compare (s3, s2) != 0)
    OB_FATAL_ERROR_CODE (0x20317008, "not equal\n");
  slaw_free (s2);

  sb = slabu_new ();
  for (i = 0; i < num_things; i++)
    add_something (sb, i, VARIETY);
  s2 = slaw_map_f (sb);

  if (!slawx_equal (s1, s2))
    OB_FATAL_ERROR_CODE (0x20317009, "not equal\n");
  if (slaw_semantic_compare (s1, s2) != 0)
    OB_FATAL_ERROR_CODE (0x2031700a, "not equal\n");
  slaw_free (s2);

  sb = slabu_new ();
  for (i = num_things; i > 0; i--)
    add_something (sb, i - 1, VARIETY);
  s2 = slaw_map_f (sb);

  if (!slawx_equal (s3, s2))
    OB_FATAL_ERROR_CODE (0x2031700b, "not equal\n");
  if (slaw_semantic_compare (s3, s2) != 0)
    OB_FATAL_ERROR_CODE (0x2031700c, "not equal\n");
  slaw_free (s2);

  sb = slabu_new ();
  for (j = 0; j < 2; j++)
    for (i = 0; i < num_things; i++)
      add_something (sb, i, VARIETY);
  s2 = slaw_map (sb);

  if (!slawx_equal (s1, s2))
    {
      FILE *g = fopen ("scratch/s1", "w");
      slaw_spew_overview (s1, g, NULL);
      fclose (g);
      g = fopen ("scratch/s2", "w");
      slaw_spew_overview (s2, g, NULL);
      fclose (g);
      OB_FATAL_ERROR_CODE (0x2031700d, "not equal\n");
    }
  if (slaw_semantic_compare (s1, s2) != 0)
    OB_FATAL_ERROR_CODE (0x2031700e, "not equal\n");
  slaw_free (s2);

  sf = slaw_fabricator_new ();
  if ((err = slaw_walk (sf, &slaw_fabrication_handler, s1)) != OB_OK)
    OB_FATAL_ERROR_CODE (0x2031700f, "slaw_walk returned an error: %s\n",
                         ob_error_string (err));
  if (!slawx_equal (s1, sf->result))
    OB_FATAL_ERROR_CODE (0x20317010, "not equal\n");
  if (slaw_semantic_compare (s1, sf->result) != 0)
    OB_FATAL_ERROR_CODE (0x20317011, "not equal\n");
  slaw_fabricator_free (sf);

  slaw_free (s1);
  slaw_free (s3);
  slabu_free (sb);

  // free stuff to please valgrind
  for (i = 0; i < free_counter; i++)
    slaw_free (to_be_freed[i]);

  return EXIT_SUCCESS;
}
