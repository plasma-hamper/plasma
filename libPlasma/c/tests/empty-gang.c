
/* (c)  oblong industries */

// see what happens if we use a gang with nothing in it

#include "libLoam/c/ob-log.h"
#include "libPlasma/c/pool.h"

#include <stdlib.h>

int main (int argc, char **argv)
{
  pool_gang jets;

  OB_DIE_ON_ERROR (pool_new_gang (&jets));
  ob_retort err = pool_next_multi (jets, NULL, NULL, NULL, NULL);
  if (err != POOL_EMPTY_GANG)
    OB_FATAL_ERROR_CODE (
      0x20404000, "pool_next_multi: Got %s but expected %s\n",
      ob_error_string (err), ob_error_string (POOL_EMPTY_GANG));
  err = pool_await_next_multi (jets, 0, NULL, NULL, NULL, NULL);
  if (err != POOL_EMPTY_GANG)
    OB_FATAL_ERROR_CODE (
      0x20404001, "pool_await_next_multi: Got %s but expected %s\n",
      ob_error_string (err), ob_error_string (POOL_EMPTY_GANG));
  OB_DIE_ON_ERROR (pool_disband_gang (jets, true));

  return EXIT_SUCCESS;
}
