
/* (c)  oblong industries */

// Test conversion to and from old slaw versions

#include "libLoam/c/ob-log.h"
#include "libPlasma/c/slaw-interop.h"
#include "libPlasma/c/slaw-io.h"
#include "libPlasma/c/pool_cmd.h"
#include "libPlasma/c/protein.h"

void do_check (bslaw expected)
{
  // slaw_spew_overview_to_stderr (expected);
  // fputc ('\n', stderr);
  slaw actual = slaw_dup (expected);
  OB_DIE_ON_ERROR (slaw_convert_to (&actual, 1, NULL));
  OB_DIE_ON_ERROR (slaw_convert_from (&actual, SLAW_ENDIAN_CURRENT, 1));
  if (!slawx_equal (expected, actual))
    {
      fprintf (stderr, "expected:\n");
      slaw_spew_overview_to_stderr (expected);
      fprintf (stderr, "\nactual:\n");
      slaw_spew_overview_to_stderr (actual);
      fputc ('\n', stderr);
      OB_FATAL_ERROR_CODE (0x20310000, "not equal!\n");
    }
  slaw_free (actual);
}

static slaw make_map (void)
{
  slaw m1 = slaw_map_inline_cc (
    "mortals", "foolish", "pallor", "cadaverous", "aura", "foreboding",
    "metamorphosis", "disquieting", "observation", "dismaying", NULL);

  slaw m2 = slaw_map_inline_cf (
    "no", slaw_list_inline_c ("windows", "doors", NULL), "challenge",
    slaw_string ("chilling"), "way", slaw_list_inline_c ("out", "my", NULL),
    "frighten", slaw_string ("prematurely"), "haunts", slaw_int16 (999), NULL);

  slaw m3 = slaw_map_inline_ff (slaw_list_inline_c ("room", "for", NULL),
                                slaw_unt32 (1000), slaw_string ("volunteers?"),
                                slaw_boolean (false), NULL);

  return slaw_maps_merge_f (m1, m2, m3, NULL);
}

#define CHECK(type)                                                            \
  type var_##type;                                                             \
  memset (&var_##type, 0, sizeof (var_##type));                                \
  s = slaw_##type (var_##type);                                                \
  do_check (s);                                                                \
  slaw_free (s);                                                               \
  s = slaw_##type##_array_filled (0, var_##type);                              \
  do_check (s);                                                                \
  slaw_free (s);                                                               \
  s = slaw_##type##_array_filled (1, var_##type);                              \
  do_check (s);                                                                \
  slaw_free (s);                                                               \
  s = slaw_##type##_array_filled (3, var_##type);                              \
  do_check (s);                                                                \
  slaw_free (s);

static int mainish (int argc, char **argv)
{
  slaw s;

  s = slaw_cons_cf ("size", slaw_int32 (1234));
  do_check (s);
  slaw_free (s);

  s = slaw_list_inline_c ("one", "two", "three", NULL);
  do_check (s);
  slaw_free (s);

  s = slaw_nil ();
  do_check (s);
  slaw_free (s);

  s = slaw_boolean (false);
  do_check (s);
  slaw_free (s);

  s = slaw_boolean (true);
  do_check (s);
  slaw_free (s);

  s = slaw_string ("Satan eats Cheez Whiz");
  do_check (s);
  slaw_free (s);

  s = slaw_list_f (slabu_new ());
  do_check (s);
  slaw_free (s);

  s = slaw_map_f (slabu_new ());
  do_check (s);
  slaw_free (s);

  s = make_map ();
  do_check (s);
  slaw_free (s);

  CHECK (int32);
  CHECK (unt32);
  CHECK (int64);
  CHECK (unt64);
  CHECK (float32);
  CHECK (float64);
  CHECK (int8);
  CHECK (unt8);
  CHECK (int16);
  CHECK (unt16);
  CHECK (int32c);
  CHECK (unt32c);
  CHECK (int64c);
  CHECK (unt64c);
  CHECK (float32c);
  CHECK (float64c);
  CHECK (int8c);
  CHECK (unt8c);
  CHECK (int16c);
  CHECK (unt16c);
  CHECK (v2int32);
  CHECK (v2unt32);
  CHECK (v2int64);
  CHECK (v2unt64);
  CHECK (v2float32);
  CHECK (v2float64);
  CHECK (v2int8);
  CHECK (v2unt8);
  CHECK (v2int16);
  CHECK (v2unt16);
  CHECK (v3int32);
  CHECK (v3unt32);
  CHECK (v3int64);
  CHECK (v3unt64);
  CHECK (v3float32);
  CHECK (v3float64);
  CHECK (v3int8);
  CHECK (v3unt8);
  CHECK (v3int16);
  CHECK (v3unt16);
  CHECK (v4int32);
  CHECK (v4unt32);
  CHECK (v4int64);
  CHECK (v4unt64);
  CHECK (v4float32);
  CHECK (v4float64);
  CHECK (v4int8);
  CHECK (v4unt8);
  CHECK (v4int16);
  CHECK (v4unt16);
  CHECK (v2int32c);
  CHECK (v2unt32c);
  CHECK (v2int64c);
  CHECK (v2unt64c);
  CHECK (v2float32c);
  CHECK (v2float64c);
  CHECK (v2int8c);
  CHECK (v2unt8c);
  CHECK (v2int16c);
  CHECK (v2unt16c);
  CHECK (v3int32c);
  CHECK (v3unt32c);
  CHECK (v3int64c);
  CHECK (v3unt64c);
  CHECK (v3float32c);
  CHECK (v3float64c);
  CHECK (v3int8c);
  CHECK (v3unt8c);
  CHECK (v3int16c);
  CHECK (v3unt16c);
  CHECK (v4int32c);
  CHECK (v4unt32c);
  CHECK (v4int64c);
  CHECK (v4unt64c);
  CHECK (v4float32c);
  CHECK (v4float64c);
  CHECK (v4int8c);
  CHECK (v4unt8c);
  CHECK (v4int16c);
  CHECK (v4unt16c);

  CHECK (m2int32);
  CHECK (m2unt32);
  CHECK (m2int64);
  CHECK (m2unt64);
  CHECK (m2float32);
  CHECK (m2float64);
  CHECK (m2int8);
  CHECK (m2unt8);
  CHECK (m2int16);
  CHECK (m2unt16);

  CHECK (m3int32);
  CHECK (m3unt32);
  CHECK (m3int64);
  CHECK (m3unt64);
  CHECK (m3float32);
  CHECK (m3float64);
  CHECK (m3int8);
  CHECK (m3unt8);
  CHECK (m3int16);
  CHECK (m3unt16);

  CHECK (m4int32);
  CHECK (m4unt32);
  CHECK (m4int64);
  CHECK (m4unt64);
  CHECK (m4float32);
  CHECK (m4float64);
  CHECK (m4int8);
  CHECK (m4unt8);
  CHECK (m4int16);
  CHECK (m4unt16);

  CHECK (m5int32);
  CHECK (m5unt32);
  CHECK (m5int64);
  CHECK (m5unt64);
  CHECK (m5float32);
  CHECK (m5float64);
  CHECK (m5int8);
  CHECK (m5unt8);
  CHECK (m5int16);
  CHECK (m5unt16);

  int i;
  for (i = 1; i < argc; i++)
    {
      OB_DIE_ON_ERROR (slaw_read_from_binary_file (argv[i], &s));
      do_check (s);
      slaw_free (s);
    }

  // check that rude data gets translated correctly
  byte data[40];

  for (i = 0; i < sizeof (data); i++)
    data[i] = i + 100;

  slaw descrips = slaw_list_f (slabu_new ());
  slaw ingests = slaw_map_inline_cc ("cold greeting", "have an ice day", NULL);

  for (i = sizeof (data) - 1; i >= 0; i--)
    {
      protein p = protein_from_llr (descrips, ingests, data, i);
      do_check (p);
      protein_free (p);
    }

  slaw_free (descrips);
  slaw_free (ingests);

  return EXIT_SUCCESS;
}

int main (int argc, char **argv)
{
  return check_for_leaked_file_descriptors_scoped (mainish, argc, argv);
}
