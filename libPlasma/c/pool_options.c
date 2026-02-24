
/* (c)  oblong industries */

#include "libPlasma/c/slaw-string.h"
#include "libPlasma/c/slaw-path.h"

#include <stdlib.h>

#include "pool_options.h"
#include "slaw-coerce.h"

static const char FILE_SIZE_OPTION_NAME[] = "size";
static const char INDEX_CAPACITY_OPTION_NAME[] = "index-capacity";
static const char TOC_CAPACITY_OPTION_NAME[] = "toc-capacity";
static const unt64 DEFAULT_CAPACITY = 0;

protein mmap_pool_options (unt64 size)
{
  return toc_mmap_pool_options (size, DEFAULT_CAPACITY);
}

protein toc_mmap_pool_options (unt64 size, unt64 cap)
{
  slaw m =
    slaw_map_inline_cf (FILE_SIZE_OPTION_NAME, slaw_unt64 (size),
                        // for backwards compatibility
                        INDEX_CAPACITY_OPTION_NAME, slaw_unt64 (cap),
                        // the new name is preferred
                        TOC_CAPACITY_OPTION_NAME, slaw_unt64 (cap), NULL);
  return m ? protein_from_ff (NULL, m) : NULL;
}

protein small_mmap_pool_options (void)
{
  return mmap_pool_options (POOL_SIZE_SMALL);
}

protein medium_mmap_pool_options (void)
{
  return mmap_pool_options (POOL_SIZE_MEDIUM);
}

protein large_mmap_pool_options (void)
{
  return mmap_pool_options (POOL_SIZE_LARGE);
}

protein obscene_mmap_pool_options (void)
{
  return mmap_pool_options (POOL_SIZE_HUGE);
}

unt64 mmap_pool_options_file_size (bprotein options)
{
  return slaw_path_get_unt64 (options, FILE_SIZE_OPTION_NAME, 0);
}

unt64 mmap_pool_options_toc_capacity (bprotein options)
{
  const unt64 old_way =
    slaw_path_get_unt64 (options, INDEX_CAPACITY_OPTION_NAME, 0);
  return slaw_path_get_unt64 (options, TOC_CAPACITY_OPTION_NAME, old_way);
}
