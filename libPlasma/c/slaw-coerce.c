
/* (c)  oblong industries */

#include "libLoam/c/ob-sys.h"
#include "libLoam/c/ob-types.h"
#include "libPlasma/c/slaw.h"
#include "libPlasma/c/slaw-coerce.h"
#include "libPlasma/c/slaw-string.h"
#include "libPlasma/c/slaw-walk.h"
#include "libPlasma/c/private/plasma-private.h"
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Although gcc can successfully convert a float64 (i. e. double) to
 * an unt64, it appears that the Microsoft compiler cannot, if the
 * value is outside the range of an int64:
 * http://social.microsoft.com/Forums/en-US/vcgeneral/thread/d6703e17-a79b-4dbb-8760-bdb7f31dade0
 * http://connect.microsoft.com/VisualStudio/feedback/ViewFeedback.aspx?FeedbackID=106531
 * So let's leave the FPU behind and do this ourselves, without relying
 * on the compiler to generate the correct code.
 * Yes, this assumes IEEE floating point, but we were already
 * assuming that anyway.  (Sorry, VAXen!)
 * f64bits is the bit-wise representation of the float64, in an unt64.
 * result is where the converted unt64 is stored, unless an error occurs.
 */
static ob_retort homemade_float64bits_to_unt64 (unt64 f64bits, unt64 *result)
{
  const unt64 fifty_two = OB_CONST_U64 (1) << 52;
  unt64 fraction = f64bits & (fifty_two - 1);
  unt16 exponent = ((1 << 11) - 1) & (unt16) (f64bits >> 52);
  unt8 sign = (unt8) (f64bits >> 63);

  // zero is zero, regardless of sign bit
  if (fraction == 0 && exponent == 0)
    {
      *result = 0;
      return OB_OK;
    }

  // denormals, NaNs, and infinities are not going to convert to integers
  if (exponent == 0 || exponent == ((1 << 11) - 1))
    return SLAW_RANGE_ERR;

  // negative numbers won't convert to an unsigned integer
  if (sign)
    return SLAW_RANGE_ERR;

  // Let's stick in the implied most significant bit of the significand
  fraction |= fifty_two;

  // and let's unbias the exponent
  int16 sexponent = exponent - ((1 << 10) - 1);

  // well, but we also need to add our own bias for the fact that we're
  // already shifted 52 bits to the left
  sexponent -= 52;

  // then just shift until the exponent is zero
  while (sexponent > 0)
    {
      if ((fraction >> 63) != 0) /* will we lose a bit when we shift? */
        return SLAW_RANGE_ERR;
      fraction <<= 1;
      sexponent--;
    }

  while (sexponent < 0)
    {
      if ((fraction & 1) != 0) /* will we lose a bit when we shift? */
        return SLAW_RANGE_ERR;
      fraction >>= 1;
      sexponent++;
    }

  *result = fraction;
  return OB_OK;
}

typedef union
{
  float64 f;
  unt64 u;
} pun64;

static ob_retort homemade_float64_to_unt64 (float64 src, unt64 *result)
{
  pun64 bizarre;
  bizarre.f = src;
  return homemade_float64bits_to_unt64 (bizarre.u, result);
}

typedef enum
{
  NT_FINITE,
  NT_OVERFLOW,
  NT_INFINITY,
  NT_NAN,
  NT_PARSE_ERROR
} number_type;

/* This parses numbers in a way similar to Linux strtod, with the
 * following exceptions:
 *
 * - Does not allow leading whitespace or trailing extra characters
 * - Does not support '(' syntax for specific NaNs
 * - Allows an optional '.' before "NaN" and "Inf", for YAML compatibility
 */

static void private_parse_number (const char *str, number_type *what, bool *neg,
                                  unt64 *my_significand, unt8 *exp_base,
                                  int32 *exponent)
{
  *neg = false;
  bool sawDP = false;

  if (str[0] == '-')
    {
      *neg = true;
      str++;
    }
  else if (str[0] == '+')
    str++;

  if (str[0] == '.')
    {
      sawDP = true;
      str++;
    }

  if ((str[0] == 'N' || str[0] == 'n') && (str[1] == 'A' || str[1] == 'a')
      && (str[2] == 'N' || str[2] == 'n'))
    {
      *what = NT_NAN;
      return;
    }

  if ((str[0] == 'I' || str[0] == 'i') && (str[1] == 'N' || str[1] == 'n')
      && (str[2] == 'F' || str[2] == 'f'))
    {
      *what = NT_INFINITY;
      return;
    }

  bool hex = false;
  if (!sawDP && str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
    {
      hex = true;
      str += 2;
    }

  *exp_base = (hex ? 2 : 10);

  unt64 ficand = 0;
  int32 my_exp = 0;

parse_significand:

  while ((str[0] >= '0' && str[0] <= '9') || (str[0] >= 'a' && str[0] <= 'f')
         || (str[0] >= 'A' && str[0] <= 'F'))
    {
      char c = str[0];
      if (c >= 'a')
        c -= ('a' - 'A');

      unsigned char dig = 0;

      if (c >= '0' && c <= '9')
        dig = c - '0';
      else if (c >= 'A' && c <= 'F' && hex)
        dig = c - 'A' + 10;
      else if (c == 'E')
        break;
      else /* A, B, C, D, or F in a decimal number */
        {
          *what = NT_PARSE_ERROR;
          return;
        }

      unt64 old_ficand = ficand;

      if (hex)
        ficand <<= 4;
      else
        ficand *= 10;

      if (ficand < old_ficand)
        {
          *what = NT_OVERFLOW;
          return;
        }

      ficand += dig;
      str++;

      if (sawDP)
        my_exp -= (hex ? 4 : 1);
    }

  if (str[0] == '.' && !sawDP)
    {
      sawDP = true;
      str++;
      goto parse_significand;
    }

  *exponent = my_exp;
  *my_significand = ficand;

  if (str[0] == 0)
    {
      *what = NT_FINITE;
      return;
    }

  bool sawE = (str[0] == 'E' || str[0] == 'e');
  bool sawP = (str[0] == 'P' || str[0] == 'p');

  if (!(hex ? sawP : sawE))
    {
      *what = NT_PARSE_ERROR;
      return;
    }

  str++;

  my_exp = 0;
  int32 expmult = 1;

  if (str[0] == '-')
    {
      expmult = -1;
      str++;
    }
  else if (str[0] == '+')
    str++;

  while (str[0] >= '0' && str[0] <= '9')
    {
      my_exp *= 10;
      my_exp += (str[0] - '0');
      str++;
    }

  if (str[0] != 0)
    {
      *what = NT_PARSE_ERROR;
      return;
    }

  *exponent += my_exp * expmult;
  *what = NT_FINITE;
}

/* Windows strtod does not support all the features of Linux/Mac strtod,
 * like hex numbers.  Also, on no platform does strtod let us know
 * when we experience a loss of precision, so the number we print back
 * out might not match the number the user types in.
 * Avoid all this by writing our own replacement for strtod, which
 * will behave identically on all platforms, and will let us
 * detect loss of precision.
 */
static ob_retort private_strtof64 (const char *str, float64 *dest)
{
  number_type what;
  bool neg;
  unt64 my_significand = 0;
  unt8 exp_base = 0;
  int32 exponent = 0;

  private_parse_number (str, &what, &neg, &my_significand, &exp_base,
                        &exponent);

  switch (what)
    {
      case NT_FINITE:
        break; /* handle this below */
      case NT_OVERFLOW:
        return SLAW_RANGE_ERR;
      case NT_INFINITY:
        *dest = (neg ? OB_NEGINF : OB_POSINF);
        return OB_OK;
      case NT_NAN:
        *dest = OB_NAN;
        return OB_OK;
      case NT_PARSE_ERROR:
      default:
        return SLAW_NOT_NUMERIC;
    }

  int highestSet = -1;
  int lowestSet = -1;

  int i;

  for (i = 0; i < 64; i++)
    {
      if (((my_significand >> i) & 1) != 0)
        {
          highestSet = i;
          if (lowestSet < 0)
            lowestSet = i;
        }
    }

  int nBits = (highestSet - lowestSet) + 1;
  if (nBits > 52)
    return SLAW_RANGE_ERR;

  errno = 0;
  *dest = my_significand * pow ((double) exp_base, exponent);
  if (errno != 0)
    return SLAW_RANGE_ERR;

  if (neg)
    *dest = -*dest;

  return OB_OK;
}

static int64 private_coerce (bslaw s, bool *done)
{
  *done = true;

  if (slaw_is_boolean (s))
    return *slaw_boolean_emit (s);

  bool scalar = !slaw_is_numeric_vector (s) && !slaw_is_numeric_multivector (s);

  if (slaw_is_numeric (s) && !slaw_is_numeric_array (s) && scalar
      && !slaw_is_numeric_float (s) && !slaw_is_numeric_complex (s))
    {
      if (slaw_is_numeric_int (s))
        {
          if (slaw_is_numeric_8 (s))
            return *slaw_int8_emit (s);
          else if (slaw_is_numeric_16 (s))
            return *slaw_int16_emit (s);
          else if (slaw_is_numeric_32 (s))
            return *slaw_int32_emit (s);
          else if (slaw_is_numeric_64 (s))
            return *slaw_int64_emit (s);
        }
      else
        {
          if (slaw_is_numeric_8 (s))
            return *slaw_unt8_emit (s);
          else if (slaw_is_numeric_16 (s))
            return *slaw_unt16_emit (s);
          else if (slaw_is_numeric_32 (s))
            return *slaw_unt32_emit (s);
        }
    }

  if (slaw_is_numeric_array (s) && scalar && !slaw_is_numeric_float (s)
      && !slaw_is_numeric_complex (s) && slaw_numeric_array_count (s) == 1)
    {
      if (slaw_is_numeric_int (s))
        {
          if (slaw_is_numeric_8 (s))
            return *slaw_int8_array_emit (s);
          else if (slaw_is_numeric_16 (s))
            return *slaw_int16_array_emit (s);
          else if (slaw_is_numeric_32 (s))
            return *slaw_int32_array_emit (s);
          else if (slaw_is_numeric_64 (s))
            return *slaw_int64_array_emit (s);
        }
      else
        {
          if (slaw_is_numeric_8 (s))
            return *slaw_unt8_array_emit (s);
          else if (slaw_is_numeric_16 (s))
            return *slaw_unt16_array_emit (s);
          else if (slaw_is_numeric_32 (s))
            return *slaw_unt32_array_emit (s);
        }
    }

  *done = false;
  return 0;
}

static bslaw private_delistify (bslaw s)
{
  while (slaw_is_list (s) && (slaw_list_count (s) == 1))
    s = slaw_list_emit_first (s);

  return s;
}

ob_retort slaw_to_unt64 (bslaw s, unt64 *result)
{
  bool done;
  int64 n;
  ob_retort err;

  if (!s)
    return OB_ARGUMENT_WAS_NULL;

  s = private_delistify (s);

  n = private_coerce (s, &done);
  if (done)
    {
      if (n < 0)
        {
          err = SLAW_RANGE_ERR;
          *result = 0;
          return err;
        }
      else
        {
          err = OB_OK;
          *result = n;
          return err;
        }
    }

  err = OB_OK;
  bool scalar = !slaw_is_numeric_vector (s) && !slaw_is_numeric_multivector (s);

  if (slaw_is_numeric_64 (s) && scalar && !slaw_is_numeric_array (s)
      && !slaw_is_numeric_complex (s))
    {
      unt64 result_value;
      if (slaw_is_numeric_unt (s))
        {
          result_value = *slaw_unt64_emit (s);
        }
      else
        {
          float64 f1 = *slaw_float64_emit (s);
          result_value = 0;
          err = homemade_float64_to_unt64 (f1, &result_value);
        }
      *result = result_value;
      return err;
    }

  if (slaw_is_numeric_float (s) && slaw_is_numeric_32 (s) && scalar
      && !slaw_is_numeric_array (s) && !slaw_is_numeric_complex (s))
    {
      unt64 result_value = 0;
      float64 f1;
      f1 = *slaw_float32_emit (s);
      err = homemade_float64_to_unt64 (f1, &result_value);
      *result = result_value;
      return err;
    }

  if (slaw_is_numeric_64 (s) && scalar && slaw_is_numeric_array (s)
      && !slaw_is_numeric_complex (s) && slaw_numeric_array_count (s) == 1)
    {
      unt64 result_value;
      if (slaw_is_numeric_unt (s))
        {
          result_value = *slaw_unt64_array_emit (s);
        }
      else
        {
          float64 f1 = *slaw_float64_array_emit (s);
          result_value = 0;
          err = homemade_float64_to_unt64 (f1, &result_value);
        }
      *result = result_value;
      return err;
    }

  if (slaw_is_numeric_float (s) && slaw_is_numeric_32 (s) && scalar
      && slaw_is_numeric_array (s) && !slaw_is_numeric_complex (s)
      && slaw_numeric_array_count (s) == 1)
    {
      unt64 result_value = 0;
      float64 f1;
      f1 = *slaw_float32_array_emit (s);
      err = homemade_float64_to_unt64 (f1, &result_value);
      *result = result_value;
      return err;
    }

  if (slaw_is_string (s))
    {
      const char *str = slaw_string_emit (s);
      char *endptr;
      unt64 result_value;

      errno = 0;
      result_value = strtoull (str, &endptr, 0);
      if (errno == EINVAL)
        err = SLAW_NOT_NUMERIC;
      else if (errno == ERANGE)
        err = SLAW_RANGE_ERR;
      else if (errno != 0)
        err = ob_errno_to_retort (errno);
      else if (endptr == str || *endptr != 0)
        err = SLAW_NOT_NUMERIC;

      *result = result_value;
      return err;
    }

  err = SLAW_NOT_NUMERIC;
  *result = 0;
  return err;
}

ob_retort slaw_to_int64 (bslaw s, int64 *result)
{
  bool done;
  int64 n;
  ob_retort err;

  if (!s)
    return OB_ARGUMENT_WAS_NULL;

  err = OB_OK;

  s = private_delistify (s);

  n = private_coerce (s, &done);
  if (done)
    {
      *result = n;
      return err;
    }

  bool scalar = !slaw_is_numeric_vector (s) && !slaw_is_numeric_multivector (s);

  if (slaw_is_numeric_64 (s) && scalar && !slaw_is_numeric_array (s)
      && !slaw_is_numeric_complex (s))
    {
      if (slaw_is_numeric_unt (s))
        {
          unt64 u = *slaw_unt64_emit (s);
          if (u > OB_CONST_U64 (0x7fffffffffffffff))
            err = SLAW_RANGE_ERR;
          *result = u;
          return err;
        }
      else
        {
          int64 result_value;
          float64 f1, f2;
          f1 = *slaw_float64_emit (s);
          result_value = f1;
          f2 = result_value;
          if (f1 != f2)
            err = SLAW_RANGE_ERR;
          *result = result_value;
          return err;
        }
    }

  if (slaw_is_numeric_float (s) && slaw_is_numeric_32 (s) && scalar
      && !slaw_is_numeric_array (s) && !slaw_is_numeric_complex (s))
    {
      int64 result_value;
      float32 f1, f2;
      f1 = *slaw_float32_emit (s);
      result_value = f1;
      f2 = result_value;
      if (f1 != f2)
        err = SLAW_RANGE_ERR;
      *result = result_value;
      return err;
    }

  if (slaw_is_numeric_64 (s) && scalar && slaw_is_numeric_array (s)
      && !slaw_is_numeric_complex (s) && slaw_numeric_array_count (s) == 1)
    {
      if (slaw_is_numeric_unt (s))
        {
          unt64 u = *slaw_unt64_array_emit (s);
          if (u > OB_CONST_U64 (0x7fffffffffffffff))
            err = SLAW_RANGE_ERR;
          *result = u;
          return err;
        }
      else
        {
          int64 result_value;
          float64 f1, f2;
          f1 = *slaw_float64_array_emit (s);
          result_value = f1;
          f2 = result_value;
          if (f1 != f2)
            err = SLAW_RANGE_ERR;
          *result = result_value;
          return err;
        }
    }

  if (slaw_is_numeric_float (s) && slaw_is_numeric_32 (s)
      && slaw_is_numeric_array (s) && scalar && !slaw_is_numeric_complex (s)
      && slaw_numeric_array_count (s) == 1)
    {
      int64 result_value;
      float32 f1, f2;
      f1 = *slaw_float32_array_emit (s);
      result_value = f1;
      f2 = result_value;
      if (f1 != f2)
        err = SLAW_RANGE_ERR;
      *result = result_value;
      return err;
    }

  if (slaw_is_string (s))
    {
      const char *str = slaw_string_emit (s);
      char *endptr;
      int64 result_value;

      errno = 0;
      result_value = strtoll (str, &endptr, 0);
      if (errno == EINVAL)
        err = SLAW_NOT_NUMERIC;
      else if (errno == ERANGE)
        err = SLAW_RANGE_ERR;
      else if (errno != 0)
        err = ob_errno_to_retort (errno);
      else if (endptr == str || *endptr != 0)
        err = SLAW_NOT_NUMERIC;

      *result = result_value;
      return err;
    }

  err = SLAW_NOT_NUMERIC;
  *result = 0;
  return err;
}

ob_retort slaw_to_float64 (bslaw s, float64 *result)
{
  bool done;
  int64 n;
  ob_retort err;
  /* "volatile" to prevent the compiler from optimizing the round trip
   * when we are trying to see if we lost precision. */
  volatile float64 *res = result;

  if (!s)
    return OB_ARGUMENT_WAS_NULL;

  s = private_delistify (s);

  err = OB_OK;

  n = private_coerce (s, &done);
  if (done)
    {
      int64 n2;
      *res = n;
      n2 = *res;
      if (n != n2)
        err = SLAW_RANGE_ERR;
      return err;
    }

  bool scalar = !slaw_is_numeric_vector (s) && !slaw_is_numeric_multivector (s);

  if (slaw_is_numeric_64 (s) && scalar && !slaw_is_numeric_array (s)
      && !slaw_is_numeric_complex (s))
    {
      if (slaw_is_numeric_unt (s))
        {
          unt64 u1, u2;
          u1 = *slaw_unt64_emit (s);
          *res = u1;
          u2 = *res;
          if (u1 != u2)
            err = SLAW_RANGE_ERR;
        }
      else
        {
          *res = *slaw_float64_emit (s);
        }
      return err;
    }

  if (slaw_is_numeric_float (s) && slaw_is_numeric_32 (s) && scalar
      && !slaw_is_numeric_array (s) && !slaw_is_numeric_complex (s))
    {
      *res = *slaw_float32_emit (s);
      return err;
    }

  if (slaw_is_numeric_64 (s) && scalar && slaw_is_numeric_array (s)
      && !slaw_is_numeric_complex (s) && slaw_numeric_array_count (s) == 1)
    {
      if (slaw_is_numeric_unt (s))
        {
          unt64 u1, u2;
          u1 = *slaw_unt64_array_emit (s);
          *res = u1;
          u2 = *res;
          if (u1 != u2)
            err = SLAW_RANGE_ERR;
        }
      else
        {
          *res = *slaw_float64_array_emit (s);
        }
      return err;
    }

  if (slaw_is_numeric_float (s) && slaw_is_numeric_32 (s) && scalar
      && slaw_is_numeric_array (s) && !slaw_is_numeric_complex (s)
      && slaw_numeric_array_count (s) == 1)
    {
      *res = *slaw_float32_array_emit (s);
      return err;
    }

  if (slaw_is_string (s))
    {
      const char *str = slaw_string_emit (s);
      float64 f = OB_NAN;
      err = private_strtof64 (str, &f);
      *res = f;
      return err;
    }

  err = SLAW_NOT_NUMERIC;
  *res = OB_NAN;
  return err;
}

// XXX: seems like these don't generate SLAW_RANGE_ERR when dest is too narrow

#define DEFINE_NARROWING_COERCION(FROM, TO)                                    \
  ob_retort slaw_to_##TO (bslaw s, TO *to)                                     \
  {                                                                            \
    ob_retort err;                                                             \
    FROM v;                                                                    \
    err = slaw_to_##FROM (s, &v);                                              \
    if (OB_OK == err)                                                          \
      *to = (TO) (v);                                                          \
    return err;                                                                \
  }

DEFINE_NARROWING_COERCION (unt64, unt32);
DEFINE_NARROWING_COERCION (unt64, unt16);
DEFINE_NARROWING_COERCION (unt64, unt8);
DEFINE_NARROWING_COERCION (int64, int32);
DEFINE_NARROWING_COERCION (int64, int16);
DEFINE_NARROWING_COERCION (int64, int8);
DEFINE_NARROWING_COERCION (float64, float32);

#undef DEFINE_NARROWING_COERCION

ob_retort slaw_to_boolean (bslaw s, bool *result)
{
  ob_retort err;
  unt64 n;

  if (!s)
    return OB_ARGUMENT_WAS_NULL;

  s = private_delistify (s);

  if (slaw_is_string (s))
    {
      const char *str = slaw_string_emit (s);

      if ((str[0] == 'T' || str[0] == 't') && (str[1] == 'R' || str[1] == 'r')
          && (str[2] == 'U' || str[2] == 'u')
          && (str[3] == 'E' || str[3] == 'e') && str[4] == 0)
        {
          *result = true;
          return OB_OK;
        }
      else if ((str[0] == 'F' || str[0] == 'f')
               && (str[1] == 'A' || str[1] == 'a')
               && (str[2] == 'L' || str[2] == 'l')
               && (str[3] == 'S' || str[3] == 's')
               && (str[4] == 'E' || str[4] == 'e') && str[5] == 0)
        {
          *result = false;
          return OB_OK;
        }
    }

  err = slaw_to_unt64 (s, &n);
  if (err == OB_OK)
    {
      if (n > 1)
        err = SLAW_RANGE_ERR;
      else
        *result = (bool) n;
    }

  return err;
}

typedef struct array_walk_struct
{
  volatile float64 *dest;
  int *len;
} array_walk_struct;

// these next four functions shouldn't ever be called
static ob_retort private_not_numeric (OB_UNUSED void *dummy)
{
  return SLAW_NOT_NUMERIC;
}

static ob_retort private_not_numeric_length (OB_UNUSED void *dummy,
                                             OB_UNUSED int64 idiot)
{
  return SLAW_NOT_NUMERIC;
}

static ob_retort
private_not_numeric_string (OB_UNUSED void *dummy,
                            OB_UNUSED const char *ventriloquist,
                            OB_UNUSED int64 idiot)
{
  return SLAW_NOT_NUMERIC;
}

static ob_retort private_not_numeric_ptr (OB_UNUSED void *dummy,
                                          OB_UNUSED const void *ventriloquist,
                                          OB_UNUSED int64 idiot)
{
  return SLAW_NOT_NUMERIC;
}

static ob_retort private_nop (OB_UNUSED void *dummy)
{
  return OB_OK;
}

static ob_retort private_nop_length (OB_UNUSED void *dummy,
                                     OB_UNUSED int64 idiot)
{
  return OB_OK;
}

static ob_retort private_nop_array (OB_UNUSED void *dummy,
                                    OB_UNUSED int64 idiot, OB_UNUSED int foo)
{
  return OB_OK;
}

static ob_retort private_handle_int (void *cookie, int64 val,
                                     OB_UNUSED int bits)
{
  array_walk_struct *aws = (array_walk_struct *) cookie;
  int64 roundTrip;
  aws->dest[(*aws->len)] = val;
  roundTrip = aws->dest[(*aws->len)++];
  if (roundTrip != val)
    return SLAW_RANGE_ERR;
  else
    return OB_OK;
}

static ob_retort private_handle_unt (void *cookie, unt64 val,
                                     OB_UNUSED int bits)
{
  array_walk_struct *aws = (array_walk_struct *) cookie;
  unt64 roundTrip;
  aws->dest[(*aws->len)] = val;
  roundTrip = aws->dest[(*aws->len)++];
  if (roundTrip != val)
    return SLAW_RANGE_ERR;
  else
    return OB_OK;
}

static ob_retort private_handle_float (void *cookie, float64 val,
                                       OB_UNUSED int bits)
{
  array_walk_struct *aws = (array_walk_struct *) cookie;
  aws->dest[(*aws->len)++] = val;
  return OB_OK;
}

static ob_retort
private_handle_empty (OB_UNUSED void *cookie, OB_UNUSED int vecsize,
                      OB_UNUSED bool isMVec, OB_UNUSED bool isComplex,
                      OB_UNUSED bool isUnsigned, OB_UNUSED bool isFloat,
                      OB_UNUSED int bits)
{
  return SLAW_WRONG_LENGTH;
}

static const slaw_handler array_walk_handler = {
  private_not_numeric,        private_not_numeric,
  private_not_numeric_length, private_not_numeric,
  private_not_numeric_length, private_not_numeric,
  private_nop_array,          private_nop,
  private_nop_length,         private_nop,
  private_nop_length,         private_nop,
  private_not_numeric,        private_not_numeric,
  private_not_numeric,        private_not_numeric_string,
  private_handle_int,         private_handle_unt,
  private_handle_float,       private_handle_empty,
  private_not_numeric,        private_not_numeric,
  private_not_numeric_ptr,    private_not_numeric,
  private_not_numeric,        private_not_numeric,
  private_not_numeric,        private_not_numeric_ptr};

ob_retort slaw_to_vn (bslaw src, float64 *dest, int capacity, int *len)
{
  if (slaw_is_list (src) && slaw_list_count (src) <= capacity)
    {
      int i = 0;
      bslaw cole;

      for (cole = slaw_list_emit_first (src); cole != NULL;
           cole = slaw_list_emit_next (src, cole))
        {
          ob_retort err = slaw_to_float64 (cole, dest + i++);
          if (err != OB_OK)
            return err;
        }

      *len = i;
      return OB_OK;
    }
  else if (slaw_is_string (src))
    {
      slaw lst;
      ob_retort err = OB_OK;
      int64 i;

      slabu *sb = slabu_of_strings_from_split (slaw_string_emit (src), ",");
      if (!sb)
        return OB_NO_MEM;

      // Remove leading and trail spaces from each element
      for (i = 0; i < slabu_count (sb); i++)
        {
          const char *str = slaw_string_emit (slabu_list_nth (sb, i));
          size_t origLen = strlen (str);
          size_t newLen = origLen;

          while (newLen > 0 && str[newLen - 1] == ' ')
            newLen--;

          while (newLen > 0 && str[0] == ' ')
            {
              str++;
              newLen--;
            }

          if (newLen != origLen)
            {
              slaw sub = slaw_string_from_substring (str, newLen);
              if (!sub || (err = slabu_list_replace_nth_x (sb, i, sub)) < OB_OK)
                {
                  slabu_free (sb);
                  return (sub ? err : OB_NO_MEM);
                }
            }
        }

      lst = slaw_list_f (sb);
      if (!lst)
        return OB_NO_MEM;

      err = slaw_to_vn (lst, dest, capacity, len);
      slaw_free (lst);
      return err;
    }
  else if ((slaw_is_numeric (src) && !slaw_is_numeric_complex (src))
           && ((slaw_is_numeric_vector (src)
                && (slaw_is_numeric (src) && !slaw_is_numeric_array (src)))
               || ((slaw_is_numeric (src) && !slaw_is_numeric_vector (src))
                   && slaw_is_numeric_array (src)
                   && slaw_numeric_array_count (src) <= capacity)))
    {
      array_walk_struct aws;
      aws.dest = dest;
      aws.len = len;
      *len = 0;
      return slaw_walk (&aws, &array_walk_handler, src);
    }
  else if ((slaw_is_list (src) && slaw_list_count (src) > capacity)
           || (slaw_is_numeric_array (src)
               && slaw_numeric_array_count (src) > capacity))
    {
      return SLAW_WRONG_LENGTH;
    }
  else
    {
      return SLAW_NOT_NUMERIC;
    }
}

ob_retort slaw_to_v2 (bslaw s, v2float64 *result)
{
  ob_retort err;
  float64 v[4];
  int len;

  err = slaw_to_vn (s, v, 4, &len);
  if (err != OB_OK)
    return err;

  if (len != 2)
    return SLAW_WRONG_LENGTH;

  result->x = v[0];
  result->y = v[1];
  return OB_OK;
}

ob_retort slaw_to_v2float64 (bslaw s, v2float64 *r)
{
  return slaw_to_v2 (s, r);
}

ob_retort slaw_to_v3 (bslaw s, v3float64 *result)
{
  ob_retort err;
  float64 v[4];
  int len;

  // for performance, shortcut when s is already a v3float64
  const v3float64 *scut = slaw_v3float64_emit (s);
  if (scut)
    {
      *result = *scut;
      return OB_OK;
    }
  // end shortcut

  err = slaw_to_vn (s, v, 4, &len);
  if (err != OB_OK)
    return err;

  if (len == 3)
    {
      result->x = v[0];
      result->y = v[1];
      result->z = v[2];
      return OB_OK;
    }
  else if (len == 2)
    {
      result->x = v[0];
      result->y = v[1];
      result->z = 0.0;
      return OB_OK;
    }
  return SLAW_WRONG_LENGTH;
}

ob_retort slaw_to_v3float64 (bslaw s, v3float64 *r)
{
  return slaw_to_v3 (s, r);
}

ob_retort slaw_to_v4 (bslaw s, v4float64 *result)
{
  ob_retort err;
  float64 v[4];
  int len;

  err = slaw_to_vn (s, v, 4, &len);
  if (err != OB_OK)
    return err;

  if (len != 4)
    return SLAW_WRONG_LENGTH;

  result->x = v[0];
  result->y = v[1];
  result->z = v[2];
  result->w = v[3];
  return OB_OK;
}

ob_retort slaw_to_v4float64 (bslaw s, v4float64 *r)
{
  return slaw_to_v4 (s, r);
}

#define DEFINE_V2_COERCION(T)                                                  \
  ob_retort slaw_to_v2##T (bslaw s, v2##T *r)                                  \
  {                                                                            \
    v2float64 v;                                                               \
    ob_retort result = slaw_to_v2 (s, &v);                                     \
    if (OB_OK == result)                                                       \
      {                                                                        \
        r->x = (T) v.x;                                                        \
        r->y = (T) v.y;                                                        \
      }                                                                        \
    return result;                                                             \
  }

#define DEFINE_V3_COERCION(T)                                                  \
  ob_retort slaw_to_v3##T (bslaw s, v3##T *r)                                  \
  {                                                                            \
    v3float64 v;                                                               \
    ob_retort result = slaw_to_v3 (s, &v);                                     \
    if (OB_OK == result)                                                       \
      {                                                                        \
        r->x = (T) v.x;                                                        \
        r->y = (T) v.y;                                                        \
        r->z = (T) v.z;                                                        \
      }                                                                        \
    return result;                                                             \
  }

#define DEFINE_V4_COERCION(T)                                                  \
  ob_retort slaw_to_v4##T (bslaw s, v4##T *r)                                  \
  {                                                                            \
    v4float64 v;                                                               \
    ob_retort result = slaw_to_v4 (s, &v);                                     \
    if (OB_OK == result)                                                       \
      {                                                                        \
        r->x = (T) v.x;                                                        \
        r->y = (T) v.y;                                                        \
        r->z = (T) v.z;                                                        \
        r->w = (T) v.w;                                                        \
      }                                                                        \
    return result;                                                             \
  }

#define DEFINE_V_COERCIONS(T)                                                  \
  DEFINE_V2_COERCION (T)                                                       \
  DEFINE_V3_COERCION (T)                                                       \
  DEFINE_V4_COERCION (T)

DEFINE_V_COERCIONS (int64);
DEFINE_V_COERCIONS (int32);
DEFINE_V_COERCIONS (int16);
DEFINE_V_COERCIONS (int8);
DEFINE_V_COERCIONS (unt64);
DEFINE_V_COERCIONS (unt32);
DEFINE_V_COERCIONS (unt16);
DEFINE_V_COERCIONS (unt8);
DEFINE_V_COERCIONS (float32);
