
/* (c)  oblong industries */

#include "libPlasma/c/slaw.h"
#include "libPlasma/c/protein.h"
#include "libPlasma/c/slaw-path.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-vers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main (int argc, char **argv)
{
  OB_DIE_ON_ERROR (OB_CHECK_ABI ());

  slabu *sounds = slabu_new ();
  OB_DIE_ON_ERROR (slabu_map_put_cf (sounds, "lion", slaw_string ("roar")));
  OB_DIE_ON_ERROR (slabu_map_put_cf (sounds, "cat", slaw_string ("meow")));
  OB_DIE_ON_ERROR (slabu_map_put_cf (sounds, "dog", slaw_string ("bark")));

  slabu *actions = slabu_new ();
  OB_DIE_ON_ERROR (slabu_map_put_cf (actions, "rabbit", slaw_string ("hop")));
  OB_DIE_ON_ERROR (slabu_map_put_cf (actions, "cheetah", slaw_string ("run")));
  OB_DIE_ON_ERROR (slabu_map_put_cf (actions, "fish", slaw_string ("swim")));
  OB_DIE_ON_ERROR (slabu_map_put_cf (actions, "turtle", slaw_string ("crawl")));

  slabu *animals = slabu_new ();
  OB_DIE_ON_ERROR (slabu_map_put_cf (animals, "sounds", slaw_list_f (sounds)));
  OB_DIE_ON_ERROR (
    slabu_map_put_cf (animals, "actions", slaw_list_f (actions)));

  slabu *numbers = slabu_new ();
  OB_DIE_ON_ERROR (slabu_map_put_cf (numbers, "one", slaw_unt8 (1)));
  OB_DIE_ON_ERROR (slabu_map_put_cf (numbers, "two", slaw_float64 (2)));
  OB_DIE_ON_ERROR (slabu_map_put_cf (numbers, "three", slaw_int32 (3)));
  OB_DIE_ON_ERROR (slabu_map_put_cf (numbers, "four", slaw_unt64 (4)));
  OB_DIE_ON_ERROR (slabu_map_put_cf (numbers, "five", slaw_float32 (5)));

  slaw m = slaw_map_inline_cf ("animals", slaw_list_f (animals), "numbers",
                               slaw_list_f (numbers), NULL);
  protein p = protein_from_ff (NULL, m);

  int64 one = slaw_path_get_int64 (p, "numbers/one", 6);
  unt64 two = slaw_path_get_unt64 (p, "numbers/two", 6);
  float64 three = slaw_path_get_float64 (p, "numbers/three", 6);
  bslaw s = slaw_path_get_slaw (p, "numbers");
  int64 four = slaw_path_get_int64 (s, "four", 6);
  unt64 five = slaw_path_get_unt64 (s, "five", 6);
  float64 six = slaw_path_get_float64 (s, "six", 6);

  const char *lion = slaw_path_get_string (p, "animals/sounds/lion", "chirp");
  const char *cat = slaw_path_get_string (p, "animals/sounds/cat", "chirp");
  const char *dog = slaw_path_get_string (p, "animals/sounds/dog", "chirp");
  const char *cricket =
    slaw_path_get_string (p, "animals/sounds/cricket", "chirp");

  s = slaw_path_get_slaw (p, "animals/actions");
  const char *rabbit = slaw_path_get_string (s, "rabbit", "fly");
  const char *cheetah = slaw_path_get_string (s, "cheetah", "fly");
  const char *fish = slaw_path_get_string (s, "fish", "fly");
  const char *turtle = slaw_path_get_string (s, "turtle", "fly");
  const char *bird = slaw_path_get_string (s, "bird", "fly");

  if (one != 1)
    {
      fprintf (stderr,
               "one = %" OB_FMT_64 "d"
               "\n",
               one);
      OB_FATAL_ERROR_CODE (0x20313000, "one\n");
    }
  if (two != 2)
    OB_FATAL_ERROR_CODE (0x20313001, "two\n");
  if (three != 3)
    OB_FATAL_ERROR_CODE (0x20313002, "three\n");
  if (four != 4)
    OB_FATAL_ERROR_CODE (0x20313003, "four\n");
  if (five != 5)
    OB_FATAL_ERROR_CODE (0x20313004, "five\n");
  if (six != 6)
    OB_FATAL_ERROR_CODE (0x20313005, "six\n");

  if (strcmp (lion, "roar") != 0)
    OB_FATAL_ERROR_CODE (0x20313006, "lion\n");
  if (strcmp (cat, "meow") != 0)
    OB_FATAL_ERROR_CODE (0x20313007, "cat\n");
  if (strcmp (dog, "bark") != 0)
    OB_FATAL_ERROR_CODE (0x20313008, "dog\n");
  if (strcmp (cricket, "chirp") != 0)
    OB_FATAL_ERROR_CODE (0x20313009, "cricket\n");

  if (strcmp (rabbit, "hop") != 0)
    OB_FATAL_ERROR_CODE (0x2031300a, "rabbit\n");
  if (strcmp (cheetah, "run") != 0)
    OB_FATAL_ERROR_CODE (0x2031300b, "cheetah\n");
  if (strcmp (fish, "swim") != 0)
    OB_FATAL_ERROR_CODE (0x2031300c, "fish\n");
  if (strcmp (turtle, "crawl") != 0)
    OB_FATAL_ERROR_CODE (0x2031300d, "turtle\n");
  if (strcmp (bird, "fly") != 0)
    OB_FATAL_ERROR_CODE (0x2031300e, "bird\n");

  protein_free (p);

  return EXIT_SUCCESS;
}
