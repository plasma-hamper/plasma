
/* (c)  oblong industries */

#include "SlawRef.h"
#include "libPlasma/c++/PlasmaStreams.h"
#include "libLoam/c++/Str.h"

#include "libPlasma/c/slaw-coerce.h"

#include <algorithm>
#include <ostream>
#include <cassert>


namespace oblong {
namespace plasma {


/// Convenience null slaw values
const slaw NULL_SLAW (0);
const bslaw NULL_BSLAW (0);

namespace detail {

using namespace oblong::loam;


const SlawRef::slaw_ref SlawRef::NULL_REF;

SlawRef SlawRef::NilRef ()
{
  static SlawRef nil (slaw_nil ());
  return nil;
}

SlawRef::SlawRef () : parent_ (NULL_REF), slaw_ (NULL_BSLAW) {}

SlawRef::SlawRef (slaw s) : parent_ (s), slaw_ (s) {}

SlawRef::SlawRef (bslaw s, SlawRef parent) : parent_ (parent.parent_), slaw_ (s)
{
}

void SlawRef::SwapIfNull (SlawRef &other)
{
  if (this != &other)
    {
      slaw *ss = (slaw *) &slaw_;
      if (parent_.AtomicCompareAndSwap (NULL_REF, other.parent_))
        ob_atomic_pointer_compare_and_swap ((void **) ss, (void *) slaw_,
                                            (void *) other.slaw_);
    }
}

bool SlawRef::IsAtomic () const
{
  return !(IsNull () || IsArray () || IsCons () || IsList () || IsMap ()
           || IsProtein ());
}

Str SlawRef::ToStr () const
{
  if (!slaw_)
    return Str ("#NULL");

  if (slaw_is_numeric_vector (slaw_))
    {
      float64 v[4];
      int len;
      ob_retort tort = slaw_to_vn (slaw_, v, 4, &len);
      if (tort < OB_OK)
        return ob_error_string (tort);
      Str s;
      const char *sep = "<";
      for (int i = 0; i < len; i++)
        {
          s.Append (Str ().Sprintf ("%s%.17g", sep, v[i]));
          sep = ", ";
        }
      s.Append (UChar32 ('>'));
      return s;
    }

  if (slaw_is_numeric_float (slaw_))
    {
      float64 v = 0;
      ob_retort tort = slaw_to_float64 (slaw_, &v);
      if (tort < OB_OK)
        return ob_error_string (tort);
      return Str ().Sprintf ("%lf", v);
    }

  if (slaw_is_numeric_int (slaw_))
    {
      int64 v = 0;
      ob_retort tort = slaw_to_int64 (slaw_, &v);
      if (tort < OB_OK)
        return ob_error_string (tort);
      return Str ().Sprintf ("%" OB_FMT_64 "d", v);
    }

  if (slaw_is_numeric_unt (slaw_))
    {
      unt64 v = 0;
      ob_retort tort = slaw_to_unt64 (slaw_, &v);
      if (tort < OB_OK)
        return ob_error_string (tort);
      return Str ().Sprintf ("%" OB_FMT_64 "u", v);
    }

  if (slaw_is_boolean (slaw_))
    return Str (*slaw_boolean_emit (slaw_) ? "true" : "false");

  if (slaw_is_nil (slaw_))
    return Str ("nil");

  if (slaw_is_string (slaw_))
    return Str (slaw_string_emit (slaw_));

  assert (!IsAtomic ());

  return Str ("<composite slaw>");
}

void SlawRef::Spew (OStreamReference os) const
{
  slaw str = slaw_spew_overview_to_string (slaw_);
  os.os << slaw_string_emit (str);
  slaw_free (str);
}

::std::ostream &operator<< (::std::ostream &os,
                            const oblong::plasma::detail::SlawRef &ref)
{
  ref.Spew (os);
  return os;
}
}
}
}  // namespace oblong::plasma::detail
