
/* (c)  oblong industries */

// check the options that can be used with slaw_output_open_text_options:
// tag_numbers, directives, and ordered_maps

#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-retorts.h"
#include "libLoam/c/ob-types.h"
#include "libPlasma/c/slaw.h"
#include "libPlasma/c/slaw-io.h"
#include "libPlasma/c/slaw-string.h"
#include "libPlasma/c/slaw-coerce.h"

#include <stdlib.h>

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

#define DO_CHECK(key, spect)                                                   \
  found = slaw_map_find_c (x, key);                                            \
  if (!found)                                                                  \
    OB_FATAL_ERROR_CODE (0x20319000, "%d: '%s' not found\n", id, key);         \
  if (!slawx_equal_lc (found, spect))                                          \
  OB_FATAL_ERROR_CODE (0x20319001,                                             \
                       "%d: expected '%s' to be '%s', but it was '%s'\n", id,  \
                       key, spect, slaw_string_emit (found))

static void verify (int id, slaw x, bool (*haunt_func) (bslaw s),
                    bool (*room_func) (bslaw s))
{
  int64 c;
  bslaw found;
  const int n = 12;

  if (!slaw_is_map (x))
    OB_FATAL_ERROR_CODE (0x20319002, "%d: slaw_is_map (x) was false\n", id);

  if ((c = slaw_list_count (x)) != n)
    OB_FATAL_ERROR_CODE (
      0x20319003, "%d: expected count %d but got %" OB_FMT_64 "d\n", id, n, c);

  DO_CHECK ("mortals", "foolish");
  DO_CHECK ("pallor", "cadaverous");
  DO_CHECK ("aura", "foreboding");
  DO_CHECK ("metamorphosis", "disquieting");
  DO_CHECK ("observation", "dismaying");
  DO_CHECK ("challenge", "chilling");
  DO_CHECK ("frighten", "prematurely");

  slaw j = slaw_strings_join (slaw_map_find_c (x, "no"), ",");
  const char *e = "windows,doors";
  if (!slawx_equal_lc (j, e))
    OB_FATAL_ERROR_CODE (0x20319004, "%d: expected '%s' but got '%s'\n", id, e,
                         slaw_string_emit (j));
  slaw_free (j);

  j = slaw_strings_join (slaw_map_find_c (x, "way"), ":");
  e = "out:my";
  if (!slawx_equal_lc (j, e))
    OB_FATAL_ERROR_CODE (0x20319005, "%d: expected '%s' but got '%s'\n", id, e,
                         slaw_string_emit (j));
  slaw_free (j);

  found = slaw_map_find_f (x, slaw_list_inline_c ("room", "for", NULL));
  if (!room_func (found))
    OB_FATAL_ERROR_CODE (0x20319006, "%d: !room_func (found)\n", id);
  OB_DIE_ON_ERROR (slaw_to_int64 (found, &c));
  if (c != 1000)
    OB_FATAL_ERROR_CODE (0x20319007, "%d: room for %" OB_FMT_64 "d\n", id, c);

  found = slaw_map_find_c (x, "haunts");
  if (!haunt_func (found))
    OB_FATAL_ERROR_CODE (0x20319008, "%d: !haunt_func (found)\n", id);
  OB_DIE_ON_ERROR (slaw_to_int64 (found, &c));
  if (c != 999)
    OB_FATAL_ERROR_CODE (0x20319009, "%d: %" OB_FMT_64 "d happy haunts\n", id,
                         c);

  found = slaw_map_find_c (x, "volunteers?");
  const bool *b = slaw_boolean_emit (found);
  if (!b)
    OB_FATAL_ERROR_CODE (0x2031900a, "%d: volunteers is not a boolean\n", id);
  if (*b)
    OB_FATAL_ERROR_CODE (0x2031900b, "%d: volunteers is not false\n", id);

  slaw_free (x);
}

int main (int argc, char **argv)
{
  int i;

  slaw m = make_map ();

  for (i = 0; i < 8; i++)
    {
      bool tag_numbers = (1 & i);
      bool directives = (1 & (i >> 1));
      bool ordered_maps = (1 & (i >> 2));
      bool lossless = (tag_numbers && ordered_maps);
      slaw str, s;

      slaw o =
        slaw_map_inline_cf ("tag_numbers", slaw_boolean (tag_numbers),
                            "directives", slaw_boolean (directives),
                            "ordered_maps", slaw_boolean (ordered_maps), NULL);
      OB_DIE_ON_ERROR (slaw_to_string_options_f (m, &str, o));
      bool hasOmap = (NULL != strstr (slaw_string_emit (str), "omap"));
      bool hasUnt32 = (NULL != strstr (slaw_string_emit (str), "u32"));
      bool hasCons = (NULL != strstr (slaw_string_emit (str), "cons"));

      if (ordered_maps != hasOmap)
        OB_FATAL_ERROR_CODE (0x2031900c, "%d: hasOmap was unexpectedly %d\n", i,
                             (int) hasOmap);
      if (tag_numbers != hasUnt32)
        OB_FATAL_ERROR_CODE (0x2031900d, "%d: hasUnt32 was unexpectedly %d\n",
                             i, (int) hasUnt32);

      // None of these variants should have an explicit !cons
      if (hasCons)
        OB_FATAL_ERROR_CODE (0x2031900e, "%d: unexpectedly had a cons\n", i);

      OB_DIE_ON_ERROR (slaw_from_string (slaw_string_emit (str), &s));
      slaw_free (str);

      if (lossless)
        {
          if (!slawx_equal_lf (m, s))
            OB_FATAL_ERROR_CODE (0x2031900f, "%d: s != m\n", i);
        }
      else
        {
          verify (i, s, tag_numbers ? slaw_is_int16 : slaw_is_int64,
                  tag_numbers ? slaw_is_unt32 : slaw_is_int64);
        }
    }

  slaw_free (m);

  return EXIT_SUCCESS;
}
