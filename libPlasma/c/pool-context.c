
/* (c)  oblong industries */

#include "libLoam/c/ob-log.h"

#include "libPlasma/c/slaw.h"
#include "libPlasma/c/protein.h"
#include "libPlasma/c/pool.h"
#include "libPlasma/c/private/pool_impl.h"

#include <stdlib.h>

ob_retort pool_new_context (pool_context *ctx)
{
  if (!ctx)
    return OB_ARGUMENT_WAS_NULL;

  pool_context new_ctx = (pool_context) calloc (1, sizeof (*new_ctx));
  if (new_ctx)
    {
      *ctx = new_ctx;
      new_ctx->magic = POOL_CTX_MAGIC;
      return OB_OK;
    }
  else
    return OB_NO_MEM;
}

void pool_validate_context (pool_context ctx, const char *where)
{
  if (ctx) /* allow NULL contexts */
    {
      const unt32 magic = ctx->magic;
      if (magic != POOL_CTX_MAGIC)
        OB_FATAL_BUG_CODE (0x20111000,
                           "in %s: %p is %s\n"
                           "magic is 0x%08x; should be 0x%08x\n",
                           where, ctx,
                           magic == POOL_CTX_FREED
                             ? "an already-freed pool context"
                             : "not a pool context (presumed "
                               "memory corruption)",
                           magic, POOL_CTX_MAGIC);
    }
}

void pool_free_context (pool_context ctx)
{
  if (!ctx)
    return;

  pool_validate_context (ctx, "pool_free_context");

  Free_Slaw (ctx->connection_options);
  ctx->magic = POOL_CTX_FREED;

  free (ctx);
}

ob_retort pool_ctx_set_options (pool_context ctx, bslaw opts)
{
  if (!ctx)
    return OB_ARGUMENT_WAS_NULL;

  pool_validate_context (ctx, "pool_ctx_set_options");

  if (slaw_is_protein (opts))
    opts = protein_ingests (opts);

  slaw oldie = ctx->connection_options;
  if (oldie)
    ctx->connection_options = slaw_maps_merge (oldie, opts, NULL);
  else
    ctx->connection_options = slaw_dup (opts);

  if (ctx->connection_options)
    {
      slaw_free (oldie);
      return OB_OK;
    }
  else
    {
      ctx->connection_options = oldie;
      return OB_NO_MEM;
    }
}

bslaw pool_ctx_get_options (pool_context ctx)
{
  if (!ctx)
    return NULL;

  pool_validate_context (ctx, "pool_ctx_get_options");

  return ctx->connection_options;
}
