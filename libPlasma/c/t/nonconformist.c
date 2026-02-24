
/* (c)  oblong industries */

// check slabu_map_conform and other mapifying functions to see
// in what order duplicates are resolved
// For in-depth discussion, see:
// https://bugs.oblong.com/show_bug.cgi?id=28

#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-retorts.h"
#include "libLoam/c/ob-types.h"
#include "libLoam/c/ob-vers.h"
#include "libPlasma/c/slaw.h"

#include <stdlib.h>

static const char *const words[] = {
  "mortals",    "foolish",       "pallor",      "cadaverous",  "aura",
  "foreboding", "metamorphosis", "disquieting", "observation", "dismaying",
  "no",         "windows",       "no",          "doors",       "challenge",
  "chilling",   "way",           "out",         "way",         "my",
  "frighten",   "prematurely"};

static const int nwords = sizeof (words) / sizeof (words[0]);
#define NPAIRS ((sizeof (words) / sizeof (words[0])) / 2)
static const int nunique = ((sizeof (words) / sizeof (words[0])) / 2) - 2;

#define DO_CHECK(key, spect)                                                   \
  found = slaw_map_find_c (x, key);                                            \
  if (!found)                                                                  \
    OB_FATAL_ERROR_CODE (0x20307000, "%s: '%s' not found\n", id, key);         \
  if (!slawx_equal_lc (found, spect))                                          \
  OB_FATAL_ERROR_CODE (0x20307001,                                             \
                       "%s: expected '%s' to be '%s', but it was '%s'\n", id,  \
                       key, spect, slaw_string_emit (found))

static void verify (const char *id, bool (*func) (bslaw s), slaw x, int n,
                    const char *aperture, const char *way)
{
  int64 c;
  bslaw found;

  if (!func (x))
    OB_FATAL_ERROR_CODE (0x20307002, "%s: func (x) was false\n", id);

  if (!slaw_is_list_or_map (x))
    OB_FATAL_ERROR_CODE (0x20307003, "%s: slaw_is_list_or_map (x) was false\n",
                         id);

  if ((c = slaw_list_count (x)) != n)
    OB_FATAL_ERROR_CODE (
      0x20307004, "%s: expected count %d but got %" OB_FMT_64 "d\n", id, n, c);

  DO_CHECK ("mortals", "foolish");
  DO_CHECK ("pallor", "cadaverous");
  DO_CHECK ("aura", "foreboding");
  DO_CHECK ("metamorphosis", "disquieting");
  DO_CHECK ("observation", "dismaying");
  DO_CHECK ("no", aperture);
  DO_CHECK ("challenge", "chilling");
  DO_CHECK ("way", way);
  DO_CHECK ("frighten", "prematurely");

  slaw_free (x);
}

#define NONNEG(x)                                                              \
  if ((ret = (x)) < 0)                                                         \
  OB_FATAL_ERROR_CODE (0x20307005, "'%s' returned %" OB_FMT_64 "d\n", #x, ret)

int main (int argc, char **argv)
{
  OB_DIE_ON_ERROR (OB_CHECK_ABI ());

  int i;
  slabu *sb;
  int64 ret;
  ob_retort err, expected;
  slaw manymaps[NPAIRS];
  slaw m;
  slabu *bs[2];

  // put as map, create as map, expect last, no duplicates

  sb = slabu_new ();
  for (i = 0; i < nwords; i += 2)
    NONNEG (slabu_map_put_cc (sb, words[i], words[i + 1]));
  verify ("A", slaw_is_map, slaw_map_f (sb), nunique, "doors", "my");

  // put as map, create as list, expect last, no duplicates

  sb = slabu_new ();
  for (i = 0; i < nwords; i += 2)
    NONNEG (slabu_map_put_cc (sb, words[i], words[i + 1]));
  verify ("B", slaw_is_list, slaw_list_f (sb), nunique, "doors", "my");

  // put as list, create as map, expect last, no duplicates

  sb = slabu_new ();
  for (i = 0; i < nwords; i += 2)
    NONNEG (slabu_list_add_x (sb, slaw_cons_cc (words[i], words[i + 1])));
  verify ("C", slaw_is_map, slaw_map_f (sb), nunique, "doors", "my");

  // put as list, create as list, expect last, with duplicates

  sb = slabu_new ();
  for (i = 0; i < nwords; i += 2)
    NONNEG (slabu_list_add_x (sb, slaw_cons_cc (words[i], words[i + 1])));
  verify ("D", slaw_is_list, slaw_list_f (sb), NPAIRS, "doors", "my");

  // put as list, conform + create as list, expect last, no duplicates

  sb = slabu_new ();
  for (i = 0; i < nwords; i += 2)
    NONNEG (slabu_list_add_x (sb, slaw_cons_cc (words[i], words[i + 1])));
  if ((err = slabu_map_conform (sb)) != (expected = OB_OK))
    OB_FATAL_ERROR_CODE (0x20307006,
                         "slabu_map_conform returned %s; expected %s\n",
                         ob_error_string (err), ob_error_string (expected));
  verify ("E", slaw_is_list, slaw_list (sb), nunique, "doors", "my");
  if ((err = slabu_map_conform (sb)) != (expected = OB_NOTHING_TO_DO))
    OB_FATAL_ERROR_CODE (0x20307007,
                         "slabu_map_conform returned %s; expected %s\n",
                         ob_error_string (err), ob_error_string (expected));
  slabu_free (sb);

  // create many small maps and merge them, expect last, no duplicates

  for (i = 0; i < nwords; i += 2)
    manymaps[i / 2] = slaw_map_inline_cc (words[i], words[i + 1], NULL);
  m = slaw_maps_merge_f (manymaps[0], manymaps[1], manymaps[2], manymaps[3],
                         manymaps[4], manymaps[5], manymaps[6], manymaps[7],
                         manymaps[8], manymaps[9], manymaps[10], NULL);
  verify ("F", slaw_is_map, m, nunique, "doors", "my");

  // create two maps ("no" is split across them, but "way" is all in the 2nd)
  // and merge them, expect last, no duplicates

  for (i = 0; i < 2; i++)
    bs[i] = slabu_new ();
  for (i = 0; i < nwords; i += 2)
    NONNEG (
      slabu_list_add_x (bs[i / 12], slaw_cons_cc (words[i], words[i + 1])));
  for (i = 0; i < 2; i++)
    manymaps[i] = slaw_list_f (bs[i]);
  m = slaw_maps_merge_f (manymaps[0], manymaps[1], NULL);
  verify ("G", slaw_is_map, m, nunique, "doors", "my");

  return EXIT_SUCCESS;
}
