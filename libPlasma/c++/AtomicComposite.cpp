
/* (c)  oblong industries */

#include "AtomicComposite.h"
#include "PlasmaStreams.h"


namespace oblong {
namespace plasma {
namespace detail {


AtomicComposite::AtomicComposite (SlawRef s) : slaw_ (s) {}

bool AtomicComposite::IsNull () const
{
  return slaw_.IsNull ();
}

bool AtomicComposite::IsAtomic () const
{
  return true;
}

SlawRef AtomicComposite::AsSlaw () const
{
  return slaw_;
}

int64 AtomicComposite::Count () const
{
  return 1;
}

Str AtomicComposite::ToStr () const
{
  return slaw_.ToStr ();
}

SlawRef AtomicComposite::Nth (int64 idx) const
{
  return (idx == 0) ? slaw_ : SlawRef ();
}

int64 AtomicComposite::IndexOf (bslaw s, unt64 start) const
{
  return (start == 0) && slaw_.Equals (s) ? 0 : -1;
}

SlawRefs AtomicComposite::Slice (int64 begin, int64 end) const
{
  SlawRefs result;
  if ((begin < end && begin == 0) || (begin < 0 && end == 1))
    result.push_back (slaw_);
  return result;
}

SlawRefs AtomicComposite::KVList () const
{
  SlawRefs result;
  result.push_back (slaw_);
  result.push_back (SlawRef::NilRef ());
  return result;
}

unt64 AtomicComposite::KeyCount () const
{
  return 1;
}

SlawRef AtomicComposite::Find (OB_UNUSED bslaw key) const
{
  return SlawRef ();
}

bool AtomicComposite::IsEqual (const CompositeSlaw *other) const
{
  return (1 == other->Count () && slaw_ == other->Nth (0));
}

SlawRefs AtomicComposite::IngestList () const
{
  return SlawRefs ();
}

SlawRefs AtomicComposite::DescripList () const
{
  return SlawRefs (1, slaw_);
}

void AtomicComposite::Spew (OStreamReference os) const
{
  slaw_.Spew (os);
}
}
}
}  // namespace oblong::plasma::detail
