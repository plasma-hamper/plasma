
/* (c)  oblong industries */

#include "libPlasma/c/protein.h"
#include "libPlasma/c/private/plasma-private.h"
#include "libLoam/c/ob-log.h"

#include <stdlib.h>
#include <string.h>

bool protein_is_empty (bprotein p)
{
  // if it's not a protein, it's not an empty protein
  if (!p || !SLAW_IS_PROTEIN (p))
    return false;
  // let's say, if it's nonstandard, it's not empty either
  if (PROTEIN_IS_NONSTANDARD (p))
    return false;
  // an empty protein has no descrips, ingests, or rude data
  return (!PROTEIN_HAS_DESCRIPS (p) && !PROTEIN_HAS_INGESTS (p)
          && !PROTEIN_HAS_RUDE (p));
}



bool protein_is_nonstandard (bprotein p)
{
  if (!p)
    return false;
  return PROTEIN_IS_NONSTANDARD (p);
}


ob_retort protein_swap_endian (protein p)
{
  return slaw_swap (p, p + slaw_octlen (p));
}

ob_retort protein_fix_endian (protein p)
{
  if (slaw_is_swapped_protein (p))
    return protein_swap_endian (p);
  else if (slaw_is_protein (p))
    return OB_OK;
  else /* not a protein-- this is bad */
    {
      // this pops up in bug 882.
      OB_LOG_WARNING_CODE (
        0x20006000,
        "header oct %016" OB_FMT_64 "X does not represent a protein\n", p->o);
      return SLAW_CORRUPT_PROTEIN;
    }
}

/* This is the master version of the protein constructor functions; all the
 * other variations call it.
 */
protein protein_from_llr (bslaw descrips, bslaw ingests, const void *rude,
                          int64 rude_len)
{
  opaque_protein_info op;

  op.descrips = descrips;
  op.descrips_len = 8 * slaw_octlen (descrips);
  op.ingests = ingests;
  op.ingests_len = 8 * slaw_octlen (ingests);
  op.rude = rude;
  op.rude_len = rude_len;

  return protein_from_opaque (&op);
}

protein protein_from_opaque (const opaque_protein_info *opaque)
{
  const void *rude = opaque->rude;
  const int64 rude_len = opaque->rude_len;

  unt64 oLen, oDescrips, oIngests, oRude;
  slaw_oct ilk, oct2;  // first and second octs, respectively, of the protein
  slaw s;
  slaw payload;
  bool wee_rude = (rude_len <= 7);
  void *rude_dest;

  if (rude_len < 0 || (rude_len > 0 && !rude))  // rude-imentary error checking
    return NULL;

  oDescrips = opaque->descrips_len / 8;
  oIngests = opaque->ingests_len / 8;
  oRude = wee_rude ? 0 : ((rude_len + 7) / 8);

  oLen = 2 + oDescrips + oIngests + oRude;

  ilk = SLAW_PROTEIN_ILK | (oLen & 0xf) | ((oLen & ~0xf) << 4);
  oct2 = 0;

  if (oDescrips)
    oct2 |= SLAW_PROTEIN_DESCRIPS_FLAG;
  if (oIngests)
    oct2 |= SLAW_PROTEIN_INGESTS_FLAG;

  if (wee_rude)
    oct2 |= (rude_len << SLAW_PROTEIN_WEE_RUDE_SHIFTY);
  else
    oct2 |= (SLAW_PROTEIN_VERY_RUDE_FLAG | rude_len);

  s = slaw_alloc (oLen);
  if (!s)
    return NULL;
  s->o = ilk;
  s[1].o = oct2;

  if (!wee_rude)
    s[oLen - 1].o = 0;  // initialize rude data padding to 0

  payload = s + 2;

  slaw_copy_octs_from_to ((bslaw) opaque->descrips, payload, oDescrips);
  payload += oDescrips;
  slaw_copy_octs_from_to ((bslaw) opaque->ingests, payload, oIngests);
  if (wee_rude)
    {
      payload = s + 1;
      rude_dest = SLAW_SPECIAL_BYTES (payload, rude_len);
    }
  else
    {
      payload += oIngests;
      rude_dest = payload;
    }

  if (rude_len > 0)
    memcpy (rude_dest, rude, rude_len);

  return s;
}

protein protein_from_ffr (slaw descrips, slaw ingests, const void *rude,
                          int64 rude_len)
{
  protein result = protein_from_llr (descrips, ingests, rude, rude_len);
  if (result)
    {
      slaw_free (descrips);
      slaw_free (ingests);
    }
  return result;
}

protein protein_from_lfr (bslaw descrips, slaw ingests, const void *rude,
                          int64 rude_len)
{
  protein result = protein_from_llr (descrips, ingests, rude, rude_len);
  if (result)
    slaw_free (ingests);
  return result;
}

protein protein_from_flr (slaw descrips, bslaw ingests, const void *rude,
                          int64 rude_len)
{
  protein result = protein_from_llr (descrips, ingests, rude, rude_len);
  if (result)
    slaw_free (descrips);
  return result;
}

protein protein_from (bslaw descrips, bslaw ingests)
{
  return protein_from_llr (descrips, ingests, NULL, 0);
}

protein protein_from_ff (slaw descrips, slaw ingests)
{
  return protein_from_ffr (descrips, ingests, NULL, 0);
}

protein protein_from_lf (bslaw descrips, slaw ingests)
{
  return protein_from_lfr (descrips, ingests, NULL, 0);
}

protein protein_from_fl (slaw descrips, bslaw ingests)
{
  return protein_from_flr (descrips, ingests, NULL, 0);
}

bslaw protein_descrips (bprotein prot)
{
  if (!prot)
    return NULL;

  bool hasDescrips = PROTEIN_HAS_DESCRIPS (prot);

  if (hasDescrips)
    {
      const unt64 octlen = slaw_octlen (prot);
      if (octlen < 3)
        return NULL;
      prot += 2;
      if (octlen < slaw_octlen (prot) + 2)
        return NULL;
      return prot;
    }
  else
    return NULL;
}

bslaw protein_ingests (bprotein prot)
{
  if (!prot)
    return NULL;

  bool hasDescrips = PROTEIN_HAS_DESCRIPS (prot);
  bool hasIngests = PROTEIN_HAS_INGESTS (prot);

  const unt64 octlen = slaw_octlen (prot);
  if (octlen < 3)
    return NULL;

  prot += 2;

  unt64 descoct = 0;

  if (hasDescrips)
    {
      descoct = slaw_octlen (prot);
      if (octlen < descoct + 2)
        return NULL;
      prot += descoct;
    }

  if (hasIngests && octlen >= 2 + descoct + slaw_octlen (prot))
    return prot;
  else
    return NULL;
}

const void *protein_rude (bprotein prot, int64 *len)
{
  if (!prot || !SLAW_IS_PROTEIN (prot) || !PROTEIN_HAS_RUDE (prot))
    {
      *len = 0;
      return NULL;
    }

  if (PROTEIN_IS_VERY_RUDE (prot))
    {
      bool hasDescrips = PROTEIN_HAS_DESCRIPS (prot);
      bool hasIngests = PROTEIN_HAS_INGESTS (prot);
      bprotein end = prot + slaw_octlen (prot);

      *len = (SLAW_PROTEIN_OCT2 (prot) & SLAW_PROTEIN_VERY_RUDE_MASK);
      prot += 2;

      if (prot >= end)
        goto bad;

      if (hasDescrips)
        prot += slaw_octlen (prot);

      if (prot >= end)
        goto bad;

      if (hasIngests)
        prot += slaw_octlen (prot);

      if (prot >= end)
        goto bad;

      if ((end - prot) * 8 < *len)
        {
bad:
          *len = 0;
          return NULL;
        }

      return prot;
    }
  else
    {
      *len = ((SLAW_PROTEIN_OCT2 (prot) & SLAW_PROTEIN_WEE_RUDE_MASK)
              >> SLAW_PROTEIN_WEE_RUDE_SHIFTY);
      prot++;
      return SLAW_SPECIAL_BYTES (prot, *len);
    }
}

int64 protein_search (bprotein haystack, bslaw needle)
{
  return protein_search_ex (haystack, needle, SEARCH_GAP);
}

typedef int64 (*search_func) (bslaw s, bslaw search);

int64 protein_search_ex (bprotein haystack, bslaw needle,
                         Protein_Search_Type how)
{
  search_func func;

  switch (how)
    {
      case SEARCH_GAP:
        func = slaw_list_gapsearch;
        break;
      case SEARCH_CONTIG:
        func = slaw_list_contigsearch;
        break;
      default:
        return OB_INVALID_ARGUMENT;
    }

  return (slaw_is_list (needle) ? func : slaw_list_find) (
    protein_descrips (haystack), needle);
}
