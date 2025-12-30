
/* (c)  oblong industries */

#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-vers.h"
#include "libPlasma/c/slaw.h"
#include "libPlasma/c/slaw-string.h"
#include "libPlasma/c/pool.h"
#include <stdlib.h>

// This is just to make valgrind happy!
static slaw to_be_freed[50000];
static int free_counter = 0;

static slaw f (slaw s)
{
  to_be_freed[free_counter++] = s;
  return s;
}

static bool isPrime (int x)
{
  int i;

  if ((x % 2) == 0)
    return false;

  for (i = 3; i < x; i += 2)
    if ((x % i) == 0)
      return false;

  return true;
}

static void test (int16 num)
{
  slabu *sb = slabu_new ();
  int16 i;
  int n;
  slaw s;
  int64 x;
  ob_retort tort;

  for (i = 0; i < num; i++)
    {
      bool doFree = isPrime (i);
      s = slaw_int16 (i);
      if (doFree)
        OB_DIE_ON_ERROR (slabu_list_add_x (sb, s));
      else
        OB_DIE_ON_ERROR (slabu_list_add_z (sb, f (s)));
    }

  n = (rand () % num);
  tort = slabu_list_remove_nth (sb, n);
  if (tort != OB_OK)
    OB_FATAL_ERROR_CODE (0x2030b000, "not OK");

  for (i = 0; i < num; i++)
    {
      int r = rand ();
      bool doFree = ((r & 1) == 0);
      int64 expected;
      s = slaw_int16 (i);
      if (doFree)
        x = slabu_list_find_f (sb, s);
      else
        x = slabu_list_find (sb, f (s));

      if (i == n)
        expected = SLAW_NOT_FOUND;
      else if (i > n)
        expected = i - 1;
      else
        expected = i;

      if (x != expected)
        OB_FATAL_ERROR_CODE (0x2030b001,
                             "%" OB_FMT_64 "d"
                             " != %" OB_FMT_64 "d"
                             "\n",
                             x, expected);
    }

  for (i = 0; i < num; i++)
    {
      int r = rand ();
      bool eliminate = ((r % 3) == 0);
      if (eliminate)
        {
          bool doFree = ((r & 1) == 0);
          s = slaw_int16 (i);
          if (doFree)
            tort = slabu_list_remove_f (sb, s);
          else
            tort = slabu_list_remove (sb, f (s));

          if (i == n)
            {
              if (tort != SLAW_NOT_FOUND)
                OB_FATAL_ERROR_CODE (0x2030b002,
                                     "got %" OB_FMT_RETORT
                                     "d for something already removed\n",
                                     tort);
            }
          else
            {
              if (tort != OB_OK)
                OB_FATAL_ERROR_CODE (0x2030b003, "not OK");
            }
        }
    }

  OB_DIE_ON_ERROR (slabu_list_add_c (sb, "banana"));
  x = slabu_list_find_c (sb, "banana");
  if (x != slabu_count (sb) - 1)
    OB_FATAL_ERROR_CODE (0x2030b004,
                         "didn't expect banana at %" OB_FMT_64 "d"
                         "\n",
                         x);

  tort = slabu_list_remove_c (sb, "panama");
  if (tort != SLAW_NOT_FOUND)
    OB_FATAL_ERROR_CODE (0x2030b005, "unexpected panama\n");

  tort = slabu_list_remove_c (sb, "banana");
  if (tort != OB_OK)
    OB_FATAL_ERROR_CODE (0x2030b006, "not a banana\n");

  slabu_free (sb);
}

int main (int argc, char **argv)
{
  OB_DIE_ON_ERROR (OB_CHECK_ABI ());

  int i;

  for (i = 2; i <= 4096; i <<= 1)
    test (i);

  // free stuff to please valgrind
  for (i = 0; i < free_counter; i++)
    slaw_free (to_be_freed[i]);

  return EXIT_SUCCESS;
}
