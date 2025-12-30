
/* (c)  oblong industries */

#include "libPlasma/c/slaw-interop.h"
#include "libPlasma/c/slaw-io.h"
#include "libPlasma/c/slaw.h"
#include "libPlasma/c/protein.h"
#include "libPlasma/c/private/plasma-private.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-endian.h"
#include "libLoam/c/ob-util.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* 8-byte header for binary protein/slaw files:
 *
 * ----- first four bytes are a magic number -----
 *
 * The rationale for the first two bytes is that
 * ff ff guarantees that this is not a legal UTF-8,
 * UTF-16, or UTF-32 file.  (Because the byte ff is
 * never allowed in UTF-8 at all.  And it can't
 * be UTF-16, because no matter whether you look at
 * it as big-endian or little endian, you get
 * U+FFFF, which is a noncharacter.  If you
 * attempt UTF-32 little endian, you get a character
 * whose least-significant 16 bits are ffff, which
 * Unicode guarantees to always be a non-character.
 * And if you attempt UTF-32 big endian, you get a
 * value which is much, much larger than U+10FFFF,
 * the largest legal Unicode value.)
 *
 * The second two bytes, when printed in hexadecimal,
 * look somewhat like the letters "oblo", the first
 * four letters of our company name.
 *
 * ----- last four bytes contain information about the file -----
 *
 * The remaining four bytes are allocated as one byte of
 * version, one byte of type, and two bytes of flags.
 *
 * The version is the slaw version of the slaw contained
 * in this file.  Currently, we always write version 2,
 * but can read either 1 or 2.  See the files slaw-v1.txt
 * and slaw-v2.txt for details of the two versions.
 *
 * The type is PLASMA_BINARY_FILE_TYPE_SLAW to indicate
 * that this is a slaw file.  Additional types
 * could be allocated in the future if we need to have
 * binary files that contain something other than slawx
 * or proteins.
 *
 * The flags are interpreted as a 16-bit big-endian
 * number, and current only one bit is used:
 * PLASMA_BINARY_FILE_FLAG_BIG_ENDIAN_SLAW.
 * Unrecognized flags are ignored.
 */
const byte ob_binary_header[4] = {0xff, 0xff, 0x0b, 0x10};

#define CURRENT_VERSION 2

static ob_retort private_write_binary_header (slaw_write_handler h,
                                              unt8 version, unt8 typ,
                                              unt16 flags)
{
  byte buf[8];

  memcpy (buf, ob_binary_header, 4);

  buf[4] = version;
  buf[5] = typ;
  /* write flags as big-endian */
  buf[6] = flags >> 8;
  buf[7] = flags & 0xff;

  return h.write (h.cookie, buf, sizeof (buf));
}

static ob_retort private_read_binary_header (slaw_read_handler h, unt8 *version,
                                             unt8 *typ, unt16 *flags)
{
  byte buf[8];
  size_t size_read;

  // OB_INVALIDATE (buf);
  // OB_INVALIDATE (size_read);

  ob_retort tort = h.read (h.cookie, buf, sizeof (buf), &size_read);
  if (tort < OB_OK)
    return tort;

  if (size_read < sizeof (buf))
    {
      OB_LOG_ERROR_CODE (0x20002000,
                         "binary slaw file (%" OB_FMT_SIZE
                         "u bytes) is less than 8 bytes long!\n",
                         size_read);
      return SLAW_WRONG_FORMAT;
    }
  else if (size_read > sizeof (buf))
    {
      OB_LOG_ERROR_CODE (
        0x20002005, "Didn't expect %" OB_FMT_SIZE "u to be greater than 8\n",
        size_read);
      return OB_UNKNOWN_ERR;
    }

  if (memcmp (buf, ob_binary_header, sizeof (ob_binary_header)) != 0)
    {
      OB_LOG_ERROR_CODE (0x20002001,
                         "binary slaw file does not begin with magic "
                         "number %02x %02x %02x %02x\n",
                         ob_binary_header[0], ob_binary_header[1],
                         ob_binary_header[2], ob_binary_header[3]);
      return SLAW_WRONG_FORMAT;
    }

  *version = buf[4];
  *typ = buf[5];
  *flags = buf[7] + (buf[6] << 8);

  return OB_OK;
}

typedef struct binary_slaw_input
{
  slaw_read_handler h;
  const slaw_vfuncs *vf;
  slaw_version version;
  bool big_endian;
} binary_slaw_input;

typedef struct binary_slaw_output
{
  slaw_write_handler h;
} binary_slaw_output;

bool ob_i_am_big_endian (void)
{
#ifdef __BIG_ENDIAN__
  return true;
#else
  return false;
#endif
}

static ob_retort binary_output_write (slaw_output f, bslaw s)
{
  binary_slaw_output *bso = (binary_slaw_output *) f->data;
  ob_retort tort = bso->h.write (bso->h.cookie, (const byte *) s,
                                 8 * (size_t) slaw_octlen (s));
  if (tort < OB_OK)
    return tort;

  /* There are many times when we want the slaw output stream to be
   * flushed, so for now let's do that automatically whenever we
   * write anything.  If there are reasons (performance?) not to do
   * this, then we might need to separate it out.
   */
  return bso->h.flush (bso->h.cookie);
}

static ob_retort binary_output_close (void *data)
{
  binary_slaw_output *bso = (binary_slaw_output *) data;
  ob_retort tort = bso->h.close (bso->h.cookie);
  free (bso);
  return tort;
}

static ob_retort binary_input_close (void *data)
{
  binary_slaw_input *bsi = (binary_slaw_input *) data;
  ob_retort tort = bsi->h.close (bsi->h.cookie);
  free (bsi);
  return tort;
}

ob_retort slaw_output_open_binary_handler (slaw_write_handler h,
                                           OB_UNUSED bslaw options,
                                           slaw_output *f)
{
  ob_retort err;

  err = private_write_binary_header (
    h, CURRENT_VERSION, PLASMA_BINARY_FILE_TYPE_SLAW,
    (ob_i_am_big_endian () ? PLASMA_BINARY_FILE_FLAG_BIG_ENDIAN_SLAW : 0));
  if (err != OB_OK)
    return err;

  *f = new_slaw_output ();
  if (!*f)
    return OB_NO_MEM;

  binary_slaw_output *bso =
    (binary_slaw_output *) calloc (1, sizeof (binary_slaw_output));
  if (!bso)
    {
      free (*f);
      *f = NULL;
      return OB_NO_MEM;
    }

  (*f)->wfunc = binary_output_write;
  (*f)->cfunc = binary_output_close;
  (*f)->data = bso;

  bso->h = h;

  return OB_OK;
}

ob_retort slaw_output_write (slaw_output f, bslaw s)
{
  return f->wfunc (f, s);
}

ob_retort slaw_output_close (slaw_output f)
{
  ob_retort err = f->cfunc (f->data);
  free (f);
  return err;
}

static ob_retort binary_input_read (slaw_input f, slaw *s)
{
  binary_slaw_input *bsi = (binary_slaw_input *) f->data;
  struct _slaw slawhead;
  struct _slaw swappedhead;
  unsigned int headWords = 1;
  unt64 octlen;
  slaw cole;
  ob_retort err = OB_OK;
  bool needToSwap = (bsi->big_endian != ob_i_am_big_endian ());
  size_t size_read;

  OB_INVALIDATE (size_read);

  if (bsi->version != SLAW_VERSION_CURRENT)
    {
      err = bsi->vf->vbinary_input_read (bsi->h, needToSwap, s);
      if (err >= OB_OK)
        err = slaw_convert_from (s, SLAW_ENDIAN_CURRENT, bsi->version);
      return err;  //                ^^ already converted endianness during read
    }

  /* read the first oct, to read to get the length */
  err = bsi->h.read (bsi->h.cookie, (byte *) &slawhead, sizeof (slawhead),
                     &size_read);
  if (err < OB_OK)
    return err;
  else if (size_read < sizeof (slawhead))
    return SLAW_END_OF_FILE;
  else if (size_read > sizeof (slawhead))
    {
      OB_LOG_ERROR_CODE (0x20002006,
                         "Didn't expect %" OB_FMT_SIZE
                         "u to be greater than %" OB_FMT_SIZE "u\n",
                         size_read, sizeof (slawhead));
      return OB_UNKNOWN_ERR;
    }

  swappedhead = slawhead;
  if (needToSwap)
    swappedhead.o = ob_swap64 (swappedhead.o);

  const unt64 octocat = octlen = slaw_octlen (&swappedhead);
  if (octlen == 0)
    return SLAW_UNIDENTIFIED_SLAW;

  // XXX: might want to validate octlen against the size of the file,
  // and/or the amount of available memory, to avoid malicious
  // slaw files (bug 537, and Coverity defect 10966 in trial database)
  cole = slaw_alloc (octlen);
  if (cole == NULL)
    return OB_NO_MEM;

  slaw_copy_octs_from_to (&slawhead, cole, headWords);
  *s = cole;
  cole += headWords;
  octlen -= headWords;

  /* read the rest of the slaw, if any */
  if (octlen > 0)
    {
      const size_t size_wanted = octlen * sizeof (slawhead);
      err = bsi->h.read (bsi->h.cookie, (byte *) cole, size_wanted, &size_read);
      if (err < OB_OK || size_read != size_wanted)
        {
          slaw_free (*s);
          if (err < OB_OK)
            OB_LOG_ERROR_CODE (0x20002007, "read callback returned '%s'\n",
                               ob_error_string (err));
          else
            OB_LOG_ERROR_CODE (0x20002008,
                               "wanted to read %" OB_FMT_SIZE "u bytes,\n"
                               "but only read  %" OB_FMT_SIZE "u bytes.\n",
                               size_wanted, size_read);
          return (err < OB_OK ? err : SLAW_CORRUPT_SLAW);
        }
    }

  if (needToSwap)
    err = slaw_swap (*s, *s + octocat);

  if (err != OB_OK)
    slaw_free (*s);

  return err;
}

ob_retort slaw_input_read (slaw_input f, slaw *s)
{
  return f->rfunc (f, s);
}

ob_retort slaw_input_close (slaw_input f)
{
  ob_retort err = f->cfunc (f->data);
  free (f);
  return err;
}

static ob_retort make_binary_input_file (slaw_read_handler h, slaw_input *f,
                                         bool big_endian, slaw_version version)
{
  const slaw_vfuncs *vf = get_vfuncs (version);

  if (!vf)
    return SLAW_WRONG_VERSION;

  binary_slaw_input *bsi =
    (binary_slaw_input *) calloc (1, sizeof (binary_slaw_input));
  if (!bsi)
    return OB_NO_MEM;

  *f = new_slaw_input ();
  if (!*f)
    {
      free (bsi);
      return OB_NO_MEM;
    }

  (*f)->rfunc = binary_input_read;
  (*f)->cfunc = binary_input_close;
  (*f)->data = bsi;

  bsi->h = h;
  bsi->vf = vf;
  bsi->version = version;
  bsi->big_endian = big_endian;

  return OB_OK;
}

ob_retort slaw_input_open_binary_handler (slaw_read_handler h, slaw_input *f)
{
  unt8 version = ~0;
  unt8 typ = ~0;
  unt16 flags = ~0;
  ob_retort err;

  err = private_read_binary_header (h, &version, &typ, &flags);
  if (err != OB_OK)
    return err;

  if (version > CURRENT_VERSION)
    {
      OB_LOG_ERROR_CODE (0x20002002,
                         "binary slaw file is version %u but "
                         "expected version %u or earlier\n",
                         version, CURRENT_VERSION);
      return SLAW_WRONG_VERSION;
    }

  if (typ != PLASMA_BINARY_FILE_TYPE_SLAW)
    {
      OB_LOG_ERROR_CODE (0x20002003,
                         "binary slaw file has type %u but "
                         "expected type %u\n",
                         typ, PLASMA_BINARY_FILE_TYPE_SLAW);
      return SLAW_WRONG_FORMAT;
    }

  return make_binary_input_file (
    h, f, ((flags & PLASMA_BINARY_FILE_FLAG_BIG_ENDIAN_SLAW) != 0), version);
}

slaw_input new_slaw_input (void)
{
  slaw_input x = (slaw_input) calloc (1, sizeof (struct slaw_input_struct));
  return x;
}

slaw_output new_slaw_output (void)
{
  slaw_output x = (slaw_output) calloc (1, sizeof (struct slaw_output_struct));
  return x;
}
