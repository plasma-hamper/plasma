
/* (c)  oblong industries */

#include "SlawList.h"

#include "slaw-traits.h"
#include "SlawMap.h"
#include "PlasmaStreams.h"

#include <libPlasma/c/protein.h>

#include "compat.h"

#include <functional>
#include <algorithm>
#include <ostream>
#include <cassert>


namespace oblong {
namespace plasma {
namespace detail {

namespace slawlist {

SlawRefs GetAtomComponents (SlawRef s)
{
  SlawRefs result;
  if (!s.IsNull ())
    result.push_back (s);
  return result;
}

SlawRefs GetConsComponents (SlawRef cons)
{
  SlawRefs result;
  result.push_back (SlawRef (slaw_cons_emit_car (cons), cons));
  result.push_back (SlawRef (slaw_cons_emit_car (cons), cons));
  return result;
}

SlawRefs GetListComponents (SlawRef list)
{
  SlawRefs result;
  const int64 cnt (slaw_list_count (list));
  if (cnt > 0)
    result.reserve (cnt);
  for (bslaw s = slaw_list_emit_first (list); s != NULL;
       s = slaw_list_emit_next (list, s))
    result.push_back (SlawRef (s, list));
  return result;
}

SlawRefs GetComponents (SlawRef s);

SlawRefs GetProteinComponents (SlawRef protein)
{
  return GetComponents (SlawRef (protein_descrips (protein), protein));
}

template <typename T>
SlawRefs GetTypedArrayComponents (bslaw array)
{
  SlawRefs result;
  const int64 len = slaw_numeric_array_count (array);
  if (len > 0)
    result.reserve (len);
  const T *data = (const T *) (slaw_numeric_array_emit (array));
  for (int64 i = 0; i < len; ++i)
    result.push_back (SlawRef (make_slaw<T> (data[i])));
  return result;
}

SlawRefs GetArrayComponents (bslaw array)
{
#define COMPONENTS_FOR_ATYPE(T)                                                \
  if (slaw_traits<const T *>::TypeP (array))                                   \
  return GetTypedArrayComponents<T> (array)

#define COMPONENTS_FOR_VTYPES(T)                                               \
  COMPONENTS_FOR_ATYPE (v2##T);                                                \
  COMPONENTS_FOR_ATYPE (v3##T);                                                \
  COMPONENTS_FOR_ATYPE (v4##T);

#define COMPONENTS_FOR_TYPE(T)                                                 \
  COMPONENTS_FOR_ATYPE (T);                                                    \
  COMPONENTS_FOR_VTYPES (T)

  COMPONENTS_FOR_TYPE (int8);
  COMPONENTS_FOR_TYPE (int16);
  COMPONENTS_FOR_TYPE (int32);
  COMPONENTS_FOR_TYPE (int64);
  COMPONENTS_FOR_TYPE (unt8);
  COMPONENTS_FOR_TYPE (unt16);
  COMPONENTS_FOR_TYPE (unt32);
  COMPONENTS_FOR_TYPE (unt64);
  COMPONENTS_FOR_TYPE (float32);
  COMPONENTS_FOR_TYPE (float64);

  slaw spew = slaw_spew_overview_to_string (array);
  OB_FATAL_BUG_CODE (0x21020000, "unrecognized type in GetArrayComponents\n%s",
                     slaw_string_emit (spew));
  return SlawRefs ();

#undef COMPONENTS_FOR_TYPE
#undef COMPONENTS_FOR_VTYPES
#undef COMPONENTS_FOR_ATYPE
}

SlawRefs GetComponents (SlawRef s)
{
  if (s.IsCons ())
    return GetConsComponents (s);
  if (s.IsList () || s.IsMap ())
    return GetListComponents (s);
  if (s.IsProtein ())
    return GetProteinComponents (s);
  if (s.IsArray ())
    return GetArrayComponents (s);
  return GetAtomComponents (s);
}

}  // namespace

/* ---------------------------------------------------------------------- */
// Construction

SlawList::SlawList () {}

SlawList::SlawList (SlawRef s) : elements_ (slawlist::GetComponents (s)) {}

SlawList::SlawList (const SlawRefs &elems) : elements_ (elems) {}

/* ---------------------------------------------------------------------- */
// Typing

bool SlawList::IsList () const
{
  return true;
}

bool SlawList::IsNull () const
{
  return false;
}


SlawRef SlawList::AsSlaw () const
{
  const SlawRefs &cmps = elements_;
  slabu *sb = slabu_new ();
  for (int64 i = 0, s = Count (); i < s; ++i)
    slabu_list_add (sb, cmps[i]);
  return SlawRef (slaw_list_f (sb));
}

bool SlawList::IsEqual (const CompositeSlaw *other) const
{
  int64 s = Count ();
  if (other->Count () != s)
    return false;
  for (int64 i = 0; i < s; ++i)
    if (Nth (i) != other->Nth (i))
      return false;
  return true;
}

/* ---------------------------------------------------------------------- */
// List interface

int64 SlawList::Count () const
{
  return elements_.size ();
}

int64 SlawList::IndexOf (bslaw s, unt64 start) const
{
  using namespace std;
  if (start >= elements_.size ())
    return -1;
  ConstSlawIter it = find_if (elements_.begin () + start, elements_.end (),
                              bind2nd (mem_fun_ref (&SlawRef::Equals), s));
  if (it == elements_.end ())
    return -1;
  return int64 (it - elements_.begin ());
}

SlawRef SlawList::Nth (int64 n) const
{
  // don't call Count() because it is virtual and can't be inlined
  const int64 len = elements_.size ();
  if (n < 0)
    n += len;
  if (n < 0 || n >= len)
    return SlawRef ();
  return elements_[size_t (n)];
}

SlawRefs SlawList::Slice (int64 begin, int64 end) const
{
  const int64 len = Count ();
  if (begin < 0)
    begin = len + begin;
  if (end < 0)
    end = len + end;

  SlawRefs result;
  if (begin >= 0 && end > begin)
    for (int64 idx = begin, top = (std::min) (len, int64 (end)); idx < top;
         ++idx)
      result.push_back (elements_[idx]);

  return result;
}

/* ---------------------------------------------------------------------- */
// Map interface

SlawRef SlawList::Find (bslaw key) const
{
  SlawRef value;
  for (ConstSlawIter i = elements_.begin (), e = elements_.end (); i != e; ++i)
    {
      const SlawRef &r = *i;
      if (r.IsCons () && slawx_equal (key, slaw_cons_emit_car (r)))
        value = SlawRef (slaw_cons_emit_cdr (r), r);
    }
  return value;
}

unt64 SlawList::KeyCount () const
{
  return Count ();
}

SlawRefs SlawList::KVList () const
{
  SlawRefs result;
  for (ConstSlawIter i = elements_.begin (), e = elements_.end (); i != e; ++i)
    {
      if (i->IsCons ())
        {
          result.push_back (SlawRef (slaw_cons_emit_car (*i), *i));
          result.push_back (SlawRef (slaw_cons_emit_cdr (*i), *i));
        }
    }

  return result;
}

/* ---------------------------------------------------------------------- */
// Protein interface

SlawRefs SlawList::IngestList () const
{
  return SlawRefs ();
}

SlawRefs SlawList::DescripList () const
{
  return elements_;
}

/* ---------------------------------------------------------------------- */
// Implementation and debugging

void SlawList::Add (SlawRef elem)
{
  elements_.push_back (elem);
}

Str SlawList::ToStr () const
{
  Str r ("[");
  for (SRSize i = 0, N = elements_.size (); i < N; ++i)
    {
      r += RefToStr (elements_[i]);
      if (i < N - 1)
        r += ", ";
    }
  return r + "]";
}

void SlawList::Spew (OStreamReference os) const
{
  const SRSize LEN = elements_.size ();
  os.os << "#LIST(" << LEN << ")<" << ::std::endl;
  for (size_t i = 0; i < LEN; ++i)
    os.os << elements_[i] << ::std::endl;
  os.os << ">";
}
}
}
}  // namespace oblong::plasma::detail

PLASMA_RESTORE_WARNINGS
