
/* (c)  oblong industries */

#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-vers.h"
#include "libPlasma/c/slaw.h"
#include "libPlasma/c/slaw-string.h"
#include "libPlasma/c/pool.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define NUM 500
#define ITER 400

// This is just to make valgrind happy!
static slaw to_be_freed[50000];
static int free_counter = 0;

static slaw f (slaw s)
{
  to_be_freed[free_counter++] = s;
  return s;
}

static void test (slabu *sb)
{
  bool exists[NUM];
  int i;
  ob_retort tort;

  memset (exists, 0, sizeof (exists));

  for (i = 0; i < ITER; i++)
    {
      int r = (rand () % NUM);
      slaw key = slaw_unt16 (r);
      slaw value = slaw_float32 (sqrt ((float) r));
      bslaw result;

      switch (rand () % 4)
        {
          case 0:
            OB_DIE_ON_ERROR (slabu_map_put (sb, f (key), f (value)));
            break;
          case 1:
            OB_DIE_ON_ERROR (slabu_map_put_ff (sb, key, value));
            break;
          case 2:
            OB_DIE_ON_ERROR (slabu_map_put_lf (sb, f (key), value));
            break;
          case 3:
            OB_DIE_ON_ERROR (slabu_map_put_fl (sb, key, f (value)));
            break;
        }

      exists[r] = true;

      r = (rand () % NUM);
      key = slaw_unt16 (r);
      if ((rand () & 1) == 0)
        result = slabu_map_find (sb, f (key), NULL);
      else
        result = slabu_map_find_f (sb, key, NULL);

      if (exists[r])
        {
          if (result == NULL)
            OB_FATAL_ERROR_CODE (0x2030c000, "expected to find %d but didn't\n",
                                 r);
          else if (!slaw_is_float32 (result))
            OB_FATAL_ERROR_CODE (0x2030c001,
                                 "expected a float32 and it wasn't\n");
        }
      else
        {
          if (result != NULL)
            OB_FATAL_ERROR_CODE (0x2030c002, "found %d but didn't expect to\n",
                                 r);
        }

      r = (rand () % NUM);
      key = slaw_unt16 (r);
      if ((rand () & 1) == 0)
        tort = slabu_map_remove (sb, f (key));
      else
        tort = slabu_map_remove_f (sb, key);

      if (exists[r])
        {
          if (tort != OB_OK)
            OB_FATAL_ERROR_CODE (0x2030c003, "expected to find %d but didn't\n",
                                 r);
        }
      else
        {
          if (tort == OB_OK)
            OB_FATAL_ERROR_CODE (0x2030c004, "found %d but didn't expect to\n",
                                 r);
        }

      exists[r] = false;
    }
}

int main (int argc, char **argv)
{
  OB_DIE_ON_ERROR (OB_CHECK_ABI ());

  slabu *sb;
  int64 got, idx;
  bslaw s;
  int i;
  ob_retort tort;

  sb = slabu_new ();
  OB_DIE_ON_ERROR (slabu_map_put_cl (sb, "dog", f (slaw_string ("bark"))));
  OB_DIE_ON_ERROR (slabu_map_put_cf (sb, "cat", slaw_string ("meow")));
  idx = -1;
  slabu_map_find_c (sb, "cat", &idx);
  if (idx != 1)
    OB_FATAL_ERROR_CODE (0x2030c005, "expected 1 but got %" OB_FMT_64 "d\n",
                         idx);
  idx = -1;
  slabu_map_find_c (sb, "dog", &idx);
  if (idx != 0)
    OB_FATAL_ERROR_CODE (0x2030c006, "expected 0 but got %" OB_FMT_64 "d\n",
                         idx);
  idx = -1;
  slabu_map_find_c (sb, "fish", &idx);
  if (idx != -1)
    OB_FATAL_ERROR_CODE (0x2030c007, "expected -1 but got %" OB_FMT_64 "d\n",
                         idx);
  OB_DIE_ON_ERROR (slabu_map_put_cc (sb, "chicken", "cluck"));
  idx = -1;
  slabu_map_find_c (sb, "chicken", &idx);
  if (idx != 2)
    OB_FATAL_ERROR_CODE (0x2030c008, "expected 2 but got %" OB_FMT_64 "d\n",
                         idx);
  OB_DIE_ON_ERROR (slabu_map_put_cc (sb, "turkey", "gobble"));
  idx = -1;
  slabu_map_find_c (sb, "turkey", &idx);
  if (idx != 3)
    OB_FATAL_ERROR_CODE (0x2030c009, "expected 3 but got %" OB_FMT_64 "d\n",
                         idx);
  OB_DIE_ON_ERROR (slabu_map_put_cc (sb, "raven", "nevermore"));
  idx = -1;
  slabu_map_find_c (sb, "raven", &idx);
  if (idx != 4)
    OB_FATAL_ERROR_CODE (0x2030c00a, "expected 4 but got %" OB_FMT_64 "d\n",
                         idx);
  idx = -1;
  slabu_map_find_c (sb, "turkey", &idx);
  if (idx != 3)
    OB_FATAL_ERROR_CODE (0x2030c00b, "expected 3 but got %" OB_FMT_64 "d\n",
                         idx);
  if (slabu_map_remove_c (sb, "cat") != OB_OK)
    OB_FATAL_ERROR_CODE (0x2030c00c, "not OK");
  if (slabu_map_remove_c (sb, "dog") != OB_OK)
    OB_FATAL_ERROR_CODE (0x2030c00d, "not OK");
  idx = -1;
  slabu_map_find_c (sb, "raven", &idx);
  if (idx != 2)
    OB_FATAL_ERROR_CODE (0x2030c00e, "expected 2 but got %" OB_FMT_64 "d\n",
                         idx);
  slabu_free (sb);

  sb = slabu_new ();
  test (sb);
  slabu_free (sb);

  sb = slabu_new ();
  OB_DIE_ON_ERROR (slabu_list_add_c (sb, "I am not a cons"));
  got = slabu_list_find_c (sb, "I am not a cons");
  if (got != 0)
    OB_FATAL_ERROR_CODE (0x2030c00f, "expected 0 and got %" OB_FMT_64 "d\n",
                         got);
  test (sb);
  got = slabu_list_find_c (sb, "I am not a cons");
  if (got != SLAW_NOT_FOUND)
    OB_FATAL_ERROR_CODE (
      0x2030c010, "expected SLAW_NOT_FOUND and got %" OB_FMT_64 "d\n", got);
  slabu_free (sb);

  sb = slabu_new ();
  OB_DIE_ON_ERROR (slabu_list_add_c (sb, "I am not a cons"));
  if (slabu_list_remove_c (sb, "I am not a cons") != OB_OK)
    OB_FATAL_ERROR_CODE (0x2030c011, "not OK");
  test (sb);
  slabu_free (sb);

  sb = slabu_new ();
  OB_DIE_ON_ERROR (slabu_list_add_z (sb, f (slaw_cons_cc ("fish", "swim"))));
  OB_DIE_ON_ERROR (slabu_list_add_x (sb, slaw_int32 (123)));
  OB_DIE_ON_ERROR (slabu_list_add_x (sb, slaw_cons_cc ("rabbit", "hop")));
  OB_DIE_ON_ERROR (slabu_list_add_x (sb, slaw_cons_cc ("bird", "fly")));
  OB_DIE_ON_ERROR (slabu_list_add_z (sb, f (slaw_cons_cc ("turtle", "crawl"))));
  OB_DIE_ON_ERROR (slabu_list_add_z (sb, f (slaw_cons_cc ("duck", "swim"))));
  OB_DIE_ON_ERROR (slabu_list_add_x (sb, slaw_cons_cc ("duck", "walk")));
  OB_DIE_ON_ERROR (slabu_list_add_z (sb, f (slaw_cons_cc ("duck", "quack"))));
  OB_DIE_ON_ERROR (slabu_list_add_z (sb, f (slaw_unt8 (0))));
  test (sb);
  idx = -1;
  s = slabu_map_find_c (sb, "duck", &idx);
  if (s == NULL || idx < 0)
    OB_FATAL_ERROR_CODE (0x2030c012, "can't find duck\n");
  if (!slawx_equal_lc (s, "quack"))
    OB_FATAL_ERROR_CODE (0x2030c013, "duck wants to %s\n",
                         slaw_string_emit (s));
  idx = -1;
  s = slabu_map_find_c (sb, "panama", &idx);
  if (s != NULL || idx != -1)
    OB_FATAL_ERROR_CODE (0x2030c014, "unexpected panama");
  tort = slabu_map_remove_c (sb, "duck");
  if (tort != OB_OK)
    OB_FATAL_ERROR_CODE (0x2030c015, "can't remove duck\n");
  idx = -1;
  s = slabu_map_find_c (sb, "duck", &idx);
  if (s != NULL || idx != -1)
    OB_FATAL_ERROR_CODE (0x2030c016, "unexpected duck");
  idx = -1;
  s = slabu_map_find_c (sb, "turtle", &idx);
  if (s == NULL || idx < 0)
    OB_FATAL_ERROR_CODE (0x2030c017, "can't find turtle\n");
  if (!slawx_equal_lc (s, "crawl"))
    OB_FATAL_ERROR_CODE (0x2030c018, "turtle wants to %s\n",
                         slaw_string_emit (s));
  OB_DIE_ON_ERROR (slabu_map_put_cc (sb, "fish", "spawn"));
  idx = -1;
  s = slabu_map_find_c (sb, "fish", &idx);
  if (s == NULL || idx < 0)
    OB_FATAL_ERROR_CODE (0x2030c019, "can't find fish\n");
  if (!slawx_equal_lc (s, "spawn"))
    OB_FATAL_ERROR_CODE (0x2030c01a, "fish wants to %s\n",
                         slaw_string_emit (s));
  slabu_free (sb);

  // free stuff to please valgrind
  for (i = 0; i < free_counter; i++)
    slaw_free (to_be_freed[i]);

  return EXIT_SUCCESS;
}
