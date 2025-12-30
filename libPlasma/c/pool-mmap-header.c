
/* (c)  oblong industries */

#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-dirs.h"
#include "libLoam/c/ob-file.h"
#include "libPlasma/c/private/pool_mmap.h"
#include "libPlasma/c/private/plasma-private.h"
#include "libPlasma/c/slaw-coerce.h"
#include "libPlasma/c/slaw-string.h"
#include "libPlasma/c/protein.h"
#include <assert.h>

// If header_size is 12 or greater, header is a pool_mmap_oldnew
// followed by an unt64 containing the magic number and version
#define POOL_MMAP_V0_HEADER_SIZE (sizeof (pool_mmap_oldnew) + sizeof (unt64))

#define POOL_MMAP_GET_MAGICV0(m)                                               \
  (*(volatile unt64 *) ((m) + sizeof (pool_mmap_oldnew)))

#define POOL_MMAP_MAGICV0 OB_CONST_U64 (0x00065b0000af4c81)
//                                               ^^ slaw version goes here
#define POOL_MMAP_SLAW_VERSION_SHIFTY 24

static void *get_chunk (void *mem, unt64 hdr_len, unt64 sig, ob_retort *errp)
{
  if (already_failed (errp))
    return NULL;

  if (hdr_len < 1 || 0 != memcmp (mem, ob_binary_header, 4))
    {
      *errp = POOL_CORRUPT;
      OB_LOG_ERROR_CODE (0x20110041, "POOL_CORRUPT\n");
      return NULL;
    }

  mem = 1 + (unt64 *) mem;
  hdr_len--;

  while (hdr_len > 2)
    {
      pool_chunk_header *hdr = (pool_chunk_header *) mem;
      // TODO: atomic ops instead?  (Probably not, since not mutable)
      if (hdr->len < 2 || hdr->len > hdr_len)
        {
          *errp = POOL_CORRUPT;
          OB_LOG_ERROR_CODE (0x20110042, "POOL_CORRUPT\n");
          return NULL;
        }
      if (sig == hdr->sig)
        return mem;
      hdr_len -= hdr->len;
      mem = hdr->len + (unt64 *) mem;
    }
  if (hdr_len != 0)
    {
      *errp = POOL_CORRUPT;
      OB_LOG_ERROR_CODE (0x20110043, "POOL_CORRUPT\n");
    }
  return NULL;
}

static pool_toc_t *init_pool_toc_v0 (byte *mem, unt64 capacity)
{
  assert (mem);
  pool_toc_t *result = NULL;
  if (capacity > 0)
    result = pool_toc_init (mem + POOL_MMAP_V0_HEADER_SIZE, capacity);
  return result;
}

static void initialize_v0_header (unt8 slaw_vers, byte *mem, unt64 toc_cap)
{
  const unt64 slav = slaw_vers; /* explicitly promote type to avoid surprises */
  POOL_MMAP_GET_MAGICV0 (mem) =
    (POOL_MMAP_MAGICV0 | (slav << POOL_MMAP_SLAW_VERSION_SHIFTY));
  if (toc_cap > 0)
    {
      OB_UNUSED pool_toc_t *ptoc = init_pool_toc_v0 (mem, toc_cap);
      assert (ptoc);
    }
}

#define SZ(x) (x.hdr.len = sizeof (x) / sizeof (x.hdr.len))

static void initialize_v1_header (unt8 slaw_vers, byte *mem, unt64 toc_cap)
{
  default_v1_header *h = (default_v1_header *) mem;
  OB_CLEAR (*h);

  unt16 flags = 0;
  if (ob_i_am_big_endian ())
    flags |= PLASMA_BINARY_FILE_FLAG_BIG_ENDIAN_SLAW;

  memcpy (h->magic, ob_binary_header, 4);
  h->magic[4] = slaw_vers;
  h->magic[5] = PLASMA_BINARY_FILE_TYPE_POOL;
  h->magic[6] = (flags >> 8);
  h->magic[7] = (flags & 0xff);
  h->conf.hdr.sig = POOL_CHUNK_CONF;
  h->perm.hdr.sig = POOL_CHUNK_PERM;
  h->ptrs.hdr.sig = POOL_CHUNK_PTRS;

  SZ (h->conf);
  SZ (h->perm);
  SZ (h->ptrs);

  byte *next = mem + sizeof (*h);
  if (toc_cap > 0)
    {
      pool_chunk_header *ih = (pool_chunk_header *) next;
      unt64 elephant = sizeof (*ih) + pool_toc_room (toc_cap);
      ih->sig = POOL_CHUNK_TOC;
      ih->len = elephant / sizeof (ih->len);
      next += sizeof (*ih);
      OB_UNUSED volatile void *foo = pool_toc_init (next, toc_cap);
      assert (foo == (volatile void *) next);
      next += pool_toc_room (toc_cap);
    }

  h->conf.header_size = (next - mem);
  h->conf.mmap_version = 1;  // well, we are initialize_v1_header(), after all!
}

#undef SZ

static pool_toc_t *read_pool_toc_v0 (byte *mem, unt64 header_size,
                                     ob_retort *err)
{
  assert (mem);
  pool_toc_t *result = NULL;
  *err = OB_OK;
  if (header_size > POOL_MMAP_V0_HEADER_SIZE)
    {
      result = pool_toc_read (mem + POOL_MMAP_V0_HEADER_SIZE);
      if (result)
        {
          unt64 room = pool_toc_room (pool_toc_capacity (result));
          if (header_size != room + POOL_MMAP_V0_HEADER_SIZE)
            {
              // This means that the version of the pool index in the file
              // is different than the version currently supported.
              // Rather than rudely aborting the user's entire program,
              // let's just return an error code like we do for
              // other pool errors.
              // XXX: debatable whether to return POOL_WRONG_VERSION or
              // POOL_CORRUPT.  Although wrong version is the most likely
              // cause, I decided to return the more general POOL_CORRUPT
              // because there aren't explicit version numbers for pool
              // indexes (I think there ought to be, but that's a whole
              // other thing), so all we really know is that the index
              // isn't what we're expecting, and it could have been
              // corrupted for other reasons.
              // assert (!"Pool room mismatch");
              *err = POOL_CORRUPT;
              OB_LOG_ERROR_CODE (0x20110044, "POOL_CORRUPT\n");
              result = NULL;
            }
        }
    }
  return result;
}

static pool_toc_t *read_pool_toc_v1 (byte *mem, unt64 header_size,
                                     ob_retort *errp)
{
  // we are given header size in bytes, but get_chunk wants
  // size in octs
  byte *idx = (byte *) get_chunk (mem, header_size / sizeof (unt64),
                                  POOL_CHUNK_TOC, errp);
  if (already_failed (errp) || !idx)
    return NULL;
  pool_toc_t *result = pool_toc_read (idx + sizeof (pool_chunk_header));
  if (result)
    {
      pool_chunk_header *h = (pool_chunk_header *) idx;
      const unt64 room = pool_toc_room (pool_toc_capacity (result));
      if (room != (sizeof (unt64) * h->len) - sizeof (pool_chunk_header))
        {
          OB_LOG_ERROR_CODE (0x2011003b,
                             "Pool room mismatch:\n"
                             "%" OB_FMT_64 "u != (%" OB_FMT_SIZE "u * "
                             "%" OB_FMT_64 "u) - %" OB_FMT_SIZE "u\n",
                             room, sizeof (unt64), h->len,
                             sizeof (pool_chunk_header));
          *errp = POOL_CORRUPT;
          result = NULL;
        }
    }
  else
    {
      OB_LOG_INFO_CODE (0x2011003a,
                        "index chunk present but index not recognized\n");
    }
  return result;
}

static ob_retort read_v0_header (pool_mmap_data *d)
{
  // We keep the header at the beginning of the file
  d->oldnew = (pool_mmap_oldnew *) d->mem;

  d->conf_chunk->mmap_version = 0;
  d->conf_chunk->flags = 0;

  if (get_header_size (d) >= POOL_MMAP_V0_HEADER_SIZE)
    {
      unt64 magic = POOL_MMAP_GET_MAGICV0 (d->mem);
      d->slaw_version = magic >> POOL_MMAP_SLAW_VERSION_SHIFTY;
      magic &= ~(OB_CONST_U64 (0xff) << POOL_MMAP_SLAW_VERSION_SHIFTY);
      if (magic != POOL_MMAP_MAGICV0)
        {
          OB_LOG_ERROR_CODE (0x20110045, "POOL_CORRUPT\n");
          return POOL_CORRUPT;
        }
      ob_retort pret = OB_OK;
      d->ptoc = read_pool_toc_v0 (d->mem, get_header_size (d), &pret);
      if (pret < OB_OK)
        return pret;
      if (!d->ptoc && POOL_MMAP_V0_HEADER_SIZE < get_header_size (d))
        OB_LOG_INFO_CODE (0x20110024,
                          "hose '%s' pool '%s': header greater than minimum, "
                          "but no pool index found.",
                          hname (d), pname (d));
    }
  else
    {
      // Without a version, default to oldest
      // (because newer pools will always have a version)
      d->slaw_version = 1;
      d->ptoc = NULL;
    }

  return OB_OK;
}

static ob_retort read_v1_header (pool_mmap_data *d)
{
  ob_retort tort = OB_OK;
  unt8 x;

  d->slaw_version = d->mem[4];
  if ((x = d->mem[5]) != PLASMA_BINARY_FILE_TYPE_POOL)
    {
      OB_LOG_ERROR_CODE (0x2011003f, "expected 0x%02x but got 0x%02x\n",
                         PLASMA_BINARY_FILE_TYPE_POOL, x);
      return POOL_CORRUPT;
    }

  // This header size was read by the "bootstrap" function
  const unt64 header_bytes = get_header_size (d);
  const unt64 header_octs = header_bytes / sizeof (unt64);
  d->conf_chunk =
    (pool_chunk_conf *) get_chunk (d->mem, header_octs, POOL_CHUNK_CONF, &tort);
  if (already_failed (&tort))
    return tort;
  const unt64 header_bytes2 = get_header_size (d);
  if (header_bytes != header_bytes2)
    {
      OB_LOG_ERROR_CODE (0x20110040,
                         "mismatching header sizes: %" OB_FMT_64
                         "u and %" OB_FMT_64 "u\n",
                         header_bytes, header_bytes2);
      return POOL_CORRUPT;
    }
  pool_chunk_ptrs *ptrs =
    (pool_chunk_ptrs *) get_chunk (d->mem, header_octs, POOL_CHUNK_PTRS, &tort);
  if (already_failed (&tort))
    return tort;
  d->oldnew = &(ptrs->ptrs);
  d->ptoc = read_pool_toc_v1 (d->mem, header_bytes, &tort);
  d->perm_chunk =
    (pool_chunk_perm *) get_chunk (d->mem, header_octs, POOL_CHUNK_PERM, &tort);
  return tort;
}

static unt64 size_of_v0_header (unt64 toc_cap)
{
  unt64 hs = POOL_MMAP_V0_HEADER_SIZE;
  if (toc_cap > 0)
    hs += pool_toc_room (toc_cap);
  return hs;
}

static unt64 size_of_v1_header (unt64 toc_cap)
{
  unt64 hs = sizeof (default_v1_header);
  if (toc_cap > 0)
    hs += pool_toc_room (toc_cap) + sizeof (pool_chunk_header);
  return hs;
}

static const char CFG_KEY_FILE_SIZE[] = "file-size";
static const char CFG_KEY_HEADER_SIZE[] = "header-size";

static protein make_mmap_config_protein (unt64 fsize, unt64 hsize,
                                         OB_UNUSED unt64 toc_cap)
{
  slaw conf_b =
    slaw_map_inline_cf (CFG_KEY_FILE_SIZE, slaw_unt64 (fsize),
                        CFG_KEY_HEADER_SIZE, slaw_unt64 (hsize), NULL);
  return conf_b ? protein_from_ff (NULL, conf_b) : NULL;
}

static bool read_config_unt64 (bprotein conf, const char *key, unt64 *u,
                               ob_retort *r)
{
  bslaw size_s = slaw_map_find_c (protein_ingests (conf), key);
  // type-check and allow upconversion on slaw type int32.
  *r = slaw_to_unt64 (size_s, u);
  return (*r == OB_OK);
}

static ob_retort read_mmap_config (bprotein conf, unt64 *fsize, unt64 *hsize)
{
  ob_retort result = OB_OK;
  // file size is required; error if it is not found
  if (!read_config_unt64 (conf, CFG_KEY_FILE_SIZE, fsize, &result))
    return POOL_CONFIG_BADTH;
  // header size is optional; use 0 which will later get converted to
  // the minimum header size
  if (!read_config_unt64 (conf, CFG_KEY_HEADER_SIZE, hsize, &result))
    *hsize = 0;
  return OB_OK;
}

static ob_retort pool_mmap_bootstrap_config_v0 (pool_mmap_data *d)
{
  pool_hose ph = d->ph;

  protein conf;
  ob_retort pret = pool_read_config_file (ph->method, ph->name, &conf);

  unt64 fsize = 0;
  unt64 hsize = 0;

  if (OB_OK == pret)
    {
      pret = read_mmap_config (conf, &fsize, &hsize);
      protein_free (conf);
      d->conf_chunk->file_size = fsize;
      d->conf_chunk->header_size =
        hsize > 0 ? hsize : sizeof (pool_mmap_oldnew);
    }

  return pret;
}

static ob_retort pool_mmap_bootstrap_config_v1_prime (const char *path,
                                                      va_list vargs);

static ob_retort pool_mmap_bootstrap_config_v1 (pool_mmap_data *d)
{
  return pool_mmap_call_with_backing_file (
    d->ph->name, d->ph->pool_directory_version, false,
    pool_mmap_bootstrap_config_v1_prime, d);
}

static ob_retort pool_mmap_bootstrap_config_v1_prime (const char *path,
                                                      va_list vargs)
{
  pool_mmap_data *d = va_arg (vargs, pool_mmap_data *);

  FILE *f = ob_fopen_cloexec (path, "rb");
  if (!f)
    return ob_errno_to_retort (errno);

  default_v1_header h;
  OB_INVALIDATE (h);
  const size_t a_little = sizeof (h.magic) + sizeof (h.conf);
  const size_t actual = fread (&h, 1, a_little, f);
  ob_retort pret = OB_OK;
  if (actual != a_little)
    {
      if (ferror (f))  // for any other error
        pret = ob_errno_to_retort (errno);
      else
        {
          pret = POOL_CORRUPT;  // for unexpected end-of-file
          int64 sz = -1;
#ifdef _MSC_VER
          LARGE_INTEGER thanks_bill;
          if (GetFileSizeEx ((HANDLE) _get_osfhandle (fileno (f)),
                             &thanks_bill))
            sz = thanks_bill.QuadPart;
#endif
          OB_LOG_ERROR_CODE (0x20110046,
                             "POOL_CORRUPT: %" OB_FMT_SIZE "u != %" OB_FMT_SIZE
                             "u; feof = %d; size = %" OB_FMT_64
                             "d; path = %s\n",
                             actual, a_little, feof (f), sz, path);
        }
    }

  if (0 != fclose (f))
    ob_err_accum (&pret, ob_errno_to_retort (errno));

  if (pret < OB_OK)
    return pret;

  if (0 != memcmp (h.magic, ob_binary_header, 4))
    {
      OB_LOG_ERROR_CODE (0x20110047,
                         "POOL_CORRUPT: magic was %02x %02x %02x %02x\n"
                         "   but should have been %02x %02x %02x %02x\n",
                         h.magic[0], h.magic[1], h.magic[2], h.magic[3],
                         ob_binary_header[0], ob_binary_header[1],
                         ob_binary_header[2], ob_binary_header[3]);
      return POOL_CORRUPT;
    }

  if (h.magic[5] != PLASMA_BINARY_FILE_TYPE_POOL)
    {
      OB_LOG_ERROR_CODE (0x20110048, "POOL_CORRUPT: %d != %d\n", h.magic[5],
                         PLASMA_BINARY_FILE_TYPE_POOL);
      return POOL_CORRUPT;
    }

  /* Although the chunk format is mostly meant to be order-independent,
   * we're going to require that the conf chunk be first, to avoid
   * having to go hunt for it before we mmap the file.
   */
  if (h.conf.hdr.sig != POOL_CHUNK_CONF)
    {
      OB_LOG_ERROR_CODE (0x20110049,
                         "POOL_CORRUPT: 0x%016" OB_FMT_64 "x != "
                         "0x%016" OB_FMT_64 "x\n",
                         h.conf.hdr.sig, POOL_CHUNK_CONF);
      return POOL_CORRUPT;
    }

  if (h.conf.hdr.len < sizeof (h.conf) / sizeof (h.conf.hdr.len))
    {
      OB_LOG_ERROR_CODE (
        0x2011004a, "POOL_CORRUPT: %" OB_FMT_64 "d < %" OB_FMT_SIZE "u\n",
        h.conf.hdr.len, sizeof (h.conf) / sizeof (h.conf.hdr.len));
      return POOL_CORRUPT;
    }

  if (h.conf.file_size < h.conf.header_size)
    {
      OB_LOG_ERROR_CODE (0x2011004b,
                         "POOL_CORRUPT: %" OB_FMT_64 "d < %" OB_FMT_64 "d\n",
                         h.conf.file_size, h.conf.header_size);
      return POOL_CORRUPT;
    }

  if (h.conf.header_size < sizeof (h.conf.hdr.len) * (1 + h.conf.hdr.len))
    {
      OB_LOG_ERROR_CODE (0x2011004c, "POOL_CORRUPT\n");
      return POOL_CORRUPT;
    }

  memcpy (&(d->legacy_conf), &(h.conf), sizeof (d->legacy_conf));
  return OB_OK;
}

static ob_retort write_config_file_v0 (const char *name, pool_perms perms,
                                       unt64 file_size, unt64 header_size,
                                       unt64 toc_cap)
{
  protein conf = make_mmap_config_protein (file_size, header_size, toc_cap);
  if (!conf)
    return OB_NO_MEM;
  ob_retort pret = pool_write_config_file ("mmap", name, conf, perms.mode,
                                           perms.uid, perms.gid);
  protein_free (conf);
  return pret;
}

static ob_retort write_config_file_v1 (OB_UNUSED const char *name,
                                       OB_UNUSED pool_perms perms,
                                       OB_UNUSED unt64 file_size,
                                       OB_UNUSED unt64 header_size,
                                       OB_UNUSED unt64 toc_cap)
{
  // Nothing to do; v1 has no separate "mmap" config file
  return OB_OK;
}

static ob_retort delete_config_file_v0 (const char *name)
{
  return pool_remove_config_file ("mmap", name);
}

static ob_retort delete_config_file_v1 (OB_UNUSED const char *name)
{
  // Nothing to do; v1 has no separate "mmap" config file
  return OB_OK;
}

OB_CONST mmap_version_funcs pool_mmap_get_version_funcs (unt8 mmap_vers)
{
  mmap_version_funcs f;
  switch (mmap_vers)
    {
      case 0:
        f.size_of_header = size_of_v0_header;
        f.initialize_header = initialize_v0_header;
        f.read_header = read_v0_header;
        f.bootstrap = pool_mmap_bootstrap_config_v0;
        f.write_config_file = write_config_file_v0;
        f.delete_config_file = delete_config_file_v0;
        break;
      case 1:
        f.size_of_header = size_of_v1_header;
        f.initialize_header = initialize_v1_header;
        f.read_header = read_v1_header;
        f.bootstrap = pool_mmap_bootstrap_config_v1;
        f.write_config_file = write_config_file_v1;
        f.delete_config_file = delete_config_file_v1;
        break;
      default:
        OB_FATAL_BUG_CODE (0x20110036, "didn't expect %u\n", mmap_vers);
    }
  return f;
}

// See text file "versions.txt" in this directory for an explanation.
OB_CONST unt8
pool_mmap_version_from_directory_version (unt8 pool_directory_version)
{
  switch (pool_directory_version)
    {
      case POOL_DIRECTORY_VERSION_CONFIG_IN_FILE:
        return 0;
      case POOL_DIRECTORY_VERSION_CONFIG_IN_MMAP:
      case POOL_DIRECTORY_VERSION_SINGLE_FILE:
        return 1;
      default:
        OB_FATAL_BUG_CODE (0x2011003d, "didn't expect %u\n",
                           pool_directory_version);
    }
}

ob_retort pool_mmap_call_with_backing_file (const char *name,
                                            unt8 pool_directory_version,
                                            bool locked, backing_file_func bff,
                                            ...)
{
  char dir_path[PATH_MAX];
  ob_retort pret;

  pret = pool_build_pool_dir_path (dir_path, name);
  if (pret != OB_OK)
    return pret;

  const bool legacy =
    (POOL_DIRECTORY_VERSION_CONFIG_IN_FILE == pool_directory_version);
  slaw s;
  if (pool_directory_version == POOL_DIRECTORY_VERSION_SINGLE_FILE)
    s = slaw_string (dir_path);
  else
    s =
      slaw_string_format ("%s/%s%smmap-pool", dir_path,
                          legacy ? ob_basename (name) : "", legacy ? "." : "");
  if (!s)
    return OB_NO_MEM;

#ifndef _MSC_VER
  int fd;
  OB_INVALIDATE (fd);
  if (locked)
    {
      const char *path = slaw_string_emit (s);
      fd = ob_open_cloexec (path, O_RDONLY, 0);
      if (fd < 0)
        {
          const int erryes = errno;
          slaw_free (s);
          if (ENOENT == erryes)
            return POOL_NO_SUCH_POOL;
          else
            return ob_errno_to_retort (erryes);
        }
      while (flock (fd, LOCK_EX | LOCK_NB) < 0)
        {
          const int erryes = errno;
          if (EINTR != erryes)
            {
              OB_CHECK_POSIX_CODE (0x2011003e, close (fd));
              slaw_free (s);
              if (EWOULDBLOCK == erryes)
                return POOL_IN_USE;
              else
                return ob_errno_to_retort (erryes);
            }
        }
    }
#endif

  va_list vargs;
  va_start (vargs, bff);
  pret = bff (slaw_string_emit (s), vargs);
  va_end (vargs);
  slaw_free (s);
#ifndef _MSC_VER
  if (locked && 0 != close (fd))
    ob_err_accum (&pret, ob_errno_to_retort (errno));
#endif
  return pret;
}
