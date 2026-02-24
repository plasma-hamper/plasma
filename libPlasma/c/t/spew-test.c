
/* (c)  oblong industries */

#include <unicode/ucnv.h>
#include <unicode/uregex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-file.h"
#include "libLoam/c/ob-util.h"
#include "libPlasma/c/slaw-io.h"
#include "libPlasma/c/pool_cmd.h"

#define HUGE 4096

static void from_utf8 (UConverter *conv, const char *utf8, UChar *dest,
                       int32_t *destLen)
{
  UErrorCode err = U_ZERO_ERROR;
  int32_t result =
    ucnv_toUChars (conv, dest, *destLen, utf8, strlen (utf8), &err);
  if (U_FAILURE (err))
    {
      fprintf (stderr, "ucnv_toUChars: %s\n", u_errorName (err));
      exit (EXIT_FAILURE);
    }
  else if (result >= *destLen)
    {
      fprintf (stderr, "ucnv_toUChars: needed %d but only had %d\n", result,
               *destLen);
      exit (EXIT_FAILURE);
    }
  *destLen = result;
}

static void do_replace (UConverter *conv, URegularExpression *regexp,
                        const char *str, const char *repl, char *dest,
                        int32_t *destLen)
{
  int32_t text_l = 100 + 2 * strlen (str);
  UChar *text = (UChar *) calloc (text_l, sizeof (UChar));
  int32_t replaced_l = 100 + 2 * *destLen;
  UChar *replaced = (UChar *) calloc (replaced_l, sizeof (UChar));
  int32_t replacement_l = 100 + 2 * strlen (repl);
  UChar *replacement = (UChar *) calloc (replacement_l, sizeof (UChar));
  ;
  UErrorCode err = U_ZERO_ERROR;
  const int32_t orig_replaced_l = replaced_l;

  from_utf8 (conv, str, text, &text_l);
  from_utf8 (conv, repl, replacement, &replacement_l);

  uregex_setText (regexp, text, text_l, &err);
  if (U_FAILURE (err))
    {
      fprintf (stderr, "uregex_setText: %s\n", u_errorName (err));
      exit (EXIT_FAILURE);
    }

  replaced_l = uregex_replaceAll (regexp, replacement, replacement_l, replaced,
                                  replaced_l, &err);
  if (U_FAILURE (err))
    {
      fprintf (stderr, "uregex_replaceAll: %s\n", u_errorName (err));
      exit (EXIT_FAILURE);
    }
  else if (replaced_l >= orig_replaced_l)
    {
      fprintf (stderr, "uregex_replaceAll: needed %d but only had %d\n",
               replaced_l, orig_replaced_l);
      exit (EXIT_FAILURE);
    }

  int32_t result =
    ucnv_fromUChars (conv, dest, *destLen, replaced, replaced_l, &err);
  if (U_FAILURE (err))
    {
      fprintf (stderr, "ucnv_fromUChars: %s\n", u_errorName (err));
      exit (EXIT_FAILURE);
    }
  else if (result >= *destLen)
    {
      fprintf (stderr, "ucnv_fromUChars: needed %d but only had %d\n", result,
               *destLen);
      exit (EXIT_FAILURE);
    }
  *destLen = result;

  free (text);
  free (replaced);
  free (replacement);
}

typedef struct MyState
{
  UConverter *conv;
  URegularExpression *regexp;
  slabu *sb;
  const char *slawFile;
} MyState;

static void add_spew (MyState *ms, const char *name, bslaw sl)
{
  const char *fn = "scratch/spewage";
  FILE *f = fopen (fn, "wb");
  if (f == NULL)
    OB_FATAL_ERROR_CODE (0x2030e000, "fopen: %s\n", strerror (errno));
  slaw_spew_overview (sl, f, NULL);
  if (0 != fclose (f))
    OB_FATAL_ERROR_CODE (0x2030e001, "fclose: %s\n", strerror (errno));
  char *spewed = ob_read_file (fn);
  if (!spewed)
    OB_FATAL_ERROR_CODE (0x2030e002, "failed to open %s\n", fn);
  int32_t replaced_l = 100 + 2 * strlen (spewed);
  char *replaced = (char *) malloc (replaced_l);
  do_replace (ms->conv, ms->regexp, spewed, "slaw[xxx]:", replaced,
              &replaced_l);
  slabu_map_put_cc (ms->sb, name, replaced);
  free (replaced);
  free (spewed);
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
  add_spew (ms, #type, s);                                                     \
  slaw_free (s);                                                               \
  s = slaw_##type##_array_filled (0, var_##type);                              \
  add_spew (ms, #type "[0]", s);                                               \
  slaw_free (s);                                                               \
  s = slaw_##type##_array_filled (1, var_##type);                              \
  add_spew (ms, #type "[1]", s);                                               \
  slaw_free (s);                                                               \
  s = slaw_##type##_array_filled (3, var_##type);                              \
  add_spew (ms, #type "[3]", s);                                               \
  slaw_free (s);

static void add_stuff (MyState *ms)
{
  slaw s = NULL;

  add_spew (ms, "null", s);

  s = slaw_nil ();
  add_spew (ms, "nil", s);
  slaw_free (s);

  s = slaw_boolean (false);
  add_spew (ms, "false", s);
  slaw_free (s);

  s = slaw_boolean (true);
  add_spew (ms, "true", s);
  slaw_free (s);

  s = slaw_string ("Satan eats Cheez Whiz");
  add_spew (ms, "string", s);
  slaw_free (s);

  s = slaw_list_f (slabu_new ());
  add_spew (ms, "empty-list", s);
  slaw_free (s);

  s = slaw_map_f (slabu_new ());
  add_spew (ms, "empty-map", s);
  slaw_free (s);

  s = make_map ();
  add_spew (ms, "hm-map", s);
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

  OB_DIE_ON_ERROR (slaw_read_from_file (ms->slawFile, &s));
  add_spew (ms, "file", s);
  slaw_free (s);
}

static slaw make_big_map (const char *slawFile)
{
  UErrorCode err = U_ZERO_ERROR;

  UConverter *conv = ucnv_open ("utf-8", &err);
  if (U_FAILURE (err))
    {
      fprintf (stderr, "ucnv_open: %s\n", u_errorName (err));
      exit (EXIT_FAILURE);
    }

  UChar pattern[HUGE];
  int32_t pattern_l = HUGE;
  from_utf8 (conv, "slaw\\[\\d+[oq]\\.[0-9A-Fa-fxX]+\\]:", pattern, &pattern_l);
  URegularExpression *regexp = uregex_open (pattern, pattern_l, 0, NULL, &err);

  MyState ms;
  ms.conv = conv;
  ms.regexp = regexp;
  ms.sb = slabu_new ();
  ms.slawFile = slawFile;

  add_stuff (&ms);

  uregex_close (regexp);
  ucnv_close (conv);
  return slaw_map_f (ms.sb);
}

static void print_map_keys (bslaw m)
{
  bslaw cole;
  const char *prefix = "map keys are:";
  for (cole = slaw_list_emit_first (m); cole != NULL;
       cole = slaw_list_emit_next (m, cole))
    {
      const char *key = slaw_string_emit (slaw_cons_emit_car (cole));
      fprintf (stderr, "%s %s", prefix, key);
      prefix = ",";
    }
  fputs ("\n", stderr);
}

static int mainish (int argc, char **argv)
{
  if (argc == 2)
    {
      slaw m = make_big_map (argv[1]);
      slaw_output so;
      OB_DIE_ON_ERROR (slaw_output_open_text_z (stdout, &so));
      OB_DIE_ON_ERROR (slaw_output_write (so, m));
      OB_DIE_ON_ERROR (slaw_output_close (so));
      slaw_free (m);
      return EXIT_SUCCESS;
    }
  else if (argc == 3)
    {
      slaw actual = make_big_map (argv[1]);
      slaw expected;
      bslaw cole;
      OB_DIE_ON_ERROR (slaw_read_from_file (argv[2], &expected));
      for (cole = slaw_list_emit_first (expected); cole != NULL;
           cole = slaw_list_emit_next (expected, cole))
        {
          bslaw key = slaw_cons_emit_car (cole);
          bslaw expected_value = slaw_cons_emit_cdr (cole);
          bslaw actual_value = slaw_map_find (actual, key);
          if (actual_value == NULL)
            {
              OB_LOG_ERROR_CODE (0x2030e003, "weird: actual is NULL for '%s'\n",
                                 slaw_string_emit (key));
              print_map_keys (actual);
              return EXIT_FAILURE;
            }
          if (!slawx_equal (expected_value, actual_value))
            {
              const char *expected_str = slaw_string_emit (expected_value);
              const char *actual_str = slaw_string_emit (actual_value);

              if (actual_str == NULL)
                OB_FATAL_ERROR_CODE (0x2030e004,
                                     "weird: actual is NULL for '%s'\n",
                                     slaw_string_emit (key));
              if (expected_str == NULL)
                OB_FATAL_ERROR_CODE (0x2030e005,
                                     "weird: expected is NULL for '%s'\n",
                                     slaw_string_emit (key));
              if (strlen (expected_str) < 60 && strlen (actual_str) < 60)
                {
                  fprintf (stderr, "Mismatch for '%s':\n",
                           slaw_string_emit (key));
                  fprintf (stderr, "expected: %s\n", expected_str);
                  fprintf (stderr, "actual  : %s\n", actual_str);
                  return EXIT_FAILURE;
                }
              else
                {
                  const char *expected_file = "scratch/spew-expected.txt";
                  const char *actual_file = "scratch/spew-actual.txt";
                  FILE *f = fopen (expected_file, "w");
                  fputs (expected_str, f);
                  fclose (f);
                  f = fopen (actual_file, "w");
                  fputs (actual_str, f);
                  fclose (f);
                  fprintf (stderr, "Mismatch for '%s': see files %s %s\n",
                           slaw_string_emit (key), expected_file, actual_file);
                  return EXIT_FAILURE;
                }
            }
        }
      slaw_free (expected);
      slaw_free (actual);
      return EXIT_SUCCESS;
    }
  else
    {
      fprintf (stderr,
               "Usage: %s big-endian-protein.bin [spew-expected.yaml]\n",
               argv[0]);
      return EXIT_FAILURE;
    }
}

int main (int argc, char **argv)
{
  return check_for_leaked_file_descriptors_scoped (mainish, argc, argv);
}
