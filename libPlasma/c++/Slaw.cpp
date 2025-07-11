
/* (c)  oblong industries */

#include "Slaw.h"
#include "SlawIterator.h"
#include "libPlasma/c++/PlasmaStreams.h"


#include "compat.h"

#include <libPlasma/c/protein.h>
#include <libPlasma/c/slaw-io.h>

#include <libLoam/c/ob-log.h>

#include <algorithm>
#include <cstdarg>
#include <ostream>
#include <functional>


namespace oblong {
namespace plasma {

namespace {

const Slaw A_NULL_SLAW;

}  // namespace

/* ---------------------------------------------------------------------- */
// Construction

const Slaw &Slaw::NilSlaw ()
{
  static const Slaw nil (slaw_nil ());
  return nil;
}

const Slaw &Slaw::Null ()
{
  return A_NULL_SLAW;
}

Slaw::~Slaw ()
{
}

Slaw::Slaw ()
{
}

Slaw::Slaw (slaw s) : slaw_ (s)
{
}

Slaw::Slaw (detail::SlawRef s) : slaw_ (s)
{
}

Slaw::Slaw (detail::CompositeSlaw::Ref c) : composite_ (c)
{
}

/* ---------------------------------------------------------------------- */
// Equality and typing

bool Slaw::operator== (const Slaw &other) const
{
  if (IsNull ())
    return other.IsNull ();
  if (other.IsNull ())
    return false;

  return ((!slaw_.IsNull () && slaw_.Equals (other.slaw_))
          || Composite ()->Equals (other.Composite ()));
}

bool Slaw::operator!= (const Slaw &other) const
{
  return !(other == *this);
}

bool Slaw::IsNull () const
{
  return (composite_->IsNull () && slaw_.IsNull ());
}

bool Slaw::IsNil () const
{
  return slaw_is_nil (slaw_);
}

bool Slaw::IsAtomic () const
{
  return (!IsArray () && !IsComposite ());
}

bool Slaw::IsArray () const
{
  return slaw_.IsArray ();
}

bool Slaw::IsComposite () const
{
  return (!composite_->IsNull () || IsCons () || IsList () || IsMap ()
          || IsProtein ());
}

bool Slaw::IsCons () const
{
  return (composite_->IsCons () || slaw_.IsCons ());
}

bool Slaw::IsList () const
{
  return (composite_->IsList () || slaw_.IsList ());
}

bool Slaw::IsMap () const
{
  return (composite_->IsMap () || slaw_.IsMap ());
}

bool Slaw::IsProtein () const
{
  return (composite_->IsProtein () || slaw_.IsProtein ());
}


bool Slaw::Into (Slaw &value) const
{
  value = *this;
  return true;
}

/* ---------------------------------------------------------------------- */
// Serialization

bool Slaw::ToFile (const Str &path, ObRetort *err,
                   const OutputFormat &fmt) const
{
  ob_retort res = OB_OK;
  if (IsNull ())
    res = OB_ARGUMENT_WAS_NULL;
  else if (fmt.IsBinary ())
    res = slaw_write_to_binary_file (path, SlawValue ());
  else
    {
      slaw_output f;
      res =
        slaw_output_open_text_options (path, &f, fmt.AsSlaw ().SlawValue ());
      if (OB_OK == res)
        {
          res = slaw_output_write (f, SlawValue ());
          ob_retort e = slaw_output_close (f);
          if (OB_OK == res)
            res = e;
        }
    }
  if (err)
    *err = ObRetort (res);
  return (res == OB_OK);
}

Slaw Slaw::FromFile (const Str &path, ObRetort *err)
{
  slaw s (NULL);
  ObRetort res (slaw_read_from_file (path, &s));
  if (err)
    *err = res;
  return Slaw (s);
}

Slaw Slaw::ToStringSlaw (ObRetort *err, const YAMLFormat &fmt) const
{
  slaw s (NULL);
  ObRetort res (
    slaw_to_string_options (SlawValue (), &s, fmt.AsSlaw ().SlawValue ()));
  if (err)
    *err = res;
  Slaw result (s);
  return result;
}

Str Slaw::ToString (ObRetort *err, const YAMLFormat &fmt) const
{
  Slaw str (ToStringSlaw (err, fmt));
  return str.Emit<Str> ();
}

Slaw Slaw::FromString (const Str &str, ObRetort *err)
{
  slaw s (NULL);
  ObRetort res (slaw_from_string (str, &s));
  if (err)
    *err = res;
  return Slaw (s);
}

Str Slaw::ToPrintableString () const
{
  return Composite ()->ToStr ();
}

void Slaw::Spew (OStreamReference os) const
{
  if (slaw_.IsNull ())
    composite_->Spew (os);
  else
    slaw_.Spew (os);
}

void Slaw::Spew (FILE *ph) const
{
  if (!ph)
    return;
  slaw_spew_overview (SlawValue (), ph, NULL);
}

void Slaw::SpewToStderr () const
{
  slaw_spew_overview_to_stderr (SlawValue ());
}


/* ---------------------------------------------------------------------- */
// Access to sub-structure

bslaw Slaw::SlawValue () const
{
  return SlawRef ();
}

SlawIterator Slaw::begin () const
{
  return SlawIterator (*this);
}

SlawIterator Slaw::end () const
{
  return (SlawIterator (*this) + std::max (int64 (0), Count ()));
}

const detail::CompositeSlaw::Ref &Slaw::Composite () const
{
  if (!slaw_.IsNull () && composite_->IsNull ())
    {
      detail::CompositeSlaw::Ref c (detail::CompositeSlaw::Make (slaw_));
      composite_.SwapIfNull (c);
    }
  return composite_;
}

const detail::SlawRef &Slaw::SlawRef () const
{
  if (slaw_.IsNull () && !composite_->IsNull ())
    {
      detail::SlawRef s (composite_->AsSlaw ());
      slaw_.SwapIfNull (s);
    }
  return slaw_;
}

detail::SlawRef Slaw::GetRef (const Slaw &s, const detail::SlawRefs &)
{
  return s.SlawRef ();
}

detail::SlawRef Slaw::GetRef (slaw s, const detail::SlawRefs &c)
{
  if (!s)
    return detail::SlawRef ();
  for (detail::ConstSlawIter i = c.begin (), e = c.end (); i != e; ++i)
    {
      if (bslaw (*i) == s)
        return *i;
    }
  return detail::SlawRef (s);
}

/* ---------------------------------------------------------------------- */
// Cons interface

Slaw Slaw::Car () const
{
  return Slaw (Composite ()->Nth (0));
}

Slaw Slaw::Cdr () const
{
  return (IsCons () ? Slaw (Composite ()->Nth (1)) : Slice (1, Count ()));
}

Slaw Slaw::Cons (const Slaw &car, const Slaw &cdr)
{
  if (car.IsNull () || cdr.IsNull ())
    return Slaw ();
  return Slaw (detail::CompositeSlaw::Cons (car.SlawRef (), cdr.SlawRef ()));
}

Slaw Slaw::Cons (slaw car, slaw cdr)
{
  detail::SlawRef car_ref (car);
  detail::SlawRef cdr_ref ((car != cdr) ? detail::SlawRef (cdr) : car_ref);
  if (car_ref.IsNull () || cdr_ref.IsNull ())
    return Slaw ();
  return Slaw (detail::CompositeSlaw::Cons (car_ref, cdr_ref));
}

/* ---------------------------------------------------------------------- */
// List interface

namespace {

const Slaw ENDL = A_NULL_SLAW;

}  // namespace

Slaw Slaw::MakeList (const Slaw &s0 = ENDL, const Slaw &s1 = ENDL,
                     const Slaw &s2 = ENDL, const Slaw &s3 = ENDL,
                     const Slaw &s4 = ENDL, const Slaw &s5 = ENDL,
                     const Slaw &s6 = ENDL, const Slaw &s7 = ENDL,
                     const Slaw &s8 = ENDL, const Slaw &s9 = ENDL)
{
  const std::size_t LEN = 10;
  const Slaw *s[LEN] = {&s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7, &s8, &s9};
  detail::SlawRefs components;
  components.reserve (LEN);
  for (std::size_t i = 0; i < LEN && s[i] != &ENDL; ++i)
    if (!s[i]->IsNull ())
      components.push_back (s[i]->SlawRef ());
  return Slaw (detail::CompositeSlaw::List (components));
}

Slaw Slaw::MakeList (slaw s0, slaw s1 = NULL, slaw s2 = NULL, slaw s3 = NULL,
                     slaw s4 = NULL, slaw s5 = NULL, slaw s6 = NULL,
                     slaw s7 = NULL, slaw s8 = NULL, slaw s9 = NULL)
{
  const ::std::size_t LEN = 10;
  slaw s[LEN] = {s0, s1, s2, s3, s4, s5, s6, s7, s8, s9};
  return ListCollect (s, s + LEN, LEN);
}

Slaw Slaw::ListCollect (slaw *s, unt32 len)
{
  return ListCollect (s, s + len, len);
}

Slaw Slaw::List ()
{
  static Slaw empty (detail::CompositeSlaw::List (detail::SlawRefs ()));
  return empty;
}

Slaw Slaw::List (const Slaw &s0)
{
  return MakeList (s0);
}

Slaw Slaw::List (const Slaw &s0, const Slaw &s1)
{
  return MakeList (s0, s1);
}

Slaw Slaw::List (const Slaw &s0, const Slaw &s1, const Slaw &s2)
{
  return MakeList (s0, s1, s2);
}

Slaw Slaw::List (const Slaw &s0, const Slaw &s1, const Slaw &s2, const Slaw &s3)
{
  return MakeList (s0, s1, s2, s3);
}

Slaw Slaw::List (const Slaw &s0, const Slaw &s1, const Slaw &s2, const Slaw &s3,
                 const Slaw &s4)
{
  return MakeList (s0, s1, s2, s3, s4);
}

Slaw Slaw::List (const Slaw &s0, const Slaw &s1, const Slaw &s2, const Slaw &s3,
                 const Slaw &s4, const Slaw &s5)
{
  return MakeList (s0, s1, s2, s3, s4, s5);
}

Slaw Slaw::List (const Slaw &s0, const Slaw &s1, const Slaw &s2, const Slaw &s3,
                 const Slaw &s4, const Slaw &s5, const Slaw &s6)
{
  return MakeList (s0, s1, s2, s3, s4, s5, s6);
}

Slaw Slaw::List (const Slaw &s0, const Slaw &s1, const Slaw &s2, const Slaw &s3,
                 const Slaw &s4, const Slaw &s5, const Slaw &s6, const Slaw &s7)
{
  return MakeList (s0, s1, s2, s3, s4, s5, s6, s7);
}

Slaw Slaw::List (const Slaw &s0, const Slaw &s1, const Slaw &s2, const Slaw &s3,
                 const Slaw &s4, const Slaw &s5, const Slaw &s6, const Slaw &s7,
                 const Slaw &s8)
{
  return MakeList (s0, s1, s2, s3, s4, s5, s6, s7, s8);
}

Slaw Slaw::List (const Slaw &s0, const Slaw &s1, const Slaw &s2, const Slaw &s3,
                 const Slaw &s4, const Slaw &s5, const Slaw &s6, const Slaw &s7,
                 const Slaw &s8, const Slaw &s9)
{
  return MakeList (s0, s1, s2, s3, s4, s5, s6, s7, s8, s9);
}

Slaw Slaw::List (slaw s0)
{
  return MakeList (s0);
}

Slaw Slaw::List (slaw s0, slaw s1)
{
  return MakeList (s0, s1);
}

Slaw Slaw::List (slaw s0, slaw s1, slaw s2)
{
  return MakeList (s0, s1, s2);
}

Slaw Slaw::List (slaw s0, slaw s1, slaw s2, slaw s3)
{
  return MakeList (s0, s1, s2, s3);
}

Slaw Slaw::List (slaw s0, slaw s1, slaw s2, slaw s3, slaw s4)
{
  return MakeList (s0, s1, s2, s3, s4);
}

Slaw Slaw::List (slaw s0, slaw s1, slaw s2, slaw s3, slaw s4, slaw s5)
{
  return MakeList (s0, s1, s2, s3, s4, s5);
}

Slaw Slaw::List (slaw s0, slaw s1, slaw s2, slaw s3, slaw s4, slaw s5, slaw s6)
{
  return MakeList (s0, s1, s2, s3, s4, s5, s6);
}

Slaw Slaw::List (slaw s0, slaw s1, slaw s2, slaw s3, slaw s4, slaw s5, slaw s6,
                 slaw s7)
{
  return MakeList (s0, s1, s2, s3, s4, s5, s6, s7);
}

Slaw Slaw::List (slaw s0, slaw s1, slaw s2, slaw s3, slaw s4, slaw s5, slaw s6,
                 slaw s7, slaw s8)
{
  return MakeList (s0, s1, s2, s3, s4, s5, s6, s7, s8);
}

Slaw Slaw::List (slaw s0, slaw s1, slaw s2, slaw s3, slaw s4, slaw s5, slaw s6,
                 slaw s7, slaw s8, slaw s9)
{
  return MakeList (s0, s1, s2, s3, s4, s5, s6, s7, s8, s9);
}

int64 Slaw::Count () const
{
  if (slaw_.IsNull ())
    return composite_->Count ();
  if (slaw_.IsAtomic ())
    return 1;
  if (slaw_.IsArray ())
    return slaw_numeric_array_count (slaw_);
  return slaw_list_count (slaw_);
}

Slaw Slaw::Nth (int64 n) const
{
  return Slaw (Composite ()->Nth (n));
}

Slaw Slaw::operator[] (int64 n) const
{
  return Nth (n);
}

Slaw Slaw::ListInsert (const Slaw &s, int64 n) const
{
  if (n < 0)
    n += Count ();
  if (n < 0)
    return ListPrepend (s);
  if (n >= Count ())
    return ListAppend (s);
  detail::SlawRefs cmps = Composite ()->Components ();
  if (!s.IsNull ())
    cmps.insert (cmps.begin () + n, s.SlawRef ());
  return Slaw (detail::CompositeSlaw::List (cmps));
}

Slaw Slaw::ListInsert (bslaw s, int64 n) const
{
  return ListInsert (Slaw (slaw_dup (s)), n);
}

Slaw Slaw::ListAppend (const Slaw &s) const
{
  detail::SlawRefs cmps = Composite ()->Components ();
  if (!s.IsNull ())
    cmps.push_back (s.SlawRef ());
  return Slaw (detail::CompositeSlaw::List (cmps));
}

Slaw Slaw::ListPrepend (const Slaw &s) const
{
  detail::SlawRefs cmps = Composite ()->Components ();
  if (!s.IsNull ())
    cmps.insert (cmps.begin (), s.SlawRef ());
  return Slaw (detail::CompositeSlaw::List (cmps));
}

Slaw Slaw::ListConcat (const Slaw &other) const
{
  const detail::SlawRefs &pre = Composite ()->Components ();
  const detail::SlawRefs &post = other.Composite ()->Components ();
  const detail::SRSize N (pre.size ());
  const detail::SRSize M (post.size ());
  detail::SlawRefs cmps;
  if (N + M > 0)
    {
      cmps.reserve (N + M);
      cmps = pre;
      for (detail::SRSize i = 0; i < M; ++i)
        cmps.push_back (post[i]);
    }
  return Slaw (detail::CompositeSlaw::List (cmps));
}

Slaw Slaw::ListRemoveFirst (const Slaw &s) const
{
  return ListRemoveFirst (s.SlawValue ());
}

Slaw Slaw::ListRemoveFirst (bslaw s) const
{
  detail::SlawRefs cmps = Composite ()->Components ();
  int idx (IndexOf (s));
  if (idx > -1)
    cmps.erase (cmps.begin () + idx);
  return Slaw (detail::CompositeSlaw::List (cmps));
}

Slaw Slaw::ListRemove (const Slaw &s) const
{
  return ListRemove (s.SlawValue ());
}

Slaw Slaw::ListRemove (bslaw s) const
{
  using namespace std;
  detail::SlawRefs cmps = Composite ()->Components ();
  detail::SlawIter it =
    remove_if (cmps.begin (), cmps.end (),
               bind2nd (mem_fun_ref (&detail::SlawRef::Equals), s));
  cmps.erase (it, cmps.end ());
  return Slaw (detail::CompositeSlaw::List (cmps));
}

Slaw Slaw::ListRemoveNth (int64 n) const
{
  if (n < 0)
    n += Count ();
  detail::SlawRefs cmps = Composite ()->Components ();
  if (n >= 0 && n < Count ())
    cmps.erase (cmps.begin () + n);
  return Slaw (detail::CompositeSlaw::List (cmps));
}

Slaw Slaw::ListReplace (const Slaw &from, const Slaw &to) const
{
  return ListReplace (from.SlawRef (), to);
}

Slaw Slaw::ListReplace (bslaw from, const Slaw &to) const
{
  using namespace std;
  detail::SlawRefs cmps = Composite ()->Components ();
  if (from && !to.IsNull ())
    replace_if (cmps.begin (), cmps.end (),
                bind2nd (mem_fun_ref (&detail::SlawRef::Equals), from),
                to.SlawRef ());
  return Slaw (detail::CompositeSlaw::List (cmps));
}

Slaw Slaw::ListReplaceFirst (const Slaw &from, const Slaw &to) const
{
  return ListReplaceFirst (from.SlawValue (), to);
}

Slaw Slaw::ListReplaceFirst (bslaw from, const Slaw &to) const
{
  int64 idx (IndexOf (from));
  detail::SlawRefs cmps = Composite ()->Components ();
  if (idx >= 0 && !to.IsNull ())
    cmps[idx] = to.SlawRef ();
  return Slaw (detail::CompositeSlaw::List (cmps));
}

Slaw Slaw::ListReplaceNth (int64 n, const Slaw &s) const
{
  if (n < 0)
    n += Count ();
  detail::SlawRefs cmps = Composite ()->Components ();
  if (n >= 0 && n < Count ())
    cmps[n] = s.SlawRef ();
  return Slaw (detail::CompositeSlaw::List (cmps));
}

int64 Slaw::IndexOf (bslaw s, unt64 start) const
{
  return (s ? Composite ()->IndexOf (s, start) : -1);
}

int64 Slaw::IndexOf (slaw s, unt64 start) const
{
  return (s ? Composite ()->IndexOf (s, start) : -1);
}

int64 Slaw::IndexOf (const Slaw &s, unt64 start) const
{
  return (s.IsNull () ? -1 : Composite ()->IndexOf (s.SlawRef (), start));
}

bool Slaw::ListContains (const Slaw &s, unt64 start) const
{
  return (IndexOf (s, start) > -1);
}

bool Slaw::ListContains (bslaw s, unt64 start) const
{
  return (IndexOf (s, start) > -1);
}

bool Slaw::ListContains (slaw s, unt64 start) const
{
  return (IndexOf (s, start) > -1);
}

int64 Slaw::IndexOfList (const Slaw &list, bool no_gap) const
{
  if (list.IsNull () || IsNull ())
    return -1;
  const int64 lc = list.Count ();
  const int64 count = Count ();
  if (lc > count)
    return -1;
  const Slaw &fst = list[0];
  int64 pos = IndexOf (fst);
  if (pos < 0)
    return pos;
  if (no_gap)
    {
      while (pos >= 0 && count - pos >= lc)
        {
          if (Slice (pos, pos + lc) == list)
            return pos;
          else
            pos = IndexOf (fst, pos + 1);
        }
      return -1;
    }
  else
    {
      for (int64 i = 1, p = pos; i < lc; ++i)
        if ((p = IndexOf (list[i], p + 1)) < 0)
          return -1;
      return pos;
    }
}

Slaw Slaw::Slice (int64 b, int64 e) const
{
  detail::CompositeSlaw::Ref slice =
    detail::CompositeSlaw::List (Composite ()->Slice (b, e));
  return Slaw (slice);
}

int64 Slaw::ListFind (bslaw key) const
{
  return slaw_list_find (SlawValue (), key);
}

int64 Slaw::ListFind (slaw key) const
{
  return ListFind (bslaw (key));
}

int64 Slaw::ListFind (const Slaw &key) const
{
  return ListFind (key.SlawValue ());
}

/* ---------------------------------------------------------------------- */
// Map interface

namespace {

const Slaw ENDM = A_NULL_SLAW;

}  // namespace

Slaw Slaw::MakeMap (const Slaw &k0 = ENDM, const Slaw &v0 = ENDM,
                    const Slaw &k1 = ENDM, const Slaw &v1 = ENDM,
                    const Slaw &k2 = ENDM, const Slaw &v2 = ENDM,
                    const Slaw &k3 = ENDM, const Slaw &v3 = ENDM,
                    const Slaw &k4 = ENDM, const Slaw &v4 = ENDM,
                    const Slaw &k5 = ENDM, const Slaw &v5 = ENDM,
                    const Slaw &k6 = ENDM, const Slaw &v6 = ENDM,
                    const Slaw &k7 = ENDM, const Slaw &v7 = ENDM,
                    const Slaw &k8 = ENDM, const Slaw &v8 = ENDM,
                    const Slaw &k9 = ENDM, const Slaw &v9 = ENDM)
{
  const unt8 SIZE (20);
  const Slaw *slawx[SIZE] = {&k0, &v0, &k1, &v1, &k2, &v2, &k3, &v3, &k4, &v4,
                             &k5, &v5, &k6, &v6, &k7, &v7, &k8, &v8, &k9, &v9};
  detail::SlawRefs refs;
  refs.reserve (SIZE);
  for (unt8 i = 0; i < (SIZE - 1) && slawx[i] != &ENDM; i += 2)
    if (!slawx[i]->IsNull () && !slawx[i + 1]->IsNull ())
      {
        refs.push_back (slawx[i]->SlawRef ());
        refs.push_back (slawx[i + 1]->SlawRef ());
      }

  return Slaw (detail::CompositeSlaw::Map (refs));
}

// clang-format off
Slaw Slaw::MakeMap (slaw k0 = NULL, slaw v0 = NULL,
                    slaw k1 = NULL, slaw v1 = NULL,
                    slaw k2 = NULL, slaw v2 = NULL,
                    slaw k3 = NULL, slaw v3 = NULL,
                    slaw k4 = NULL, slaw v4 = NULL,
                    slaw k5 = NULL, slaw v5 = NULL,
                    slaw k6 = NULL, slaw v6 = NULL,
                    slaw k7 = NULL, slaw v7 = NULL,
                    slaw k8 = NULL, slaw v8 = NULL,
                    slaw k9 = NULL, slaw v9 = NULL)
{
  // clang-format on
  const unt8 SIZE (20);
  slaw s[SIZE] = {k0, v0, k1, v1, k2, v2, k3, v3, k4, v4,
                  k5, v5, k6, v6, k7, v7, k8, v8, k9, v9};
  return MapCollect (s, s + SIZE, SIZE);
}

Slaw Slaw::Map ()
{
  static Slaw empty (detail::CompositeSlaw::Map (detail::SlawRefs ()));
  return empty;
}

Slaw Slaw::Map (const Slaw &k0, const Slaw &v0)
{
  return MakeMap (k0, v0);
}

Slaw Slaw::Map (const Slaw &k0, const Slaw &v0, const Slaw &k1, const Slaw &v1)
{
  return MakeMap (k0, v0, k1, v1);
}

Slaw Slaw::Map (const Slaw &k0, const Slaw &v0, const Slaw &k1, const Slaw &v1,
                const Slaw &k2, const Slaw &v2)
{
  return MakeMap (k0, v0, k1, v1, k2, v2);
}

Slaw Slaw::Map (const Slaw &k0, const Slaw &v0, const Slaw &k1, const Slaw &v1,
                const Slaw &k2, const Slaw &v2, const Slaw &k3, const Slaw &v3)
{
  return MakeMap (k0, v0, k1, v1, k2, v2, k3, v3);
}

Slaw Slaw::Map (const Slaw &k0, const Slaw &v0, const Slaw &k1, const Slaw &v1,
                const Slaw &k2, const Slaw &v2, const Slaw &k3, const Slaw &v3,
                const Slaw &k4, const Slaw &v4)
{
  return MakeMap (k0, v0, k1, v1, k2, v2, k3, v3, k4, v4);
}

Slaw Slaw::Map (const Slaw &k0, const Slaw &v0, const Slaw &k1, const Slaw &v1,
                const Slaw &k2, const Slaw &v2, const Slaw &k3, const Slaw &v3,
                const Slaw &k4, const Slaw &v4, const Slaw &k5, const Slaw &v5)
{
  return MakeMap (k0, v0, k1, v1, k2, v2, k3, v3, k4, v4, k5, v5);
}

Slaw Slaw::Map (const Slaw &k0, const Slaw &v0, const Slaw &k1, const Slaw &v1,
                const Slaw &k2, const Slaw &v2, const Slaw &k3, const Slaw &v3,
                const Slaw &k4, const Slaw &v4, const Slaw &k5, const Slaw &v5,
                const Slaw &k6, const Slaw &v6)
{
  return MakeMap (k0, v0, k1, v1, k2, v2, k3, v3, k4, v4, k5, v5, k6, v6);
}

Slaw Slaw::Map (const Slaw &k0, const Slaw &v0, const Slaw &k1, const Slaw &v1,
                const Slaw &k2, const Slaw &v2, const Slaw &k3, const Slaw &v3,
                const Slaw &k4, const Slaw &v4, const Slaw &k5, const Slaw &v5,
                const Slaw &k6, const Slaw &v6, const Slaw &k7, const Slaw &v7)
{
  return MakeMap (k0, v0, k1, v1, k2, v2, k3, v3, k4, v4, k5, v5, k6, v6, k7,
                  v7);
}

Slaw Slaw::Map (const Slaw &k0, const Slaw &v0, const Slaw &k1, const Slaw &v1,
                const Slaw &k2, const Slaw &v2, const Slaw &k3, const Slaw &v3,
                const Slaw &k4, const Slaw &v4, const Slaw &k5, const Slaw &v5,
                const Slaw &k6, const Slaw &v6, const Slaw &k7, const Slaw &v7,
                const Slaw &k8, const Slaw &v8)
{
  return MakeMap (k0, v0, k1, v1, k2, v2, k3, v3, k4, v4, k5, v5, k6, v6, k7,
                  v7, k8, v8);
}

Slaw Slaw::Map (const Slaw &k0, const Slaw &v0, const Slaw &k1, const Slaw &v1,
                const Slaw &k2, const Slaw &v2, const Slaw &k3, const Slaw &v3,
                const Slaw &k4, const Slaw &v4, const Slaw &k5, const Slaw &v5,
                const Slaw &k6, const Slaw &v6, const Slaw &k7, const Slaw &v7,
                const Slaw &k8, const Slaw &v8, const Slaw &k9, const Slaw &v9)
{
  return MakeMap (k0, v0, k1, v1, k2, v2, k3, v3, k4, v4, k5, v5, k6, v6, k7,
                  v7, k8, v8, k9, v9);
}

Slaw Slaw::Map (slaw k0, slaw v0)
{
  return MakeMap (k0, v0);
}

Slaw Slaw::Map (slaw k0, slaw v0, slaw k1, slaw v1)
{
  return MakeMap (k0, v0, k1, v1);
}

Slaw Slaw::Map (slaw k0, slaw v0, slaw k1, slaw v1, slaw k2, slaw v2)
{
  return MakeMap (k0, v0, k1, v1, k2, v2);
}

// clang-format off
Slaw Slaw::Map (slaw k0, slaw v0, slaw k1, slaw v1, slaw k2, slaw v2,
                slaw k3, slaw v3)
{
  return MakeMap (k0, v0, k1, v1, k2, v2, k3, v3);
}

Slaw Slaw::Map (slaw k0, slaw v0, slaw k1, slaw v1, slaw k2, slaw v2,
                slaw k3, slaw v3, slaw k4, slaw v4)
{
  return MakeMap (k0, v0, k1, v1, k2, v2, k3, v3, k4, v4);
}

Slaw Slaw::Map (slaw k0, slaw v0, slaw k1, slaw v1, slaw k2, slaw v2,
                slaw k3, slaw v3, slaw k4, slaw v4, slaw k5, slaw v5)
{
  return MakeMap (k0, v0, k1, v1, k2, v2, k3, v3, k4, v4, k5, v5);
}

Slaw Slaw::Map (slaw k0, slaw v0, slaw k1, slaw v1, slaw k2, slaw v2,
                slaw k3, slaw v3, slaw k4, slaw v4, slaw k5, slaw v5,
                slaw k6, slaw v6)
{
  return MakeMap (k0, v0, k1, v1, k2, v2, k3, v3, k4, v4, k5, v5, k6, v6);
}

Slaw Slaw::Map (slaw k0, slaw v0, slaw k1, slaw v1, slaw k2, slaw v2,
                slaw k3, slaw v3, slaw k4, slaw v4, slaw k5, slaw v5,
                slaw k6, slaw v6, slaw k7, slaw v7)
{
  return MakeMap (k0, v0, k1, v1, k2, v2, k3, v3, k4, v4, k5, v5, k6, v6, k7,
                  v7);
}

Slaw Slaw::Map (slaw k0, slaw v0, slaw k1, slaw v1, slaw k2, slaw v2,
                slaw k3, slaw v3, slaw k4, slaw v4, slaw k5, slaw v5,
                slaw k6, slaw v6, slaw k7, slaw v7, slaw k8, slaw v8)
{
  return MakeMap (k0, v0, k1, v1, k2, v2, k3, v3, k4, v4, k5, v5, k6, v6, k7,
                  v7, k8, v8);
}

Slaw Slaw::Map (slaw k0, slaw v0, slaw k1, slaw v1, slaw k2, slaw v2,
                slaw k3, slaw v3, slaw k4, slaw v4, slaw k5, slaw v5,
                slaw k6, slaw v6, slaw k7, slaw v7, slaw k8, slaw v8,
                slaw k9, slaw v9)
{
  return MakeMap (k0, v0, k1, v1, k2, v2, k3, v3, k4, v4, k5, v5, k6, v6, k7,
                  v7, k8, v8, k9, v9);
}
// clang-format on

Slaw Slaw::MapKVs (bool values) const
{
  const detail::CompositeSlaw::Ref c =
    IsMap () ? Composite ()
             : detail::CompositeSlaw::Map (Composite ()->KVList ());

  const detail::SlawRefs &kvs (c->KVList ());
  detail::SlawRefs ks;
  const detail::SRSize N (kvs.size ());
  if (N > 0)
    ks.reserve (N / 2);
  for (detail::SRSize i (values ? 1 : 0); i < N; i += 2)
    ks.push_back (kvs[i]);
  return Slaw (detail::CompositeSlaw::List (ks));
}

Slaw Slaw::MapKeys () const
{
  return MapKVs (false);
}

Slaw Slaw::MapValues () const
{
  return MapKVs (true);
}

Slaw Slaw::MapPut (detail::SlawRef key, detail::SlawRef value) const
{
  return Slaw (detail::CompositeSlaw::ExtendMap (Composite (), key, value));
}

Slaw Slaw::MapPut (const Slaw &cons) const
{
  return MapPut (cons.Car (), cons.Cdr ());
}

Slaw Slaw::MapPut (const Slaw &key, const Slaw &value) const
{
  return MapPut (key.SlawRef (), value.SlawRef ());
}

Slaw Slaw::MapPut (slaw key, slaw value) const
{
  return MapPut (detail::SlawRef (key), detail::SlawRef (value));
}

Slaw Slaw::MapMerge (const Slaw &other) const
{
  return Slaw (detail::CompositeSlaw::Map (Composite (), other.Composite ()));
}

Slaw Slaw::MapRemove (bslaw key) const
{
  using namespace std;

  detail::CompositeSlaw::Ref c;
  if (IsMap ())
    c = Composite ();
  else
    c = detail::CompositeSlaw::Map (Composite ()->Components ());

  detail::SlawRefs cmps (c->KVList ());
  for (detail::SRSize i = 0; i < cmps.size ();)
    if (cmps[i].Equals (key))
      cmps.erase (cmps.begin () + i, cmps.begin () + i + 2);
    else
      i += 2;

  return Slaw (detail::CompositeSlaw::Map (cmps));
}

Slaw Slaw::MapRemove (slaw key) const
{
  return MapRemove (bslaw (key));
}

Slaw Slaw::MapRemove (const Slaw &key) const
{
  return MapRemove (bslaw (key.SlawRef ()));
}

Slaw Slaw::Find (const Slaw &key) const
{
  return Find (key.SlawValue ());
}

Slaw Slaw::Find (slaw key) const
{
  return Find (bslaw (key));
}

Slaw Slaw::Find (bslaw key) const
{
  detail::SlawRef value (Composite ()->Find (key));
  if (value.IsNull ())
    return Slaw ();
  return Slaw (value);
}

Slaw Slaw::MapFind (bslaw key) const
{
  bslaw val = slaw_map_find (SlawRef (), key);
  return val ? Slaw (detail::SlawRef (val, SlawRef ())) : Slaw ();
}

Slaw Slaw::MapFind (slaw key) const
{
  return MapFind (bslaw (key));
}

Slaw Slaw::MapFind (const Slaw &key) const
{
  return MapFind (key.SlawValue ());
}

/* ---------------------------------------------------------------------- */
// Protein interface

Slaw Slaw::Protein ()
{
  static Slaw protein (protein_from (NULL, NULL));
  return protein;
}

Slaw Slaw::Protein (const Slaw &descrips, const Slaw &ingests)
{
  detail::CompositeSlaw::Ref ds = descrips.Composite ();
  detail::CompositeSlaw::Ref is = ingests.Composite ();
  detail::CompositeSlaw::Ref p = detail::CompositeSlaw::Protein (ds, is);
  return Slaw (p);
}

Slaw Slaw::Descrips () const
{
  return Slaw (Composite ()->Descrips ());
}

Slaw Slaw::Ingests () const
{
  return Slaw (Composite ()->Ingests ());
}

::std::ostream &operator<< (::std::ostream &os, const oblong::plasma::Slaw &s)
{
  s.Spew (os);
  return os;
}
}
}  // namespace oblong::plasma


// define the vector ostreaming operators declared in slaw-traits.h

#define DEFINE_VOSTREAM(T)                                                     \
  ::std::ostream &operator<< (::std::ostream &os, const v2##T &v)              \
  {                                                                            \
    return os << "v2" << #T << " <" << v.x << ", " << v.y << ">";              \
  }                                                                            \
                                                                               \
  ::std::ostream &operator<< (::std::ostream &os, const v3##T &v)              \
  {                                                                            \
    return os << "v3" << #T << " <" << v.x << ", " << v.y << ", " << v.z       \
              << ">";                                                          \
  }                                                                            \
                                                                               \
  ::std::ostream &operator<< (::std::ostream &os, const v4##T &v)              \
  {                                                                            \
    return os << "v4" << #T << " <" << v.x << ", " << v.y << ", " << v.z       \
              << ", " << v.w << ">";                                           \
  }

OB_FOR_ALL_NUMERIC_TYPES (DEFINE_VOSTREAM);

// In a lot of libraries, we have a separate file for the retort strings,
// but I think I'll just keep them here for now, since everything depends
// on Slaw.C anyway.

static const char *plasmaxx_error_string (ob_retort err)
{
#define E(x)                                                                   \
  case x:                                                                      \
    return #x
  switch (err)
    {
      E (OB_HOSE_NOT_CONFIGURED);
      default:
        return NULL;
    }
#undef E
}

static ob_retort dummy_plasmaxx OB_UNUSED =
  ob_add_error_names (plasmaxx_error_string);

PLASMA_RESTORE_WARNINGS
