
/* (c)  oblong industries */

#include "libLoam/c/ob-log.h"
#include "libPlasma/c/slaw-walk.h"
#include "libPlasma/c/slaw-string.h"
#include "libPlasma/c/protein.h"
#include "libPlasma/c/private/plasma-private.h"
#include "libPlasma/c/private/plasma-util.h"
#include <stdlib.h>
#include <string.h>


// slaw => events

/* Yes, I too can write complicated template-like macros! */

#define HANDLE_MULTIVECTOR(sz, typ, hand)                                      \
  case sz:                                                                     \
    {                                                                          \
      const typ *el = (typ *) elemPtr;                                         \
      int k;                                                                   \
      if (sizeof (typ) != bsize)                                               \
        {                                                                      \
          OB_LOG_ERROR_CODE (0x20004000,                                       \
                             "For type %s, %" OB_FMT_SIZE                      \
                             "d != %d unexpectedly!\n",                        \
                             #typ, sizeof (typ), bsize);                       \
          return SLAW_CORRUPT_SLAW;                                            \
        }                                                                      \
      for (k = 0; k < (1 << sz); k++)                                          \
        {                                                                      \
          err = handler->hand (cookie, el->coef[k], isiz);                     \
          if (err)                                                             \
            return err;                                                        \
        }                                                                      \
      break;                                                                   \
    }

#define HANDLE_VECTOR(sz, typ, numtyp, hand, ...)                              \
  case sz:                                                                     \
    {                                                                          \
      const typ *el = (typ *) elemPtr;                                         \
      numtyp nums[] = __VA_ARGS__;                                             \
      int k;                                                                   \
      if (sizeof (typ) != bsize)                                               \
        {                                                                      \
          OB_LOG_ERROR_CODE (0x20004001,                                       \
                             "For type %s, %" OB_FMT_SIZE                      \
                             "d != %d unexpectedly!\n",                        \
                             #typ, sizeof (typ), bsize);                       \
          return SLAW_CORRUPT_SLAW;                                            \
        }                                                                      \
      for (k = 0; k < sz; k++)                                                 \
        {                                                                      \
          err = handler->hand (cookie, nums[k], isiz);                         \
          if (err)                                                             \
            return err;                                                        \
        }                                                                      \
      break;                                                                   \
    }

#define HANDLE_COMPLEX_VECTOR(sz, typ, numtyp, hand, ...)                      \
  case sz:                                                                     \
    {                                                                          \
      const typ *el = (typ *) elemPtr;                                         \
      numtyp nums[] = __VA_ARGS__;                                             \
      int k;                                                                   \
      if (sizeof (typ) != bsize)                                               \
        {                                                                      \
          OB_LOG_ERROR_CODE (0x20004002,                                       \
                             "For type %s, %" OB_FMT_SIZE                      \
                             "d != %d unexpectedly!\n",                        \
                             #typ, sizeof (typ), bsize);                       \
          return SLAW_CORRUPT_SLAW;                                            \
        }                                                                      \
      for (k = 0; k < sz; k++)                                                 \
        {                                                                      \
          err = handler->begin_complex (cookie);                               \
          if (err)                                                             \
            return err;                                                        \
          err = handler->hand (cookie, nums[k], isiz);                         \
          if (err)                                                             \
            return err;                                                        \
          err = handler->hand (cookie, nums[sz + k], isiz);                    \
          if (err)                                                             \
            return err;                                                        \
          err = handler->end_complex (cookie);                                 \
          if (err)                                                             \
            return err;                                                        \
        }                                                                      \
      break;                                                                   \
    }

#define HANDLE_NUMERIC(typ, largetyp, hand)                                    \
  {                                                                            \
    const int isiz = 8 * sizeof (typ);                                         \
    if (isMVec)                                                                \
      {                                                                        \
        switch (vecLen)                                                        \
          {                                                                    \
            HANDLE_MULTIVECTOR (2, m2##typ, hand);                             \
            HANDLE_MULTIVECTOR (3, m3##typ, hand);                             \
            HANDLE_MULTIVECTOR (4, m4##typ, hand);                             \
            HANDLE_MULTIVECTOR (5, m5##typ, hand);                             \
            default:                                                           \
              OB_LOG_ERROR_CODE (0x20004003, "Multivector claims length %d\n", \
                                 vecLen);                                      \
              return SLAW_CORRUPT_SLAW;                                        \
          }                                                                    \
      }                                                                        \
    else if (isComplex)                                                        \
      {                                                                        \
        switch (vecLen)                                                        \
          {                                                                    \
            HANDLE_COMPLEX_VECTOR (1, typ##c, largetyp, hand,                  \
                                   {el->re, el->im});                          \
            HANDLE_COMPLEX_VECTOR (2, v2##typ##c, largetyp, hand,              \
                                   {el->x.re, el->y.re, el->x.im, el->y.im});  \
            HANDLE_COMPLEX_VECTOR (                                            \
              3, v3##typ##c, largetyp, hand,                                   \
              {el->x.re, el->y.re, el->z.re, el->x.im, el->y.im, el->z.im});   \
            HANDLE_COMPLEX_VECTOR (4, v4##typ##c, largetyp, hand,              \
                                   {el->x.re, el->y.re, el->z.re, el->w.re,    \
                                    el->x.im, el->y.im, el->z.im, el->w.im});  \
            default:                                                           \
              OB_LOG_ERROR_CODE (0x20004004, "Vector claims length %d\n",      \
                                 vecLen);                                      \
              return SLAW_CORRUPT_SLAW;                                        \
          }                                                                    \
      }                                                                        \
    else                                                                       \
      {                                                                        \
        switch (vecLen)                                                        \
          {                                                                    \
            HANDLE_VECTOR (1, typ, largetyp, hand, {*el});                     \
            HANDLE_VECTOR (2, v2##typ, largetyp, hand, {el->x, el->y});        \
            HANDLE_VECTOR (3, v3##typ, largetyp, hand, {el->x, el->y, el->z}); \
            HANDLE_VECTOR (4, v4##typ, largetyp, hand,                         \
                           {el->x, el->y, el->z, el->w});                      \
            default:                                                           \
              OB_LOG_ERROR_CODE (0x20004005, "Vector claims length %d\n",      \
                                 vecLen);                                      \
              return SLAW_CORRUPT_SLAW;                                        \
          }                                                                    \
      }                                                                        \
  }

static ob_retort protein_walk_versioned (void *cookie,
                                         const slaw_handler *handler,
                                         bprotein victim, const slaw_vfuncs *v);

ob_retort slaw_walk_versioned (void *cookie, const slaw_handler *handler,
                               bslaw victim, const slaw_vfuncs *v)
{
  ob_retort err;
  bool isArray;
  bool isMVec;
  bool isVector;
  bool isComplex;
  int vecLen;
  unt64 arrayLen;
  const void *elemPtr;
  int bsize;
  unt64 j;
  bslaw element;
  bool isFloat, isUnsigned, isWide, isStumpy;

  switch (v->vslaw_gettype (victim))
    {
      case SLAW_TYPE_NULL:
        return SLAW_CORRUPT_SLAW;
      case SLAW_TYPE_CONS:
        {
          err = handler->begin_cons (cookie);
          if (err)
            return err;
          err = slaw_walk_versioned (cookie, handler,
                                     v->vslaw_cons_emit_car (victim), v);
          if (err)
            return err;
          err = slaw_walk_versioned (cookie, handler,
                                     v->vslaw_cons_emit_cdr (victim), v);
          if (err)
            return err;
          return handler->end_cons (cookie);
        }
      case SLAW_TYPE_LIST:
        {
          int64 length = -1;
          if (v->vslaw_list_length)
            length = v->vslaw_list_length (victim);
          bool isMap = v->vslaw_is_map (victim);
          err =
            (isMap ? handler->begin_map : handler->begin_list) (cookie, length);
          if (err)
            return err;
          for (element = v->vslaw_list_emit_first (victim); element != NULL;
               element = v->vslaw_list_emit_next (victim, element))
            {
              err = slaw_walk_versioned (cookie, handler, element, v);
              if (err)
                return err;
            }
          return (isMap ? handler->end_map : handler->end_list) (cookie);
        }
      case SLAW_TYPE_STRING:
        {
          const char *str = v->vslaw_string_emit (victim);
          const int64 len = v->vslaw_string_emit_length (victim);
          if (len < 0 || !str)
            return SLAW_CORRUPT_SLAW;
          return handler->handle_string (cookie, str, len);
        }
      case SLAW_TYPE_PROTEIN:
        return protein_walk_versioned (cookie, handler, victim, v);
      case SLAW_TYPE_NIL:
        return handler->handle_nil (cookie);
      case SLAW_TYPE_BOOLEAN:
        {
          const bool *boolptr = v->vslaw_boolean_emit (victim);
          if (!boolptr)
            return SLAW_CORRUPT_SLAW;
          return handler->handle_unt (cookie, *boolptr, 1);
        }
      case SLAW_TYPE_NUMERIC:
        break;  // rest of the function is for numeric
      default:
        return SLAW_UNIDENTIFIED_SLAW;
    }

  isArray = v->vslaw_is_numeric_array (victim);
  isMVec = v->vslaw_is_numeric_multivector (victim);
  isVector = v->vslaw_is_numeric_vector (victim);
  isComplex = v->vslaw_is_numeric_complex (victim);

  arrayLen = (isArray ? v->vslaw_numeric_array_count (victim) : 1);
  vecLen =
    ((isVector || isMVec) ? v->vslaw_numeric_vector_dimension (victim) : 1);
  bsize = v->vslaw_numeric_unit_bsize (victim);

  isFloat = v->vslaw_is_numeric_float (victim);
  isUnsigned = v->vslaw_is_numeric_unt (victim);
  bool is16 = v->vslaw_is_numeric_16 (victim);
  isWide = is16 || v->vslaw_is_numeric_64 (victim);
  isStumpy = is16 || v->vslaw_is_numeric_8 (victim);

  if (isMVec && isComplex) /* not a legal combination */
    return SLAW_UNIDENTIFIED_SLAW;

  if (arrayLen == 0)
    {
      return handler->handle_empty_array (
        cookie, vecLen, isMVec, isComplex, isUnsigned, isFloat,
        (isStumpy ? (isWide ? 16 : 8) : (isWide ? 64 : 32)));
    }

  if (isArray)
    elemPtr = v->vslaw_numeric_array_emit (victim);
  else
    elemPtr = v->vslaw_numeric_nonarray_emit (victim);

  if (isArray)
    {
      int bits_hint = 8 * bsize;
      if (isVector || isComplex || isMVec)
        bits_hint = -1;
      err = handler->begin_array (cookie, arrayLen, bits_hint);
      if (err)
        return err;
    }

  for (j = 0; j < arrayLen; j++)
    {
      if (isVector)
        {
          err = handler->begin_vector (cookie, vecLen);
          if (err)
            return err;
        }
      else if (isMVec)
        {
          err = handler->begin_multivector (cookie, vecLen);
          if (err)
            return err;
        }

      if (isFloat)
        {
          if (isUnsigned || isStumpy)
            return SLAW_UNIDENTIFIED_SLAW;
          if (isWide)
            {
              HANDLE_NUMERIC (float64, float64, handle_float);
            }
          else
            {
              HANDLE_NUMERIC (float32, float64, handle_float);
            }
        }
      else
        {
          if (isUnsigned)
            {
              if (isStumpy)
                {
                  if (isWide)
                    {
                      HANDLE_NUMERIC (unt16, unt64, handle_unt);
                    }
                  else
                    {
                      HANDLE_NUMERIC (unt8, unt64, handle_unt);
                    }
                }
              else
                {
                  if (isWide)
                    {
                      HANDLE_NUMERIC (unt64, unt64, handle_unt);
                    }
                  else
                    {
                      HANDLE_NUMERIC (unt32, unt64, handle_unt);
                    }
                }
            }
          else
            {
              if (isStumpy)
                {
                  if (isWide)
                    {
                      HANDLE_NUMERIC (int16, int64, handle_int);
                    }
                  else
                    {
                      HANDLE_NUMERIC (int8, int64, handle_int);
                    }
                }
              else
                {
                  if (isWide)
                    {
                      HANDLE_NUMERIC (int64, int64, handle_int);
                    }
                  else
                    {
                      HANDLE_NUMERIC (int32, int64, handle_int);
                    }
                }
            }
        }

      if (isVector)
        {
          err = handler->end_vector (cookie);
          if (err)
            return err;
        }
      else if (isMVec)
        {
          err = handler->end_multivector (cookie);
          if (err)
            return err;
        }

      elemPtr = (void *) (bsize + (byte *) elemPtr);
    }

  if (isArray)
    return handler->end_array (cookie);
  else
    return 0;
}

static ob_retort protein_walk_versioned (void *cookie,
                                         const slaw_handler *handler,
                                         bprotein victim, const slaw_vfuncs *v)
{
  bslaw cole;
  ob_retort err;
  int64 rude_len;
  const void *rude;

  if (v->vprotein_is_nonstandard (victim))
    return handler->handle_nonstd_protein (cookie, (void *) victim,
                                           v->vprotein_len (victim));

  err = handler->begin_protein (cookie);
  if (err)
    return err;

  cole = v->vprotein_descrips (victim);
  if (cole)
    {
      err = handler->begin_descrips (cookie);
      if (err)
        return err;
      err = slaw_walk_versioned (cookie, handler, cole, v);
      if (err)
        return err;
      err = handler->end_descrips (cookie);
      if (err)
        return err;
    }

  cole = v->vprotein_ingests (victim);
  if (cole)
    {
      err = handler->begin_ingests (cookie);
      if (err)
        return err;
      err = slaw_walk_versioned (cookie, handler, cole, v);
      if (err)
        return err;
      err = handler->end_ingests (cookie);
      if (err)
        return err;
    }

  rude = v->vprotein_rude (victim, &rude_len);
  if (rude_len != 0)
    {
      err = handler->handle_rude_data (cookie, rude, rude_len);
      if (err)
        return err;
    }

  return handler->end_protein (cookie);
}

static ob_retort v2build (slaw *s, const slaw_vfuncs *v)
{
  slaw_fabricator *sf = slaw_fabricator_new ();
  if (!sf)
    return OB_NO_MEM;
  ob_retort err = slaw_walk_versioned (sf, &slaw_fabrication_handler, *s, v);
  if (err < OB_OK)
    {
      slaw_fabricator_free (sf);
      return err;
    }
  v->vslaw_free (*s);
  *s = sf->result;
  sf->result = NULL;
  slaw_fabricator_free (sf);
  return OB_OK;
}

const slaw_vfuncs slaw_v2funcs = {
  slaw_gettype,
  slaw_list_emit_first,
  slaw_list_emit_next,
  slaw_is_map,
  slaw_is_numeric_array,
  slaw_is_numeric_multivector,
  slaw_is_numeric_vector,
  slaw_is_numeric_complex,
  slaw_is_numeric_float,
  slaw_is_numeric_int,
  slaw_is_numeric_unt,
  slaw_is_numeric_8,
  slaw_is_numeric_16,
  slaw_is_numeric_32,
  slaw_is_numeric_64,
  slaw_numeric_vector_dimension,
  slaw_numeric_unit_bsize,
  slaw_numeric_array_count,
  slaw_numeric_array_emit,
  slaw_numeric_nonarray_emit,
  slaw_cons_emit_car,
  slaw_cons_emit_cdr,
  slaw_boolean_emit,
  slaw_string_emit,
  slaw_string_emit_length,
  protein_is_nonstandard,
  protein_len,
  protein_descrips,
  protein_ingests,
  protein_rude,
  slaw_free,
  protein_fix_endian,
  slaw_swap,
  v2build,
  NULL,  // XXX: currently we handle this implicitly
  NULL   // slaw_list_len
};

ob_retort slaw_walk (void *cookie, const slaw_handler *handler, bslaw victim)
{
  if (!handler || !victim)
    return OB_ARGUMENT_WAS_NULL;
  return slaw_walk_versioned (cookie, handler, victim, &slaw_v2funcs);
}

// events => slaw

typedef struct future_protein
{
  slaw descrips;
  slaw ingests;
  void *rude;
  int64 rudeLen;
} future_protein;

struct slabu_chain
{
  slabu_chain *next;

  // exactly one of these will be non-NULL; sort of like a union
  // XXX: so maybe I should actually be using a union instead!
  slabu *sb;
  future_protein *pb;
  ob_numeric_builder *nb;

  // only meaningful if pb is non-NULL
  bool doing_ingests;
};

slaw_fabricator *slaw_fabricator_new (void)
{
  slaw_fabricator *ret = (slaw_fabricator *) malloc (sizeof (slaw_fabricator));
  if (ret != NULL)
    memset (ret, 0, sizeof (slaw_fabricator));
  return ret;
}

static ob_retort sf_push (slaw_fabricator *sf)
{
  slabu_chain *sc = (slabu_chain *) malloc (sizeof (slabu_chain));
  if (sc == NULL)
    return OB_NO_MEM;
  memset (sc, 0, sizeof (slabu_chain));
  sc->sb = slabu_new ();
  if (sc->sb == NULL)
    {
      free (sc);
      return OB_NO_MEM;
    }
  sc->next = sf->stack;
  sf->stack = sc;
  return OB_OK;
}

static ob_retort sf_push_probu (slaw_fabricator *sf)
{
  slabu_chain *sc = (slabu_chain *) malloc (sizeof (slabu_chain));
  if (sc == NULL)
    return OB_NO_MEM;
  memset (sc, 0, sizeof (slabu_chain));
  sc->pb = (future_protein *) calloc (1, sizeof (future_protein));
  if (sc->pb == NULL)
    {
      free (sc);
      return OB_NO_MEM;
    }
  sc->next = sf->stack;
  sf->stack = sc;
  return OB_OK;
}

static ob_retort sf_push_nb (slaw_fabricator *sf)
{
  slabu_chain *sc = (slabu_chain *) malloc (sizeof (slabu_chain));
  if (sc == NULL)
    return OB_NO_MEM;
  memset (sc, 0, sizeof (slabu_chain));
  ob_retort tort = ob_nb_new (&sc->nb);
  if (tort < OB_OK)
    {
      free (sc);
      return tort;
    }
  sc->next = sf->stack;
  sf->stack = sc;
  return OB_OK;
}

static void sf_pop (slaw_fabricator *sf)
{
  if (sf->stack != NULL)
    {
      slabu_chain *tmp;
      if (sf->stack->sb)
        slabu_free (sf->stack->sb);
      else if (sf->stack->pb)
        {
          slaw_free (sf->stack->pb->descrips);
          slaw_free (sf->stack->pb->ingests);
          free (sf->stack->pb->rude);
          free (sf->stack->pb);
        }
      else if (sf->stack->nb)
        ob_nb_free (&sf->stack->nb);
      else
        OB_FATAL_BUG_CODE (0x20004006, "shouldn't happen\n");
      tmp = sf->stack->next;
      free (sf->stack);
      sf->stack = tmp;
    }
}

void slaw_fabricator_free (slaw_fabricator *sf)
{
  while (sf->stack)
    sf_pop (sf);
  if (sf->result)
    slaw_free (sf->result);
  free (sf);
}

static ob_retort sf_add_slaw (slaw_fabricator *sf, slaw cole)
{
  ob_retort err = OB_OK;
  if (sf->stack)
    {
      if (sf->stack->sb)
        ob_err_accum (&err, slabu_list_add_x (sf->stack->sb, cole));
      else if (sf->stack->nb)
        err = SLAW_FABRICATOR_BADNESS;
      else if (sf->stack->doing_ingests)
        sf->stack->pb->ingests = cole;
      else
        sf->stack->pb->descrips = cole;
    }
  else
    sf->result = cole;

  return err;
}

static ob_retort sf_enter (slaw_fabricator *sf, unt8 flags)
{
  ob_retort tort;
  if ((!sf->stack || !sf->stack->nb) && OB_OK > (tort = sf_push_nb (sf)))
    return tort;
  return ob_nb_enter (sf->stack->nb, flags);
}

static ob_retort sf_leave (slaw_fabricator *sf, unt8 leave_flags)
{
  if (!sf->stack->nb)
    return SLAW_FABRICATOR_BADNESS;
  ob_retort tort = ob_nb_leave (sf->stack->nb, leave_flags);
  if (tort < OB_OK)
    return tort;
  if (ob_nb_done (sf->stack->nb))
    {
      unt8 flags, vsize, bits;
      unt64 breadth;
      tort = ob_nb_dimensions (sf->stack->nb, &flags, &vsize, &bits, &breadth);
      if (tort < OB_OK)
        return tort;
      const void *ptr = ob_nb_pointer (sf->stack->nb);
      slaw_oct ilk = SLAW_NUMERIC_ILK;
      if ((flags & OB_MULTI) != 0)
        {
          switch (vsize)
            {
              case 4:
                ilk |= SLAW_NUMERIC_MVEC2_BITS;
                break;
              case 8:
                ilk |= SLAW_NUMERIC_MVEC3_BITS;
                break;
              case 16:
                ilk |= SLAW_NUMERIC_MVEC4_BITS;
                break;
              case 32:
                ilk |= SLAW_NUMERIC_MVEC5_BITS;
                break;
              default:
                return SLAW_FABRICATOR_BADNESS;
            }
        }
      else if (vsize > 4)
        return SLAW_FABRICATOR_BADNESS;
      else
        ilk |= ((vsize - OB_CONST_U64 (1)) << SLAW_NUMERIC_VEC_SHIFTY);

      bool isComplex = false;
      if ((flags & OB_COMPLEX) != 0)
        {
          isComplex = true;
          ilk |= SLAW_NUMERIC_COMPLEX_FLAG;
        }

      if ((flags & OB_UNT) != 0)
        ilk |= SLAW_NUMERIC_UNSIGNED_FLAG;

      if ((flags & OB_FLOAT) != 0)
        ilk |= SLAW_NUMERIC_FLOAT_FLAG;

      unt64 sz;
      switch (bits)
        {
          case 8:
            sz = 0;
            break;
          case 16:
            sz = 1;
            break;
          case 32:
            sz = 2;
            break;
          case 64:
            sz = 3;
            break;
          default:
            return SLAW_FABRICATOR_BADNESS;
        }

      ilk |= (sz << SLAW_NUMERIC_SIZE_SHIFTY);

      unt32 blen = (bits / 8) * vsize * (isComplex ? 2 : 1);
      slaw cole;
      byte *p;

      if ((flags & OB_ARRAY) != 0)
        {
          cole = slaw_numeric_array_raw (ilk, blen, breadth);
          if (!cole)
            return OB_NO_MEM;
          p = (byte *) (cole + 1);
        }
      else
        {
          unt64 new_bsize = blen;
          unt64 new_osize = (((new_bsize) >> 3) + (((new_bsize) % 8) ? 1 : 0));
          bool wee = new_bsize <= 4;
          if (wee)
            new_osize = 0;

          ilk >>= SLAW_NUMERIC_BSIZE_SHIFTY;
          ilk &= ~SLAW_NUMERIC_UNIT_BSIZE_MASK;
          ilk |= (new_bsize - 1);
          ilk <<= SLAW_NUMERIC_BSIZE_SHIFTY;
          cole = slaw_alloc (1 + new_osize);
          if (!cole)
            return OB_NO_MEM;
          cole->o = ilk;
          if (wee)
            p = SLAW_SPECIAL_BYTES (cole, new_bsize);
          else
            p = (byte *) (cole + 1);
          byte *last = (byte *) (cole + 1 + new_osize);
          if (!wee)
            memset (p, 0, last - p); /* initialize padding */
        }

      memcpy (p, ptr, blen * breadth);
      sf_pop (sf);
      return sf_add_slaw (sf, cole);
    }
  return OB_OK;
}

static ob_retort sf_begin_cons (void *cookie)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  return sf_push (sf);
}

static ob_retort sf_end_cons (void *cookie)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  slaw cole;
  if (!sf->stack || !sf->stack->sb || slabu_count (sf->stack->sb) != 2)
    return SLAW_FABRICATOR_BADNESS;
  cole = slaw_cons (sf->stack->sb->esses[0], sf->stack->sb->esses[1]);
  if (!cole)
    return OB_NO_MEM;
  sf_pop (sf);
  return sf_add_slaw (sf, cole);
}

static ob_retort sf_begin_list (void *cookie, int64 unused)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  ob_retort err = sf_push (sf);
  return err;
}

static ob_retort sf_end_list (void *cookie)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  slaw cole;
  if (!sf->stack || !sf->stack->sb)
    return SLAW_FABRICATOR_BADNESS;
  cole = slaw_list (sf->stack->sb);
  if (!cole)
    return OB_NO_MEM;
  sf_pop (sf);
  return sf_add_slaw (sf, cole);
}

static ob_retort sf_end_map (void *cookie)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  slaw cole;
  if (!sf->stack || !sf->stack->sb)
    return SLAW_FABRICATOR_BADNESS;
  cole = slaw_map (sf->stack->sb);
  if (!cole)
    return OB_NO_MEM;
  sf_pop (sf);
  return sf_add_slaw (sf, cole);
}

static ob_retort sf_begin_array (void *cookie, int64 unused, int notused)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  return sf_enter (sf, OB_ARRAY);
}

static ob_retort sf_end_array (void *cookie)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  return sf_leave (sf, OB_ARRAY);
}

static ob_retort sf_begin_multivector (void *cookie, int64 unused)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  return sf_enter (sf, OB_MULTI);
}

static ob_retort sf_end_multivector (void *cookie)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  return sf_leave (sf, OB_MULTI);
}

static ob_retort sf_begin_vector (void *cookie, int64 unused)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  return sf_enter (sf, OB_VECTOR);
}

static ob_retort sf_end_vector (void *cookie)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  return sf_leave (sf, OB_VECTOR);
}

static ob_retort sf_begin_complex (void *cookie)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  return sf_enter (sf, OB_COMPLEX);
}

static ob_retort sf_end_complex (void *cookie)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  return sf_leave (sf, OB_COMPLEX);
}

static ob_retort sf_handle_nil (void *cookie)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  slaw cole = slaw_nil ();
  if (!cole)
    return OB_NO_MEM;
  return sf_add_slaw (sf, cole);
}

static ob_retort sf_handle_string (void *cookie, const char *utf8, int64 len)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  slaw cole = slaw_string_from_substring (utf8, len);
  if (!cole)
    return OB_NO_MEM;
  return sf_add_slaw (sf, cole);
}

static ob_retort sf_handle_int (void *cookie, int64 val, int bits)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  slaw cole;

  if (sf->stack && sf->stack->nb)
    return ob_nb_push_integer (&sf->stack->nb, false, bits, val);

  switch (bits)
    {
      case 8:
        cole = slaw_int8 ((int8) val);
        break;
      case 16:
        cole = slaw_int16 ((int16) val);
        break;
      case 32:
        cole = slaw_int32 ((int32) val);
        break;
      case 64:
        cole = slaw_int64 (val);
        break;
      default:
        return SLAW_FABRICATOR_BADNESS;
    }

  if (!cole)
    return OB_NO_MEM;
  return sf_add_slaw (sf, cole);
}

static ob_retort sf_handle_unt (void *cookie, unt64 val, int bits)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  slaw cole;

  if (sf->stack && sf->stack->nb)
    return ob_nb_push_integer (&sf->stack->nb, true, bits, val);

  switch (bits)
    {
      case 1:
        cole = slaw_boolean ((int) val);
        break;
      case 8:
        cole = slaw_unt8 ((unt8) val);
        break;
      case 16:
        cole = slaw_unt16 ((unt16) val);
        break;
      case 32:
        cole = slaw_unt32 ((unt32) val);
        break;
      case 64:
        cole = slaw_unt64 (val);
        break;
      default:
        return SLAW_FABRICATOR_BADNESS;
    }

  if (!cole)
    return OB_NO_MEM;
  return sf_add_slaw (sf, cole);
}

static ob_retort sf_handle_float (void *cookie, float64 val, int bits)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  slaw cole;

  if (sf->stack && sf->stack->nb)
    return ob_nb_push_float (&sf->stack->nb, bits, val);

  switch (bits)
    {
      case 32:
        cole = slaw_float32 ((float32) val);
        break;
      case 64:
        cole = slaw_float64 (val);
        break;
      default:
        return SLAW_FABRICATOR_BADNESS;
    }

  if (!cole)
    return OB_NO_MEM;
  return sf_add_slaw (sf, cole);
}

static ob_retort sf_handle_empty_array (void *cookie, int vecsize, bool isMVec,
                                        bool isComplex, bool isUnsigned,
                                        bool isFloat, int bits)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  slaw cole;
  unt32 blen;
  slaw_oct ilk = SLAW_NUMERIC_ILK
                 | ((vecsize - OB_CONST_U64 (1)) << SLAW_NUMERIC_VEC_SHIFTY);

  if (bits < 8 || bits > 64 || (bits & (bits - 1)) != 0
      || (isFloat && bits < 32))
    return SLAW_FABRICATOR_BADNESS;

  if (isMVec)
    {
      ilk = (SLAW_NUMERIC_ILK | SLAW_NUMERIC_MVEC_FLAG
             | ((vecsize - OB_CONST_U64 (2)) << SLAW_NUMERIC_VEC_SHIFTY));
      if (isComplex || vecsize < 2 || vecsize > 5)
        return SLAW_FABRICATOR_BADNESS;
      vecsize = (1 << vecsize);
    }
  else if (vecsize < 1 || vecsize > 4)
    return SLAW_FABRICATOR_BADNESS;

  if (isComplex)
    ilk |= SLAW_NUMERIC_COMPLEX_FLAG;

  if (isUnsigned)
    ilk |= SLAW_NUMERIC_UNSIGNED_FLAG;

  if (isFloat)
    ilk |= SLAW_NUMERIC_FLOAT_FLAG;

  unt64 sz;
  switch (bits)
    {
      case 8:
        sz = 0;
        break;
      case 16:
        sz = 1;
        break;
      case 32:
        sz = 2;
        break;
      case 64:
        sz = 3;
        break;
      default:
        return SLAW_FABRICATOR_BADNESS;
    }

  ilk |= (sz << SLAW_NUMERIC_SIZE_SHIFTY);

  blen = (bits / 8) * vecsize * (isComplex ? 2 : 1);

  cole = slaw_numeric_array_raw (ilk, blen, 0);
  if (!cole)
    return OB_NO_MEM;
  return sf_add_slaw (sf, cole);
}

static ob_retort sf_begin_protein (void *cookie)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  return sf_push_probu (sf);
}

static ob_retort sf_end_protein (void *cookie)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  protein p;
  if (!sf->stack || !sf->stack->pb)
    return SLAW_FABRICATOR_BADNESS;
  p = protein_from_llr (sf->stack->pb->descrips, sf->stack->pb->ingests,
                        sf->stack->pb->rude, sf->stack->pb->rudeLen);
  if (!p)
    return OB_NO_MEM;
  sf_pop (sf);
  return sf_add_slaw (sf, p);
}

static ob_retort sf_handle_nonstd_protein (void *cookie, const void *pp,
                                           int64 len)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  protein p = (protein) pp;
  unt64 p_len = protein_len (p);
  if (p_len != len)
    {
      OB_LOG_ERROR_CODE (0x20004007,
                         "nonstandard protein is %" OB_FMT_64 "u"
                         " but thought it was %" OB_FMT_64 "u\n",
                         p_len, len);
      return SLAW_CORRUPT_SLAW;
    }
  return sf_add_slaw (sf, protein_dup (p));
}

static ob_retort sf_begin_descrips (void *cookie)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  if (!sf->stack || !sf->stack->pb)
    return SLAW_FABRICATOR_BADNESS;
  sf->stack->doing_ingests = false;

  return OB_OK;
}

static ob_retort sf_end_descips (void *cookie)
{
  return OB_OK;
}

static ob_retort sf_begin_ingests (void *cookie)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  if (!sf->stack || !sf->stack->pb)
    return SLAW_FABRICATOR_BADNESS;
  sf->stack->doing_ingests = true;

  return OB_OK;
}

static ob_retort sf_end_ingests (void *cookie)
{
  return OB_OK;
}

static ob_retort sf_handle_rude_data (void *cookie, const void *d, int64 len)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  if (!sf->stack || !sf->stack->pb || sf->stack->pb->rude)
    return SLAW_FABRICATOR_BADNESS;

  sf->stack->pb->rudeLen = len;
  sf->stack->pb->rude = malloc (len);
  if (!sf->stack->pb->rude)
    return OB_NO_MEM;

  memcpy (sf->stack->pb->rude, d, len);

  return OB_OK;
}

const slaw_handler slaw_fabrication_handler = {
  sf_begin_cons,
  sf_end_cons,
  sf_begin_list, /* same function to begin list or map */
  sf_end_map,
  sf_begin_list,
  sf_end_list,
  sf_begin_array,
  sf_end_array,
  sf_begin_multivector,
  sf_end_multivector,
  sf_begin_vector,
  sf_end_vector,
  sf_begin_complex,
  sf_end_complex,
  sf_handle_nil,
  sf_handle_string,
  sf_handle_int,
  sf_handle_unt,
  sf_handle_float,
  sf_handle_empty_array,
  sf_begin_protein,
  sf_end_protein,
  sf_handle_nonstd_protein,
  sf_begin_descrips,
  sf_end_descips,
  sf_begin_ingests,
  sf_end_ingests,
  sf_handle_rude_data};
