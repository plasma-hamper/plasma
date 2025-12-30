
/* (c)  oblong industries */

// Some miscellaneous functions used internally by libPlasma
#include "libLoam/c/ob-sys.h"
#include "libPlasma/c/private/plasma-util.h"
#include "libPlasma/c/slaw.h"
#include "libLoam/c/ob-log.h"
#include "libPlasma/c/private/pool_tcp.h"
#include "libPlasma/c/pool.h"

#include <stdlib.h>

int private_float64_compare (unt64 f1, unt64 f2)
{
  bool neg1, neg2;

  neg1 = (bool) (f1 >> 63);
  neg2 = (bool) (f2 >> 63);

  if (neg1 && !neg2)
    return -1;
  if (!neg1 && neg2)
    return 1;

  if (neg1)
    {
      if (f1 > f2)
        return -1;
      else if (f1 < f2)
        return 1;
    }
  else
    {
      if (f1 < f2)
        return -1;
      else if (f1 > f2)
        return 1;
    }

  return 0;
}

int private_float32_compare (unt32 f1, unt32 f2)
{
  bool neg1, neg2;

  neg1 = (bool) (f1 >> 31);
  neg2 = (bool) (f2 >> 31);

  if (neg1 && !neg2)
    return -1;
  if (!neg1 && neg2)
    return 1;

  if (neg1)
    {
      if (f1 > f2)
        return -1;
      else if (f1 < f2)
        return 1;
    }
  else
    {
      if (f1 < f2)
        return -1;
      else if (f1 > f2)
        return 1;
    }

  return 0;
}

typedef union ob_various
{
  unt64 u64[1];
  unt32 u32[2];
  unt16 u16[4];
  unt8 u8[8];
  float64 f64[1];
  float32 f32[1];
} ob_various;

struct ob_numeric_builder
{
  unt64 capacity;  // in bytes
  unt64 elements;
  unt64 bitsize : 7;      // 0 (not yet specified), 8, 16, 32, or 64
  unt64 isCurrently : 4;  // OB_ARRAY, OB_MULTI, OB_VECTOR, OB_COMPLEX only
  unt64 wasEver : 7;      // above plus OB_FLOAT, OB_INT, OB_UNT
  unt64 nVects : 46;      // the number of (multi)vectors we entered and left
  ob_various u;
};

static ob_retort ensure_capacity (ob_numeric_builder **nb_inout)
{
  ob_numeric_builder *nb = *nb_inout;

  // we need one more element than we have now
  unt64 needed = (nb->elements) + 1;

  // convert elements to bytes
  needed *= (nb->bitsize / 8);

  // expand if necessary
  if (needed > nb->capacity)
    {
      unt64 newcapacity = nb->capacity * 2;
      nb = (ob_numeric_builder *) realloc (nb, sizeof (ob_numeric_builder)
                                                 + newcapacity);
      if (!nb)
        return OB_NO_MEM;
      nb->capacity = newcapacity;
      *nb_inout = nb;
    }

  return OB_OK;
}

#define INITIAL_CAPACITY 64

ob_retort ob_nb_new (ob_numeric_builder **nb_out)
{
  *nb_out = (ob_numeric_builder *) calloc (
    sizeof (ob_numeric_builder) + INITIAL_CAPACITY, 1);
  if (!*nb_out)
    return OB_NO_MEM;
  (*nb_out)->capacity = INITIAL_CAPACITY;
  // all other fields are correct being initialized to 0 by calloc
  return OB_OK;
}

ob_retort ob_nb_enter (ob_numeric_builder *nb_in, unt8 flags)
{
  if ((nb_in->isCurrently & flags) != 0)
    return SLAW_FABRICATOR_BADNESS;
  nb_in->isCurrently |= flags;
  nb_in->wasEver |= flags;
  return OB_OK;
}

ob_retort ob_nb_leave (ob_numeric_builder *nb_in, unt8 flags)
{
  if ((nb_in->isCurrently & flags) != flags)
    return SLAW_FABRICATOR_BADNESS;
  nb_in->isCurrently &= ~flags;
  if ((flags & (OB_VECTOR | OB_MULTI)) != 0)
    nb_in->nVects++;
  return OB_OK;
}

ob_retort ob_nb_push_integer (ob_numeric_builder **nb_inout, bool isUnsigned,
                              int bits, unt64 val)
{
  ob_numeric_builder *nb = *nb_inout;
  if (nb->bitsize == 0)
    nb->bitsize = bits;
  else if (nb->bitsize != bits)
    return SLAW_FABRICATOR_BADNESS;
  nb->wasEver |= (isUnsigned ? OB_UNT : OB_INT);
  ob_retort tort = ensure_capacity (nb_inout);
  if (tort < OB_OK)
    return tort;
  nb = *nb_inout;  // may have been changed by ensure_capacity
  switch (bits)
    {
      case 8:
        nb->u.u8[nb->elements++] = val;
        break;
      case 16:
        nb->u.u16[nb->elements++] = val;
        break;
      case 32:
        nb->u.u32[nb->elements++] = val;
        break;
      case 64:
        nb->u.u64[nb->elements++] = val;
        break;
      default:
        return SLAW_FABRICATOR_BADNESS;
    }
  return OB_OK;
}

ob_retort ob_nb_push_float (ob_numeric_builder **nb_inout, int bits,
                            float64 val)
{
  ob_numeric_builder *nb = *nb_inout;
  if (nb->bitsize == 0)
    nb->bitsize = bits;
  else if (nb->bitsize != bits)
    return SLAW_FABRICATOR_BADNESS;
  nb->wasEver |= OB_FLOAT;
  ob_retort tort = ensure_capacity (nb_inout);
  if (tort < OB_OK)
    return tort;
  nb = *nb_inout;  // may have been changed by ensure_capacity
  switch (bits)
    {
      case 32:
        nb->u.f32[nb->elements++] = val;
        break;
      case 64:
        nb->u.f64[nb->elements++] = val;
        break;
      default:
        return SLAW_FABRICATOR_BADNESS;
    }
  return OB_OK;
}

const void *ob_nb_pointer (const ob_numeric_builder *nb_in)
{
  return &(nb_in->u);
}

ob_retort ob_nb_dimensions (const ob_numeric_builder *nb_in, unt8 *flags_out,
                            unt8 *vsize_out, unt8 *bits, unt64 *breadth_out)
{
  if (nb_in->isCurrently != 0 || nb_in->bitsize == 0)
    return SLAW_FABRICATOR_BADNESS;
  unt8 x = nb_in->wasEver >> 4;
  unt8 y = 0;
  int i;
  for (i = 0; i < 3; i++)
    y += (1 & (x >> i));
  if (y != 1)
    return SLAW_FABRICATOR_BADNESS;  // was a mixture of int, unt, and float
  if ((nb_in->wasEver & (OB_MULTI | OB_VECTOR)) == (OB_MULTI | OB_VECTOR))
    return SLAW_FABRICATOR_BADNESS;  // can't be a vector and a multivector
  if ((nb_in->wasEver & (OB_COMPLEX | OB_MULTI)) == (OB_COMPLEX | OB_MULTI))
    return SLAW_FABRICATOR_BADNESS;  // can't be a complex and a multivector
  if (((nb_in->wasEver & (OB_MULTI | OB_VECTOR)) != 0) != (nb_in->nVects != 0))
    return SLAW_FABRICATOR_BADNESS;  // must have had vectors if we are a vector

  if ((nb_in->wasEver & OB_ARRAY) == 0 && nb_in->nVects > 1)
    // need an array to contain more than one vector
    return SLAW_FABRICATOR_BADNESS;

  unt64 elements = nb_in->elements;
  if ((nb_in->wasEver & OB_COMPLEX) != 0)
    {
      if (elements % 2 != 0)
        return SLAW_FABRICATOR_BADNESS;  // doesn't divide evenly
      elements /= 2;
    }

  if (nb_in->nVects == 0)
    {
      *vsize_out = 1;
      *breadth_out = elements;
    }
  else
    {
      if (elements % nb_in->nVects != 0)
        return SLAW_FABRICATOR_BADNESS;  // doesn't divide evenly
      elements /= nb_in->nVects;
      *breadth_out = nb_in->nVects;
      if (elements < 2 || elements > 32)
        return SLAW_FABRICATOR_BADNESS;  // invalid (multi)vector length
      *vsize_out = (unt8) elements;
    }

  *flags_out = nb_in->wasEver;
  *bits = nb_in->bitsize;
  return OB_OK;
}

bool ob_nb_done (const ob_numeric_builder *nb_in)
{
  return (nb_in->isCurrently == 0);
}

void ob_nb_free (ob_numeric_builder **nb_inout)
{
  free (*nb_inout);
  *nb_inout = NULL;
}

void ob_analyze_utf8 (const char *utf8, int64 len, bool *invalid,
                      bool *multiline)
{
  int64 i;
  int seq = 1;

  *invalid = true;
  *multiline = false;

  for (i = 0; i < len; i++)
    {
      unt8 c = utf8[i];
      if (seq > 1)
        {
          if (c >= 0x80 && c <= 0xbf)
            seq--;
          else
            return;
        }
      else
        {
          if (c == '\n')
            *multiline = true;
          else if (c >= 0x80 && c <= 0xc1)
            return;
          else if (c >= 0xc2 && c <= 0xdf)
            seq = 2;
          else if (c >= 0xe0 && c <= 0xef)
            seq = 3;
          else if (c >= 0xf0 && c <= 0xf4)
            seq = 4;
          else if (c >= 0xf5)
            return;
        }
    }

  // if we made it to the end, we're valid, unless we ended in the middle
  // of a multibyte sequence.
  *invalid = (seq > 1);
}

typedef struct old_retort
{
  ob_retort modern_value;
  unt32 first_version;
  unt32 last_version;
  ob_retort ancient_value;
} old_retort;

static old_retort old_retorts[] = {
  {OB_UNKNOWN_ERR, 0, 2, -203},
  {OB_ARGUMENT_WAS_NULL, 0, 2, -210000},
  {SLAW_CORRUPT_PROTEIN, 0, 2, -210001},
  {SLAW_CORRUPT_SLAW, 0, 2, -210002},
  {SLAW_FABRICATOR_BADNESS, 0, 2, -210003},
  {SLAW_NOT_NUMERIC, 0, 2, -210004},
  {SLAW_RANGE_ERR, 0, 2, -210005},
  {SLAW_UNIDENTIFIED_SLAW, 0, 2, -210006},
  {SLAW_WRONG_LENGTH, 0, 2, -210007},
  {SLAW_NOT_FOUND, 0, 2, -210008},
  {POOL_NO_SUCH_PROTEIN, 0, 2, -200010},  // POOL_EMPTY
  {OB_NO_MEM, 0, 2, -200530},             // POOL_MALLOC_BADTH
  {POOL_INVALID_SIZE, 0, 2, -200590},     // POOL_ARG_BADTH
  {POOL_NO_SUCH_PROTEIN, 0, 2, -200610},  // POOL_DISCARDED_PROTEIN
  {POOL_NO_SUCH_PROTEIN, 0, 2, -200620},  // POOL_FUTURE_PROTEIN
};

#define N_OLD_RETORTS (sizeof (old_retorts) / sizeof (old_retorts[0]))

ob_retort ob_old_retort_to_new (ob_retort ancient_value, unt32 version)
{
  int i;
  // shortcuts
  if (ancient_value == OB_OK || version == POOL_TCP_VERSION_CURRENT)
    return ancient_value;
  // look up in table
  for (i = 0; i < N_OLD_RETORTS; i++)
    {
      if (old_retorts[i].ancient_value == ancient_value
          && old_retorts[i].first_version <= version
          && old_retorts[i].last_version >= version)
        return old_retorts[i].modern_value;
    }
  // not found in table; assume unchanged
  return ancient_value;
}

ob_retort ob_new_retort_to_old (ob_retort modern_value, unt32 version)
{
  int i;
  // shortcuts
  if (modern_value == OB_OK || version == POOL_TCP_VERSION_CURRENT)
    return modern_value;
  // look up in table
  for (i = 0; i < N_OLD_RETORTS; i++)
    {
      if (old_retorts[i].modern_value == modern_value
          && old_retorts[i].first_version <= version
          && old_retorts[i].last_version >= version)
        return old_retorts[i].ancient_value;
    }
  // not found in table; assume unchanged
  return modern_value;
}
