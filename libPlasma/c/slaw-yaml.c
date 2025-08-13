
/* (c)  oblong industries */

#include <math.h>
#include <yaml.h>
#include <stdlib.h>
#include "libPlasma/c/slaw-walk.h"
#include "libPlasma/c/slaw-io.h"
#include "libPlasma/c/private/plasma-private.h"
#include "libPlasma/c/private/plasma-testing.h"
#include "libPlasma/c/private/plasma-util.h"
#include "libPlasma/c/slaw-string.h"
#include "libPlasma/c/slaw-path.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-sys.h"
#include "libLoam/c/ob-math.h"
#include "libLoam/c/ob-file.h"
#include "libLoam/c/ob-hash.h"
#include "libLoam/c/ob-vers.h"


/**************************************************
 * These are YAML global tag prefixes, and are
 * URIs compliant with RFC 4151.  http://www.taguri.org/
 **************************************************/
#define SLAW_TAG_PREFIX "tag:oblong.com,2009:slaw/"
#define YAML_TAG_PREFIX "tag:yaml.org,2002:"

#define SLAW_MAP_TAG         (yaml_char_t *) YAML_TAG_PREFIX "map"
#define SLAW_OMAP_TAG        (yaml_char_t *) YAML_TAG_PREFIX "omap"
#define SLAW_LIST_TAG        (yaml_char_t *) YAML_TAG_PREFIX "seq"
#define SLAW_ARRAY_TAG       (yaml_char_t *) SLAW_TAG_PREFIX "array"
#define SLAW_MULTIVECTOR_TAG (yaml_char_t *) SLAW_TAG_PREFIX "multivector"
#define SLAW_VECTOR_TAG      (yaml_char_t *) SLAW_TAG_PREFIX "vector"
#define SLAW_COMPLEX_TAG     (yaml_char_t *) SLAW_TAG_PREFIX "complex"
#define SLAW_NULL_TAG        (yaml_char_t *) YAML_TAG_PREFIX "null"
#define SLAW_STRING_TAG      (yaml_char_t *) YAML_TAG_PREFIX "str"
#define SLAW_PROTEIN_TAG     (yaml_char_t *) SLAW_TAG_PREFIX "protein"
#define SLAW_NONSTD_TAG      (yaml_char_t *) SLAW_TAG_PREFIX "nonstd"
#define SLAW_CONS_TAG        (yaml_char_t *) SLAW_TAG_PREFIX "cons"
#define SLAW_BOOL_TAG        (yaml_char_t *) YAML_TAG_PREFIX "bool"
#define SLAW_BINARY_TAG      (yaml_char_t *) YAML_TAG_PREFIX "binary"
#define SLAW_BADUTF8_TAG     (yaml_char_t *) SLAW_TAG_PREFIX "badutf8"

/* If you were to read the YAML spec:
 *
 * http://www.yaml.org/spec/1.2/spec.html#id2802842
 *
 * it's clear that the standard string tag is:
 *
 * tag:yaml.org,2002:str
 *
 * and most definitely, positively not:
 *
 * tag:yaml.org,2002:string
 *
 * However, some broken implementations have decided to
 * ignore the spec (bug 2939), and for the sake of interoperability,
 * we will humor them.
 */
#define SLAW_BROKEN_STRING_TAG (yaml_char_t *) YAML_TAG_PREFIX "string"

typedef enum sly_plain_scalar_type {
  SLY_SCALAR_NULL,
  SLY_SCALAR_BOOL,
  SLY_SCALAR_10,
  SLY_SCALAR_8,
  SLY_SCALAR_16,
  SLY_SCALAR_FLOAT,
  SLY_SCALAR_INF,
  SLY_SCALAR_NAN,
  SLY_SCALAR_STRING,
  SLY_SCALAR_UNDECIDED
} sly_plain_scalar_type;

static const char my_nul = 0;
#define INC(x, n)                                                              \
  if (((x) += n) - utf8 >= len)                                                \
  (x) = &my_nul

static bool my_eq (const char *str, int64 len, const char *literal)
{
  int64 litlen = strlen (literal);
  if (litlen != len)
    return false;
  return (0 == memcmp (str, literal, len));
}

// categorize according to http://yaml.org/spec/1.2/#id2603930
// this function is intended to exactly match those regular expressions
// (but without the benefit of a regexp library)
static sly_plain_scalar_type sly_classify_plain (const char *utf8, int64 len)
{
  const char *q = utf8;
  const char *u;
  const char *v;

  if (len <= 0)
    return SLY_SCALAR_STRING;

  u = v = utf8;
  INC (u, 1);
  INC (v, 2);

  switch (*utf8)
    {
      case 'n':
      case 'N':
      case '~':
        if (my_eq (utf8, len, "null") || my_eq (utf8, len, "Null")
            || my_eq (utf8, len, "NULL") || my_eq (utf8, len, "~"))
          return SLY_SCALAR_NULL;
        return SLY_SCALAR_STRING;
      case 't':
      case 'T':
      case 'f':
      case 'F':
        if (my_eq (utf8, len, "true") || my_eq (utf8, len, "True")
            || my_eq (utf8, len, "TRUE") || my_eq (utf8, len, "false")
            || my_eq (utf8, len, "False") || my_eq (utf8, len, "FALSE"))
          return SLY_SCALAR_BOOL;
        return SLY_SCALAR_STRING;
      case '.':
        if (my_eq (utf8, len, ".nan") || my_eq (utf8, len, ".NaN")
            || my_eq (utf8, len, ".NAN"))
          return SLY_SCALAR_NAN;
        if (my_eq (utf8, len, ".inf") || my_eq (utf8, len, ".Inf")
            || my_eq (utf8, len, ".INF"))
          return SLY_SCALAR_INF;
        return SLY_SCALAR_STRING;
      case '0':
        if (*u == 'o')
          {
            const char *p = utf8;
            INC (p, 2);
            while (*p >= '0' && *p <= '7')
              INC (p, 1);
            if (*p == 0)
              return SLY_SCALAR_8;
            return SLY_SCALAR_STRING;
          }
        else if (*u == 'x')
          {
            const char *p = utf8;
            INC (p, 2);
            while ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f')
                   || (*p >= 'A' && *p <= 'F'))
              INC (p, 1);
            if (*p == 0)
              return SLY_SCALAR_16;
            return SLY_SCALAR_STRING;
          }
        else if (*u == 0)
          {
            return SLY_SCALAR_10;
          }
        else if (*u != '.')
          {
            return SLY_SCALAR_STRING;
          }
        INC (q, 2);
        goto after_decimal;
      case '+':
      case '-':
        if (my_eq (u, len - 1, ".inf") || my_eq (u, len - 1, ".Inf")
            || my_eq (u, len - 1, ".INF"))
          return SLY_SCALAR_INF;
        if (*u == '0' && *v == 0)
          return SLY_SCALAR_10;
        if (*u == '0' && *v == '.')
          {
            INC (q, 3);
            goto after_decimal;
          }
        if (*u == '0' && *v != '.')
          return SLY_SCALAR_STRING;
        INC (q, 1);
        break;
    }

  if (*q < '1' || *q > '9')
    return SLY_SCALAR_STRING;

  INC (q, 1);

  while (*q >= '0' && *q <= '9')
    INC (q, 1);

  if (*q == 0)
    return SLY_SCALAR_10;
  if (*q != '.')
    return SLY_SCALAR_STRING;

  INC (q, 1);

after_decimal:
  while (*q >= '0' && *q <= '9')
    INC (q, 1);

  if (*q == 0)
    return SLY_SCALAR_FLOAT;

  if (*q != 'e')
    return SLY_SCALAR_STRING;

  INC (q, 1);

  if (*q == '+' || *q == '-')
    INC (q, 1);

  if (*q < '0' || *q > '9')
    return SLY_SCALAR_STRING;

  INC (q, 1);

  while (*q >= '0' && *q <= '9')
    INC (q, 1);

  if (*q == 0)
    return SLY_SCALAR_FLOAT;

  return SLY_SCALAR_STRING;
}

#undef INC

typedef struct unt64_with_sign
{
  unt64 magnitude;
  bool negative;
} unt64_with_sign;

static void parse_integer (const char *s, int64 len, unt64_with_sign *result,
                           sly_plain_scalar_type typ)
{
  unsigned int base = 10;
  int64 i;

  if (typ == SLY_SCALAR_8)
    base = 8;
  else if (typ == SLY_SCALAR_16)
    base = 16;

  result->negative = false;
  result->magnitude = 0;

  for (i = 0; i < len; i++)
    {
      int digit = -1;
      char c = *s++;

      if (c == '-')
        result->negative = true;
      else if (c >= '0' && c <= '9')
        digit = c - '0';
      else if (c >= 'a' && c <= 'f')
        digit = c - 'a' + 10;
      else if (c >= 'A' && c <= 'F')
        digit = c - 'A' + 10;

      /* characters like '+', 'x', and 'o' just get ignored */

      if (digit >= 0)
        {
          result->magnitude *= base;
          result->magnitude += digit;
        }
    }
}

static unt64 bin_to_base64_upper_bound (unt64 bin_len)
{
  return 1 + (77 * ((bin_len + 56) / 57));
}

static unt64 bin_to_base64_convert (unt64 bin_len, const byte *bin,
                                    unt64 dstlen, char *dst)
{
  unt32 bits;
  int i;
  int leftover;
  unt64 ret = 0;
  int g;

  while (bin_len > 0 && dstlen > 0)
    {
      for (g = 0; g < 19 && bin_len > 0; g++)
        {
          bits = 0;
          leftover = 0;

          for (i = 0; i < 3; i++)
            {
              bits <<= 8;
              if (bin_len > 0)
                {
                  bits |= *bin;
                  bin++;
                  bin_len--;
                }
              else
                {
                  leftover++;
                }
            }

          for (i = 0; i < 4; i++)
            {
              int n = 0x3f & (bits >> (6 * (3 - i)));
              char c = '/';
              if (n < 26)
                c = 'A' + n;
              else if (n < 52)
                c = 'a' + n - 26;
              else if (n < 62)
                c = '0' + n - 52;
              else if (n == 62)
                c = '+';

              if (i > 3 - leftover)
                c = '=';

              if (dstlen > 0)
                {
                  *dst = c;
                  dst++;
                  dstlen--;
                  ret++;
                }
            }
        }

      if (dstlen > 0 && bin_len > 0)
        {
          *dst = '\n';
          dst++;
          dstlen--;
          ret++;
        }
    }

  return ret;
}

static unt64 base64_to_bin_upper_bound (unt64 base64_len)
{
  return 3 * ((base64_len + 3) / 4);
}

static unt64 base64_to_bin_convert (unt64 base64_len, const char *base64,
                                    unt64 dstlen, unsigned char *dst)
{
  unt64 isrc = 0, idst = 0;
  unt32 bits = 0;
  int nbits = 0;

  while (isrc < base64_len && idst < dstlen)
    {
      char c = base64[isrc++];
      int n;

      if (c >= 'A' && c <= 'Z')
        n = c - 'A';
      else if (c >= 'a' && c <= 'z')
        n = c - 'a' + 26;
      else if (c >= '0' && c <= '9')
        n = c - '0' + 52;
      else if (c == '+')
        n = 62;
      else if (c == '/')
        n = 63;
      else if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
        continue;
      else
        break;

      bits <<= 6;
      nbits += 6;
      bits |= n;

      if (nbits >= 8)
        {
          dst[idst++] = 0xff & (bits >> (nbits - 8));
          nbits -= 8;
        }
    }

  return idst;
}

typedef enum {
  NOT_IN_A_MAP         = 10,
  INSIDE_UNORDERED_MAP = 20,
  INSIDE_ORDERED_MAP   = 30
} mappiness;

typedef struct sly_emitter_info
{
  // ----- state information -----
  yaml_emitter_t emitter;
  slabu *stack;       // stack of "mappiness" enum, encoded as unt8
  int64 array_index;  // -1 for not in array

  // ----- options -----
  int64 max_elements;  // -1 for unlimited
  bool tag_numbers;
  bool directives;
  bool ordered_maps;
} sly_emitter_info;

static slaw sly_pop (slabu *sb)
{
  int64 count = slabu_count (sb);
  if (sb)
    {
      slaw result = slaw_dup (slabu_list_nth (sb, count - 1));
      slabu_list_remove_nth (sb, count - 1);
      return result;
    }
  else
    return NULL;
}

static bslaw sly_peek (slabu *sb)
{
  int64 count = slabu_count (sb);
  if (sb)
    return slabu_list_nth (sb, count - 1);
  else
    return NULL;
}

static ob_retort info_push (sly_emitter_info *info, mappiness b)
{
  ob_retort err = OB_OK;
  ob_err_accum (&err, slabu_list_add_x (info->stack, slaw_unt8 (b)));
  return err;
}

static mappiness info_pop (sly_emitter_info *info)
{
  slaw s = sly_pop (info->stack);
  const unt8 *b = slaw_unt8_emit (s);
  mappiness m = (mappiness) (b ? *b : NOT_IN_A_MAP);
  slaw_free (s);
  return m;
}

static mappiness info_peek (sly_emitter_info *info)
{
  const unt8 *b = slaw_unt8_emit (sly_peek (info->stack));
  return (mappiness) (b ? *b : NOT_IN_A_MAP);
}

// These are Val-like error handling convenience functions
// that return a constant but have a side effect.

#define PRINT_YAML_PARSE_ERROR(e) print_parser_error (__FILE__, __LINE__, (e))
#define PRINT_YAML_EMIT_ERROR(e) print_emitter_error (__FILE__, __LINE__, (e))

#define YAMERR(...) ob_log_loc (file, line, OBLV_ERROR, 0x2000500c, __VA_ARGS__)

/*
Unfortunately, libyaml doesn't seem to have any public functions for
grabbing the error info out of the parser and emitter structures, so
I've "stolen" some code from yaml-0.1.2/tests/example-deconstructor-alt.c.
This could potentially have negative forward-compatibility implications
since I'm violating the public interface.  Also, for the sake of
completeness, I should give credit for the code I stole:

Copyright (c) 2006 Kirill Simonov

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

static ob_retort print_parser_error (const char *file, int line,
                                     const yaml_parser_t *parser)
{
  switch (parser->error)
    {
      case YAML_MEMORY_ERROR:
        YAMERR ("Memory error: Not enough memory for parsing\n");
        break;

      case YAML_READER_ERROR:
        if (parser->problem_value != -1)
          {
            YAMERR ("Reader error: %s: #%X at %" OB_FMT_SIZE "d\n",
                    parser->problem, parser->problem_value,
                    parser->problem_offset);
          }
        else
          {
            YAMERR ("Reader error: %s at %" OB_FMT_SIZE "d\n", parser->problem,
                    parser->problem_offset);
          }
        break;

      case YAML_SCANNER_ERROR:
        if (parser->context)
          {
            YAMERR ("Scanner error: %s at line %" OB_FMT_SIZE
                    "d, column %" OB_FMT_SIZE "d\n"
                    "%s at line %" OB_FMT_SIZE "d, column %" OB_FMT_SIZE "d\n",
                    parser->context, parser->context_mark.line + 1,
                    parser->context_mark.column + 1, parser->problem,
                    parser->problem_mark.line + 1,
                    parser->problem_mark.column + 1);
          }
        else
          {
            YAMERR ("Scanner error: %s at line %" OB_FMT_SIZE
                    "d, column %" OB_FMT_SIZE "d\n",
                    parser->problem, parser->problem_mark.line + 1,
                    parser->problem_mark.column + 1);
          }
        break;

      case YAML_PARSER_ERROR:
        if (parser->context)
          {
            YAMERR ("Parser error: %s at line %" OB_FMT_SIZE
                    "d, column %" OB_FMT_SIZE "d\n"
                    "%s at line %" OB_FMT_SIZE "d, column %" OB_FMT_SIZE "d\n",
                    parser->context, parser->context_mark.line + 1,
                    parser->context_mark.column + 1, parser->problem,
                    parser->problem_mark.line + 1,
                    parser->problem_mark.column + 1);
          }
        else
          {
            YAMERR ("Parser error: %s at line %" OB_FMT_SIZE
                    "d, column %" OB_FMT_SIZE "d\n",
                    parser->problem, parser->problem_mark.line + 1,
                    parser->problem_mark.column + 1);
          }
        break;

      default:
        /* Couldn't happen. */
        YAMERR ("Internal error\n");
        break;
    }

  return SLAW_YAML_ERR;
}

static ob_retort print_emitter_error (const char *file, int line,
                                      const yaml_emitter_t *emitter)
{
  switch (emitter->error)
    {
      case YAML_MEMORY_ERROR:
        YAMERR ("Memory error: Not enough memory for emitting\n");
        break;

      case YAML_WRITER_ERROR:
        YAMERR ("Writer error: %s\n", emitter->problem);
        break;

      case YAML_EMITTER_ERROR:
        YAMERR ("Emitter error: %s\n", emitter->problem);
        break;

      default:
        /* Couldn't happen. */
        YAMERR ("Internal error\n");
        break;
    }

  return SLAW_YAML_ERR;
}

// (end of libyaml-stolen code)

static ob_retort sly_begin_cons (void *cookie)
{
  sly_emitter_info *info = (sly_emitter_info *) cookie;
  yaml_emitter_t *emitter = &(info->emitter);
  yaml_event_t event;
  mappiness insideMap = info_peek (info);

  ob_retort err = info_push (info, NOT_IN_A_MAP);
  if (err < OB_OK)
    return err;
  if (insideMap == INSIDE_UNORDERED_MAP)
    return OB_OK;

  if (yaml_mapping_start_event_initialize (&event, NULL, SLAW_CONS_TAG,
                                           (insideMap == INSIDE_ORDERED_MAP),
                                           YAML_BLOCK_MAPPING_STYLE)
      && yaml_emitter_emit (emitter, &event))
    return OB_OK;
  else
    return PRINT_YAML_EMIT_ERROR (emitter);
}

static ob_retort sly_end_cons (void *cookie)
{
  sly_emitter_info *info = (sly_emitter_info *) cookie;
  yaml_emitter_t *emitter = &(info->emitter);
  yaml_event_t event;
  mappiness insideMap;

  info_pop (info);
  insideMap = info_peek (info);
  if (insideMap == INSIDE_UNORDERED_MAP)
    return OB_OK;

  if (yaml_mapping_end_event_initialize (&event)
      && yaml_emitter_emit (emitter, &event))
    return OB_OK;
  else
    return PRINT_YAML_EMIT_ERROR (emitter);
}

static ob_retort sly_begin_map (void *cookie, int64 unused)
{
  sly_emitter_info *info = (sly_emitter_info *) cookie;
  yaml_emitter_t *emitter = &(info->emitter);
  yaml_event_t event;
  if (info->ordered_maps)
    {
      ob_retort err = info_push (info, INSIDE_ORDERED_MAP);
      if (err < OB_OK)
        return err;
      if (yaml_sequence_start_event_initialize (&event, NULL, SLAW_OMAP_TAG,
                                                false,
                                                YAML_BLOCK_SEQUENCE_STYLE)
          && yaml_emitter_emit (emitter, &event))
        return OB_OK;
      else
        return PRINT_YAML_EMIT_ERROR (emitter);
    }
  else
    {
      ob_retort err = info_push (info, INSIDE_UNORDERED_MAP);
      if (err < OB_OK)
        return err;
      if (yaml_mapping_start_event_initialize (&event, NULL, SLAW_MAP_TAG, true,
                                               YAML_BLOCK_MAPPING_STYLE)
          && yaml_emitter_emit (emitter, &event))
        return OB_OK;
      else
        return PRINT_YAML_EMIT_ERROR (emitter);
    }
}

static ob_retort sly_end_map (void *cookie)
{
  sly_emitter_info *info = (sly_emitter_info *) cookie;
  yaml_emitter_t *emitter = &(info->emitter);
  yaml_event_t event;
  mappiness insideMap = info_pop (info);
  bool omap = (INSIDE_ORDERED_MAP == insideMap);
  if ((omap ? yaml_sequence_end_event_initialize (&event)
            : yaml_mapping_end_event_initialize (&event))
      && yaml_emitter_emit (emitter, &event))
    return OB_OK;
  else
    {
      OB_LOG_ERROR_CODE (0x20005000, "ending map of type %d\n",
                         (int) insideMap);
      return PRINT_YAML_EMIT_ERROR (emitter);
    }
}

static ob_retort sly_begin_list (void *cookie, int64 unused)
{
  sly_emitter_info *info = (sly_emitter_info *) cookie;
  yaml_emitter_t *emitter = &(info->emitter);
  yaml_event_t event;
  ob_retort err = info_push (info, NOT_IN_A_MAP);
  if (err < OB_OK)
    return err;
  if (yaml_sequence_start_event_initialize (&event, NULL, SLAW_LIST_TAG, true,
                                            YAML_BLOCK_SEQUENCE_STYLE)
      && yaml_emitter_emit (emitter, &event))
    return OB_OK;
  else
    return PRINT_YAML_EMIT_ERROR (emitter);
}

static ob_retort sly_end_list (void *cookie)
{
  sly_emitter_info *info = (sly_emitter_info *) cookie;
  yaml_emitter_t *emitter = &(info->emitter);
  yaml_event_t event;
  info_pop (info);
  if (yaml_sequence_end_event_initialize (&event)
      && yaml_emitter_emit (emitter, &event))
    return OB_OK;
  else
    return PRINT_YAML_EMIT_ERROR (emitter);
}

static ob_retort sly_begin_array (void *cookie, int64 length_hint,
                                  int bits_hint)
{
  sly_emitter_info *info = (sly_emitter_info *) cookie;
  yaml_emitter_t *emitter = &(info->emitter);
  yaml_event_t event;
  const bool f = (bits_hint > 0);

  if (f)
    /* Only start counting the elements if this is an array of plain
     * old numbers, not arrays or vectors. */
    info->array_index = 0;

  if (yaml_sequence_start_event_initialize (&event, NULL, SLAW_ARRAY_TAG, false,
                                            f ? YAML_FLOW_SEQUENCE_STYLE
                                              : YAML_BLOCK_SEQUENCE_STYLE)
      && yaml_emitter_emit (emitter, &event))
    return OB_OK;
  else
    return PRINT_YAML_EMIT_ERROR (emitter);
}

static ob_retort sly_end_array (void *cookie)
{
  sly_emitter_info *info = (sly_emitter_info *) cookie;
  yaml_emitter_t *emitter = &(info->emitter);
  yaml_event_t event;

  if (info->array_index != -1 && info->max_elements != -1
      && info->array_index > info->max_elements)
    {
      /* This means we abbreviated some of the elements */
      char buf[80];
      snprintf (buf, sizeof (buf),
                "and %" OB_FMT_64 "d more (%" OB_FMT_64 "d total)",
                info->array_index - info->max_elements, info->array_index);
      if (!yaml_scalar_event_initialize (&event, NULL, SLAW_STRING_TAG,
                                         (yaml_char_t *) buf, strlen (buf),
                                         true, true, YAML_PLAIN_SCALAR_STYLE)
          || !yaml_emitter_emit (emitter, &event))
        return PRINT_YAML_EMIT_ERROR (emitter);
    }

  info->array_index = -1; /* indicates we're no longer in an array */

  if (yaml_sequence_end_event_initialize (&event)
      && yaml_emitter_emit (emitter, &event))
    return OB_OK;
  else
    return PRINT_YAML_EMIT_ERROR (emitter);
}

static ob_retort sly_begin_multivector (void *cookie, int64 unused)
{
  sly_emitter_info *info = (sly_emitter_info *) cookie;
  yaml_emitter_t *emitter = &(info->emitter);
  yaml_event_t event;
  if (yaml_sequence_start_event_initialize (&event, NULL, SLAW_MULTIVECTOR_TAG,
                                            false, YAML_FLOW_SEQUENCE_STYLE)
      && yaml_emitter_emit (emitter, &event))
    return OB_OK;
  else
    return PRINT_YAML_EMIT_ERROR (emitter);
}

static ob_retort sly_end_multivector (void *cookie)
{
  sly_emitter_info *info = (sly_emitter_info *) cookie;
  yaml_emitter_t *emitter = &(info->emitter);
  yaml_event_t event;
  if (yaml_sequence_end_event_initialize (&event)
      && yaml_emitter_emit (emitter, &event))
    return OB_OK;
  else
    return PRINT_YAML_EMIT_ERROR (emitter);
}

static ob_retort sly_begin_vector (void *cookie, int64 unused)
{
  sly_emitter_info *info = (sly_emitter_info *) cookie;
  yaml_emitter_t *emitter = &(info->emitter);
  yaml_event_t event;
  if (yaml_sequence_start_event_initialize (&event, NULL, SLAW_VECTOR_TAG,
                                            false, YAML_FLOW_SEQUENCE_STYLE)
      && yaml_emitter_emit (emitter, &event))
    return OB_OK;
  else
    return PRINT_YAML_EMIT_ERROR (emitter);
}

static ob_retort sly_end_vector (void *cookie)
{
  sly_emitter_info *info = (sly_emitter_info *) cookie;
  yaml_emitter_t *emitter = &(info->emitter);
  yaml_event_t event;
  if (yaml_sequence_end_event_initialize (&event)
      && yaml_emitter_emit (emitter, &event))
    return OB_OK;
  else
    return PRINT_YAML_EMIT_ERROR (emitter);
}

static ob_retort sly_begin_complex (void *cookie)
{
  sly_emitter_info *info = (sly_emitter_info *) cookie;
  yaml_emitter_t *emitter = &(info->emitter);
  yaml_event_t event;
  if (yaml_sequence_start_event_initialize (&event, NULL, SLAW_COMPLEX_TAG,
                                            false, YAML_FLOW_SEQUENCE_STYLE)
      && yaml_emitter_emit (emitter, &event))
    return OB_OK;
  else
    return PRINT_YAML_EMIT_ERROR (emitter);
}

static ob_retort sly_end_complex (void *cookie)
{
  sly_emitter_info *info = (sly_emitter_info *) cookie;
  yaml_emitter_t *emitter = &(info->emitter);
  yaml_event_t event;
  if (yaml_sequence_end_event_initialize (&event)
      && yaml_emitter_emit (emitter, &event))
    return OB_OK;
  else
    return PRINT_YAML_EMIT_ERROR (emitter);
}

static ob_retort sly_handle_nil (void *cookie)
{
  sly_emitter_info *info = (sly_emitter_info *) cookie;
  yaml_emitter_t *emitter = &(info->emitter);
  yaml_event_t event;
  if (yaml_scalar_event_initialize (&event, NULL, SLAW_NULL_TAG,
                                    (yaml_char_t *) "~", 1, true, false,
                                    YAML_PLAIN_SCALAR_STYLE)
      && yaml_emitter_emit (emitter, &event))
    return OB_OK;
  else
    return PRINT_YAML_EMIT_ERROR (emitter);
}

static ob_retort sly_handle_string (void *cookie, const char *utf8, int64 len)
{
  sly_emitter_info *info = (sly_emitter_info *) cookie;
  yaml_emitter_t *emitter = &(info->emitter);
  yaml_event_t event;
  yaml_scalar_style_t style = YAML_PLAIN_SCALAR_STYLE;
  bool invalid, multiline;
  char *b64buf = NULL;
  ob_retort tort;
  int ysei;

  if (sly_classify_plain (utf8, len) != SLY_SCALAR_STRING)
    style = YAML_SINGLE_QUOTED_SCALAR_STYLE;
  ob_analyze_utf8 (utf8, len, &invalid, &multiline);

  if (invalid)
    {
      unt64 buf_len = bin_to_base64_upper_bound (len);
      unt64 base64_len;

      b64buf = (char *) malloc (buf_len);
      if (!b64buf)
        return OB_NO_MEM;

      base64_len = bin_to_base64_convert (len, (byte *) utf8, buf_len, b64buf);

      ysei =
        yaml_scalar_event_initialize (&event, NULL, SLAW_BADUTF8_TAG,
                                      (yaml_char_t *) b64buf, base64_len, false,
                                      false, YAML_LITERAL_SCALAR_STYLE);
    }
  else
    {
      if (multiline)
        style = YAML_LITERAL_SCALAR_STYLE;

      ysei = yaml_scalar_event_initialize (&event, NULL, SLAW_STRING_TAG,
                                           (yaml_char_t *) utf8, len,
                                           (style == YAML_PLAIN_SCALAR_STYLE),
                                           true, style);
    }

  if (!ysei)
    {
      OB_LOG_ERROR_CODE (0x20005001, "yaml_scalar_event_initialize failed!?\n");
      return SLAW_YAML_ERR;
    }

  if (yaml_emitter_emit (emitter, &event))
    tort = OB_OK;
  else
    tort = PRINT_YAML_EMIT_ERROR (emitter);

  free (b64buf);
  return tort;
}

#define ARRAY_ELEMENT_CHECK()                                                  \
  const int64 array_index = info->array_index;                                 \
  const int64 max_elements = info->max_elements;                               \
  if (array_index != -1 && max_elements != -1)                                 \
    {                                                                          \
      info->array_index++;                                                     \
      if (array_index >= max_elements)                                         \
        return OB_OK;                                                          \
    }

static ob_retort sly_handle_int (void *cookie, int64 val, int bits)
{
  sly_emitter_info *info = (sly_emitter_info *) cookie;
  yaml_emitter_t *emitter = &(info->emitter);
  yaml_event_t event;
  char tag[80];
  char buf[80];

  ARRAY_ELEMENT_CHECK ();

  snprintf (tag, sizeof (tag), "%si%d", SLAW_TAG_PREFIX, bits);
  snprintf (buf, sizeof (buf), "%" OB_FMT_64 "d", val);
  if (yaml_scalar_event_initialize (&event, NULL, (yaml_char_t *) tag,
                                    (yaml_char_t *) buf, strlen (buf),
                                    !info->tag_numbers, false,
                                    YAML_PLAIN_SCALAR_STYLE)
      && yaml_emitter_emit (emitter, &event))
    return OB_OK;
  else
    return PRINT_YAML_EMIT_ERROR (emitter);
}

static ob_retort sly_handle_unt (void *cookie, unt64 val, int bits)
{
  sly_emitter_info *info = (sly_emitter_info *) cookie;
  yaml_emitter_t *emitter = &(info->emitter);
  yaml_event_t event;
  char tag[80];
  char buf[80];

  ARRAY_ELEMENT_CHECK ();

  if (bits == 1)
    {
      snprintf (tag, sizeof (tag), "%s", (const char *) SLAW_BOOL_TAG);
      snprintf (buf, sizeof (buf), "%s", (val ? "true" : "false"));
    }
  else
    {
      snprintf (tag, sizeof (tag), "%su%d", SLAW_TAG_PREFIX, bits);
      snprintf (buf, sizeof (buf), "%" OB_FMT_64 "u", val);
    }
  if (yaml_scalar_event_initialize (&event, NULL, (yaml_char_t *) tag,
                                    (yaml_char_t *) buf, strlen (buf),
                                    (bits == 1 || !info->tag_numbers), false,
                                    YAML_PLAIN_SCALAR_STYLE)
      && yaml_emitter_emit (emitter, &event))
    return OB_OK;
  else
    return PRINT_YAML_EMIT_ERROR (emitter);
}

static ob_retort sly_handle_float (void *cookie, float64 val, int bits)
{
  sly_emitter_info *info = (sly_emitter_info *) cookie;
  yaml_emitter_t *emitter = &(info->emitter);
  yaml_event_t event;
  int sigdigs;
  char tag[80];
  char buf[80];

  ARRAY_ELEMENT_CHECK ();

  sigdigs = ((bits == 32) ? 8 : 17);
  snprintf (tag, sizeof (tag), "%sf%d", SLAW_TAG_PREFIX, bits);

  bool negative;
  switch (ob_fpclassify (val, &negative))
    {
      case OB_FP_NAN:
        snprintf (buf, sizeof (buf), ".NaN");
        break;
      case OB_FP_INFINITE:
        snprintf (buf, sizeof (buf), "%c.Inf", negative ? '-' : '+');
        break;
      default:
        snprintf (buf, sizeof (buf), "%.*g", sigdigs, val);
        if (!info->tag_numbers && strchr (buf, '.') == NULL
            && strchr (buf, 'e') == NULL && strlen (buf) + 3 < sizeof (buf))
          strcat (buf, ".0");
        break;
    }

  if (yaml_scalar_event_initialize (&event, NULL, (yaml_char_t *) tag,
                                    (yaml_char_t *) buf, strlen (buf),
                                    !info->tag_numbers, false,
                                    YAML_PLAIN_SCALAR_STYLE)
      && yaml_emitter_emit (emitter, &event))
    return OB_OK;
  else
    return PRINT_YAML_EMIT_ERROR (emitter);
}

static ob_retort sly_handle_empty_array (void *cookie, int vecsize, bool isMVec,
                                         bool isComplex, bool isUnsigned,
                                         bool isFloat, int bits)
{
  sly_emitter_info *info = (sly_emitter_info *) cookie;
  yaml_emitter_t *emitter = &(info->emitter);
  yaml_event_t event;

  char vec[32];
  char buf[128];

  snprintf (vec, sizeof (vec), "%svector/%d/", (isMVec ? "multi" : ""),
            vecsize);
  snprintf (buf, sizeof (buf), "%sempty/%s%s%c%d", SLAW_TAG_PREFIX,
            (vecsize == 1 ? "" : vec), (isComplex ? "complex/" : ""),
            (isFloat ? 'f' : (isUnsigned ? 'u' : 'i')), bits);

  if (yaml_scalar_event_initialize (&event, NULL, (yaml_char_t *) buf,
                                    (yaml_char_t *) "~", 1, false, false,
                                    YAML_PLAIN_SCALAR_STYLE)
      && yaml_emitter_emit (emitter, &event))
    return OB_OK;
  else
    return PRINT_YAML_EMIT_ERROR (emitter);
}

static ob_retort sly_begin_protein (void *cookie)
{
  sly_emitter_info *info = (sly_emitter_info *) cookie;
  yaml_emitter_t *emitter = &(info->emitter);
  yaml_event_t event;

  ob_retort err = info_push (info, NOT_IN_A_MAP);
  if (err < OB_OK)
    return err;

  if (!(yaml_mapping_start_event_initialize (&event, NULL, SLAW_PROTEIN_TAG,
                                             false, YAML_BLOCK_MAPPING_STYLE)
        && yaml_emitter_emit (emitter, &event)))
    return PRINT_YAML_EMIT_ERROR (emitter);
  else
    return OB_OK;
}

static ob_retort sly_end_protein (void *cookie)
{
  sly_emitter_info *info = (sly_emitter_info *) cookie;
  yaml_emitter_t *emitter = &(info->emitter);
  yaml_event_t event;

  info_pop (info);

  if (!(yaml_mapping_end_event_initialize (&event)
        && yaml_emitter_emit (emitter, &event)))
    return PRINT_YAML_EMIT_ERROR (emitter);
  else
    return OB_OK;
}

static ob_retort sly_handle_nonstd_protein (void *cookie, const void *p,
                                            int64 len)
{
  sly_emitter_info *info = (sly_emitter_info *) cookie;
  yaml_emitter_t *emitter = &(info->emitter);
  yaml_event_t event;
  unt64 buf_len = bin_to_base64_upper_bound (len);
  unt64 base64_len;
  char *buf = (char *) malloc (buf_len);

  if (buf_len > 0 && !buf)
    return OB_NO_MEM;

  base64_len = bin_to_base64_convert (len, (byte *) p, buf_len, buf);

  ob_retort rval = OB_OK;

  if (!(yaml_scalar_event_initialize (&event, NULL, SLAW_NONSTD_TAG,
                                      (yaml_char_t *) buf, base64_len, false,
                                      false, YAML_LITERAL_SCALAR_STYLE)
        && yaml_emitter_emit (emitter, &event)))
    rval = PRINT_YAML_EMIT_ERROR (emitter);

  free (buf);
  return rval;
}

static ob_retort sly_begin_descrips (void *cookie)
{
  sly_emitter_info *info = (sly_emitter_info *) cookie;
  yaml_emitter_t *emitter = &(info->emitter);
  yaml_event_t event;

  if (!(yaml_scalar_event_initialize (&event, NULL, SLAW_STRING_TAG,
                                      (yaml_char_t *) "descrips", 8, true, true,
                                      YAML_PLAIN_SCALAR_STYLE)
        && yaml_emitter_emit (emitter, &event)))
    return PRINT_YAML_EMIT_ERROR (emitter);
  else
    return OB_OK;
}

static ob_retort sly_end_descrips (void *cookie)
{
  return OB_OK;
}

static ob_retort sly_begin_ingests (void *cookie)
{
  sly_emitter_info *info = (sly_emitter_info *) cookie;
  yaml_emitter_t *emitter = &(info->emitter);
  yaml_event_t event;

  if (!(yaml_scalar_event_initialize (&event, NULL, SLAW_STRING_TAG,
                                      (yaml_char_t *) "ingests", 7, true, true,
                                      YAML_PLAIN_SCALAR_STYLE)
        && yaml_emitter_emit (emitter, &event)))
    return PRINT_YAML_EMIT_ERROR (emitter);
  else
    return OB_OK;
}

static ob_retort sly_end_ingests (void *cookie)
{
  return OB_OK;
}

static ob_retort sly_handle_rude_data (void *cookie, const void *d, int64 len)
{
  sly_emitter_info *info = (sly_emitter_info *) cookie;
  yaml_emitter_t *emitter = &(info->emitter);
  yaml_event_t event;

  unt64 buf_len = bin_to_base64_upper_bound (len);
  unt64 base64_len;
  char *buf = (char *) malloc (buf_len);

  base64_len = bin_to_base64_convert (len, (byte *) d, buf_len, buf);
  if (!(yaml_scalar_event_initialize (&event, NULL, SLAW_STRING_TAG,
                                      (yaml_char_t *) "rude_data", 9, true,
                                      true, YAML_PLAIN_SCALAR_STYLE)
        && yaml_emitter_emit (emitter, &event)))
    {
      free (buf);
      return PRINT_YAML_EMIT_ERROR (emitter);
    }

  if (!(yaml_scalar_event_initialize (&event, NULL, SLAW_BINARY_TAG,
                                      (yaml_char_t *) buf, base64_len, false,
                                      false, YAML_LITERAL_SCALAR_STYLE)
        && yaml_emitter_emit (emitter, &event)))
    {
      free (buf);
      return PRINT_YAML_EMIT_ERROR (emitter);
    }

  free (buf);
  return OB_OK;
}

static const slaw_handler slaw_yaml_handler = {sly_begin_cons,
                                               sly_end_cons,
                                               sly_begin_map,
                                               sly_end_map,
                                               sly_begin_list,
                                               sly_end_list,
                                               sly_begin_array,
                                               sly_end_array,
                                               sly_begin_multivector,
                                               sly_end_multivector,
                                               sly_begin_vector,
                                               sly_end_vector,
                                               sly_begin_complex,
                                               sly_end_complex,
                                               sly_handle_nil,
                                               sly_handle_string,
                                               sly_handle_int,
                                               sly_handle_unt,
                                               sly_handle_float,
                                               sly_handle_empty_array,
                                               sly_begin_protein,
                                               sly_end_protein,
                                               sly_handle_nonstd_protein,
                                               sly_begin_descrips,
                                               sly_end_descrips,
                                               sly_begin_ingests,
                                               sly_end_ingests,
                                               sly_handle_rude_data};

static ob_retort sly_write_slaw_to_yaml (sly_emitter_info *emitter, bslaw s)
{
  ob_retort err;
  emitter->stack = slabu_new ();
  if (!emitter->stack)
    return OB_NO_MEM;
  err = slaw_walk (emitter, &slaw_yaml_handler, s);
  slabu_free (emitter->stack);
  emitter->stack = NULL;
  return err;
}

// yaml => events

#define SE_ARRAY        OB_CONST_U64 (0x32b9c7ec210c1a04)
#define SE_BADUTF8      OB_CONST_U64 (0xa71e08969f161fb7)
#define SE_BINARY       OB_CONST_U64 (0x5cafcc4621b348fa)
#define SE_BOOL         OB_CONST_U64 (0xb6492c832b82f189)
#define SE_COMPLEX      OB_CONST_U64 (0x2195c54d76476112)
#define SE_CONS         OB_CONST_U64 (0xf7b4a5d51438b792)
#define SE_DESCRIPS     OB_CONST_U64 (0x3f8d4bbcea903711)
#define SE_F32          OB_CONST_U64 (0x479db3b52758c108)
#define SE_F64          OB_CONST_U64 (0xcd876fe462578c4f)
#define SE_I16          OB_CONST_U64 (0xa8024548a1ca2305)
#define SE_I32          OB_CONST_U64 (0xa489c4c31b630c5a)
#define SE_I64          OB_CONST_U64 (0xc940065b925d3a15)
#define SE_I8           OB_CONST_U64 (0xa266b0470338e398)
#define SE_INGESTS      OB_CONST_U64 (0x16a60446ba1f5119)
#define SE_MAP          OB_CONST_U64 (0xb17678ff2f939aa2)
#define SE_MULTIVECTOR  OB_CONST_U64 (0x5b6eb1410fe7bd04)
#define SE_NON_SPECIFIC OB_CONST_U64 (0x095934e7f55b39ba)
#define SE_NONSTD       OB_CONST_U64 (0xd70c69f2b823fb40)
#define SE_NULL         OB_CONST_U64 (0x9e34c34e89b60f38)
#define SE_OMAP         OB_CONST_U64 (0x42443404839a2550)
#define SE_PROTEIN      OB_CONST_U64 (0xdf8a6b35e9acd602)
#define SE_RUDE_DATA    OB_CONST_U64 (0xf5cb44dedc2dd3d4)
#define SE_SEQ          OB_CONST_U64 (0xd7ec31d8a099ef3c)
#define SE_STRING       OB_CONST_U64 (0x2e7062203251384f)
#define SE_STR          OB_CONST_U64 (0x6e17023b1ba5ddbf)
#define SE_U16          OB_CONST_U64 (0x2cdc5570e3b4f4c0)
#define SE_U32          OB_CONST_U64 (0xa1d53e6f55db8ac8)
#define SE_U64          OB_CONST_U64 (0x2c67cbce14289d59)
#define SE_U8           OB_CONST_U64 (0x9cdaf818f6756e1a)
#define SE_VECTOR       OB_CONST_U64 (0x64fdc5a5ca6cd3c6)

/* keep track of our element, position, and context
 * using a stack of v3unt64. */
#define CURRENT_ELEMENT  x
#define CURRENT_POSITION y
#define CURRENT_CONTEXT  z

/* All this does is call ob_city_hash64 on a NUL-terminated string. */
static inline unt64 sly_hash (const yaml_char_t *s)
{
  return ob_city_hash64 (s, strlen ((const char *) s));
}

// XXX: would eventually be nice to get rid of this by using
// strings with lengths.
/* The idea here is that most allocations will be small, and in that
 * case, we want to avoid malloc overhead.  (In my test case, the
 * mallocs in sly_parse were taking up 0.67% of the program's total
 * execution time, which is not huge, but worth addressing when it's
 * this easy to do.)  So we normally use a buffer on the stack, and
 * then only malloc if we need something larger.
 *
 * The other alternative would have been to use C99 variable-length
 * arrays, but a) the Microsoft compiler doesn't support this, and
 * b) if it was truly huge we wouldn't want it on the stack anyway.
 */

#define MAYBE_MALLOC(siz)                                                      \
  ((siz) <= sizeof (little_buffer) ? (void *) little_buffer : malloc (siz))

#define MAYBE_FREE(ptr)                                                        \
  if (((void *) (ptr)) != (void *) little_buffer)                              \
  free (ptr)

static ob_retort sly_parse (void *cookie, const slaw_handler *handler,
                            yaml_parser_t *parser)
{
  yaml_event_t event;
  byte little_buffer[64];
  slabu *stack = slabu_new ();
  ob_retort err = OB_OK;
  if (stack == NULL)
    return OB_NO_MEM;

  do
    {
      slaw popped = NULL;
      bool pushed = false;
      unt64 h;
      const v3unt64 *prev = slaw_v3unt64_emit (sly_peek (stack));
      v3unt64 v;
      sly_plain_scalar_type plainTyp = SLY_SCALAR_UNDECIDED;

      if (!yaml_parser_parse (parser, &event))
        {
          err = PRINT_YAML_PARSE_ERROR (parser);
          break;
        }

      if (prev && prev->CURRENT_ELEMENT == SE_MAP
          && 0 == (prev->CURRENT_POSITION & 1)
          && event.type != YAML_MAPPING_END_EVENT)
        {
          err = handler->begin_cons (cookie);
        }
      else if (prev && prev->CURRENT_ELEMENT == SE_PROTEIN
               && 1 == (prev->CURRENT_POSITION & 1)
               && event.type != YAML_MAPPING_END_EVENT)
        {
          if (prev->CURRENT_CONTEXT == SE_DESCRIPS)
            err = handler->begin_descrips (cookie);
          else if (prev->CURRENT_CONTEXT == SE_INGESTS)
            err = handler->begin_ingests (cookie);
        }

      if (err != OB_OK)
        goto badness;

      switch (event.type)
        {
          case YAML_STREAM_START_EVENT:
            ob_err_accum (&err, slabu_list_add_x (stack, slaw_nil ()));
            pushed = true;
            break;
          case YAML_STREAM_END_EVENT:
            popped = sly_pop (stack);
            if (popped == NULL)
              err = SLAW_END_OF_FILE;
            break;
          case YAML_DOCUMENT_START_EVENT:
            ob_err_accum (&err, slabu_list_add_x (stack, slaw_nil ()));
            pushed = true;
            break;
          case YAML_DOCUMENT_END_EVENT:
            popped = sly_pop (stack);
            break;
          case YAML_ALIAS_EVENT:
            slabu_free (stack);
            return SLAW_ALIAS_NOT_SUPPORTED;
          case YAML_SCALAR_EVENT:
            if (!event.data.scalar.tag
                || (h = sly_hash (event.data.scalar.tag)) == SE_NON_SPECIFIC)
              {
                if (event.data.scalar.style == YAML_PLAIN_SCALAR_STYLE)
                  {
                    plainTyp = sly_classify_plain ((const char *)
                                                     event.data.scalar.value,
                                                   event.data.scalar.length);
                    switch (plainTyp)
                      {
                        case SLY_SCALAR_NULL:
                          h = SE_NULL;
                          break;
                        case SLY_SCALAR_10:
                        case SLY_SCALAR_8:
                        case SLY_SCALAR_16:
                          h = SE_I64;
                          break;
                        case SLY_SCALAR_FLOAT:
                        case SLY_SCALAR_INF:
                        case SLY_SCALAR_NAN:
                          h = SE_F64;
                          break;
                        case SLY_SCALAR_BOOL:
                          h = SE_BOOL;
                          break;
                        default:
                        case SLY_SCALAR_STRING:
                          h = SE_STR;
                          break;
                      }
                  }
                else
                  {
                    /* untagged quoted scalars are always strings */
                    h = SE_STR;
                  }
              }
            switch (h)
              {
                case SE_BINARY:
                  if (!prev || prev->CURRENT_CONTEXT != SE_RUDE_DATA)
                    {
                      OB_LOG_ERROR_CODE (0x2000500d, "slaw yaml: got %s in "
                                                     "unexpected place\n",
                                         event.data.scalar.tag);
                      err = SLAW_PARSING_BADNESS;
                      break;
                    }
                // fall thru
                case SE_NONSTD:
                case SE_BADUTF8:
                  {
                    unt64 buflen =
                      base64_to_bin_upper_bound (event.data.scalar.length);
                    unt64 len;
                    unt8 *buf = (unt8 *) MAYBE_MALLOC (buflen);
                    memset (buf, 0, buflen);
                    len = base64_to_bin_convert (event.data.scalar.length,
                                                 (const char *)
                                                   event.data.scalar.value,
                                                 buflen, buf);
                    if (h == SE_NONSTD)
                      err = handler->handle_nonstd_protein (cookie, buf, len);
                    else if (h == SE_BADUTF8)
                      err = handler->handle_string (cookie, (const char *) buf,
                                                    len);
                    else
                      err = handler->handle_rude_data (cookie, buf, len);
                    MAYBE_FREE (buf);
                  }
                  break;
                case SE_BOOL:
                  {
                    char c = event.data.scalar.value[0];
                    err =
                      handler->handle_unt (cookie, (c == 't' || c == 'T'), 1);
                    break;
                  }
                case SE_F32:
                case SE_F64:
                  {
                    float64 number;
                    int bits = 8;
                    char *buf =
                      (char *) MAYBE_MALLOC (event.data.scalar.length + 1);

                    memcpy (buf, event.data.scalar.value,
                            event.data.scalar.length);
                    buf[event.data.scalar.length] = 0;

                    if (plainTyp == SLY_SCALAR_UNDECIDED)
                      plainTyp = sly_classify_plain ((const char *)
                                                       event.data.scalar.value,
                                                     event.data.scalar.length);

                    switch (plainTyp)
                      {
                        case SLY_SCALAR_NAN:
                          number = OB_NAN;
                          break;
                        case SLY_SCALAR_INF:
                          number = OB_POSINF;
                          if (buf[0] == '-')
                            number = -number;
                          break;
                        default:
                          // XXX: use a variant of private_strtof64 instead
                          number = strtod (buf, NULL);
                          break;
                      }
                    switch (h)
                      {
                        case SE_F64:
                          bits *= 2;
                        /* fall through */
                        case SE_F32:
                          bits *= 4;
                          err = handler->handle_float (cookie, number, bits);
                          break;
                      }
                    MAYBE_FREE (buf);
                  }
                  break;
                case SE_I16:
                case SE_I32:
                case SE_I64:
                case SE_I8:
                case SE_U16:
                case SE_U32:
                case SE_U64:
                case SE_U8:
                  {
                    unt64_with_sign number;
                    int bits = 8;

                    if (plainTyp == SLY_SCALAR_UNDECIDED)
                      plainTyp = sly_classify_plain ((const char *)
                                                       event.data.scalar.value,
                                                     event.data.scalar.length);

                    parse_integer ((const char *) event.data.scalar.value,
                                   event.data.scalar.length, &number, plainTyp);
                    switch (h)
                      {
                        case SE_I64:
                          bits *= 2;
                        /* fall through */
                        case SE_I32:
                          bits *= 2;
                        /* fall through */
                        case SE_I16:
                          bits *= 2;
                        /* fall through */
                        case SE_I8:
                          if (number.negative)
                            err =
                              handler->handle_int (cookie,
                                                   -(int64) number.magnitude,
                                                   bits);
                          else
                            err = handler->handle_int (cookie,
                                                       (int64) number.magnitude,
                                                       bits);
                          break;
                        case SE_U64:
                          bits *= 2;
                        /* fall through */
                        case SE_U32:
                          bits *= 2;
                        /* fall through */
                        case SE_U16:
                          bits *= 2;
                        /* fall through */
                        case SE_U8:
                          err = handler->handle_unt (cookie, number.magnitude,
                                                     bits);
                          break;
                      }
                  }
                  break;
                case SE_NULL:
                  err = handler->handle_nil (cookie);
                  break;
                case SE_STRING: /* the wrong way (bug 2939) */
                case SE_STR:    /* the right way */
                  {
                    if (prev && (prev->CURRENT_ELEMENT == SE_PROTEIN)
                        && ((prev->CURRENT_POSITION & 1) == 0))
                      {
                        unt64 h2 = ob_city_hash64 (event.data.scalar.value,
                                                   event.data.scalar.length);
                        if (h2 == SE_INGESTS || h2 == SE_DESCRIPS
                            || h2 == SE_RUDE_DATA)
                          {
                            // XXXX: FIXME: cast away const
                            ((v3unt64 *) prev)->CURRENT_CONTEXT = h2;
                          }
                        else
                          {
                            slaw tmp =
                              slaw_string_from_substring ((const char *) event
                                                            .data.scalar.value,
                                                          event.data.scalar
                                                            .length);
                            OB_LOG_ERROR_CODE (0x2000500e,
                                               "slaw yaml: expected 'ingests', "
                                               "'descrips',\n"
                                               "or 'rude_data', but got '%s'\n",
                                               slaw_string_emit (tmp));
                            slaw_free (tmp);
                            err = SLAW_PARSING_BADNESS;
                          }
                      }
                    else
                      {
                        err = handler->handle_string (cookie,
                                                      (const char *)
                                                        event.data.scalar.value,
                                                      event.data.scalar.length);
                      }
                  }
                  break;
                default:
                  {
                    const char *emptyPrefix = SLAW_TAG_PREFIX "empty/";
                    const char *multivectorPrefix = "multivector/";
                    const char *vectorPrefix = "vector/";
                    const char *complexPrefix = "complex/";
                    const size_t emptyPrefixLen = strlen (emptyPrefix);
                    const size_t multivectorPrefixLen =
                      strlen (multivectorPrefix);
                    const size_t vectorPrefixLen = strlen (vectorPrefix);
                    const size_t complexPrefixLen = strlen (complexPrefix);
                    const char *tag = (const char *) event.data.scalar.tag;

                    if (strncmp (tag, emptyPrefix, emptyPrefixLen) == 0)
                      {
                        int vecsize = 1;
                        bool isMVec = false;
                        bool isComplex = false;
                        bool isUnsigned = false;
                        bool isFloat = false;
                        int bits;

                        tag += emptyPrefixLen;

                        if (strncmp (tag, multivectorPrefix,
                                     multivectorPrefixLen)
                            == 0)
                          {
                            tag += multivectorPrefixLen;
                            if (tag[0] >= '2' && tag[0] <= '5' && tag[1] == '/')
                              {
                                vecsize = tag[0] - '0';
                                tag += 2;
                                isMVec = true;
                              }
                            else
                              {
                                err = SLAW_BAD_TAG;
                                ob_log (OBLV_WRNU, 0x20005002, "Bad tag: %s\n",
                                        (const char *) event.data.scalar.tag);
                                break;
                              }
                          }
                        else if (strncmp (tag, vectorPrefix, vectorPrefixLen)
                                 == 0)
                          {
                            tag += vectorPrefixLen;
                            if (tag[0] >= '2' && tag[0] <= '4' && tag[1] == '/')
                              {
                                vecsize = tag[0] - '0';
                                tag += 2;
                              }
                            else
                              {
                                err = SLAW_BAD_TAG;
                                ob_log (OBLV_WRNU, 0x20005003, "Bad tag: %s\n",
                                        (const char *) event.data.scalar.tag);
                                break;
                              }
                          }

                        if (strncmp (tag, complexPrefix, complexPrefixLen) == 0)
                          {
                            isComplex = true;
                            tag += complexPrefixLen;
                          }

                        if (tag[0] == 'u')
                          isUnsigned = true;
                        else if (tag[0] == 'f')
                          isFloat = true;
                        else if (tag[0] != 'i')
                          {
                            err = SLAW_BAD_TAG;
                            ob_log (OBLV_WRNU, 0x20005004, "Bad tag: %s\n",
                                    (const char *) event.data.scalar.tag);
                            break;
                          }

                        tag++;
                        bits = atoi (tag);

                        err =
                          handler->handle_empty_array (cookie, vecsize, isMVec,
                                                       isComplex, isUnsigned,
                                                       isFloat, bits);
                        break;
                      }
                  }
                  err = SLAW_BAD_TAG;
                  ob_log (OBLV_WRNU, 0x20005005, "Bad tag: %s\n",
                          (const char *) event.data.scalar.tag);
                  break;
              }
            break;
          case YAML_SEQUENCE_START_EVENT:
            if (!event.data.sequence_start.tag
                || (h = sly_hash (event.data.sequence_start.tag))
                     == SE_NON_SPECIFIC)
              h = SE_SEQ;
            v.CURRENT_ELEMENT = h;
            v.CURRENT_POSITION = 0;
            v.CURRENT_CONTEXT = 0;
            ob_err_accum (&err, slabu_list_add_x (stack, slaw_v3unt64 (v)));
            pushed = true;
            switch (h)
              {
                case SE_COMPLEX:
                  err = handler->begin_complex (cookie);
                  break;
                case SE_VECTOR:
                  err = handler->begin_vector (cookie, -1);
                  break;
                case SE_MULTIVECTOR:
                  err = handler->begin_multivector (cookie, -1);
                  break;
                case SE_ARRAY:
                  err = handler->begin_array (cookie, -1, -1);
                  break;
                case SE_SEQ:
                  err = handler->begin_list (cookie, -1);
                  break;
                case SE_OMAP:
                  err = handler->begin_map (cookie, -1);
                  break;
                default:
                  err = SLAW_BAD_TAG;
                  ob_log (OBLV_WRNU, 0x20005006, "Bad start sequence tag: %s\n",
                          (const char *) event.data.sequence_start.tag);
                  break;
              }
            break;
          case YAML_SEQUENCE_END_EVENT:
            popped = sly_pop (stack);
            if (!popped || !slaw_v3unt64_emit (popped))
              {
                // I don't think it should be possible for this to occur,
                // so I'm going to call it a programming error, rather than
                // a user error.
                OB_LOG_BUG_CODE (0x2000500f, "slaw yaml: stack popping problem "
                                             "at sequence end\n");
                err = SLAW_PARSING_BADNESS;
                break;
              }
            h = slaw_v3unt64_emit (popped)->CURRENT_ELEMENT;
            switch (h)
              {
                case SE_COMPLEX:
                  err = handler->end_complex (cookie);
                  break;
                case SE_VECTOR:
                  err = handler->end_vector (cookie);
                  break;
                case SE_MULTIVECTOR:
                  err = handler->end_multivector (cookie);
                  break;
                case SE_ARRAY:
                  err = handler->end_array (cookie);
                  break;
                case SE_SEQ:
                  err = handler->end_list (cookie);
                  break;
                case SE_OMAP:
                  err = handler->end_map (cookie);
                  break;
                default:
                  err = SLAW_BAD_TAG;
                  ob_log (OBLV_WRNU, 0x20005007,
                          "Bad end sequence tag: %" OB_FMT_64 "u\n", h);
                  break;
              }
            break;
          case YAML_MAPPING_START_EVENT:
            if (!event.data.mapping_start.tag
                || (h = sly_hash (event.data.mapping_start.tag))
                     == SE_NON_SPECIFIC)
              h = SE_MAP;
            if (prev && prev->CURRENT_ELEMENT == SE_OMAP)
              h = SE_CONS;
            v.CURRENT_ELEMENT = h;
            v.CURRENT_POSITION = 0;
            v.CURRENT_CONTEXT = 0;
            ob_err_accum (&err, slabu_list_add_x (stack, slaw_v3unt64 (v)));
            pushed = true;
            switch (h)
              {
                case SE_MAP:
                  err = handler->begin_map (cookie, -1);
                  break;
                case SE_CONS:
                  err = handler->begin_cons (cookie);
                  break;
                case SE_PROTEIN:
                  err = handler->begin_protein (cookie);
                  break;
                default:
                  err = SLAW_BAD_TAG;
                  ob_log (OBLV_WRNU, 0x20005008, "Bad start mapping tag: %s\n",
                          (const char *) event.data.mapping_start.tag);
                  break;
              }
            break;
          case YAML_MAPPING_END_EVENT:
            popped = sly_pop (stack);
            if (!popped || !slaw_v3unt64_emit (popped))
              {
                // I don't think it should be possible for this to occur,
                // so I'm going to call it a programming error, rather than
                // a user error.
                OB_LOG_BUG_CODE (0x20005010, "slaw yaml: stack popping problem "
                                             "at mapping end\n");
                err = SLAW_PARSING_BADNESS;
                break;
              }
            h = slaw_v3unt64_emit (popped)->CURRENT_ELEMENT;
            switch (h)
              {
                case SE_MAP:
                  err = handler->end_map (cookie);
                  break;
                case SE_CONS:
                  err = handler->end_cons (cookie);
                  break;
                case SE_PROTEIN:
                  err = handler->end_protein (cookie);
                  break;
                default:
                  err = SLAW_BAD_TAG;
                  ob_log (OBLV_WRNU, 0x20005009,
                          "Bad end mapping tag: %" OB_FMT_64 "u\n", h);
                  break;
              }
            break;
          case YAML_NO_EVENT:
            // This seems to be what happens if we try reading after
            // already getting SLAW_END_OF_FILE once.  That's why
            // MiscSlawTest.GenericOpen tests for SLAW_END_OF_FILE twice;
            // the second time is this case.
            err = SLAW_END_OF_FILE;
            break;
          default:
            // I don't think it should be possible for this to occur,
            // so I'm going to call it a programming error, rather than
            // a user error.
            OB_LOG_BUG_CODE (0x20005011, "slaw yaml: yaml parser produced "
                                         "strange event %d\n",
                             event.type);
            err = SLAW_PARSING_BADNESS;
            break;
        }

      if (err == OB_OK && !pushed)
        {
          prev = slaw_v3unt64_emit (sly_peek (stack));
          if (prev)
            {
              // XXXX: FIXME: cast away const
              ((v3unt64 *) prev)->CURRENT_POSITION++;
              if (0 == (prev->CURRENT_POSITION & 1))
                {
                  if (prev->CURRENT_ELEMENT == SE_MAP)
                    {
                      err = handler->end_cons (cookie);
                    }
                  else if (prev->CURRENT_ELEMENT == SE_PROTEIN)
                    {
                      if (prev->CURRENT_CONTEXT == SE_DESCRIPS)
                        {
                          err = handler->end_descrips (cookie);
                        }
                      else if (prev->CURRENT_CONTEXT == SE_INGESTS)
                        {
                          err = handler->end_ingests (cookie);
                        }
                    }
                }
            }
        }

    badness:
      yaml_event_delete (&event);

      if (popped)
        slaw_free (popped);
    }
  while (err == OB_OK && slabu_count (stack) > 0);

  slabu_free (stack);

  return err;
}

static slaw sly_read_slaw_from_yaml (yaml_parser_t *parser, ob_retort *err)
{
  slaw s;
  slaw_fabricator *sf = slaw_fabricator_new ();
  if (sf == NULL)
    {
      *err = OB_NO_MEM;
      return NULL;
    }
  *err = sly_parse (sf, &slaw_fabrication_handler, parser);
  s = sf->result;
  sf->result = NULL;
  slaw_fabricator_free (sf);
  return s;
}

/* Slaw I/O functions */

typedef struct sly_parser_holder
{
  yaml_parser_t parser;
  slaw_read_handler h;
} sly_parser_holder;

typedef struct sly_emitter_holder
{
  sly_emitter_info emitter;
  slaw_write_handler h;
} sly_emitter_holder;

static ob_retort yaml_input_close (void *data)
{
  sly_parser_holder *holder = (sly_parser_holder *) data;
  ob_retort err = OB_OK;

  yaml_parser_delete (&(holder->parser));
  err = holder->h.close (holder->h.cookie);
  free (holder);

  return err;
}

static ob_retort write_comment_header (slaw_write_handler h)
{
  ob_retort tort = OB_NO_MEM;
  char *gsv = ob_get_version (OB_VERSION_OF_GSPEAK);
  char *cfg = ob_get_version (OB_BUILD_CONFIGURATION);
  slaw s = NULL;
  if (gsv && cfg && (s = slaw_string_format ("%s (%s), libYaml %s", gsv, cfg,
                                             yaml_get_version_string ())))
    {
      slaw str = slaw_string_format ("# %s - %s\n", ob_get_prog_name (),
                                     slaw_string_emit (s));
      slaw_free (s);
      if (str)
        tort = h.write (h.cookie, (const byte *) slaw_string_emit (str),
                        slaw_string_emit_length (str));
      slaw_free (str);
    }
  free (cfg);
  free (gsv);
  return tort;
}

static ob_retort yaml_output_close (void *data)
{
  sly_emitter_holder *holder = (sly_emitter_holder *) data;
  yaml_event_t event;
  ob_retort err = OB_OK;

  if (!(yaml_stream_end_event_initialize (&event)
        && yaml_emitter_emit (&(holder->emitter.emitter), &event)))
    err = PRINT_YAML_EMIT_ERROR (&(holder->emitter.emitter));

  yaml_emitter_delete (&(holder->emitter.emitter));
  ob_err_accum (&err, holder->h.close (holder->h.cookie));
  free (holder);

  return err;
}

static ob_retort yaml_input_read_slaw (slaw_input f, slaw *s)
{
  sly_parser_holder *holder = (sly_parser_holder *) f->data;
  ob_retort err;

  *s = sly_read_slaw_from_yaml (&(holder->parser), &err);

  return err;
}

static int yaml_read_handler (void *data, unsigned char *buffer, size_t size,
                              size_t *size_read)
{
  const slaw_read_handler *h = (const slaw_read_handler *) data;
  ob_retort tort = h->read (h->cookie, buffer, size, size_read);
  return (tort >= OB_OK);
}

ob_retort slaw_input_open_text_handler (slaw_read_handler h, slaw_input *f)
{
  sly_parser_holder *holder;
  yaml_event_t event;

  holder = (sly_parser_holder *) calloc (1, sizeof (sly_parser_holder));
  if (holder == NULL)
    {
      return OB_NO_MEM;
    }

  holder->h = h;

  if (!yaml_parser_initialize (&(holder->parser)))
    {
      ob_retort tmp = PRINT_YAML_PARSE_ERROR (&(holder->parser));
      free (holder);
      return tmp;
    }

  yaml_parser_set_input (&(holder->parser), yaml_read_handler, &(holder->h));

  /* Eat the stream start event */
  if (!yaml_parser_parse (&(holder->parser), &event)
      || event.type != YAML_STREAM_START_EVENT)
    {
      ob_retort err = PRINT_YAML_PARSE_ERROR (&(holder->parser));
      yaml_parser_delete (&(holder->parser));
      free (holder);
      return err;
    }

  *f = new_slaw_input ();
  if (!*f)
    {
      yaml_parser_delete (&(holder->parser));
      free (holder);
      return OB_NO_MEM;
    }

  (*f)->rfunc = yaml_input_read_slaw;
  (*f)->cfunc = yaml_input_close;
  (*f)->data = holder;

  return OB_OK;
}

static ob_retort yaml_output_write_slaw (slaw_output f, bslaw s)
{
  sly_emitter_holder *holder = (sly_emitter_holder *) f->data;
  yaml_event_t event;
  yaml_version_directive_t yvers;
  yaml_tag_directive_t tdir;
  ob_retort err;

  yvers.major = 1;
  yvers.minor = 1;
  tdir.handle = (yaml_char_t *) "!";
  tdir.prefix = (yaml_char_t *) SLAW_TAG_PREFIX;

  bool directives = holder->emitter.directives;

  if (!(yaml_document_start_event_initialize (&event,
                                              (directives ? &yvers : NULL),
                                              (directives ? &tdir : NULL),
                                              (directives ? (1 + &tdir) : NULL),
                                              false)
        && yaml_emitter_emit (&(holder->emitter.emitter), &event)))
    {
      return PRINT_YAML_EMIT_ERROR (&(holder->emitter.emitter));
    }

  err = sly_write_slaw_to_yaml (&(holder->emitter), s);
  if (err)
    return err;

  if (!(yaml_document_end_event_initialize (&event, false)
        && yaml_emitter_emit (&(holder->emitter.emitter), &event)))
    {
      return PRINT_YAML_EMIT_ERROR (&(holder->emitter.emitter));
    }

  /* There are many times when we want the slaw output stream to be
   * flushed, so for now let's do that automatically whenever we
   * write anything.  If there are reasons (performance?) not to do
   * this, then we might need to separate it out.
   */
  if (!yaml_emitter_flush (&(holder->emitter.emitter)))
    OB_LOG_ERROR_CODE (0x2000500a, "had trouble flushing\n");

  return holder->h.flush (holder->h.cookie);
}

static void transcribe_options (sly_emitter_info *info, bslaw options)
{
  info->tag_numbers = slaw_path_get_bool (options, "tag_numbers", true);
  info->directives = slaw_path_get_bool (options, "directives", true);
  info->ordered_maps = slaw_path_get_bool (options, "ordered_maps", true);
  info->max_elements = slaw_path_get_int64 (options, "max-array-elements", -1);
}

static int yaml_write_handler (void *data, unsigned char *buffer, size_t size)
{
  const slaw_write_handler *h = (const slaw_write_handler *) data;
  ob_retort tort = h->write (h->cookie, buffer, size);
  return (tort >= OB_OK);
}

ob_retort slaw_output_open_text_handler (slaw_write_handler h, bslaw options,
                                         slaw_output *f)
{
  sly_emitter_holder *holder;
  yaml_event_t event;

  holder = (sly_emitter_holder *) calloc (1, sizeof (sly_emitter_holder));
  if (holder == NULL)
    return OB_NO_MEM;

  transcribe_options (&(holder->emitter), options);

  holder->h = h;

  if (!yaml_emitter_initialize (&(holder->emitter.emitter)))
    {
      free (holder);
      return SLAW_YAML_ERR;
    }

  if (slaw_path_get_bool (options, "comment", true))
    write_comment_header (h);

  yaml_emitter_set_width (&(holder->emitter.emitter), 70);
  yaml_emitter_set_break (&(holder->emitter.emitter), YAML_LN_BREAK);
  yaml_emitter_set_unicode (&(holder->emitter.emitter), true);

  yaml_emitter_set_output (&(holder->emitter.emitter), yaml_write_handler,
                           &(holder->h));

  if (!(yaml_stream_start_event_initialize (&event, YAML_UTF8_ENCODING)
        && yaml_emitter_emit (&(holder->emitter.emitter), &event)))
    {
      ob_retort err = PRINT_YAML_EMIT_ERROR (&(holder->emitter.emitter));
      yaml_emitter_delete (&(holder->emitter.emitter));
      free (holder);
      return err;
    }

  *f = new_slaw_output ();
  if (!*f)
    {
      yaml_emitter_delete (&(holder->emitter.emitter));
      free (holder);
      return OB_NO_MEM;
    }

  (*f)->wfunc = yaml_output_write_slaw;
  (*f)->cfunc = yaml_output_close;
  (*f)->data = holder;

  return OB_OK;
}

// a testing function to make sure our hash values are right
// returns on success, exits on failure
// (called from libPlasma/c/tests/test-yaml.c)

void private_test_yaml_hash (void)
{
  const yaml_char_t *str;
  unt64 expected;
  unt64 got;

  str = (const yaml_char_t *) "!";
  expected = SE_NON_SPECIFIC;
  if (expected != (got = sly_hash (str)))
    goto fail;

  str = (const yaml_char_t *) "ingests";
  expected = SE_INGESTS;
  if (expected != (got = sly_hash (str)))
    goto fail;

  str = (const yaml_char_t *) "descrips";
  expected = SE_DESCRIPS;
  if (expected != (got = sly_hash (str)))
    goto fail;

  str = (const yaml_char_t *) "rude_data";
  expected = SE_RUDE_DATA;
  if (expected != (got = sly_hash (str)))
    goto fail;

  str = SLAW_MAP_TAG;
  expected = SE_MAP;
  if (expected != (got = sly_hash (str)))
    goto fail;

  str = SLAW_LIST_TAG;
  expected = SE_SEQ;
  if (expected != (got = sly_hash (str)))
    goto fail;

  str = SLAW_STRING_TAG;
  expected = SE_STR;
  if (expected != (got = sly_hash (str)))
    goto fail;

  str = SLAW_BOOL_TAG;
  expected = SE_BOOL;
  if (expected != (got = sly_hash (str)))
    goto fail;

  str = SLAW_OMAP_TAG;
  expected = SE_OMAP;
  if (expected != (got = sly_hash (str)))
    goto fail;

  str = SLAW_NULL_TAG;
  expected = SE_NULL;
  if (expected != (got = sly_hash (str)))
    goto fail;

  str = SLAW_BINARY_TAG;
  expected = SE_BINARY;
  if (expected != (got = sly_hash (str)))
    goto fail;

  str = (const yaml_char_t *) SLAW_TAG_PREFIX "i8";
  expected = SE_I8;
  if (expected != (got = sly_hash (str)))
    goto fail;

  str = (const yaml_char_t *) SLAW_TAG_PREFIX "u8";
  expected = SE_U8;
  if (expected != (got = sly_hash (str)))
    goto fail;

  str = (const yaml_char_t *) SLAW_TAG_PREFIX "f32";
  expected = SE_F32;
  if (expected != (got = sly_hash (str)))
    goto fail;

  str = (const yaml_char_t *) SLAW_TAG_PREFIX "i32";
  expected = SE_I32;
  if (expected != (got = sly_hash (str)))
    goto fail;

  str = (const yaml_char_t *) SLAW_TAG_PREFIX "f64";
  expected = SE_F64;
  if (expected != (got = sly_hash (str)))
    goto fail;

  str = (const yaml_char_t *) SLAW_TAG_PREFIX "i16";
  expected = SE_I16;
  if (expected != (got = sly_hash (str)))
    goto fail;

  str = (const yaml_char_t *) SLAW_TAG_PREFIX "i64";
  expected = SE_I64;
  if (expected != (got = sly_hash (str)))
    goto fail;

  str = (const yaml_char_t *) SLAW_TAG_PREFIX "u32";
  expected = SE_U32;
  if (expected != (got = sly_hash (str)))
    goto fail;

  str = (const yaml_char_t *) SLAW_TAG_PREFIX "u16";
  expected = SE_U16;
  if (expected != (got = sly_hash (str)))
    goto fail;

  str = (const yaml_char_t *) SLAW_TAG_PREFIX "u64";
  expected = SE_U64;
  if (expected != (got = sly_hash (str)))
    goto fail;

  str = SLAW_CONS_TAG;
  expected = SE_CONS;
  if (expected != (got = sly_hash (str)))
    goto fail;

  str = SLAW_ARRAY_TAG;
  expected = SE_ARRAY;
  if (expected != (got = sly_hash (str)))
    goto fail;

  str = SLAW_BADUTF8_TAG;
  expected = SE_BADUTF8;
  if (expected != (got = sly_hash (str)))
    goto fail;

  str = SLAW_NONSTD_TAG;
  expected = SE_NONSTD;
  if (expected != (got = sly_hash (str)))
    goto fail;

  str = SLAW_VECTOR_TAG;
  expected = SE_VECTOR;
  if (expected != (got = sly_hash (str)))
    goto fail;

  str = SLAW_PROTEIN_TAG;
  expected = SE_PROTEIN;
  if (expected != (got = sly_hash (str)))
    goto fail;

  str = SLAW_COMPLEX_TAG;
  expected = SE_COMPLEX;
  if (expected != (got = sly_hash (str)))
    goto fail;

  str = SLAW_MULTIVECTOR_TAG;
  expected = SE_MULTIVECTOR;
  if (expected != (got = sly_hash (str)))
    goto fail;

  str = SLAW_BROKEN_STRING_TAG;
  expected = SE_STRING;
  if (expected != (got = sly_hash (str)))
    goto fail;

  return;

fail:
  OB_FATAL_BUG_CODE (0x2000500b, "for '%s', got %" OB_FMT_64 "u"
                                 " but expected %" OB_FMT_64 "u\n",
                     str, got, expected);
}
