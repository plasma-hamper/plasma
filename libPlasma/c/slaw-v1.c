
/* (c)  oblong industries */

#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-retorts.h"
#include "libLoam/c/ob-util.h"
#include "libPlasma/c/slaw.h"
#include "libPlasma/c/slaw-walk.h"
#include "libPlasma/c/slaw-io.h"
#include "libPlasma/c/private/private-versioning.h"
#include "libPlasma/c/private/plasma-util.h"

#include <stdlib.h>
#include <string.h>

typedef unt32 slaw_quad;


struct _slaw
{
  slaw_quad q;
};

struct _slaw_bundle
{
  slaw *esses;
  bool *effs; /* for each ess, whether to free it */
  int64 numEsses, maxNumEsses;
  bool obeysMapInvariant; /* all cons, no duplicates */
};



//



#define SLAW_UNT32_ALLBITS 0xffffffff
#define SLAW_UNT32_MAX_VAL (unt64) SLAW_UNT32_ALLBITS

#define SLAW_LENGTH_FOLLOWS_FLAG 0x80000000
#define SLAW_8_BYTE_LENGTH_FLAG 0x40000000
#define SLAW_PURE_ILKY_BITS 0x3fffffff

#define SLAW_ILK(s) ((s)->q)

#define SLAW_PURE_ILK(s) ((s)->q & SLAW_PURE_ILKY_BITS)

#define SLAW_LENGTH_FOLLOWS_FLAG_IS_SET(s) ((s)->q & SLAW_LENGTH_FOLLOWS_FLAG)

#define SLAW_8_BYTE_LENGTH_FLAG_IS_SET(s) ((s)->q & SLAW_8_BYTE_LENGTH_FLAG)

#define SLAW_HAS_4_BYTE_LENGTH(s)                                              \
  (SLAW_LENGTH_FOLLOWS_FLAG_IS_SET (s) && !SLAW_8_BYTE_LENGTH_FLAG_IS_SET (s))

#define SLAW_HAS_8_BYTE_LENGTH(s)                                              \
  (SLAW_LENGTH_FOLLOWS_FLAG_IS_SET (s) && SLAW_8_BYTE_LENGTH_FLAG_IS_SET (s))



#define SLAW_NIL_ILK 0x01010101

#define SLAW_IS_NIL(s) (SLAW_ILK (s) == SLAW_NIL_ILK)


#define SLAW_WEE_CONS_SUBMASK 0x3fffffff
#define SLAW_WEE_CONS_ILKMASK 0x40000000
#define SLAW_WEE_CONS_SUPERMASK 0x80000000

#define SLAW_IS_WEE_CONS(s)                                                    \
  (!(SLAW_ILK (s) & SLAW_WEE_CONS_SUPERMASK)                                   \
   && (SLAW_ILK (s) & SLAW_WEE_CONS_ILKMASK))

#define SLAW_WEE_CONS_QUADLEN(s) (1 + (SLAW_ILK (s) & SLAW_WEE_CONS_SUBMASK))


#define SLAW_WEE_STRING_SUBMASK 0x1fffffff
#define SLAW_WEE_STRING_ILKMASK 0x20000000
#define SLAW_WEE_STRING_SUPERMASK 0xc0000000

#define SLAW_IS_WEE_STRING(s)                                                  \
  (!(SLAW_ILK (s) & SLAW_WEE_STRING_SUPERMASK)                                 \
   && (SLAW_ILK (s) & SLAW_WEE_STRING_ILKMASK))

#define SLAW_WEE_STRING_QUADLEN(s)                                             \
  (1 + (SLAW_ILK (s) & SLAW_WEE_STRING_SUBMASK))


#define SLAW_WEE_LIST_SUBMASK 0x07ffffff
#define SLAW_WEE_LIST_MAPMASK 0x08000000
#define SLAW_WEE_LIST_ILKMASK 0x10000000
#define SLAW_WEE_LIST_SUPERMASK 0xe0000000

#define SLAW_IS_WEE_LIST(s)                                                    \
  (!(SLAW_ILK (s) & SLAW_WEE_LIST_SUPERMASK)                                   \
   && (SLAW_ILK (s) & SLAW_WEE_LIST_ILKMASK))

#define SLAW_WEE_LIST_ELEM_COUNT(s) (SLAW_ILK (s) & SLAW_WEE_LIST_SUBMASK)

#define SLAW_WEE_LIST_QUADLEN(s) (*((unt32 *) (s + 1)))

#define SLAW_WEE_LIST_ISMAP(s) ((SLAW_ILK (s) & SLAW_WEE_LIST_MAPMASK) != 0)

#define SLAW_BOOLEAN_SUBMASK 0x00000001
#define SLAW_BOOLEAN_ILKMASK 0x00000002
#define SLAW_BOOLEAN_SUPERMASK 0xfffffffc

#define SLAW_IS_BOOLEAN(s)                                                     \
  (!(SLAW_ILK (s) & SLAW_BOOLEAN_SUPERMASK)                                    \
   && (SLAW_ILK (s) & SLAW_BOOLEAN_ILKMASK))

#define SLAW_BOOLEAN_VALUE(s) (SLAW_ILK (s) & SLAW_BOOLEAN_SUBMASK)


#define SLAW_WEE_CONS_MAX_QUADLEN (unt32) SLAW_WEE_CONS_SUBMASK
#define SLAW_WEE_STRING_MAX_QUADLEN (unt32) SLAW_WEE_STRING_SUBMASK
#define SLAW_WEE_LIST_MAX_QUADLEN (unt64) 0xffffffff
#define SLAW_WEE_LIST_MAX_ELEM_COUNT (unt32) SLAW_WEE_LIST_SUBMASK



#define SLAW_CONS_PUREILK 0x20000001
#define SLAW_STRING_PUREILK 0x20000002
#define SLAW_LIST_PUREILK 0x20000004

#define SLAW_LIST_MAPMASK 0x00000001


#define SLAW_CONS_ILKMASK (SLAW_CONS_PUREILK | SLAW_LENGTH_FOLLOWS_FLAG)
#define SLAW_STRING_ILKMASK (SLAW_STRING_PUREILK | SLAW_LENGTH_FOLLOWS_FLAG)
#define SLAW_LIST_ILKMASK (SLAW_LIST_PUREILK | SLAW_LENGTH_FOLLOWS_FLAG)



#define SLAW_IS_FULL_CONS(s)                                                   \
  (SLAW_LENGTH_FOLLOWS_FLAG_IS_SET (s)                                         \
   && (SLAW_PURE_ILK (s) == SLAW_CONS_PUREILK))

#define SLAW_IS_FULL_STRING(s)                                                 \
  (SLAW_LENGTH_FOLLOWS_FLAG_IS_SET (s)                                         \
   && (SLAW_PURE_ILK (s) == SLAW_STRING_PUREILK))

#define SLAW_IS_FULL_LIST(s)                                                   \
  (SLAW_LENGTH_FOLLOWS_FLAG_IS_SET (s)                                         \
   && ((SLAW_PURE_ILK (s) & ~SLAW_LIST_MAPMASK) == SLAW_LIST_PUREILK))



#define SLAW_IS_CONS(s) (SLAW_IS_WEE_CONS (s) || SLAW_IS_FULL_CONS (s))

#define SLAW_IS_STRING(s) (SLAW_IS_WEE_STRING (s) || SLAW_IS_FULL_STRING (s))

#define SLAW_IS_LIST(s) (SLAW_IS_WEE_LIST (s) || SLAW_IS_FULL_LIST (s))

#define SLAW_LIST_ISMAP(s)                                                     \
  (SLAW_IS_WEE_LIST (s) ? SLAW_WEE_LIST_ISMAP (s)                              \
                        : (SLAW_ILK (s) & SLAW_LIST_MAPMASK))


#define SLAW_NUMERIC_SUBMASK 0x07ffffff
#define SLAW_NUMERIC_ILKMASK 0x08000000
#define SLAW_NUMERIC_SUPERMASK 0xf0000000


#define SLAW_PROTEIN_PUREILK 0x00800080
#define SLAW_PROTEIN_ILKMASK (SLAW_PROTEIN_PUREILK | SLAW_LENGTH_FOLLOWS_FLAG)

#define SLAW_PROTEIN_WEE_FLAG 0x10000000
#define SLAW_PROTEIN_NONSTD_FLAG 0x08000000
#define SLAW_PROTEIN_DESCRIPS_FLAG 0x04000000
#define SLAW_PROTEIN_INGESTS_FLAG 0x02000000
#define SLAW_PROTEIN_RUDE_FLAG 0x01000000

#define SLAW_PROTEIN_PAD_BITS 0x00007f00
#define SLAW_PROTEIN_PAD_SHIFTY 8

#define SLAW_PROTEIN_QUADLEN_BITS 0x007f0000
#define SLAW_PROTEIN_QUADLEN_SHIFTY 16
#define SLAW_WEE_PROTEIN_MAX_QUADLEN                                           \
  (SLAW_PROTEIN_QUADLEN_BITS >> SLAW_PROTEIN_QUADLEN_SHIFTY)

#define SLAW_IS_PROTEIN(s) ((SLAW_ILK (s) & 0xa08080e0) == SLAW_PROTEIN_ILKMASK)
#define SLAW_IS_WEE_PROTEIN(s)                                                 \
  (SLAW_IS_PROTEIN (s) && (SLAW_ILK (s) & SLAW_PROTEIN_WEE_FLAG) != 0)
#define PROTEIN_IS_NONSTANDARD(s)                                              \
  (SLAW_IS_PROTEIN (s) && (SLAW_ILK (s) & SLAW_PROTEIN_NONSTD_FLAG) != 0)
#define PROTEIN_HAS_DESCRIPS(s)                                                \
  (SLAW_IS_PROTEIN (s) && !PROTEIN_IS_NONSTANDARD (s)                          \
   && (SLAW_ILK (s) & SLAW_PROTEIN_DESCRIPS_FLAG) != 0)
#define PROTEIN_HAS_INGESTS(s)                                                 \
  (SLAW_IS_PROTEIN (s) && !PROTEIN_IS_NONSTANDARD (s)                          \
   && (SLAW_ILK (s) & SLAW_PROTEIN_INGESTS_FLAG) != 0)
#define PROTEIN_HAS_RUDE(s)                                                    \
  (SLAW_IS_PROTEIN (s) && !PROTEIN_IS_NONSTANDARD (s)                          \
   && (SLAW_ILK (s) & SLAW_PROTEIN_RUDE_FLAG) != 0)
#define SLAW_PROTEIN_HEADER_QUADS(s)                                           \
  (SLAW_IS_WEE_PROTEIN (s) ? 1 : (SLAW_8_BYTE_LENGTH_FLAG_IS_SET (s) ? 3 : 2))
#define SLAW_PROTEIN_PADDING(s)                                                \
  ((SLAW_ILK (s) & SLAW_PROTEIN_PAD_BITS) >> SLAW_PROTEIN_PAD_SHIFTY)

#define SLAW_NUMERIC_FLOAT_FLAG 0x04000000
#define SLAW_NUMERIC_COMPLEX_FLAG 0x02000000
#define SLAW_NUMERIC_UNSIGNED_FLAG 0x01000000
#define SLAW_NUMERIC_WIDE_FLAG 0x00800000
#define SLAW_NUMERIC_STUMPY_FLAG 0x00400000

#define SLAW_NUMERIC_MVEC_FLAG 0x00200000

#define SLAW_NUMERIC_BITS 0x0ffc0000
#define SLAW_NUMERIC_PERSONALITY_BITS 0x07f80000
#define SLAW_NUMERIC_CORE_PERSONALITY_BITS 0x05c00000

#define SLAW_NUMERIC_VEC_BITS 0x00180000
#define SLAW_NUMERIC_VEC_SHIFTY 19

#define SLAW_NUMERIC_VEC2_BITS 0x00080000
#define SLAW_NUMERIC_VEC3_BITS 0x00100000
#define SLAW_NUMERIC_VEC4_BITS 0x00180000

#define SLAW_NUMERIC_MVEC2_BITS 0x00200000
#define SLAW_NUMERIC_MVEC3_BITS 0x00280000
#define SLAW_NUMERIC_MVEC4_BITS 0x00300000
#define SLAW_NUMERIC_MVEC5_BITS 0x00380000

#define SLAW_NUMERIC_ARRAY_BREADTH_FOLLOWS_FLAG 0x00040000
#define SLAW_NUMERIC_ARRAY_8_BYTE_BREADTH_FLAG 0x00020000
#define SLAW_NUMERIC_ARRAY_MASK 0x0007ff00
#define SLAW_NUMERIC_ARRAY_WEE_BREADTH_MASK 0x0003ff00
#define SLAW_NUMERIC_ARRAY_WEE_BREADTH_SHIFTY 8
#define SLAW_NUMERIC_UNIT_BSIZE_MASK 0x000000ff

#define SLAW_NUMERIC_ARRAY_MAX_WEE_BREADTH                                     \
  ((unt64) ((SLAW_NUMERIC_ARRAY_WEE_BREADTH_MASK                               \
             >> SLAW_NUMERIC_ARRAY_WEE_BREADTH_SHIFTY)                         \
            - 1))
#define SLAW_NUMERIC_MAX_UNIT_BSIZE ((unt64) SLAW_NUMERIC_UNIT_BSIZE_MASK)

#define SLAW_IS_N_ILK(s, ilky)                                                 \
  (SLAW_IS_NUMERIC (s)                                                         \
   && ((SLAW_ILK (s) & SLAW_NUMERIC_PERSONALITY_BITS)                          \
       == (SLAW_##ilky & SLAW_NUMERIC_PERSONALITY_BITS)))

#define SLAW_IS_N_CORE_ILK(s, ilky)                                            \
  (SLAW_IS_NUMERIC (s)                                                         \
   && ((SLAW_ILK (s) & SLAW_NUMERIC_CORE_PERSONALITY_BITS) == SLAW_##ilky))

#define SLAW_IS_N_CORE_INT32(s) SLAW_IS_N_CORE_ILK (s, INT32)
#define SLAW_IS_N_CORE_UNT32(s) SLAW_IS_N_CORE_ILK (s, UNT32)
#define SLAW_IS_N_CORE_INT64(s) SLAW_IS_N_CORE_ILK (s, INT64)
#define SLAW_IS_N_CORE_UNT64(s) SLAW_IS_N_CORE_ILK (s, UNT64)

#define SLAW_IS_N_CORE_FLOAT32(s) SLAW_IS_N_CORE_ILK (s, FLOAT32)
#define SLAW_IS_N_CORE_FLOAT64(s) SLAW_IS_N_CORE_ILK (s, FLOAT64)

#define SLAW_IS_N_CORE_INT8(s) SLAW_IS_N_CORE_ILK (s, INT8)
#define SLAW_IS_N_CORE_UNT8(s) SLAW_IS_N_CORE_ILK (s, UNT8)
#define SLAW_IS_N_CORE_INT16(s) SLAW_IS_N_CORE_ILK (s, INT16)
#define SLAW_IS_N_CORE_UNT16(s) SLAW_IS_N_CORE_ILK (s, UNT16)

#define SLAW_IS_NUMERIC(s)                                                     \
  (!(SLAW_ILK (s) & SLAW_NUMERIC_SUPERMASK)                                    \
   && (SLAW_ILK (s) & SLAW_NUMERIC_ILKMASK))

// for the aftergoing, notion is that SLAW_IS_N_blah is trustworthy
// only after you already know that that slaw in question is numeric.
// whereas, you see, the SLAW_IS_NUMERIC_blah makes darn sure of it all.

#define SLAW_IS_N_FLOAT(s) ((SLAW_ILK (s) & SLAW_NUMERIC_FLOAT_FLAG) ? 1 : 0)
#define SLAW_IS_NUMERIC_FLOAT(s) (SLAW_IS_NUMERIC (s) && SLAW_IS_N_FLOAT (s))

#define SLAW_IS_N_COMPLEX(s)                                                   \
  ((SLAW_ILK (s) & SLAW_NUMERIC_COMPLEX_FLAG) ? 1 : 0)
#define SLAW_IS_NUMERIC_COMPLEX(s)                                             \
  (SLAW_IS_NUMERIC (s) && SLAW_IS_N_COMPLEX (s))

#define SLAW_IS_N_UNSIGNED(s)                                                  \
  ((SLAW_ILK (s) & SLAW_NUMERIC_UNSIGNED_FLAG) ? 1 : 0)
#define SLAW_IS_NUMERIC_UNSIGNED(s)                                            \
  (SLAW_IS_NUMERIC (s) && SLAW_IS_N_UNSIGNED (s))

#define SLAW_IS_N_WIDE(s) ((SLAW_ILK (s) & SLAW_NUMERIC_WIDE_FLAG) ? 1 : 0)
#define SLAW_IS_NUMERIC_WIDE(s) (SLAW_IS_NUMERIC (s) && SLAW_IS_N_WIDE (s))

#define SLAW_IS_N_STUMPY(s) ((SLAW_ILK (s) & SLAW_NUMERIC_STUMPY_FLAG) ? 1 : 0)
#define SLAW_IS_NUMERIC_STUMPY(s) (SLAW_IS_NUMERIC (s) && SLAW_IS_N_STUMPY (s))

#define SLAW_IS_N_MVEC(s) ((SLAW_ILK (s) & SLAW_NUMERIC_MVEC_FLAG) ? 1 : 0)
#define SLAW_IS_NUMERIC_MVEC(s) (SLAW_IS_NUMERIC (s) && SLAW_IS_N_MVEC (s))

// notes time: isn't it the time: oh yes: the time for notes:
// is 32 v. 64 bit enough? what if we had two bits describing
// how big ints & floats are/were (and pungy dung scupper if
// the logic of the subjunctive is not now more unclear than
// ever)? so that, to pick up, 0,1,2,3 as a reading of the two
// bits mapped to 32/64/128/256 bits worth of glorious int or
// float: the point being that the latter two're not even now
// contemplated, or at least not well so, but oughtn't we to get
// the jump? how embarrassed are the 12 bit folks today? mmm?
// anyway: what'd that look like? just wondering...
// ...
//
// ...
//
// no, but really.
//

#define SLAW_IS_N_INT(s) (!SLAW_IS_N_FLOAT (s))
#define SLAW_IS_NUMERIC_INT(s) (SLAW_IS_NUMERIC (s) && SLAW_IS_N_INT (s))
#define SLAW_IS_N_REAL(s) (!SLAW_IS_N_COMPLEX (s))
#define SLAW_IS_NUMERIC_REAL(s) (SLAW_IS_NUMERIC (s) && SLAW_IS_N_REAL (s))
#define SLAW_IS_N_SIGNED(s) (!SLAW_IS_N_UNSIGNED (s))
#define SLAW_IS_NUMERIC_SIGNED(s) (SLAW_IS_NUMERIC (s) && SLAW_IS_N_SIGNED (s))

#define SLAW_IS_N_32BIT(s) (!SLAW_IS_N_WIDE (s) && !SLAW_IS_N_STUMPY (s))
#define SLAW_IS_NUMERIC_32BIT(s) (SLAW_IS_NUMERIC (s) && SLAW_IS_N_32BIT (s))

#define SLAW_IS_N_64BIT(s) (SLAW_IS_N_WIDE (s) && (!SLAW_IS_N_STUMPY (s)))
#define SLAW_IS_NUMERIC_64BIT(s) (SLAW_IS_NUMERIC (s) && SLAW_IS_N_64BIT (s))

#define SLAW_IS_N_8BIT(s) (!SLAW_IS_N_WIDE (s) && SLAW_IS_N_STUMPY (s))
#define SLAW_IS_NUMERIC_8BIT(s) (SLAW_IS_NUMERIC (s) && SLAW_IS_N_8BIT (s))

#define SLAW_IS_N_16BIT(s) (SLAW_IS_N_WIDE (s) && SLAW_IS_N_STUMPY (s))
#define SLAW_IS_NUMERIC_16BIT(s)                                               \
  (SLAW_IS_NUMERIC_WIDE (s) && SLAW_IS_N_16BIT (s))


#define SLAW_IS_N_VECTOR(s)                                                    \
  ((SLAW_ILK (s) & SLAW_NUMERIC_VEC_BITS) && !SLAW_IS_N_MVEC (s))
#define SLAW_IS_NUMERIC_VECTOR(s) (SLAW_IS_NUMERIC (s) && SLAW_IS_N_VECTOR (s))

#define SLAW_IS_N_SCALAR(s) (!SLAW_IS_N_VECTOR (s) && !SLAW_IS_N_MVEC (s))
#define SLAW_IS_NUMERIC_SCALAR(s) (SLAW_IS_NUMERIC (s) && SLAW_IS_N_SCALAR (s))

#define SLAW_N_VECTOR_WIDTH(s)                                                 \
  (!SLAW_IS_NUMERIC (s) ? -1                                                   \
                        : (1 + SLAW_IS_N_MVEC (s)                              \
                           + ((SLAW_ILK (s) & SLAW_NUMERIC_VEC_BITS)           \
                              >> SLAW_NUMERIC_VEC_SHIFTY)))


#define SLAW_NUMERIC_UNIT_BSIZE(s)                                             \
  ((SLAW_ILK (s) & SLAW_NUMERIC_UNIT_BSIZE_MASK) + 1)


#define SLAW_IS_N_ARRAY(s) ((SLAW_ILK (s) & SLAW_NUMERIC_ARRAY_MASK) ? 1 : 0)
#define SLAW_IS_NUMERIC_ARRAY(s) (SLAW_IS_NUMERIC (s) && SLAW_IS_N_ARRAY (s))

#define SLAW_IS_N_SINGLETON(s) (!SLAW_IS_N_ARRAY (s))
#define SLAW_IS_NUMERIC_SINGLETON(s)                                           \
  (SLAW_IS_NUMERIC (s) && SLAW_IS_N_SINGLETON (s))

#define SLAW_N_ARRAY_HAS_WEE_BREADTH(s)                                        \
  (!(SLAW_ILK (s) & SLAW_NUMERIC_ARRAY_BREADTH_FOLLOWS_FLAG))

#define SLAW_N_ARRAY_HAS_8_BYTE_BREADTH(s)                                     \
  (!SLAW_N_ARRAY_HAS_WEE_BREADTH (s)                                           \
   && (SLAW_ILK (s) & SLAW_NUMERIC_ARRAY_8_BYTE_BREADTH_FLAG))

#define SLAW_N_ARRAY_WEE_BREADTH(s)                                            \
  (((SLAW_ILK (s) & SLAW_NUMERIC_ARRAY_WEE_BREADTH_MASK)                       \
    >> SLAW_NUMERIC_ARRAY_WEE_BREADTH_SHIFTY)                                  \
   - 1)

#define SLAW_N_ARRAY_BREADTH(s)                                                \
  (SLAW_IS_N_SINGLETON (s)                                                     \
     ? 0                                                                       \
     : (SLAW_N_ARRAY_HAS_WEE_BREADTH (s)                                       \
          ? SLAW_N_ARRAY_WEE_BREADTH (s)                                       \
          : (SLAW_N_ARRAY_HAS_8_BYTE_BREADTH (s) ? (*((unt64 *) (s + 1)))      \
                                                 : (*((unt32 *) (s + 1))))))

#define SLAW_N_ARRAY_FIRST_ELEM(s)                                             \
  ((void *) (SLAW_N_ARRAY_HAS_WEE_BREADTH (s)                                  \
               ? (s + 1)                                                       \
               : ((SLAW_N_ARRAY_HAS_8_BYTE_BREADTH (s) ? (s + 3) : (s + 2)))))

#define SLAW_N_ARRAY_NTH_ELEM(s, N)                                            \
  ((void *) (((unt8 *) SLAW_N_ARRAY_FIRST_ELEM (s))                            \
             + (N) * SLAW_NUMERIC_UNIT_BSIZE (s)))

// the basics, kid:

#define SLAW_int32 (0x00000000)
#define SLAW_unt32 (SLAW_NUMERIC_UNSIGNED_FLAG)
#define SLAW_int64 (SLAW_NUMERIC_WIDE_FLAG)
#define SLAW_unt64 (SLAW_NUMERIC_WIDE_FLAG | SLAW_NUMERIC_UNSIGNED_FLAG)
#define SLAW_float32 (SLAW_NUMERIC_FLOAT_FLAG)
#define SLAW_float64 (SLAW_NUMERIC_FLOAT_FLAG | SLAW_NUMERIC_WIDE_FLAG)
#define SLAW_int8 (SLAW_NUMERIC_STUMPY_FLAG)
#define SLAW_unt8 (SLAW_int8 | SLAW_NUMERIC_UNSIGNED_FLAG)
#define SLAW_int16 (SLAW_NUMERIC_STUMPY_FLAG | SLAW_NUMERIC_WIDE_FLAG)
#define SLAW_unt16 (SLAW_int16 | SLAW_NUMERIC_UNSIGNED_FLAG)



// complex numbah horseplay:

#define SLAW_int32c (SLAW_int32 | SLAW_NUMERIC_COMPLEX_FLAG)
#define SLAW_unt32c (SLAW_unt32 | SLAW_NUMERIC_COMPLEX_FLAG)
#define SLAW_int64c (SLAW_int64 | SLAW_NUMERIC_COMPLEX_FLAG)
#define SLAW_unt64c (SLAW_unt64 | SLAW_NUMERIC_COMPLEX_FLAG)
#define SLAW_float32c (SLAW_float32 | SLAW_NUMERIC_COMPLEX_FLAG)
#define SLAW_float64c (SLAW_float64 | SLAW_NUMERIC_COMPLEX_FLAG)
#define SLAW_int8c (SLAW_int8 | SLAW_NUMERIC_COMPLEX_FLAG)
#define SLAW_unt8c (SLAW_unt8 | SLAW_NUMERIC_COMPLEX_FLAG)
#define SLAW_int16c (SLAW_int16 | SLAW_NUMERIC_COMPLEX_FLAG)
#define SLAW_unt16c (SLAW_unt16 | SLAW_NUMERIC_COMPLEX_FLAG)



// vector shenanigans:

#define SLAW_v2int32 (SLAW_int32 | SLAW_NUMERIC_VEC2_BITS)
#define SLAW_v2unt32 (SLAW_unt32 | SLAW_NUMERIC_VEC2_BITS)
#define SLAW_v2int64 (SLAW_int64 | SLAW_NUMERIC_VEC2_BITS)
#define SLAW_v2unt64 (SLAW_unt64 | SLAW_NUMERIC_VEC2_BITS)
#define SLAW_v2float32 (SLAW_float32 | SLAW_NUMERIC_VEC2_BITS)
#define SLAW_v2float64 (SLAW_float64 | SLAW_NUMERIC_VEC2_BITS)
#define SLAW_v2int8 (SLAW_int8 | SLAW_NUMERIC_VEC2_BITS)
#define SLAW_v2unt8 (SLAW_unt8 | SLAW_NUMERIC_VEC2_BITS)
#define SLAW_v2int16 (SLAW_int16 | SLAW_NUMERIC_VEC2_BITS)
#define SLAW_v2unt16 (SLAW_unt16 | SLAW_NUMERIC_VEC2_BITS)



#define SLAW_v3int32 (SLAW_int32 | SLAW_NUMERIC_VEC3_BITS)
#define SLAW_v3unt32 (SLAW_unt32 | SLAW_NUMERIC_VEC3_BITS)
#define SLAW_v3int64 (SLAW_int64 | SLAW_NUMERIC_VEC3_BITS)
#define SLAW_v3unt64 (SLAW_unt64 | SLAW_NUMERIC_VEC3_BITS)
#define SLAW_v3float32 (SLAW_float32 | SLAW_NUMERIC_VEC3_BITS)
#define SLAW_v3float64 (SLAW_float64 | SLAW_NUMERIC_VEC3_BITS)
#define SLAW_v3int8 (SLAW_int8 | SLAW_NUMERIC_VEC3_BITS)
#define SLAW_v3unt8 (SLAW_unt8 | SLAW_NUMERIC_VEC3_BITS)
#define SLAW_v3int16 (SLAW_int16 | SLAW_NUMERIC_VEC3_BITS)
#define SLAW_v3unt16 (SLAW_unt16 | SLAW_NUMERIC_VEC3_BITS)



#define SLAW_v4int32 (SLAW_int32 | SLAW_NUMERIC_VEC4_BITS)
#define SLAW_v4unt32 (SLAW_unt32 | SLAW_NUMERIC_VEC4_BITS)
#define SLAW_v4int64 (SLAW_int64 | SLAW_NUMERIC_VEC4_BITS)
#define SLAW_v4unt64 (SLAW_unt64 | SLAW_NUMERIC_VEC4_BITS)
#define SLAW_v4float32 (SLAW_float32 | SLAW_NUMERIC_VEC4_BITS)
#define SLAW_v4float64 (SLAW_float64 | SLAW_NUMERIC_VEC4_BITS)
#define SLAW_v4int8 (SLAW_int8 | SLAW_NUMERIC_VEC4_BITS)
#define SLAW_v4unt8 (SLAW_unt8 | SLAW_NUMERIC_VEC4_BITS)
#define SLAW_v4int16 (SLAW_int16 | SLAW_NUMERIC_VEC4_BITS)
#define SLAW_v4unt16 (SLAW_unt16 | SLAW_NUMERIC_VEC4_BITS)



// complex vector extravaganza:

#define SLAW_v2int32c (SLAW_int32c | SLAW_NUMERIC_VEC2_BITS)
#define SLAW_v2unt32c (SLAW_unt32c | SLAW_NUMERIC_VEC2_BITS)
#define SLAW_v2int64c (SLAW_int64c | SLAW_NUMERIC_VEC2_BITS)
#define SLAW_v2unt64c (SLAW_unt64c | SLAW_NUMERIC_VEC2_BITS)
#define SLAW_v2float32c (SLAW_float32c | SLAW_NUMERIC_VEC2_BITS)
#define SLAW_v2float64c (SLAW_float64c | SLAW_NUMERIC_VEC2_BITS)
#define SLAW_v2int8c (SLAW_int8c | SLAW_NUMERIC_VEC2_BITS)
#define SLAW_v2unt8c (SLAW_unt8c | SLAW_NUMERIC_VEC2_BITS)
#define SLAW_v2int16c (SLAW_int16c | SLAW_NUMERIC_VEC2_BITS)
#define SLAW_v2unt16c (SLAW_unt16c | SLAW_NUMERIC_VEC2_BITS)



#define SLAW_v3int32c (SLAW_int32c | SLAW_NUMERIC_VEC3_BITS)
#define SLAW_v3unt32c (SLAW_unt32c | SLAW_NUMERIC_VEC3_BITS)
#define SLAW_v3int64c (SLAW_int64c | SLAW_NUMERIC_VEC3_BITS)
#define SLAW_v3unt64c (SLAW_unt64c | SLAW_NUMERIC_VEC3_BITS)
#define SLAW_v3float32c (SLAW_float32c | SLAW_NUMERIC_VEC3_BITS)
#define SLAW_v3float64c (SLAW_float64c | SLAW_NUMERIC_VEC3_BITS)
#define SLAW_v3int8c (SLAW_int8c | SLAW_NUMERIC_VEC3_BITS)
#define SLAW_v3unt8c (SLAW_unt8c | SLAW_NUMERIC_VEC3_BITS)
#define SLAW_v3int16c (SLAW_int16c | SLAW_NUMERIC_VEC3_BITS)
#define SLAW_v3unt16c (SLAW_unt16c | SLAW_NUMERIC_VEC3_BITS)



#define SLAW_v4int32c (SLAW_int32c | SLAW_NUMERIC_VEC4_BITS)
#define SLAW_v4unt32c (SLAW_unt32c | SLAW_NUMERIC_VEC4_BITS)
#define SLAW_v4int64c (SLAW_int64c | SLAW_NUMERIC_VEC4_BITS)
#define SLAW_v4unt64c (SLAW_unt64c | SLAW_NUMERIC_VEC4_BITS)
#define SLAW_v4float32c (SLAW_float32c | SLAW_NUMERIC_VEC4_BITS)
#define SLAW_v4float64c (SLAW_float64c | SLAW_NUMERIC_VEC4_BITS)
#define SLAW_v4int8c (SLAW_int8c | SLAW_NUMERIC_VEC4_BITS)
#define SLAW_v4unt8c (SLAW_unt8c | SLAW_NUMERIC_VEC4_BITS)
#define SLAW_v4int16c (SLAW_int16c | SLAW_NUMERIC_VEC4_BITS)
#define SLAW_v4unt16c (SLAW_unt16c | SLAW_NUMERIC_VEC4_BITS)



// multivector multiverse

#define SLAW_m2int32 (SLAW_int32 | SLAW_NUMERIC_MVEC2_BITS)
#define SLAW_m2unt32 (SLAW_unt32 | SLAW_NUMERIC_MVEC2_BITS)
#define SLAW_m2int64 (SLAW_int64 | SLAW_NUMERIC_MVEC2_BITS)
#define SLAW_m2unt64 (SLAW_unt64 | SLAW_NUMERIC_MVEC2_BITS)
#define SLAW_m2float32 (SLAW_float32 | SLAW_NUMERIC_MVEC2_BITS)
#define SLAW_m2float64 (SLAW_float64 | SLAW_NUMERIC_MVEC2_BITS)
#define SLAW_m2int8 (SLAW_int8 | SLAW_NUMERIC_MVEC2_BITS)
#define SLAW_m2unt8 (SLAW_unt8 | SLAW_NUMERIC_MVEC2_BITS)
#define SLAW_m2int16 (SLAW_int16 | SLAW_NUMERIC_MVEC2_BITS)
#define SLAW_m2unt16 (SLAW_unt16 | SLAW_NUMERIC_MVEC2_BITS)



#define SLAW_m3int32 (SLAW_int32 | SLAW_NUMERIC_MVEC3_BITS)
#define SLAW_m3unt32 (SLAW_unt32 | SLAW_NUMERIC_MVEC3_BITS)
#define SLAW_m3int64 (SLAW_int64 | SLAW_NUMERIC_MVEC3_BITS)
#define SLAW_m3unt64 (SLAW_unt64 | SLAW_NUMERIC_MVEC3_BITS)
#define SLAW_m3float32 (SLAW_float32 | SLAW_NUMERIC_MVEC3_BITS)
#define SLAW_m3float64 (SLAW_float64 | SLAW_NUMERIC_MVEC3_BITS)
#define SLAW_m3int8 (SLAW_int8 | SLAW_NUMERIC_MVEC3_BITS)
#define SLAW_m3unt8 (SLAW_unt8 | SLAW_NUMERIC_MVEC3_BITS)
#define SLAW_m3int16 (SLAW_int16 | SLAW_NUMERIC_MVEC3_BITS)
#define SLAW_m3unt16 (SLAW_unt16 | SLAW_NUMERIC_MVEC3_BITS)



#define SLAW_m4int32 (SLAW_int32 | SLAW_NUMERIC_MVEC4_BITS)
#define SLAW_m4unt32 (SLAW_unt32 | SLAW_NUMERIC_MVEC4_BITS)
#define SLAW_m4int64 (SLAW_int64 | SLAW_NUMERIC_MVEC4_BITS)
#define SLAW_m4unt64 (SLAW_unt64 | SLAW_NUMERIC_MVEC4_BITS)
#define SLAW_m4float32 (SLAW_float32 | SLAW_NUMERIC_MVEC4_BITS)
#define SLAW_m4float64 (SLAW_float64 | SLAW_NUMERIC_MVEC4_BITS)
#define SLAW_m4int8 (SLAW_int8 | SLAW_NUMERIC_MVEC4_BITS)
#define SLAW_m4unt8 (SLAW_unt8 | SLAW_NUMERIC_MVEC4_BITS)
#define SLAW_m4int16 (SLAW_int16 | SLAW_NUMERIC_MVEC4_BITS)
#define SLAW_m4unt16 (SLAW_unt16 | SLAW_NUMERIC_MVEC4_BITS)



#define SLAW_m4int32 (SLAW_int32 | SLAW_NUMERIC_MVEC4_BITS)
#define SLAW_m4unt32 (SLAW_unt32 | SLAW_NUMERIC_MVEC4_BITS)
#define SLAW_m4int64 (SLAW_int64 | SLAW_NUMERIC_MVEC4_BITS)
#define SLAW_m4unt64 (SLAW_unt64 | SLAW_NUMERIC_MVEC4_BITS)
#define SLAW_m4float32 (SLAW_float32 | SLAW_NUMERIC_MVEC4_BITS)
#define SLAW_m4float64 (SLAW_float64 | SLAW_NUMERIC_MVEC4_BITS)
#define SLAW_m4int8 (SLAW_int8 | SLAW_NUMERIC_MVEC4_BITS)
#define SLAW_m4unt8 (SLAW_unt8 | SLAW_NUMERIC_MVEC4_BITS)
#define SLAW_m4int16 (SLAW_int16 | SLAW_NUMERIC_MVEC4_BITS)
#define SLAW_m4unt16 (SLAW_unt16 | SLAW_NUMERIC_MVEC4_BITS)



#define SLAW_m5int32 (SLAW_int32 | SLAW_NUMERIC_MVEC5_BITS)
#define SLAW_m5unt32 (SLAW_unt32 | SLAW_NUMERIC_MVEC5_BITS)
#define SLAW_m5int64 (SLAW_int64 | SLAW_NUMERIC_MVEC5_BITS)
#define SLAW_m5unt64 (SLAW_unt64 | SLAW_NUMERIC_MVEC5_BITS)
#define SLAW_m5float32 (SLAW_float32 | SLAW_NUMERIC_MVEC5_BITS)
#define SLAW_m5float64 (SLAW_float64 | SLAW_NUMERIC_MVEC5_BITS)
#define SLAW_m5int8 (SLAW_int8 | SLAW_NUMERIC_MVEC5_BITS)
#define SLAW_m5unt8 (SLAW_unt8 | SLAW_NUMERIC_MVEC5_BITS)
#define SLAW_m5int16 (SLAW_int16 | SLAW_NUMERIC_MVEC5_BITS)
#define SLAW_m5unt16 (SLAW_unt16 | SLAW_NUMERIC_MVEC5_BITS)

#define SLAW_STRING_EMIT_WRITABLE(s) ((char *) v1slaw_string_emit (s))

static void v1slaw_free (slaw s);
static bslaw v1slaw_list_emit_first (bslaw s);
static bslaw v1slaw_list_emit_next (bslaw s_list, bslaw s_prev);
static int64 v1slabu_list_add_x (slabu *sb, slaw ess);
static int64 v1slabu_list_add_z (slabu *sb, bslaw ess);
static slaw v1slaw_dup (bslaw s);
static bool v1slawx_equal (bslaw s1, bslaw s2);
static bslaw v1slaw_cons_emit_car (bslaw s);
static bool v1slaw_is_cons (bslaw s);
static slaw v1slaw_cons (bslaw sA, bslaw sB);
static bslaw v1slaw_cons_emit_cdr (bslaw s);
static int v1slaw_semantic_compare (bslaw s1, bslaw s2);
static unt64 v1slaw_quadlen (bslaw s);
static slaw v1slaw_alloc (unt64 quadlen);
static slaw v1slaw_string_from_substring (const char *str, int64 len);
static slaw v1slaw_string_raw (int64 len);
static slaw v1slaw_alloc_prepfully (unt32 ilk, unt64 quadlen, slaw *first_data);
static const char *v1slaw_string_emit (bslaw s);
static int32 v1slaw_copy_quads_from_to (bslaw from_slaw, slaw to_slaw,
                                        unt64 quadlen);
static slaw v1slaw_list (const slabu *sb);
static slaw v1slaw_map_f (slabu *sb);
static void v1private_swap_bytes (void *v, int n);
static bslaw v1protein_descrips (bprotein prot);
static bslaw v1protein_ingests (bprotein prot);
static const void *v1protein_rude (bprotein prot, int64 *len);
static bool v1slaw_is_numeric_8 (bslaw s);
static bool v1slaw_is_numeric_16 (bslaw s);
static bool v1slaw_is_numeric_32 (bslaw s);
static bool v1slaw_is_numeric_vector (bslaw s);
static bool v1slaw_is_numeric_float (bslaw s);
static bool v1slaw_is_numeric_int (bslaw s);
static bool v1slaw_is_numeric_unt (bslaw s);
static bool v1slaw_is_numeric (bslaw s);
static ob_retort v1slaw_swap_sequence (slaw s, slaw stop);
static int v1slaw_numeric_unit_bsize (bslaw s);
static slaw v1slaw_numeric_array_raw (unt32 unit_ilk, unt32 unit_blen,
                                      unt64 breadth);
static const void *v1slaw_numeric_array_emit (bslaw s);
static bool v1slaw_is_numeric_multivector (bslaw s);
static bool v1slaw_is_numeric_complex (bslaw s);
static bool v1slaw_is_numeric_array (bslaw s);
static int v1slaw_numeric_vector_dimension (bslaw s);
static int64 v1slaw_numeric_array_count (bslaw s);

static slabu *v1slabu_new (void)
{
  slabu *sb = (slabu *) malloc (sizeof (slabu));
  if (!sb)
    return NULL;
  sb->esses = NULL;
  sb->effs = NULL;
  sb->numEsses = sb->maxNumEsses = 0;
  sb->obeysMapInvariant = true;
  return sb;
}

static int64 v1slabu_count (const slabu *sb)
{
  if (!sb)
    return -1;
  return sb->numEsses;
}

static bslaw v1slabu_list_nth (const slabu *sb, int64 n)
{
  if (!sb || n < 0 || n >= sb->numEsses)
    return NULL;
  return sb->esses[n];
}

static void v1slabu_free (slabu *sb)
{
  if (!sb)
    return;
  if (sb->esses)
    {
      slaw *essp = sb->esses + sb->numEsses - 1;
      bool *effp = sb->effs + sb->numEsses - 1;
      for (; sb->numEsses > 0; sb->numEsses--, essp--, effp--)
        if (*effp && *essp)
          v1slaw_free (*essp);
      free ((void *) sb->esses);
      free ((void *) sb->effs);
    }
  free ((void *) sb);
}

static slabu *v1slabu_dup_shallow (const slabu *sb)
{
  if (!sb)
    return NULL;

  slabu *newsb = v1slabu_new ();
  if (!newsb)
    return NULL;

  int64 count = v1slabu_count (sb);
  int64 i;
  ob_retort err = OB_OK;
  for (i = 0; i < count; i++)
    ob_err_accum (&err, v1slabu_list_add_z (newsb, v1slabu_list_nth (sb, i)));

  if (err < OB_OK)
    {
      v1slabu_free (newsb);
      return NULL;
    }

  return newsb;
}


static bool v1slabu_ensure (slabu *sb, int64 capacity)
{
  if (capacity <= sb->maxNumEsses)
    return true;

  int64 maxNumEsses = capacity + 8;
  slaw *esses;
  bool *effs;

  if (!sb->esses)
    {
      esses = (slaw *) malloc ((size_t) (maxNumEsses) * sizeof (slaw));
      effs = (bool *) malloc ((size_t) (maxNumEsses) * sizeof (bool));
    }
  else
    {
      esses =
        (slaw *) realloc (sb->esses, (size_t) (maxNumEsses) * sizeof (slaw));
      effs =
        (bool *) realloc (sb->effs, (size_t) (maxNumEsses) * sizeof (bool));
    }

  bool ok = true;

  if (esses)
    sb->esses = esses;
  else
    ok = false;

  if (effs)
    sb->effs = effs;
  else
    ok = false;

  if (ok)
    sb->maxNumEsses = maxNumEsses;

  return ok;
}

static int64 v1slabu_list_add_internal (slabu *sb, slaw ess, bool eff)
{
  if (!sb || !ess)
    return OB_ARGUMENT_WAS_NULL;

  if (!v1slabu_ensure (sb, sb->numEsses + 1))
    return OB_NO_MEM;

  sb->effs[sb->numEsses] = eff;
  sb->esses[sb->numEsses] = ess;
  sb->obeysMapInvariant = false;
  return sb->numEsses++;
}

static int64 v1slabu_list_add_x (slabu *sb, slaw ess)
{
  return v1slabu_list_add_internal (sb, ess, true);
}

static int64 v1slabu_list_add_z (slabu *sb, bslaw ess)
{
  slaw castaway = (slaw) ess;
  return v1slabu_list_add_internal (sb, castaway, false);
}

struct temporary_map_sorter
{
  slaw ess;
  unt64 eff : 1;
  unt64 order : 63; /* force qsort to be a stable sort */
};

static int v1tms_compare (const void *va, const void *vb)
{
  struct temporary_map_sorter *a = (struct temporary_map_sorter *) va;
  struct temporary_map_sorter *b = (struct temporary_map_sorter *) vb;
  bslaw acar = v1slaw_cons_emit_car (a->ess);
  bslaw bcar = v1slaw_cons_emit_car (b->ess);
  int result = v1slaw_semantic_compare (acar, bcar);

  if (result != 0)
    return result;

  if (a->order < b->order)
    return -1;
  else if (a->order > b->order)
    return 1;
  else
    return 0;
}

static int v1tms_revert (const void *va, const void *vb)
{
  struct temporary_map_sorter *a = (struct temporary_map_sorter *) va;
  struct temporary_map_sorter *b = (struct temporary_map_sorter *) vb;

  if (a->order < b->order)
    return -1;
  else if (a->order > b->order)
    return 1;
  else
    return 0;
}

// In the end, we chose to use DUP_KEEP_LAST_IN_POSITION_OF_FIRST in all cases:
// https://bugs.oblong.com/show_bug.cgi?id=28#c3
// But I'll leave this enum here in case different behavior is needed in
// the future.
typedef enum
{
  DUP_KEEP_FIRST,
  DUP_KEEP_LAST,
  DUP_KEEP_LAST_IN_POSITION_OF_FIRST
} DuplicateMode;

/// eliminates duplicates and non-conses, but retains order.
/// dm determines which duplicate should be chosen, and where
/// it should go.
/// returns true if successful, false if out of memory.
static bool v1enforce_map_invariant (slabu *sb, DuplicateMode dm)
{
  struct temporary_map_sorter *tms;
  int64 i, j, k;
  bslaw prevcar = NULL;

  if (sb->obeysMapInvariant)
    return true; /* already obeys */

  if (sb->numEsses == 0) /* empty list satisfies invariant */
    {
      sb->obeysMapInvariant = true;
      return true;
    }

  tms = (struct temporary_map_sorter *) malloc (
    sb->numEsses * sizeof (struct temporary_map_sorter));

  if (!tms)
    return false;

  /* copy all the conses to a temporary array for sorting */

  for (i = 0, j = 0; i < sb->numEsses; i++)
    {
      slaw ess = sb->esses[i];
      bool eff = sb->effs[i];

      if (v1slaw_is_cons (ess))
        {
          tms[j].ess = ess;
          tms[j].eff = eff;
          tms[j].order = j;
          j++;
        }
      else /* if it's not a cons, auf wiedersehen! */
        {
          if (eff)
            v1slaw_free (ess);
        }
    }

  qsort (tms, j, sizeof (struct temporary_map_sorter), v1tms_compare);

  /* eliminate duplicates */

  for (i = 0, k = 0; i < j; i++)
    {
      slaw ess = tms[i].ess;
      bool eff = tms[i].eff;
      if (prevcar && v1slawx_equal (prevcar, v1slaw_cons_emit_car (ess)))
        {
          // it's a duplicate; bye-bye!
          if (dm == DUP_KEEP_FIRST)
            {
              if (eff)
                v1slaw_free (ess);
            }
          else
            {
              int64 origOrder = tms[k - 1].order;
              if (tms[k - 1].eff)
                v1slaw_free (tms[k - 1].ess);
              tms[k - 1] = tms[i];
              if (dm == DUP_KEEP_LAST_IN_POSITION_OF_FIRST)
                tms[k - 1].order = origOrder;
              prevcar = v1slaw_cons_emit_car (ess);
            }
        }
      else
        {
          if (k != i)
            tms[k] = tms[i];
          k++;
          prevcar = v1slaw_cons_emit_car (ess);
        }
    }

  /* re-sort so it's back in the original order! */

  qsort (tms, k, sizeof (struct temporary_map_sorter), v1tms_revert);

  /* copy the array back */

  for (i = 0; i < k; i++)
    {
      slaw ess = tms[i].ess;
      bool eff = tms[i].eff;
      sb->esses[i] = ess;
      sb->effs[i] = eff;
    }

  sb->numEsses = k;
  sb->obeysMapInvariant = true;

  free (tms);
  return true;
}

static int64 v1slaw_len (bslaw s)
{
  return 4 * v1slaw_quadlen (s);
}


static slaw v1slaw_dup (bslaw s)
{
  if (!s)
    return NULL;
  unt64 scythes = v1slaw_quadlen (s);
  slaw dup = v1slaw_alloc (scythes);
  if (!dup)
    return NULL;
  bslaw from = s + scythes;
  slaw to = dup + scythes;
  while (from > s)
    *--to = *--from;
  return dup;
}


static void v1slaw_free (slaw s)
{
  if (s)
    {
      // Trash header quad to catch use of freed memory
      s->q = 0;
      free ((void *) s);
    }
}



static bool v1slawx_equal (bslaw s1, bslaw s2)
{
  if (!s1 || !s2)
    return false;
  unt64 len = v1slaw_quadlen (s1);
  if (v1slaw_quadlen (s2) != len)
    return false;
  for (; len > 0; len--)
    if (s1++->q != s2++->q)
      return false;
  return true;
}


static slaw v1slaw_nil (void)
{
  slaw s = v1slaw_alloc ((unt64) 1);
  if (!s)
    return NULL;
  s->q = (unt32) SLAW_NIL_ILK;
  return s;
}



static slaw v1slaw_string_from_substring (const char *str, int64 len)
{
  slaw result = v1slaw_string_raw (len);
  if (result)
    memcpy (SLAW_STRING_EMIT_WRITABLE (result), str, len);
  return result;
}


static slaw v1slaw_string_raw (int64 len)
{
  if (len < 0)
    return NULL;
  unt64 padLen = len + 1;
  if (padLen & 0x03)
    padLen += 4 - (padLen & 0x03);
  unt64 padQuadLen = padLen >> 2;
  slaw s, dataStart;
  if (padQuadLen <= SLAW_WEE_STRING_MAX_QUADLEN)
    {
      s = v1slaw_alloc (1 + padQuadLen);
      if (!s)
        return NULL;
      s->q = SLAW_WEE_STRING_ILKMASK | (unt32) padQuadLen;
      dataStart = s + 1;
    }
  else if (!(s = v1slaw_alloc_prepfully (SLAW_STRING_ILKMASK, padQuadLen,
                                         &dataStart)))
    return NULL;

  *((char *) ((void *) dataStart)) = '\0';

  /* initialize last word to 0 to avoid uninitialized padding problems */
  (&dataStart->q)[padQuadLen - 1] = 0;

  return s;
}


static const char *v1slaw_string_emit (bslaw s)
{
  if (!s || !s->q || !SLAW_IS_STRING (s))
    return NULL;
  if (SLAW_IS_WEE_STRING (s))
    return (char *) s + 4;
  return (char *) s + (SLAW_HAS_8_BYTE_LENGTH (s) ? 12 : 8);
}


static int64 v1slaw_string_emit_length (bslaw s)
{
  const char *str = v1slaw_string_emit (s);
  if (str)
    return strlen (str);
  else
    return 0;
}


static bool v1slaw_is_cons (bslaw s)
{
  return s ? SLAW_IS_CONS (s) : false;
}


static slaw v1slaw_cons (bslaw sA, bslaw sB)
{
  if (!sA || !sB)
    return NULL;
  unt64 lenA = v1slaw_quadlen (sA), lenB = v1slaw_quadlen (sB);
  unt64 len = lenA + lenB;
  slaw s, dataStart;
  if (len <= SLAW_WEE_CONS_MAX_QUADLEN)
    {
      s = v1slaw_alloc (1 + len);
      if (!s)
        return NULL;
      s->q = SLAW_WEE_CONS_ILKMASK | (unt32) len;
      dataStart = s + 1;
    }
  else if (!(s = v1slaw_alloc_prepfully (SLAW_CONS_ILKMASK, len, &dataStart)))
    return NULL;
  v1slaw_copy_quads_from_to (sA, dataStart, lenA);
  v1slaw_copy_quads_from_to (sB, dataStart + lenA, lenB);
  return s;
}


static bslaw v1slaw_cons_emit_car (bslaw s)
{
  if (!s)
    return NULL;
  if (SLAW_IS_WEE_CONS (s))
    return s + 1;
  if (SLAW_IS_FULL_CONS (s))
    return s + (SLAW_HAS_8_BYTE_LENGTH (s) ? 3 : 2);
  return NULL;
}


static bslaw v1slaw_cons_emit_cdr (bslaw s)
{
  if (!s)
    return NULL;
  if (SLAW_IS_WEE_CONS (s))
    s += 1;
  else if (SLAW_IS_FULL_CONS (s))
    s += (SLAW_HAS_8_BYTE_LENGTH (s) ? 3 : 2);
  else
    return NULL;
  return s + v1slaw_quadlen (s);
}



static slaw v1slaw_list_internal (const slabu *sb, bool isMap)
{
  if (!sb)
    return NULL;
  slaw s, ess, *essp = sb->esses;
  unt64 qLen = 0;
  int64 q = sb->numEsses;
  for (; q > 0; q--)
    {
      qLen += v1slaw_quadlen (*essp);
      essp++;
    }
  if (qLen <= SLAW_UNT32_MAX_VAL
      && sb->numEsses <= SLAW_WEE_LIST_MAX_ELEM_COUNT)
    {
      s = v1slaw_alloc (qLen += 2);
      if (!s)
        return NULL;
      s->q = SLAW_WEE_LIST_ILKMASK | (unt32) sb->numEsses;
      if (isMap)
        s->q |= SLAW_WEE_LIST_MAPMASK;
      (s + 1)->q = (unt32) qLen;
      ess = s + 2;
    }
  else if (sb->numEsses > SLAW_UNT32_MAX_VAL || qLen > SLAW_UNT32_MAX_VAL)
    {
      s = v1slaw_alloc (qLen += 5);
      if (!s)
        return NULL;
      s->q = SLAW_LIST_ILKMASK | SLAW_LENGTH_FOLLOWS_FLAG;
      if (isMap)
        s->q |= SLAW_LIST_MAPMASK;
      *((unt64 *) (s + 1)) = qLen;
      *((unt64 *) (s + 3)) = (unt64) sb->numEsses;
      ess = s + 5;
    }
  else
    {
      s = v1slaw_alloc (qLen += 3);
      if (!s)
        return NULL;
      s->q = SLAW_LIST_ILKMASK | SLAW_LENGTH_FOLLOWS_FLAG;
      if (isMap)
        s->q |= SLAW_LIST_MAPMASK;
      (s + 1)->q = (unt32) qLen;
      (s + 2)->q = (unt32) sb->numEsses;
      ess = s + 3;
    }
  essp = sb->esses;
  for (q = sb->numEsses; q > 0; q--)
    {
      qLen = v1slaw_quadlen (*essp);
      v1slaw_copy_quads_from_to (*essp, ess, qLen);
      essp++;
      ess += qLen;
    }
  return s;
}

static slaw v1slaw_list (const slabu *sb)
{
  return v1slaw_list_internal (sb, false);
}


static bool v1slaw_is_map (bslaw s)
{
  return s ? (SLAW_IS_LIST (s) && SLAW_LIST_ISMAP (s)) : false;
}


static int64 v1slaw_list_count (bslaw s)
{
  if (!s)
    return -1;
  if (SLAW_IS_WEE_LIST (s))
    return SLAW_WEE_LIST_ELEM_COUNT (s);
  if (!SLAW_IS_FULL_LIST (s))
    return -1;
  if (SLAW_8_BYTE_LENGTH_FLAG_IS_SET (s))
    return *((unt64 *) (s + 3));
  return *((unt32 *) (s + 2));
}


static bslaw v1slaw_list_emit_first (bslaw s)
{
  if (v1slaw_list_count (s) <= 0)
    return NULL;
  if (SLAW_IS_WEE_LIST (s))
    return s + 2;
  if (!SLAW_IS_FULL_LIST (s))
    return NULL;  // not a list, would be the conclusion here
  if (SLAW_8_BYTE_LENGTH_FLAG_IS_SET (s))
    return s + 5;
  return s + 3;
}


static bslaw v1slaw_list_emit_next (bslaw s_list, bslaw s_prev)
{
  if (!s_prev)
    return v1slaw_list_emit_first (s_list);
  if (!s_list || s_prev <= s_list)
    return NULL;
  unt64 qlen = v1slaw_quadlen (s_list);
  bslaw s_next, s_end = s_list + qlen;
  if (s_prev >= s_end)
    return NULL;
  if ((s_next = s_prev + v1slaw_quadlen (s_prev)) >= s_end)
    return NULL;
  return s_next;
}



static slaw v1slaw_map (const slabu *sb)
{
  if (sb->obeysMapInvariant)
    return v1slaw_list_internal (sb, true);
  else
    return v1slaw_map_f (v1slabu_dup_shallow (sb));
}

static slaw v1slaw_map_f (slabu *sb)
{
  // We can modify the existing slabu, since we're going to free it...
  if (!v1enforce_map_invariant (sb, DUP_KEEP_LAST_IN_POSITION_OF_FIRST))
    return NULL;

  slaw s = v1slaw_list_internal (sb, true);

  if (s)
    v1slabu_free (sb);

  return s;
}

static slaw v1slaw_boolean (bool value)
{
  slaw s = v1slaw_alloc ((unt64) 1);
  if (!s)
    return NULL;
  value = !!value; /* canonicalize value to 0 or 1 */
  s->q = (unt32) (SLAW_BOOLEAN_ILKMASK | (value & SLAW_BOOLEAN_SUBMASK));
  return s;
}

static bool v1slaw_is_boolean (bslaw s)
{
  if (!s)
    return false;

  return SLAW_IS_BOOLEAN (s);
}

static const bool *v1slaw_boolean_emit (bslaw s)
{
  static const bool values[] = {false, true};

  if (!v1slaw_is_boolean (s))
    return NULL;

  return values + SLAW_BOOLEAN_VALUE (s);
}


static bool v1slaw_is_protein (bslaw s)
{
  return s ? SLAW_IS_PROTEIN (s) : false;
}

static bool v1slaw_is_swapped_protein (bslaw s)
{
  if (!s)
    return false;

  struct _slaw tmp;
  tmp.q = SLAW_ILK (s);
  v1private_swap_bytes (&tmp, sizeof (tmp));
  return v1slaw_is_protein (&tmp);
}



static slaw v1slaw_alloc (unt64 quadlen)
{
  if (quadlen == 0)
    return NULL;
  return (slaw) malloc ((size_t) quadlen << 2);
}


static slaw v1slaw_alloc_prepfully (unt32 ilk, unt64 quadlen, slaw *first_data)
{
  unt32 bonus = (quadlen > SLAW_UNT32_MAX_VAL) ? 1 : 0;
  slaw s = v1slaw_alloc (bonus + 2 + quadlen);
  if (!s)
    {
      *first_data = NULL;
      return NULL;
    }
  s->q = ilk | SLAW_LENGTH_FOLLOWS_FLAG | (bonus ? SLAW_8_BYTE_LENGTH_FLAG : 0);
  if (bonus)
    *((unt64 *) (s + 1)) = quadlen;
  else
    *((unt32 *) (s + 1)) = (unt32) quadlen;
  *first_data = s + (2 + bonus);
  return s;
}


// Try to arrange these in order of frequency.  List is most frequent,
// because it is used on every call to v1slaw_list_emit_next.
static unt64 v1slaw_quadlen (bslaw s)
{
  if (!s)
    return 0;
  if (SLAW_IS_WEE_LIST (s))
    return SLAW_WEE_LIST_QUADLEN (s);
  if (SLAW_IS_NUMERIC (s))
    {
      unt64 len = SLAW_NUMERIC_UNIT_BSIZE (s);
      if (SLAW_IS_N_SINGLETON (s))
        return 1 + (len >> 2) + ((len % 4) ? 1 : 0);
      len *= SLAW_N_ARRAY_BREADTH (s);
      len = (len >> 2) + ((len % 4) ? 1 : 0);
      if (SLAW_N_ARRAY_HAS_WEE_BREADTH (s))
        return 1 + len;
      if (SLAW_N_ARRAY_HAS_8_BYTE_BREADTH (s))
        return 3 + len;
      return 2 + len;
    }
  if (SLAW_IS_WEE_STRING (s))
    return SLAW_WEE_STRING_QUADLEN (s);
  if (SLAW_IS_WEE_CONS (s))
    return SLAW_WEE_CONS_QUADLEN (s);
  if (SLAW_IS_NIL (s) || SLAW_IS_BOOLEAN (s))
    return 1;
  if (SLAW_IS_WEE_PROTEIN (s))
    {
      return 1
             + ((SLAW_ILK (s) & SLAW_PROTEIN_QUADLEN_BITS)
                >> SLAW_PROTEIN_QUADLEN_SHIFTY);
    }
  if (SLAW_LENGTH_FOLLOWS_FLAG_IS_SET (s))
    {
      if (SLAW_8_BYTE_LENGTH_FLAG_IS_SET (s))
        return (unt64) 3 + *((unt64 *) (s + 1));
      return (unt64) 2 + *((unt32 *) (s + 1));
    }
  if (!s->q)
    return 1;
  return 0;
}


static int32 v1slaw_copy_quads_from_to (bslaw from_slaw, slaw to_slaw,
                                        unt64 quadlen)
{
  if (!from_slaw || !to_slaw || quadlen < 1)
    return 0;
  from_slaw += quadlen;
  slaw intoS = to_slaw + quadlen;
  while (intoS > to_slaw)
    *--intoS = *--from_slaw;
  return 1;
}

// private function; byte-swaps an integer of arbitrary size
// (e. g. n = 2, n = 4, n = 8)
static void v1private_swap_bytes (void *v, int n)
{
  unt8 *b = (unt8 *) v;
  int i;

  for (i = 0; i < n / 2; i++)
    {
      unt8 tmp;
      tmp = b[i];
      b[i] = b[n - 1 - i];
      b[n - 1 - i] = tmp;
    }
}

static slaw_type v1slaw_gettype (bslaw s)
{
  slaw_type t = SLAW_TYPE_UNKNOWN;

  if (s == NULL)
    t = SLAW_TYPE_NULL;
  else if (SLAW_IS_NIL (s))
    t = SLAW_TYPE_NIL;
  else if (SLAW_IS_STRING (s))
    t = SLAW_TYPE_STRING;
  else if (SLAW_IS_NUMERIC (s))
    t = SLAW_TYPE_NUMERIC;
  else if (SLAW_IS_CONS (s))
    t = SLAW_TYPE_CONS;
  else if (SLAW_IS_LIST (s))
    t = SLAW_TYPE_LIST;
  else if (SLAW_IS_PROTEIN (s))
    t = SLAW_TYPE_PROTEIN;
  else if (SLAW_IS_BOOLEAN (s))
    t = SLAW_TYPE_BOOLEAN;

  return t;
}

static ob_retort v1slaw_swap (slaw s, slaw stop)
//                                         ^ unused
{
  slaw_type t;
  slaw payload = s + 1;

  // start by swapping the header
  v1private_swap_bytes (s, 4);
  if (SLAW_LENGTH_FOLLOWS_FLAG_IS_SET (s) && !SLAW_IS_WEE_PROTEIN (s))
    {
      v1private_swap_bytes (s + 1,
                            (SLAW_8_BYTE_LENGTH_FLAG_IS_SET (s) ? 8 : 4));
      payload += (SLAW_8_BYTE_LENGTH_FLAG_IS_SET (s) ? 2 : 1);
    }

  t = v1slaw_gettype (s);

  switch (t)
    {
      case SLAW_TYPE_NIL:     /* nil has nothing else */
      case SLAW_TYPE_BOOLEAN: /* neither does boolean */
      case SLAW_TYPE_STRING:  /* UTF-8 string is endian-neutral */
        return OB_OK;
      case SLAW_TYPE_LIST:
        /* swap the element count */
        v1private_swap_bytes (payload,
                              (SLAW_8_BYTE_LENGTH_FLAG_IS_SET (s) ? 8 : 4));
        payload += (SLAW_8_BYTE_LENGTH_FLAG_IS_SET (s) ? 2 : 1);
      /* fall thru */
      case SLAW_TYPE_CONS:
        return v1slaw_swap_sequence (payload, s + v1slaw_quadlen (s));
      case SLAW_TYPE_NUMERIC:
        {
          bool isArray;
          unt32 primBytes;
          unt64 nprims, i;
          void *e = payload;

          isArray = SLAW_IS_N_ARRAY (s);

          if (SLAW_IS_N_STUMPY (s))
            {
              if (SLAW_IS_N_WIDE (s))
                primBytes = 2;
              else
                return OB_OK; /* 1-byte units don't need swapping */
            }
          else
            {
              if (SLAW_IS_N_WIDE (s))
                primBytes = 8;
              else
                primBytes = 4;
            }

          nprims = SLAW_NUMERIC_UNIT_BSIZE (s) / primBytes;

          if (isArray)
            {
              unt64 breadth = SLAW_N_ARRAY_BREADTH (s);
              nprims *= breadth;
              e = SLAW_N_ARRAY_FIRST_ELEM (s);
            }

          for (i = 0; i < nprims; i++)
            {
              v1private_swap_bytes (e, primBytes);
              e = primBytes + (byte *) e;
            }

          return OB_OK;
        }
      case SLAW_TYPE_PROTEIN:
        {
          int nThingsToSwap =
            PROTEIN_HAS_DESCRIPS (s) + PROTEIN_HAS_INGESTS (s);
          int i;
          ob_retort err;
          slaw cole = payload;

          for (i = 0; i < nThingsToSwap; i++)
            {
              err = v1slaw_swap (cole, stop);
              if (err != OB_OK)
                return err;
              cole += v1slaw_quadlen (cole);
            }
          return OB_OK;
        }
      default:
        return SLAW_UNIDENTIFIED_SLAW;
    }
}

static ob_retort v1slaw_swap_sequence (slaw s, slaw stop)
{
  ob_retort err = OB_OK;

  while (err == OB_OK && s < stop && s->q)
    {
      err = v1slaw_swap (s, stop);
      s += v1slaw_quadlen (s);
    }

  if (s > stop)
    return SLAW_CORRUPT_SLAW; /* "stop" was in the middle of a slaw */

  return err;
}

/* make proteins a multiple of 16 bytes */
#define PAD_TO_QUADS 4


static bool v1protein_is_nonstandard (bprotein p)
{
  if (!p)
    return false;
  return PROTEIN_IS_NONSTANDARD (p);
}


static ob_retort v1protein_swap_endian (protein p)
{
  return v1slaw_swap (p, NULL);
}

static ob_retort v1protein_fix_endian (protein p)
{
  if (v1slaw_is_swapped_protein (p))
    return v1protein_swap_endian (p);
  else if (v1slaw_is_protein (p))
    return OB_OK;
  else /* not a protein-- this is bad */
    return SLAW_CORRUPT_PROTEIN;
}

static unt64 v1compute_protein_pad (unt64 qLen, unt64 overhead)
{
  unt64 q = qLen + overhead;
  unt64 qq = ((q + (PAD_TO_QUADS - 1)) / PAD_TO_QUADS) * PAD_TO_QUADS;

  return qq - q;
}

/* This is the master version of the protein constructor functions; all the
 * other variations call it.
 */
static protein v1protein_from_llr (bslaw descrips, bslaw ingests,
                                   const void *rude, int64 rude_len)
{
  unt64 qLen, qDescrips, qIngests, qRude;
  unt64 overhead = 0;
  unt64 padding = 0;
  unt64 candidateQuadlen = 0;
  unt64 extraBytes;
  unt32 ilk;
  slaw s;
  slaw payload;

  qDescrips = v1slaw_quadlen (descrips);
  qIngests = v1slaw_quadlen (ingests);
  qRude = (rude_len + 3) / 4;
  extraBytes = (qRude * 4) - rude_len;

  qLen = qDescrips + qIngests + qRude;

  ilk = SLAW_PROTEIN_ILKMASK;
  if (qDescrips)
    ilk |= SLAW_PROTEIN_DESCRIPS_FLAG;
  if (qIngests)
    ilk |= SLAW_PROTEIN_INGESTS_FLAG;
  if (qRude)
    ilk |= SLAW_PROTEIN_RUDE_FLAG;

  overhead = 1;
  padding = v1compute_protein_pad (qLen, overhead);
  candidateQuadlen = qLen + padding;

  if (candidateQuadlen <= SLAW_WEE_PROTEIN_MAX_QUADLEN)
    {
      s = v1slaw_alloc (overhead + candidateQuadlen);
      if (!s)
        return NULL;
      ilk |= SLAW_PROTEIN_PAD_BITS
             & ((padding * 4 + extraBytes) << SLAW_PROTEIN_PAD_SHIFTY);
      ilk |= SLAW_PROTEIN_WEE_FLAG;
      ilk |= SLAW_PROTEIN_QUADLEN_BITS
             & (candidateQuadlen << SLAW_PROTEIN_QUADLEN_SHIFTY);
      s->q = ilk;
      payload = s + 1;
    }
  else
    {
      overhead = 2;
      padding = v1compute_protein_pad (qLen, overhead);
      candidateQuadlen = qLen + padding;

      if (candidateQuadlen > SLAW_UNT32_MAX_VAL)
        {
          overhead = 3;
          padding = v1compute_protein_pad (qLen, overhead);
          candidateQuadlen = qLen + padding;
        }

      ilk |= SLAW_PROTEIN_PAD_BITS
             & ((padding * 4 + extraBytes) << SLAW_PROTEIN_PAD_SHIFTY);

      s = v1slaw_alloc_prepfully (ilk, candidateQuadlen, &payload);
      if (!s)
        return NULL;
    }

  if (payload - s != overhead) /* assertion, should never happen */
    OB_FATAL_BUG_CODE (0x20003000, "unexpected");

  v1slaw_copy_quads_from_to (descrips, payload, qDescrips);
  payload += qDescrips;
  v1slaw_copy_quads_from_to (ingests, payload, qIngests);
  payload += qIngests;
  memset (payload, 0, 4 * (candidateQuadlen - (qDescrips + qIngests)));
  memcpy (payload, rude, rude_len);

  return s;
}

static bslaw v1protein_descrips (bprotein prot)
{
  if (!prot)
    return NULL;

  bool hasDescrips = PROTEIN_HAS_DESCRIPS (prot);

  if (hasDescrips)
    return prot + SLAW_PROTEIN_HEADER_QUADS (prot);
  else
    return NULL;
}

static bslaw v1protein_ingests (bprotein prot)
{
  if (!prot)
    return NULL;

  bool hasDescrips = PROTEIN_HAS_DESCRIPS (prot);
  bool hasIngests = PROTEIN_HAS_INGESTS (prot);

  prot += SLAW_PROTEIN_HEADER_QUADS (prot);

  if (hasDescrips)
    prot += v1slaw_quadlen (prot);

  if (hasIngests)
    return prot;
  else
    return NULL;
}

static const void *v1protein_rude (bprotein prot, int64 *len)
{
  if (!prot || !SLAW_IS_PROTEIN (prot) || !PROTEIN_HAS_RUDE (prot))
    {
      *len = 0;
      return NULL;
    }

  bslaw beyond = prot + v1slaw_quadlen (prot);
  int64 pad = SLAW_PROTEIN_PADDING (prot);

  bool hasDescrips = PROTEIN_HAS_DESCRIPS (prot);
  bool hasIngests = PROTEIN_HAS_INGESTS (prot);

  prot += SLAW_PROTEIN_HEADER_QUADS (prot);

  if (hasDescrips)
    prot += v1slaw_quadlen (prot);

  if (hasIngests)
    prot += v1slaw_quadlen (prot);

  *len = 4 * (beyond - prot) - pad;
  return prot;
}

#define SLAW_BTOQ(bilk)                                                        \
  (((SLAW_##bilk##_BSIZE) >> 2) + (((SLAW_##bilk##_BSIZE) % 4) ? 1 : 0))

// the basics, kid:

#define SLAW_INT32 (0x00000000)
#define SLAW_UNT32 (SLAW_NUMERIC_UNSIGNED_FLAG)
#define SLAW_INT64 (SLAW_NUMERIC_WIDE_FLAG)
#define SLAW_UNT64 (SLAW_NUMERIC_WIDE_FLAG | SLAW_NUMERIC_UNSIGNED_FLAG)
#define SLAW_FLOAT32 (SLAW_NUMERIC_FLOAT_FLAG)
#define SLAW_FLOAT64 (SLAW_NUMERIC_FLOAT_FLAG | SLAW_NUMERIC_WIDE_FLAG)
#define SLAW_INT8 (SLAW_NUMERIC_STUMPY_FLAG)
#define SLAW_UNT8 (SLAW_INT8 | SLAW_NUMERIC_UNSIGNED_FLAG)
#define SLAW_INT16 (SLAW_NUMERIC_STUMPY_FLAG | SLAW_NUMERIC_WIDE_FLAG)
#define SLAW_UNT16 (SLAW_INT16 | SLAW_NUMERIC_UNSIGNED_FLAG)

#define SLAW_INT32_BSIZE 4
#define SLAW_UNT32_BSIZE 4
#define SLAW_INT64_BSIZE 8
#define SLAW_UNT64_BSIZE 8
#define SLAW_FLOAT32_BSIZE 4
#define SLAW_FLOAT64_BSIZE 8
#define SLAW_INT8_BSIZE 1
#define SLAW_UNT8_BSIZE 1
#define SLAW_INT16_BSIZE 2
#define SLAW_UNT16_BSIZE 2

#define SLAW_INT32_QSIZE SLAW_BTOQ (INT32)
#define SLAW_UNT32_QSIZE SLAW_BTOQ (UNT32)
#define SLAW_INT64_QSIZE SLAW_BTOQ (INT64)
#define SLAW_UNT64_QSIZE SLAW_BTOQ (UNT64)
#define SLAW_FLOAT32_QSIZE SLAW_BTOQ (FLOAT32)
#define SLAW_FLOAT64_QSIZE SLAW_BTOQ (FLOAT64)
#define SLAW_INT8_QSIZE SLAW_BTOQ (INT8)
#define SLAW_UNT8_QSIZE SLAW_BTOQ (UNT8)
#define SLAW_INT16_QSIZE SLAW_BTOQ (INT16)
#define SLAW_UNT16_QSIZE SLAW_BTOQ (UNT16)

#define SLAW_NUM_ALLOC_SINGLETON(ilky)                                         \
  slaw s = v1slaw_alloc (1 + SLAW_##ilky##_QSIZE);                             \
  if (!s)                                                                      \
    return NULL;                                                               \
  s->q = (SLAW_##ilky | SLAW_NUMERIC_ILKMASK | (SLAW_##ilky##_BSIZE - 1));     \
  if (SLAW_##ilky##_BSIZE != SLAW_##ilky##_QSIZE * 4)                          \
  (&s->q)[SLAW_##ilky##_QSIZE] = 0 /* make sure padding is initialized */


#define SLAW_NUM_INSTALL_SINGLETON(type, val)                                  \
  *((type *) ((void *) (s + 1))) = val;                                        \
  return s


#define SLAW_NUM_EMIT_SINGLETON_NOCHECK(s, type) ((type *) ((void *) (s + 1)))


#define ENSLAWIFY(ilk, type)                                                   \
  slaw v1slaw_##type (type val)                                                \
  {                                                                            \
    SLAW_NUM_ALLOC_SINGLETON (ilk);                                            \
    SLAW_NUM_INSTALL_SINGLETON (type, val);                                    \
  }

ENSLAWIFY (INT8, int8);
ENSLAWIFY (UNT8, unt8);
ENSLAWIFY (INT16, int16);
ENSLAWIFY (UNT16, unt16);

ENSLAWIFY (INT32, int32);
ENSLAWIFY (UNT32, unt32);
ENSLAWIFY (INT64, int64);
ENSLAWIFY (UNT64, unt64);
ENSLAWIFY (FLOAT32, float32);
ENSLAWIFY (FLOAT64, float64);

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

static slaw_fabricator *v1slaw_fabricator_new (void)
{
  slaw_fabricator *ret = (slaw_fabricator *) malloc (sizeof (slaw_fabricator));
  if (ret != NULL)
    memset (ret, 0, sizeof (slaw_fabricator));
  return ret;
}

static ob_retort v1sf_push (slaw_fabricator *sf)
{
  slabu_chain *sc = (slabu_chain *) malloc (sizeof (slabu_chain));
  if (sc == NULL)
    return OB_NO_MEM;
  memset (sc, 0, sizeof (slabu_chain));
  sc->sb = v1slabu_new ();
  if (sc->sb == NULL)
    {
      free (sc);
      return OB_NO_MEM;
    }
  sc->next = sf->stack;
  sf->stack = sc;
  return OB_OK;
}

static ob_retort v1sf_push_probu (slaw_fabricator *sf)
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

static ob_retort v1sf_push_nb (slaw_fabricator *sf)
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

static void v1sf_pop (slaw_fabricator *sf)
{
  if (sf->stack != NULL)
    {
      slabu_chain *tmp;
      if (sf->stack->sb)
        v1slabu_free (sf->stack->sb);
      else if (sf->stack->pb)
        {
          v1slaw_free (sf->stack->pb->descrips);
          v1slaw_free (sf->stack->pb->ingests);
          free (sf->stack->pb->rude);
          free (sf->stack->pb);
        }
      else if (sf->stack->nb)
        ob_nb_free (&sf->stack->nb);
      else
        OB_FATAL_BUG_CODE (0x20003001, "shouldn't happen\n");
      tmp = sf->stack->next;
      free (sf->stack);
      sf->stack = tmp;
    }
}

static void v1slaw_fabricator_free (slaw_fabricator *sf)
{
  while (sf->stack)
    v1sf_pop (sf);
  if (sf->result)
    v1slaw_free (sf->result);
  free (sf);
}

static ob_retort v1sf_add_slaw (slaw_fabricator *sf, slaw cole)
{
  ob_retort err = OB_OK;
  if (sf->stack)
    {
      if (sf->stack->sb)
        ob_err_accum (&err, v1slabu_list_add_x (sf->stack->sb, cole));
      else if (sf->stack->doing_ingests)
        sf->stack->pb->ingests = cole;
      else
        sf->stack->pb->descrips = cole;
    }
  else
    sf->result = cole;

  return err;
}

static ob_retort v1sf_enter (slaw_fabricator *sf, unt8 flags)
{
  ob_retort tort;
  if ((!sf->stack || !sf->stack->nb) && OB_OK > (tort = v1sf_push_nb (sf)))
    return tort;
  return ob_nb_enter (sf->stack->nb, flags);
}

static ob_retort v1sf_leave (slaw_fabricator *sf, unt8 leave_flags)
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
      slaw_quad ilk = SLAW_NUMERIC_ILKMASK;
      if ((flags & OB_MULTI) != 0)
        {
          unt32 m;
          switch (vsize)
            {
              case 4:
                m = 0;
                break;
              case 8:
                m = 1;
                break;
              case 16:
                m = 2;
                break;
              case 32:
                m = 3;
                break;
              default:
                return SLAW_FABRICATOR_BADNESS;
            }
          ilk |= SLAW_NUMERIC_MVEC_FLAG | (m << SLAW_NUMERIC_VEC_SHIFTY);
        }
      else if (vsize > 4)
        return SLAW_FABRICATOR_BADNESS;
      else
        ilk |= ((vsize - 1) << SLAW_NUMERIC_VEC_SHIFTY);

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

      switch (bits)
        {
          case 8:
            ilk |= SLAW_NUMERIC_STUMPY_FLAG;
            break;
          case 16:
            ilk |= SLAW_NUMERIC_STUMPY_FLAG | SLAW_NUMERIC_WIDE_FLAG;
            break;
          case 32:
            break;
          case 64:
            ilk |= SLAW_NUMERIC_WIDE_FLAG;
            break;
          default:
            return SLAW_FABRICATOR_BADNESS;
        }

      unt32 blen = (bits / 8) * vsize * (isComplex ? 2 : 1);
      slaw cole;
      byte *p;

      if ((flags & OB_ARRAY) != 0)
        {
          cole = v1slaw_numeric_array_raw (ilk, blen, breadth);
          if (!cole)
            return OB_NO_MEM;
          p = (byte *) SLAW_N_ARRAY_FIRST_ELEM (cole);
        }
      else
        {
          unt32 new_bsize = blen;
          unt32 new_qsize = (((new_bsize) >> 2) + (((new_bsize) % 4) ? 1 : 0));

          ilk |= (new_bsize - 1);
          cole = v1slaw_alloc (1 + new_qsize);
          if (!cole)
            return OB_NO_MEM;
          cole->q = ilk;
          p = (byte *) (cole + 1);
          byte *last = (byte *) (cole + 1 + new_qsize);
          memset (p, 0, last - p); /* initialize padding */
        }

      memcpy (p, ptr, blen * breadth);
      v1sf_pop (sf);
      return v1sf_add_slaw (sf, cole);
    }
  return OB_OK;
}

static ob_retort v1sf_begin_cons (void *cookie)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  return v1sf_push (sf);
}

static ob_retort v1sf_end_cons (void *cookie)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  slaw cole;
  if (!sf->stack || !sf->stack->sb || v1slabu_count (sf->stack->sb) != 2)
    return SLAW_FABRICATOR_BADNESS;
  cole = v1slaw_cons (sf->stack->sb->esses[0], sf->stack->sb->esses[1]);
  if (!cole)
    return OB_NO_MEM;
  v1sf_pop (sf);
  return v1sf_add_slaw (sf, cole);
}

static ob_retort v1sf_begin_list (void *cookie, OB_UNUSED int64 hint)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  ob_retort err = v1sf_push (sf);
  return err;
}

static ob_retort v1sf_end_list (void *cookie)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  slaw cole;
  if (!sf->stack || !sf->stack->sb)
    return SLAW_FABRICATOR_BADNESS;
  cole = v1slaw_list (sf->stack->sb);
  if (!cole)
    return OB_NO_MEM;
  v1sf_pop (sf);
  return v1sf_add_slaw (sf, cole);
}

static ob_retort v1sf_end_map (void *cookie)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  slaw cole;
  if (!sf->stack || !sf->stack->sb)
    return SLAW_FABRICATOR_BADNESS;
  cole = v1slaw_map (sf->stack->sb);
  if (!cole)
    return OB_NO_MEM;
  v1sf_pop (sf);
  return v1sf_add_slaw (sf, cole);
}

static ob_retort v1sf_begin_array (void *cookie)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  return v1sf_enter (sf, OB_ARRAY);
}

static ob_retort v1sf_begin_array_hinted (void *cookie, OB_UNUSED int64 hint,
                                          OB_UNUSED int hint2)
{
  return v1sf_begin_array (cookie);
}

static ob_retort v1sf_end_array (void *cookie)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  return v1sf_leave (sf, OB_ARRAY);
}

static ob_retort v1sf_begin_multivector (void *cookie, OB_UNUSED int64 hint)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  return v1sf_enter (sf, OB_MULTI);
}

static ob_retort v1sf_end_multivector (void *cookie)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  return v1sf_leave (sf, OB_MULTI);
}

static ob_retort v1sf_begin_vector (void *cookie, OB_UNUSED int64 hint)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  return v1sf_enter (sf, OB_VECTOR);
}

static ob_retort v1sf_end_vector (void *cookie)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  return v1sf_leave (sf, OB_VECTOR);
}

static ob_retort v1sf_begin_complex (void *cookie)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  return v1sf_enter (sf, OB_COMPLEX);
}

static ob_retort v1sf_end_complex (void *cookie)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  return v1sf_leave (sf, OB_COMPLEX);
}

static ob_retort v1sf_handle_nil (void *cookie)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  slaw cole = v1slaw_nil ();
  if (!cole)
    return OB_NO_MEM;
  return v1sf_add_slaw (sf, cole);
}

static ob_retort v1sf_handle_string (void *cookie, const char *utf8, int64 len)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  slaw cole = v1slaw_string_from_substring (utf8, len);
  if (!cole)
    return OB_NO_MEM;
  return v1sf_add_slaw (sf, cole);
}

static ob_retort v1sf_handle_int (void *cookie, int64 val, int bits)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  slaw cole;

  if (sf->stack && sf->stack->nb)
    return ob_nb_push_integer (&sf->stack->nb, false, bits, val);

  switch (bits)
    {
      case 8:
        cole = v1slaw_int8 ((int8) val);
        break;
      case 16:
        cole = v1slaw_int16 ((int16) val);
        break;
      case 32:
        cole = v1slaw_int32 ((int32) val);
        break;
      case 64:
        cole = v1slaw_int64 (val);
        break;
      default:
        return SLAW_FABRICATOR_BADNESS;
    }

  if (!cole)
    return OB_NO_MEM;
  return v1sf_add_slaw (sf, cole);
}

static ob_retort v1sf_handle_unt (void *cookie, unt64 val, int bits)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  slaw cole;

  if (sf->stack && sf->stack->nb)
    return ob_nb_push_integer (&sf->stack->nb, true, bits, val);

  switch (bits)
    {
      case 1:
        cole = v1slaw_boolean ((int) val);
        break;
      case 8:
        cole = v1slaw_unt8 ((unt8) val);
        break;
      case 16:
        cole = v1slaw_unt16 ((unt16) val);
        break;
      case 32:
        cole = v1slaw_unt32 ((unt32) val);
        break;
      case 64:
        cole = v1slaw_unt64 (val);
        break;
      default:
        return SLAW_FABRICATOR_BADNESS;
    }

  if (!cole)
    return OB_NO_MEM;
  return v1sf_add_slaw (sf, cole);
}

static ob_retort v1sf_handle_float (void *cookie, float64 val, int bits)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  slaw cole;

  if (sf->stack && sf->stack->nb)
    return ob_nb_push_float (&sf->stack->nb, bits, val);

  switch (bits)
    {
      case 32:
        cole = v1slaw_float32 ((float32) val);
        break;
      case 64:
        cole = v1slaw_float64 (val);
        break;
      default:
        return SLAW_FABRICATOR_BADNESS;
    }

  if (!cole)
    return OB_NO_MEM;
  return v1sf_add_slaw (sf, cole);
}

static ob_retort v1sf_handle_empty_array (void *cookie, int vecsize,
                                          bool isMVec, bool isComplex,
                                          bool isUnsigned, bool isFloat,
                                          int bits)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  slaw cole;
  unt32 blen;
  unt32 ilk = SLAW_NUMERIC_ILKMASK | ((vecsize - 1) << SLAW_NUMERIC_VEC_SHIFTY);

  if (bits < 8 || bits > 64 || (bits & (bits - 1)) != 0
      || (isFloat && bits < 32))
    return SLAW_FABRICATOR_BADNESS;

  if (isMVec)
    {
      ilk = (SLAW_NUMERIC_ILKMASK | SLAW_NUMERIC_MVEC_FLAG
             | ((vecsize - 2) << SLAW_NUMERIC_VEC_SHIFTY));
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

  if (bits == 8 || bits == 16)
    ilk |= SLAW_NUMERIC_STUMPY_FLAG;

  if (bits == 16 || bits == 64)
    ilk |= SLAW_NUMERIC_WIDE_FLAG;

  blen = (bits / 8) * vecsize * (isComplex ? 2 : 1);

  cole = v1slaw_numeric_array_raw (ilk, blen, 0);
  if (!cole)
    return OB_NO_MEM;
  return v1sf_add_slaw (sf, cole);
}

static ob_retort v1sf_begin_protein (void *cookie)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  return v1sf_push_probu (sf);
}

static ob_retort v1sf_end_protein (void *cookie)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  protein p;
  if (!sf->stack || !sf->stack->pb)
    return SLAW_FABRICATOR_BADNESS;
  p = v1protein_from_llr (sf->stack->pb->descrips, sf->stack->pb->ingests,
                          sf->stack->pb->rude, sf->stack->pb->rudeLen);
  if (!p)
    return OB_NO_MEM;
  v1sf_pop (sf);
  return v1sf_add_slaw (sf, p);
}

static ob_retort v1sf_handle_nonstd_protein (void *cookie, const void *pp,
                                             int64 len)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  protein p = (protein) pp;
  unt64 p_len = v1slaw_len (p);
  if (p_len != len)
    {
      OB_LOG_ERROR_CODE (0x20003002,
                         "nonstandard protein is %" OB_FMT_64 "u"
                         " but thought it was %" OB_FMT_64 "u\n",
                         p_len, len);
      return SLAW_CORRUPT_SLAW;
    }
  return v1sf_add_slaw (sf, v1slaw_dup (p));
}

static ob_retort v1sf_begin_descrips (void *cookie)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  if (!sf->stack || !sf->stack->pb)
    return SLAW_FABRICATOR_BADNESS;
  sf->stack->doing_ingests = false;

  return OB_OK;
}

static ob_retort v1sf_end_descips (OB_UNUSED void *cookie)
{
  return OB_OK;
}

static ob_retort v1sf_begin_ingests (void *cookie)
{
  slaw_fabricator *sf = (slaw_fabricator *) cookie;
  if (!sf->stack || !sf->stack->pb)
    return SLAW_FABRICATOR_BADNESS;
  sf->stack->doing_ingests = true;

  return OB_OK;
}

static ob_retort v1sf_end_ingests (OB_UNUSED void *cookie)
{
  return OB_OK;
}

static ob_retort v1sf_handle_rude_data (void *cookie, const void *d, int64 len)
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

static int v1protein_semantic_compare (bprotein p1, bprotein p2)
{
  bool nonstd1, nonstd2;
  int64 rl1, rl2;
  const void *rude1, *rude2;
  int c;

  nonstd1 = v1protein_is_nonstandard (p1);
  nonstd2 = v1protein_is_nonstandard (p2);

  if (nonstd1 && !nonstd2)
    return 1; /* treat nonstandard proteins as greater */
  else if (!nonstd1 && nonstd2)
    return -1;

  if (nonstd1)
    {
      unt64 len1, len2;
      len1 = v1slaw_len (p1);
      len2 = v1slaw_len (p2);

      if (len1 < len2)
        return -1; /* sort nonstandard proteins by size first */
      else if (len1 > len2)
        return 1;

      return memcmp (p1, p2, (size_t) len1); /* compare the bytes */
    }

  c =
    v1slaw_semantic_compare (v1protein_descrips (p1), v1protein_descrips (p2));
  if (c != 0)
    return c;

  c = v1slaw_semantic_compare (v1protein_ingests (p1), v1protein_ingests (p2));
  if (c != 0)
    return c;

  rude1 = v1protein_rude (p1, &rl1);
  rude2 = v1protein_rude (p2, &rl2);

  if (rl1 < rl2)
    return -1; /* sort by size of rude data */
  else if (rl1 > rl2)
    return 1;

  if (rl1 != 0)
    {
      return memcmp (rude1, rude2, (size_t) rl1); /* compare the bytes */
    }

  return 0; /* must be equal */
}

static int v1slaw_semantic_compare (bslaw s1, bslaw s2)
{
  slaw_type t1, t2;

  t1 = v1slaw_gettype (s1);
  t2 = v1slaw_gettype (s2);

  if (t1 < t2)
    return -1;
  else if (t1 > t2)
    return 1;

  /* slawx are the same type, so do type-specific comparison */
  switch (t1)
    {
      case SLAW_TYPE_NULL:
      case SLAW_TYPE_NIL:
        return 0; /* nil always equals nil */
      case SLAW_TYPE_BOOLEAN:
        {
          int b1, b2;
          b1 = *v1slaw_boolean_emit (s1);
          b2 = *v1slaw_boolean_emit (s2);
          if (b1 < b2)
            return -1;
          else if (b1 > b2)
            return 1;
          else
            return 0;
        }
      case SLAW_TYPE_STRING:
        return strcmp (v1slaw_string_emit (s1), v1slaw_string_emit (s2));
      case SLAW_TYPE_NUMERIC:
        {
          unt32 pers1, pers2;
          bool isArray1, isArray2;
          unt32 primBytes;
          unt64 nprims, i;
          const void *e1, *e2;
          bool isUnsigned, isFloat;

          pers1 = (SLAW_ILK (s1) & SLAW_NUMERIC_PERSONALITY_BITS);
          pers2 = (SLAW_ILK (s2) & SLAW_NUMERIC_PERSONALITY_BITS);
          if (pers1 < pers2)
            return -1;
          else if (pers1 > pers2)
            return 1;

          isArray1 = v1slaw_is_numeric_array (s1);
          isArray2 = v1slaw_is_numeric_array (s2);

          if (isArray1 && !isArray2)
            return 1; /* treat arrays as greater than non-arrays */
          if (!isArray1 && isArray2)
            return -1;

          if (SLAW_IS_N_STUMPY (s1))
            {
              if (SLAW_IS_N_WIDE (s1))
                primBytes = 2;
              else
                primBytes = 1;
            }
          else
            {
              if (SLAW_IS_N_WIDE (s1))
                primBytes = 8;
              else
                primBytes = 4;
            }

          nprims = v1slaw_numeric_unit_bsize (s1) / primBytes;
          e1 = (void *) (s1 + 1);
          e2 = (void *) (s2 + 1);

          if (isArray1)
            {
              unt64 breadth1 = v1slaw_numeric_array_count (s1);
              unt64 breadth2 = v1slaw_numeric_array_count (s2);

              if (breadth1 < breadth2)
                return -1; /* treat smaller arrays as less */
              if (breadth1 > breadth2)
                return 1;

              nprims *= breadth1;

              e1 = v1slaw_numeric_array_emit (s1);
              e2 = v1slaw_numeric_array_emit (s2);
            }

          isUnsigned = v1slaw_is_numeric_unt (s1);
          isFloat = v1slaw_is_numeric_float (s1);

          for (i = 0; i < nprims; i++)
            {
              if (isFloat)
                {
                  int c;
                  if (primBytes == 8)
                    c = private_float64_compare (*(unt64 *) e1, *(unt64 *) e2);
                  else
                    c = private_float32_compare (*(unt32 *) e1, *(unt32 *) e2);

                  if (c != 0)
                    return c;
                }
              else if (isUnsigned)
                {
                  switch (primBytes)
                    {
                      case 1:
                        if (*(unt8 *) e1 < *(unt8 *) e2)
                          return -1;
                        else if (*(unt8 *) e1 > *(unt8 *) e2)
                          return 1;
                        else
                          break;
                      case 2:
                        if (*(unt16 *) e1 < *(unt16 *) e2)
                          return -1;
                        else if (*(unt16 *) e1 > *(unt16 *) e2)
                          return 1;
                        else
                          break;
                      case 4:
                        if (*(unt32 *) e1 < *(unt32 *) e2)
                          return -1;
                        else if (*(unt32 *) e1 > *(unt32 *) e2)
                          return 1;
                        else
                          break;
                      case 8:
                        if (*(unt64 *) e1 < *(unt64 *) e2)
                          return -1;
                        else if (*(unt64 *) e1 > *(unt64 *) e2)
                          return 1;
                        else
                          break;
                    }
                }
              else
                {
                  switch (primBytes)
                    {
                      case 1:
                        if (*(int8 *) e1 < *(int8 *) e2)
                          return -1;
                        else if (*(int8 *) e1 > *(int8 *) e2)
                          return 1;
                        else
                          break;
                      case 2:
                        if (*(int16 *) e1 < *(int16 *) e2)
                          return -1;
                        else if (*(int16 *) e1 > *(int16 *) e2)
                          return 1;
                        else
                          break;
                      case 4:
                        if (*(int32 *) e1 < *(int32 *) e2)
                          return -1;
                        else if (*(int32 *) e1 > *(int32 *) e2)
                          return 1;
                        else
                          break;
                      case 8:
                        if (*(int64 *) e1 < *(int64 *) e2)
                          return -1;
                        else if (*(int64 *) e1 > *(int64 *) e2)
                          return 1;
                        else
                          break;
                    }
                }

              e1 = primBytes + (byte *) e1;
              e2 = primBytes + (byte *) e2;
            }

          return 0; /* every element is equal */
        }
      case SLAW_TYPE_CONS:
        {
          int c;
          c = v1slaw_semantic_compare (v1slaw_cons_emit_car (s1),
                                       v1slaw_cons_emit_car (s2));
          if (c != 0)
            return c;
          return v1slaw_semantic_compare (v1slaw_cons_emit_cdr (s1),
                                          v1slaw_cons_emit_cdr (s2));
        }
      case SLAW_TYPE_LIST:
        {
          bslaw e1, e2;
          bool m1, m2;

          // first need to disambiguate a list from a map
          m1 = v1slaw_is_map (s1);
          m2 = v1slaw_is_map (s2);

          if (m1 < m2)
            return -1;
          else if (m1 > m2)
            return 1;

          for (e1 = (v1slaw_list_count (s1) > 0 ? v1slaw_list_emit_first (s1)
                                                : NULL),
              e2 = (v1slaw_list_count (s2) > 0 ? v1slaw_list_emit_first (s2)
                                               : NULL);
               e1 != NULL && e2 != NULL; e1 = v1slaw_list_emit_next (s1, e1),
              e2 = v1slaw_list_emit_next (s2, e2))
            {
              int c = v1slaw_semantic_compare (e1, e2);
              if (c != 0)
                return c;
            }

          if (e1)
            return 1; /* first list is longer, therefore greater */
          else if (e2)
            return -1; /* first list is shorter, therefore less */

          return 0; /* lists ended together, therefore equal */
        }
      case SLAW_TYPE_PROTEIN:
        return v1protein_semantic_compare (s1, s2);
      default:
        return 0; /* let's say unknown slawx are equal */
    }
}

static slaw v1slaw_numeric_array_raw (unt32 unit_ilk, unt32 unit_blen,
                                      unt64 breadth)
{
  if (unit_blen < 1  //  ||  breadth < 1
      || (unit_blen > SLAW_NUMERIC_MAX_UNIT_BSIZE + 1))
    return NULL;
  slaw s;
  unt64 arr_qlen = unit_blen * breadth;
  arr_qlen = (arr_qlen >> 2) + ((arr_qlen % 4) ? 1 : 0);
  if (breadth <= SLAW_NUMERIC_ARRAY_MAX_WEE_BREADTH)
    {
      s = v1slaw_alloc (1 + arr_qlen);
      if (!s)
        return NULL;
      s->q = SLAW_NUMERIC_ILKMASK | unit_ilk | (unit_blen - 1)
             | (unt32) ((breadth + 1) << SLAW_NUMERIC_ARRAY_WEE_BREADTH_SHIFTY);
    }
  else if (breadth <= SLAW_UNT32_MAX_VAL)
    {
      s = v1slaw_alloc (2 + arr_qlen);
      if (!s)
        return NULL;
      s->q = SLAW_NUMERIC_ILKMASK | unit_ilk | (unit_blen - 1)
             | SLAW_NUMERIC_ARRAY_BREADTH_FOLLOWS_FLAG;
      (s + 1)->q = (unt32) breadth;
    }
  else
    {
      s = v1slaw_alloc (3 + arr_qlen);
      if (!s)
        return NULL;
      s->q = SLAW_NUMERIC_ILKMASK | unit_ilk | (unit_blen - 1)
             | SLAW_NUMERIC_ARRAY_BREADTH_FOLLOWS_FLAG
             | SLAW_NUMERIC_ARRAY_8_BYTE_BREADTH_FLAG;
      *((unt64 *) (s + 1)) = breadth;
    }

  if (0 != (3 & (unit_blen * breadth)))
    (&s->q)[v1slaw_quadlen (s) - 1] = 0; /* make sure padding is initialized */

  return s;
}


static int64 v1slaw_numeric_array_count (bslaw s)
{
  if (!s)
    return 0;
  if (!SLAW_IS_NUMERIC (s))
    return 0;
  return SLAW_N_ARRAY_BREADTH (s);
}

static const void *v1slaw_numeric_array_emit (bslaw s)
{
  if (!s)
    return 0;
  if (!SLAW_IS_NUMERIC (s))
    return 0;
  return SLAW_N_ARRAY_FIRST_ELEM (s);
}


static const void *v1slaw_numeric_nonarray_emit (bslaw s)
{
  return (s + 1);
}


static bool v1slaw_is_numeric (bslaw s)
{
  return s ? SLAW_IS_NUMERIC (s) : false;
}



static bool v1slaw_is_numeric_8 (bslaw s)
{
  return (!s) ? false : SLAW_IS_NUMERIC_8BIT (s);
}

static bool v1slaw_is_numeric_16 (bslaw s)
{
  return (!s) ? false : SLAW_IS_NUMERIC_16BIT (s);
}

static bool v1slaw_is_numeric_32 (bslaw s)
{
  return (!s) ? false : SLAW_IS_NUMERIC_32BIT (s);
}

static bool v1slaw_is_numeric_64 (bslaw s)
{
  return (!s) ? false : SLAW_IS_NUMERIC_64BIT (s);
}

static bool v1slaw_is_numeric_float (bslaw s)
{
  return (!s) ? false : SLAW_IS_NUMERIC_FLOAT (s);
}

static bool v1slaw_is_numeric_int (bslaw s)
{
  return (!s) ? false : (SLAW_IS_NUMERIC_INT (s) && SLAW_IS_NUMERIC_SIGNED (s));
}

static bool v1slaw_is_numeric_unt (bslaw s)
{
  return (!s) ? false
              : (SLAW_IS_NUMERIC_INT (s) && SLAW_IS_NUMERIC_UNSIGNED (s));
}

static bool v1slaw_is_numeric_complex (bslaw s)
{
  return (!s) ? false : SLAW_IS_NUMERIC_COMPLEX (s);
}

static bool v1slaw_is_numeric_vector (bslaw s)
{
  return (!s) ? false : SLAW_IS_NUMERIC_VECTOR (s);
}

static bool v1slaw_is_numeric_multivector (bslaw s)
{
  return (!s) ? false : SLAW_IS_NUMERIC_MVEC (s);
}

static bool v1slaw_is_numeric_array (bslaw s)
{
  return (!s) ? false : SLAW_IS_NUMERIC_ARRAY (s);
}

static int v1slaw_numeric_vector_dimension (bslaw s)
{
  return (s ? SLAW_N_VECTOR_WIDTH (s) : 0);
}

static int v1slaw_numeric_unit_bsize (bslaw s)
{
  return (v1slaw_is_numeric (s) ? SLAW_NUMERIC_UNIT_BSIZE (s) : 0);
}

static const slaw_handler v1slaw_fabrication_handler = {
  v1sf_begin_cons,
  v1sf_end_cons,
  v1sf_begin_list, /* same function to begin list or map */
  v1sf_end_map,
  v1sf_begin_list,
  v1sf_end_list,
  v1sf_begin_array_hinted,
  v1sf_end_array,
  v1sf_begin_multivector,
  v1sf_end_multivector,
  v1sf_begin_vector,
  v1sf_end_vector,
  v1sf_begin_complex,
  v1sf_end_complex,
  v1sf_handle_nil,
  v1sf_handle_string,
  v1sf_handle_int,
  v1sf_handle_unt,
  v1sf_handle_float,
  v1sf_handle_empty_array,
  v1sf_begin_protein,
  v1sf_end_protein,
  v1sf_handle_nonstd_protein,
  v1sf_begin_descrips,
  v1sf_end_descips,
  v1sf_begin_ingests,
  v1sf_end_ingests,
  v1sf_handle_rude_data};

static ob_retort v1build (slaw *s, const slaw_vfuncs *v)
{
  slaw_fabricator *sf = v1slaw_fabricator_new ();
  if (!sf)
    return OB_NO_MEM;
  ob_retort err = slaw_walk_versioned (sf, &v1slaw_fabrication_handler, *s, v);
  if (err < OB_OK)
    {
      v1slaw_fabricator_free (sf);
      return err;
    }
  slaw_free (*s);
  *s = sf->result;
  sf->result = NULL;
  v1slaw_fabricator_free (sf);
  return OB_OK;
}

static ob_retort v1binary_input_read (slaw_read_handler h, bool needToSwap,
                                      slaw *s)
{
  struct _slaw slawhead[3];
  struct _slaw swappedhead[3];
  unsigned int headWords = 1;
  unt64 quadlen;
  slaw cole;
  ob_retort err = OB_OK;
  size_t size_read;

  OB_INVALIDATE (size_read);

  /* read the first quad, to determine how many quads we need
   * to read to get the length */
  err = h.read (h.cookie, (byte *) slawhead, sizeof (slawhead[0]), &size_read);
  if (err < OB_OK)
    return err;
  else if (size_read != sizeof (slawhead[0]))
    return SLAW_END_OF_FILE;

  swappedhead[0] = slawhead[0];
  if (needToSwap)
    v1private_swap_bytes (&(swappedhead[0]), sizeof (swappedhead[0]));

  if (SLAW_LENGTH_FOLLOWS_FLAG_IS_SET (swappedhead)
      && !SLAW_IS_WEE_PROTEIN (swappedhead))
    {
      headWords++;
      if (SLAW_8_BYTE_LENGTH_FLAG_IS_SET (swappedhead))
        headWords++;
    }
  else if (SLAW_IS_NUMERIC (swappedhead))
    {
      if (!SLAW_N_ARRAY_HAS_WEE_BREADTH (swappedhead))
        {
          headWords++;
          if (SLAW_N_ARRAY_HAS_8_BYTE_BREADTH (swappedhead))
            headWords++;
        }
    }
  else if (SLAW_IS_WEE_LIST (swappedhead))
    {
      headWords++;
    }

  if (headWords > 1)
    {
      /* read enough quads to determine the length */
      const size_t size_wanted = sizeof (slawhead[0]) * (headWords - 1);
      err = h.read (h.cookie, (byte *) (slawhead + 1), size_wanted, &size_read);
      if (err < OB_OK)
        return err;
      else if (size_read != size_wanted)
        return SLAW_CORRUPT_SLAW; /* unexpected end-of-file */
    }

  memcpy (swappedhead + 1, slawhead + 1,
          sizeof (slawhead[0]) * (headWords - 1));
  if (needToSwap)
    v1private_swap_bytes (swappedhead + 1,
                          sizeof (slawhead[0]) * (headWords - 1));

  quadlen = v1slaw_quadlen (swappedhead);
  if (quadlen == 0)
    return SLAW_UNIDENTIFIED_SLAW;
  else if (quadlen < headWords)
    return SLAW_CORRUPT_SLAW;

  cole = v1slaw_alloc (quadlen);
  if (cole == NULL)
    return OB_NO_MEM;

  v1slaw_copy_quads_from_to (slawhead, cole, headWords);
  *s = cole;
  cole += headWords;
  quadlen -= headWords;

  /* read the rest of the slaw, if any */
  if (quadlen > 0)
    {
      const size_t size_wanted = sizeof (slawhead[0]) * quadlen;
      err = h.read (h.cookie, (byte *) cole, size_wanted, &size_read);
      if (err < OB_OK || size_read != size_wanted)
        {
          v1slaw_free (*s);
          return (err < OB_OK ? err : SLAW_CORRUPT_SLAW);
        }
    }

  if (needToSwap)
    err = v1slaw_swap (*s, NULL);

  if (err != OB_OK)
    v1slaw_free (*s);

  return err;
}

const slaw_vfuncs slaw_v1funcs = {v1slaw_gettype,
                                  v1slaw_list_emit_first,
                                  v1slaw_list_emit_next,
                                  v1slaw_is_map,
                                  v1slaw_is_numeric_array,
                                  v1slaw_is_numeric_multivector,
                                  v1slaw_is_numeric_vector,
                                  v1slaw_is_numeric_complex,
                                  v1slaw_is_numeric_float,
                                  v1slaw_is_numeric_int,
                                  v1slaw_is_numeric_unt,
                                  v1slaw_is_numeric_8,
                                  v1slaw_is_numeric_16,
                                  v1slaw_is_numeric_32,
                                  v1slaw_is_numeric_64,
                                  v1slaw_numeric_vector_dimension,
                                  v1slaw_numeric_unit_bsize,
                                  v1slaw_numeric_array_count,
                                  v1slaw_numeric_array_emit,
                                  v1slaw_numeric_nonarray_emit,
                                  v1slaw_cons_emit_car,
                                  v1slaw_cons_emit_cdr,
                                  v1slaw_boolean_emit,
                                  v1slaw_string_emit,
                                  v1slaw_string_emit_length,
                                  v1protein_is_nonstandard,
                                  v1slaw_len,
                                  v1protein_descrips,
                                  v1protein_ingests,
                                  v1protein_rude,
                                  v1slaw_free,
                                  v1protein_fix_endian,
                                  v1slaw_swap,
                                  v1build,
                                  v1binary_input_read,
                                  NULL};
