
/* (c)  oblong industries */

#include "SlawCons.h"
#include "PlasmaStreams.h"

#include <cassert>
#include <ostream>


namespace oblong {
namespace plasma {
namespace detail {

namespace {

SlawRefs MakeCons (SlawRef car, SlawRef cdr)
{
  SlawRefs result;
  result.push_back (car.IsNull () ? SlawRef::NilRef () : car);
  result.push_back (cdr.IsNull () ? SlawRef::NilRef () : cdr);
  return result;
}

SlawRefs ExtractConsComponents (SlawRef cons)
{
  SlawRefs result (MakeCons (SlawRef (slaw_cons_emit_car (cons), cons),
                             SlawRef (slaw_cons_emit_cdr (cons), cons)));
  return result;
}

SlawRefs ExtractListComponents (SlawRef list)
{
  SlawRefs result (MakeCons (SlawRef (slaw_list_emit_nth (list, 0), list),
                             SlawRef (slaw_list_emit_nth (list, 1), list)));
  return result;
}

SlawRefs ExtractComponents (SlawRef s)
{
  if (s.IsList ())
    return ExtractListComponents (s);
  if (s.IsCons ())
    return ExtractConsComponents (s);
  return MakeCons (s, SlawRef::NilRef ());
}

}  // namespace

/* ---------------------------------------------------------------------- */
// Construction

SlawCons::SlawCons (SlawRef cons) : elements_ (ExtractComponents (cons)) {}

SlawCons::SlawCons (SlawRef car, SlawRef cdr) : elements_ (MakeCons (car, cdr))
{
}

/* ---------------------------------------------------------------------- */
// Typing

bool SlawCons::IsNull () const
{
  return elements_.IsNull ();
}

bool SlawCons::IsCons () const
{
  return true;
}

/* ---------------------------------------------------------------------- */
// Structure

SlawRef SlawCons::Car () const
{
  return elements_.Nth (0);
}

SlawRef SlawCons::Cdr () const
{
  return elements_.Nth (1);
}

SlawRef SlawCons::AsSlaw () const
{
  return SlawRef (slaw_cons (Car (), Cdr ()));
}

bool SlawCons::IsEqual (const CompositeSlaw *other) const
{
  return 2 == other->Count () && Car () == other->Nth (0)
         && Cdr () == other->Nth (1);
}

SlawRef SlawCons::Nth (int64 idx) const
{
  return elements_.Nth (idx);
}

SlawRefs SlawCons::Slice (int64 x, int64 y) const
{
  return elements_.Slice (x, y);
}

SlawRef SlawCons::Find (bslaw key) const
{
  return slawx_equal (Car (), key) ? Cdr () : SlawRef ();
}

int64 SlawCons::IndexOf (bslaw s, unt64 start) const
{
  return elements_.IndexOf (s, start);
}

SlawRefs SlawCons::KVList () const
{
  SlawRefs result;
  result.push_back (Car ());
  result.push_back (Cdr ());
  return result;
}

int64 SlawCons::Count () const
{
  return 2;
}

unt64 SlawCons::KeyCount () const
{
  return 1;
}

SlawRefs SlawCons::IngestList () const
{
  return KVList ();
}

SlawRefs SlawCons::DescripList () const
{
  return SlawRefs ();
}

Str SlawCons::ToStr () const
{
  return "(" + RefToStr (Car ()) + ", " + RefToStr (Cdr ()) + ")";
}

void SlawCons::Spew (OStreamReference os) const
{
  os.os << "#CONS<" << ::std::endl
        << Car () << ::std::endl
        << Cdr () << ::std::endl
        << ">";
}
}
}
}  // namespace oblong::plasma::detail
