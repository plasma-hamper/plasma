
/* (c)  oblong industries and ANIMIST contributors */

#include "libLoam/c/ob-types.h"
#include "libLoam/c/ob-string.h"
#include "libLoam/c/ob-util.h"
#include "libPlasma/c/slaw.h"
#include "libPlasma/c/protein.h"
#include "libPlasma/c/private/plasma-private.h"
#include "libPlasma/c/private/plasma-util.h"
#include "libPlasma/c/slaw-string.h"
#include "libPlasma/c/slaw-walk.h"

#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char hexdigs[16] = "0123456789ABCDEF";
static const char escapes[7]  = "abtnvfr";

static inline int expand_buffer (char **bufp, size_t *capp)
{
  size_t newcap = 2 * (*capp);
  char  *newbuf = realloc (*bufp, newcap);

  if (newbuf == 0)
    return -1;

  *bufp = newbuf;
  *capp = newcap;
  return 0;
}

static inline char spew_escape_char_single (unsigned char c)
{
  if (c >= 7 && c < 14)
    return escapes[c - 7];
  else if (c == '\\' || c == '"')
    return c;
  else if (c == 27)
    return 'e';
  else
    return 0;
}

static inline size_t spew_escape_char (char         *dest,
                                       unsigned char c,
                                       bool          allow_utf8)
{
  if (allow_utf8 && c >= 128)
    {
      dest[0] = c;
      return 1;
    }

  char escaped = spew_escape_char_single (c);

  if (escaped != 0)
    {
      dest[0] = '\\';
      dest[1] = escaped;
      return 2;
    }
  else if (c < ' ' || c > '~')
    {
      dest[0] = '\\';
      dest[1] = 'x';
      dest[2] = hexdigs[c / 16];
      dest[3] = hexdigs[c % 16];
      return 4;
    }
  else
    {
      dest[0] = c;
      return 1;
    }
}

static char *spew_escape_string (const char *str, size_t len)
{
  size_t buf_len = 0;
  size_t buf_cap = len + 16;
  char  *buf     = (char *) malloc (buf_cap);
  size_t i;
  bool   invalid, unused;

  if (buf == NULL)
    return NULL;

  ob_analyze_utf8 (str, (int64) len, &invalid, &unused);

  for (i = 0; i < len; i++)
    {
      size_t available = buf_cap - buf_len;
      if (available < 5)
        {
          if (expand_buffer (&buf, &buf_cap) < 0)
            {
              free (buf);
              return NULL;
            }
        }

      buf_len += spew_escape_char (buf + buf_len, str[i], !invalid);
    }

  buf[buf_len] = 0;
  return buf;
}

typedef void (*fwfunc) (void *v, const char *fmt, ...) OB_FORMAT (printf, 2, 3);

static void format_to_file (void *v, const char *fmt, ...)
{
  FILE *whither = (FILE *) v;
  va_list vargs;
  va_start (vargs, fmt);
  vfprintf (whither, fmt, vargs);
  va_end (vargs);
}

static void format_to_slabu (void *v, const char *fmt, ...)
{
  slabu *sb = (slabu *) v;
  va_list vargs;
  va_start (vargs, fmt);
  slabu_list_add_x (sb, slaw_string_vformat (fmt, vargs));
  va_end (vargs);
}

/* ovew_struct ("overview struct") is the cookie passed to slaw_walk()
 * when rendering a numeric slaw's value as a human-readable string
 * for the slaw_spew_overview() family of functions.  slaw_walk()
 * calls the ovew_* callbacks registered in ovew_handler, which use
 * this struct to emit formatted text and track separator state across
 * nested numeric contexts (array, vector, complex).
 *
 * Separator logic (managed by ovew_sep()):
 *   - The first element in any context emits no separator.
 *   - Subsequent array elements are separated by " ; ".
 *   - Subsequent vector components are separated by ", ".
 *   - The imaginary part of a complex number is separated by " + j".
 *
 * Context switching:
 *   - ovew_vector_begin() redirects inc to v (", " separators) and
 *     ovew_vector_end() restores it to a (" ; " separators).
 *   - ovew_complex_begin() saves the current inc in csave, then redirects
 *     inc to c (" + j" separator); ovew_complex_end() restores inc from
 *     csave.
 *
 * Example output for an array of two 2-D complex-float vectors:
 *   VFLOAT32C/A(2) = [(1.0 + j2.0, 3.0 + j4.0) ; (5.0 + j6.0, 7.0 + j8.0)]
 */
typedef struct ovew_struct
{
  void *whither;  /* output destination: FILE * or slabu *, passed
                   * to func */
  fwfunc func;    /* formatting function: format_to_file or
                   * format_to_slabu */
  int64 *inc;     /* points to whichever counter is active (c, v, or a),
                   * determining the separator and element count for the
                   * current nesting level */
  int64 *csave;   /* saved inc from before entering a complex context,
                   * restored by ovew_complex_end() */
  int64 c;        /* element count within the current complex number
                   * (0 = real part, 1 = imaginary part) */
  int64 v;        /* element count within the current vector
                   * (0 = first component, 1+ = subsequent components) */
  int64 a;        /* element count at the array (outermost) level
                   * (0 = first element, 1+ = subsequent elements) */
} ovew_struct;

static const char *ovew_sep (ovew_struct *vw)
{
  const char *sep = "";
  if (*(vw->inc) > 0)
    {
      if (vw->inc == &(vw->c))
        sep = " + j";
      else if (vw->inc == &(vw->v))
        sep = ", ";
      else if (vw->inc == &(vw->a))
        sep = " ; ";
    }
  (*(vw->inc))++;
  return sep;
}

static ob_retort ovew_int (void *cookie, int64 val, int bits)
{
  ovew_struct *vw = (ovew_struct *) cookie;
  vw->func (vw->whither, "%s%" OB_FMT_64 "d", ovew_sep (vw), val);
  return OB_OK;
}

static ob_retort ovew_unt (void *cookie, unt64 val, int bits)
{
  ovew_struct *vw = (ovew_struct *) cookie;
  vw->func (vw->whither, "%s%" OB_FMT_64 "u", ovew_sep (vw), val);
  return OB_OK;
}

static ob_retort ovew_float (void *cookie, float64 val, int bits)
{
  ovew_struct *vw = (ovew_struct *) cookie;
  vw->func (vw->whither, "%s%f", ovew_sep (vw), val);
  return OB_OK;
}

static ob_retort ovew_array_begin (void *cookie, int64 unused, int notused)
{
  ovew_struct *vw = (ovew_struct *) cookie;
  vw->func (vw->whither, "[");
  return OB_OK;
}

static ob_retort ovew_array_end (void *cookie)
{
  ovew_struct *vw = (ovew_struct *) cookie;
  vw->func (vw->whither, "]");
  return OB_OK;
}

static ob_retort ovew_vector_begin (void *cookie, int64 unused)
{
  ovew_struct *vw = (ovew_struct *) cookie;
  vw->func (vw->whither, "%s(", ovew_sep (vw));
  vw->v = 0;
  vw->inc = &(vw->v);
  return OB_OK;
}

static ob_retort ovew_vector_end (void *cookie)
{
  ovew_struct *vw = (ovew_struct *) cookie;
  vw->func (vw->whither, ")");
  vw->inc = &(vw->a);
  return OB_OK;
}

static ob_retort ovew_complex_begin (void *cookie)
{
  ovew_struct *vw = (ovew_struct *) cookie;
  const char *sep = ovew_sep (vw);
  if (*sep)
    vw->func (vw->whither, "%s", sep);
  vw->c = 0;
  vw->csave = vw->inc;
  vw->inc = &(vw->c);
  return OB_OK;
}

static ob_retort ovew_complex_end (void *cookie)
{
  ovew_struct *vw = (ovew_struct *) cookie;
  vw->inc = vw->csave;
  return OB_OK;
}

static ob_retort ovew_array_empty (void *cookie, int vecsize, bool isMVec,
                                   bool isComplex, bool isUnsigned,
                                   bool isFloat, int bits)
{
  ovew_struct *vw = (ovew_struct *) cookie;
  vw->func (vw->whither, "[]");
  return OB_OK;
}

static const slaw_handler ovew_handler = {NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          ovew_array_begin,
                                          ovew_array_end,
                                          ovew_vector_begin,
                                          ovew_vector_end,
                                          ovew_vector_begin,
                                          ovew_vector_end,
                                          ovew_complex_begin,
                                          ovew_complex_end,
                                          NULL,
                                          NULL,
                                          ovew_int,
                                          ovew_unt,
                                          ovew_float,
                                          ovew_array_empty,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL};

#define _FW(...) func (whither, __VA_ARGS__)

static void slaw_spew_numeric_ovewview (bslaw s, fwfunc func, void *whither)
{
  int slinc =
    slaw_is_numeric_8 (s)
      ? 1
      : (slaw_is_numeric_16 (s) ? 2 : (slaw_is_numeric_32 (s) ? 4 : 8));
  bool isV = slaw_is_numeric_vector (s);
  bool isM = slaw_is_numeric_multivector (s);
  bool isC = slaw_is_numeric_complex (s);
  bool isA = slaw_is_numeric_array (s);

  int vecWid = slaw_numeric_vector_dimension (s);
  unt64 arrWid = slaw_numeric_array_count (s);

  if (isV)
    _FW ("V%d", vecWid);
  else if (isM)
    _FW ("M%d", vecWid);
  _FW ("%s%d", (slaw_is_numeric_float (s)
                  ? "FLOAT"
                  : (slaw_is_numeric_int (s)
                       ? "INT"
                       : (slaw_is_numeric_unt (s) ? "UNT" : "<unkn>"))),
       8 * slinc);

  if (isC)
    _FW ("C");
  if (isA)
    _FW ("/A(%" OB_FMT_64 "u"
         ")",
         arrWid);
  _FW (" = ");

  ovew_struct vw;
  vw.whither = whither;
  vw.func = func;
  vw.inc = &vw.a;
  vw.csave = NULL;
  vw.c = 0;
  vw.v = 0;
  vw.a = 0;

  slaw_walk (&vw, &ovew_handler, s);
}

typedef struct slaw_spew_cfg
{
  bslaw start;
  int   num_digits;
  bool  use_relative_offset;
  bool  rude_ascii;
  bool  escape_strings;
} slaw_spew_cfg;

static unt64 slaw_spew_diff (bslaw start, bslaw here)
{
  const unt8 *p1   = (const unt8 *) start;
  const unt8 *p2   = (const unt8 *) here;
  ptrdiff_t   diff = p2 - p1;

  return (unt64) diff;
}

static void slaw_spew_internal (bslaw                s,
                                fwfunc               func,
                                void                *whither,
                                const char          *prolo,
                                const slaw_spew_cfg *cfg)
{
  if (!prolo)
    prolo = "";
  if (!s)
    {
      _FW ("%s[no slaw -- NULL]", prolo);
      return;
    }
  if (cfg && cfg->use_relative_offset)
    {
      unt64 offset = slaw_spew_diff (cfg->start, s);
      _FW ("%sslaw[%" OB_FMT_64 "uo.0x%0*" OB_FMT_64 "x]: ",
           prolo, slaw_octlen (s), cfg->num_digits, offset);
    }
  else
    {
      _FW ("%sslaw[%" OB_FMT_64 "uo.%p]: ", prolo, slaw_octlen (s), s);
    }
  if (slaw_is_string (s))
    {
      int64       len = slaw_string_emit_length (s);
      const char *str = slaw_string_emit (s);
      char       *buf = NULL;

      if (cfg && cfg->escape_strings && len > 0)
        {
          buf = spew_escape_string (str, (size_t) len);
          if (buf != NULL)
            str = buf;
        }

      _FW ("STR(%" OB_FMT_64 "d): \"%s\"", len, str);
      free (buf);
    }
  else if (slaw_is_cons (s))
    {
      size_t l = strlen (prolo);
      char *down = (char *) malloc (l + 5);
      strcpy (down, prolo);
      strcpy (down + l, " L: ");
      _FW ("CONS:\n");
      slaw_spew_internal (slaw_cons_emit_car (s), func, whither, down, cfg);
      *(down + l + 1) = 'R';
      _FW ("\n");
      slaw_spew_internal (slaw_cons_emit_cdr (s), func, whither, down, cfg);
      free (down);
    }
  else if (slaw_is_numeric (s))
    slaw_spew_numeric_ovewview (s, func, whither);
  else if (slaw_is_list_or_map (s))
    {
      unt64 q, n = slaw_list_count (s);
      _FW ("%s (%" OB_FMT_64 "u elems): {", (slaw_is_map (s) ? "MAP" : "LIST"),
           n);
      q = 1;
      bslaw ess = slaw_list_emit_first (s);
      size_t l = strlen (prolo);
      const size_t extra = 32;
      char *prefix = (char *) malloc (l + extra);
      strcpy (prefix, prolo);
      for (; ess != NULL; q++, ess = slaw_list_emit_next (s, ess))
        {
          _FW ("\n");
          snprintf (prefix + l, extra, " %" OB_FMT_64 "u: ", q);
          slaw_spew_internal (ess, func, whither, prefix, cfg);
        }
      free (prefix);
      _FW ("\n%s }", prolo);
    }
  else if (slaw_is_nil (s))
    {
      _FW ("NIL.");
    }
  else if (slaw_is_protein (s))
    {
      bslaw descrips = protein_descrips (s);
      bslaw ingests = protein_ingests (s);
      int64 rudeLen;
      const unt8 *rude = (const unt8 *) protein_rude (s, &rudeLen);
      _FW ("PROT: ((\n");
      if (descrips)
        {
          _FW ("%sdescrips:\n", prolo);
          slaw_spew_internal (descrips, func, whither, prolo, cfg);
          _FW ("\n");
        }
      if (ingests)
        {
          _FW ("%singests:\n", prolo);
          slaw_spew_internal (ingests, func, whither, prolo, cfg);
          _FW ("\n");
        }
      if (rudeLen > 0)
        {
          int64 i;
          _FW ("%srude data: %" OB_FMT_64 "d bytes", prolo, rudeLen);

          if (cfg && cfg->rude_ascii)
            {
              char rudeBuf[OB_HEX_LINE_LEN];
              _FW ("\n");
              for (i = 0; i < rudeLen; i += 16)
                {
                  ob_fmt_hex_line (rudeBuf,
                                   rude + i,
                                   (size_t) (rudeLen - i));
                  _FW ("%s %s\n", prolo, rudeBuf);
                }
            }
          else
            {
              for (i = 0; i < rudeLen; i++)
                {
                  if (i % 16 == 0)
                    _FW ("\n%s", prolo);
                  _FW (" %02x", rude[i]);
                }
              _FW ("\n");
            }
        }
      _FW ("%s ))", prolo);
    }
  else if (slaw_is_boolean (s))
    {
      _FW ("BOOLEAN: %s", (*slaw_boolean_emit (s) ? "true" : "false"));
    }
  else
    {
      _FW ("[???]");
    }
}

#undef _FW

static void slaw_spew_internal_flags (bslaw                s,
                                      fwfunc               func,
                                      void                *whither,
                                      unt32                flags,
                                      const char          *prolo)
{
  bool use_relative_offset = ((flags & SLAW_SPEW_FLAG_REL_OFF) != 0);

  slaw_spew_cfg cfg;
  OB_CLEAR(cfg);
  cfg.use_relative_offset = use_relative_offset;
  cfg.rude_ascii          = ((flags & SLAW_SPEW_FLAG_RUDE_ASCII) != 0);
  cfg.escape_strings      = ((flags & SLAW_SPEW_FLAG_ESCAPE_STRINGS) != 0);
  cfg.start               = s;

  if (use_relative_offset)
    {
      char buf[24];
      int64 len = slaw_len (s) - 1;
      if (len < 1)
        len = 1;
      snprintf (buf, sizeof (buf), "%" OB_FMT_64 "x", len);
      cfg.num_digits = strlen (buf);
    }

  slaw_spew_internal (s, func, whither, prolo, &cfg);
}


void slaw_spew_overview_ex (bslaw       s,
                            FILE       *whither,
                            unt32       flags,
                            const char *prolo)
{
  slaw_spew_internal_flags (s,
                            format_to_file,
                            (void *) whither,
                            flags,
                            prolo);
}

void slaw_spew_overview (bslaw s, FILE *whither, const char *prolo)
{
  slaw_spew_overview_ex (s, whither, 0, prolo);
}

void slaw_spew_overview_to_stderr (bslaw s)
{
  slaw_spew_overview (s, stderr, NULL);
}

slaw slaw_spew_overview_to_string_ex (bslaw       s,
                                      unt32       flags,
                                      const char *prolo)
{
  slabu *sb = slabu_new ();
  if (!sb)
    return NULL;

  slaw_spew_internal_flags (s, format_to_slabu, (void *) sb, flags, prolo);
  return slaw_strings_join_slabu_f (sb, NULL);
}

slaw slaw_spew_overview_to_string (bslaw s)
{
  return slaw_spew_overview_to_string_ex (s, 0, NULL);
}

typedef struct slaw_spew_func_ctx
{
  slaw_spew_func func;
  void          *cookie;
  ob_retort      ret;
  char          *buf;
  size_t         len;
  size_t         capacity;
  size_t         chunk_size;
} slaw_spew_func_ctx;

static void flush_func_buf (slaw_spew_func_ctx *ctx)
{
  if (ctx->ret >= OB_OK && ctx->len > 0)
    {
      ctx->func (ctx->cookie, ctx->buf, ctx->len);
      ctx->len = 0;
    }
}

static void format_to_func (void *v, const char *fmt, ...)
{
  slaw_spew_func_ctx *ctx = (slaw_spew_func_ctx *) v;

  if (ctx->ret < OB_OK)
    return;

  size_t  available = ctx->capacity - ctx->len;
  va_list vargs;
  va_start (vargs, fmt);
  int retlen = vsnprintf (ctx->buf + ctx->len, available, fmt, vargs);
  int e      = errno;
  va_end (vargs);

  if (retlen < 0)
    {
      ctx->ret = ob_errno_to_retort (e);
    }
  else if (available > (size_t) retlen)
    {
      ctx->len += (size_t) retlen;
    }
  else
    {
      size_t newcap = ctx->capacity + (size_t) retlen + 1;
      char  *newbuf = (char *) realloc (ctx->buf, newcap);

      if (newbuf == NULL)
        {
          ctx->ret = OB_NO_MEM;
        }
      else
        {
          ctx->buf      = newbuf;
          ctx->capacity = newcap;
          available     = ctx->capacity - ctx->len;
          va_start (vargs, fmt);
          retlen =
            vsnprintf (ctx->buf + ctx->len, ctx->capacity, fmt, vargs);
          e = errno;
          va_end (vargs);

          if (retlen < 0)
            ctx->ret = ob_errno_to_retort (e);
          else if (available > (size_t) retlen)
            ctx->len += (size_t) retlen;
          else
            ctx->ret = OB_UNKNOWN_ERR; /* unlikely, maybe impossible? */
        }
    }

  if (ctx->len >= ctx->chunk_size)
    flush_func_buf (ctx);
}

ob_retort slaw_spew_overview_to_func (bslaw          s,
                                      slaw_spew_func func,
                                      void          *cookie,
                                      size_t         size_hint,
                                      unt32          flags,
                                      const char    *prolo)
{
  slaw_spew_func_ctx ctx;
  OB_CLEAR(ctx);
  ctx.func     = func;
  ctx.cookie   = cookie;
  ctx.ret      = OB_OK;
  ctx.len      = 0;

  if (size_hint < 80)
    ctx.chunk_size = 300;
  else if (size_hint > 10 * 1024 * 1024)
    ctx.chunk_size = 1023 * 1024;
  else
    ctx.chunk_size = size_hint;

  ctx.capacity = ctx.chunk_size + 20;
  ctx.buf      = calloc (ctx.capacity, sizeof (char));

  if (ctx.buf == NULL)
    return OB_NO_MEM;

  slaw_spew_internal_flags (s,
                            format_to_func,
                            (void *) &ctx,
                            flags,
                            prolo);
  flush_func_buf (&ctx);
  free (ctx.buf);

  return ctx.ret;
}
