
/* (c)  oblong industries */

#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-vers.h"
#include "libPlasma/c/c-plasma.h"
#include <stdlib.h>
#include <stdio.h>

static pool_hose hose;

int main (int argc, char **argv)
{
  OB_DIE_ON_ERROR (OB_CHECK_ABI ());

  const char *poolname = argv[1];
  protein pro = protein_from_ff (
    NULL, slaw_map_inline_cf ("size", slaw_int64 (128 * KILOBYTE), "resizable",
                              slaw_boolean (true), NULL));
  OB_DIE_ON_ERROR (pool_participate_creatingly (poolname, "mmap", &hose, pro));
  slaw releaseOptionSlaw = slaw_map_inline_ff (slaw_string ("auto-dispose"),
                                               slaw_boolean (true), NULL);
  OB_DIE_ON_ERROR (pool_change_options (hose, releaseOptionSlaw));
  slaw_free (releaseOptionSlaw);
  protein_free (pro);
  // exit without withdrawing hose
  // (hose remains in a global variable to avoid valgrind counting it
  // as a leak)
  return EXIT_SUCCESS;
}
