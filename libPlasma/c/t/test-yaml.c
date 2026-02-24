
/* (c)  oblong industries */

#include "libLoam/c/ob-log.h"
#include "libPlasma/c/private/plasma-testing.h" /* for private_test_yaml_hash() */
#include "libPlasma/c/slaw-io.h"
#include "libPlasma/c/slaw-path.h"
#include "libPlasma/c/pool.h"
#include "libPlasma/c/pool_cmd.h"
#include <stdlib.h>

static int mainish (int argc, char **argv)
{
  private_test_yaml_hash (); /* verify that slaw-yaml.c is written correctly */

  const char *myStr = "{steak: 0xdeadbeef, buzz_lightyear: +.Inf}";
  slaw s;
  ob_retort err = slaw_from_string (myStr, &s);
  if (err != OB_OK)
    OB_FATAL_ERROR_CODE (0x20316000, "error %s\n", ob_error_string (err));

  if (!slaw_is_map (s))
    OB_FATAL_ERROR_CODE (0x20316001, "not a map\n");

  bslaw steak = slaw_map_find_c (s, "steak");
  bslaw buzz_lightyear = slaw_map_find_c (s, "buzz_lightyear");

  if (!slaw_is_int64 (steak))
    OB_FATAL_ERROR_CODE (0x20316002, "not an int64\n");

  if (!slaw_is_float64 (buzz_lightyear))
    OB_FATAL_ERROR_CODE (0x20316003, "not a float64\n");

  int64 got;
  const int64 *p1 = slaw_int64_emit (steak);
  if (0xdeadbeef != (got = *p1))
    OB_FATAL_ERROR_CODE (0x20316004, "0x%" OB_FMT_64 "x is not 0xdeadbeef\n",
                         got);

  const float64 *p2 = slaw_float64_emit (buzz_lightyear);
  if (1e100 > *p2)
    OB_FATAL_ERROR_CODE (0x20316005, "not infinity\n");

  slaw str;
  err = slaw_to_string (s, &str);
  if (err != OB_OK)
    OB_FATAL_ERROR_CODE (0x20316006, "error %s\n", ob_error_string (err));

  slaw s2;
  err = slaw_from_string (slaw_string_emit (str), &s2);
  if (err != OB_OK)
    OB_FATAL_ERROR_CODE (0x20316007, "error %s\n", ob_error_string (err));

  if (!slawx_equal (s, s2))
    OB_FATAL_ERROR_CODE (0x20316008, "not equal\n");

  unt64 u1 = (unt64) (unsigned long) p1;
  unt64 u2 = (unt64) (unsigned long) p2;

  // check that int64 and float64 are 8-byte aligned
  if (u1 % 8 != 0 || u2 % 8 != 0)
    OB_FATAL_ERROR_CODE (
      0x20316009, "%" OB_FMT_64 "x or %" OB_FMT_64 "x is not 8-byte aligned\n",
      u1, u2);

  slaw_free (s);
  slaw_free (s2);
  slaw_free (str);

  // bug 651: what happens when slaw_from_string() is given a string
  // containing more than one YAML document?

  const char *double_trouble = "%YAML 1.1\n"
                               "%TAG ! tag:oblong.com,2009:slaw/\n"
                               "--- !protein\n"
                               "descrips:\n"
                               "- status\n"
                               "- camreader\n"
                               "ingests: !!omap\n"
                               "- status: PROGRESS\n"
                               "- message: READY\n"
                               "- progress: !f32 1\n"
                               "...\n"
                               "%YAML 1.1\n"
                               "%TAG ! tag:oblong.com,2009:slaw/\n"
                               "--- !protein\n"
                               "descrips:\n"
                               "- status\n"
                               "- camreader\n"
                               "ingests: !!omap\n"
                               "- status: PROGRESS\n"
                               "- message: SHUTDOWN\n"
                               "- progress: !f32 0\n"
                               "...\n";

  OB_DIE_ON_ERROR (slaw_from_string (double_trouble, &s));
  const char *msg = slaw_path_get_string (s, "message", "large alarmed secret");
  if (0 != strcmp (msg, "READY"))
    OB_FATAL_ERROR_CODE (0x2031600a, "Expected READY but got %s\n", msg);
  slaw_free (s);

  return EXIT_SUCCESS;
}

int main (int argc, char **argv)
{
  return check_for_leaked_file_descriptors_scoped (mainish, argc, argv);
}
