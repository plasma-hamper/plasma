
/* (c)  oblong industries */

#include "CompositeSlaw.h"

#include "AtomicComposite.h"
#include "SlawCons.h"
#include "SlawList.h"
#include "SlawMap.h"
#include "ProteinComposite.h"
#include "Slaw.h"
#include "libPlasma/c++/PlasmaStreams.h"

#include <ostream>

namespace oblong {
namespace plasma {
namespace detail {

namespace {

class NullCompositeSlaw : public CompositeSlaw
{
 public:
  bool IsNull () const override { return true; }

  SlawRef Find (bslaw) const override { return SlawRef (); }
  SlawRef AsSlaw () const override { return SlawRef (); }
  SlawRefs KVList () const override { return SlawRefs (); }
  unt64 KeyCount () const override { return 0; }
  int64 Count () const override { return 0; }
  SlawRef Nth (int64) const override { return SlawRef (); }
  int64 IndexOf (bslaw, unt64) const override { return -1; }
  SlawRefs Slice (int64, int64) const override { return SlawRefs (); }
  Str ToStr () const override { return Str ("#NULL"); }
  void Spew (OStreamReference os) const override { os.os << "#NULL"; }

  Ref Descrips () const override { return Ref (); }
  Ref Ingests () const override { return Ref (); }
  SlawRefs DescripList () const override { return SlawRefs (); }
  SlawRefs IngestList () const override { return SlawRefs (); }

  static NullCompositeSlaw *Instance ()
  {
    static NullCompositeSlaw instance;
    return &instance;
  }

 private:
  NullCompositeSlaw () {}
  bool IsEqual (const CompositeSlaw *o) const override
  {
    return Instance () == o;
  }
};

}  // namespace

/* ---------------------------------------------------------------------- */
// Ref traits

CompositeSlaw *CompositeSlaw::RefTraits::Default ()
{
  return NullCompositeSlaw::Instance ();
}

void CompositeSlaw::RefTraits::Destroy (CompositeSlaw *s)
{
  if (s != Default ())
    delete s;
}

/* ---------------------------------------------------------------------- */
// Construction

CompositeSlaw::~CompositeSlaw () {}


/* ---------------------------------------------------------------------- */
// Structure

bool CompositeSlaw::Equals (const CompositeSlaw *other) const
{
  if (this == other)
    return true;
  if (!other)
    return false;
  return IsEqual (other);
}

SlawRefs CompositeSlaw::Components () const
{
  return Slice (0, Count ());
}

/* ---------------------------------------------------------------------- */
// Protein interface

CompositeSlaw::Ref CompositeSlaw::Descrips () const
{
  Ref result (new SlawList (DescripList ()));
  return result;
}

CompositeSlaw::Ref CompositeSlaw::Ingests () const
{
  Ref result (new SlawMap (IngestList ()));
  return result;
}

/* ---------------------------------------------------------------------- */
// Factory functions

CompositeSlaw::Ref CompositeSlaw::Make (SlawRef s)
{
  if (s.IsNull ())
    return Ref ();
  if (s.IsAtomic ())
    return new AtomicComposite (s);
  if (s.IsCons ())
    return new SlawCons (s);
  if (s.IsMap ())
    return new SlawMap (s);
  if (s.IsProtein ())
    return new ProteinComposite (s);
  return new SlawList (s);
}

CompositeSlaw::Ref CompositeSlaw::Cons (SlawRef car, SlawRef cdr)
{
  return Ref (new SlawCons (car, cdr));
}

CompositeSlaw::Ref CompositeSlaw::List (const SlawRefs &cs)
{
  return Ref (new SlawList (cs));
}

CompositeSlaw::Ref CompositeSlaw::Map (const SlawRefs &cs)
{
  return Ref (new SlawMap (cs));
}

CompositeSlaw::Ref CompositeSlaw::Map (Ref b, Ref o)
{
  return Ref (new SlawMap (b, o));
}

CompositeSlaw::Ref CompositeSlaw::ExtendMap (Ref b, SlawRef k, SlawRef v)
{
  return Ref (new SlawMap (b, k, v));
}

CompositeSlaw::Ref CompositeSlaw::Protein (Ref d, Ref i)
{
  return Ref (new ProteinComposite (d, i));
}


// Utilites
Str RefToStr (const SlawRef &r)
{
  return (r.IsAtomic () ? r.ToStr () : CompositeSlaw::Make (r)->ToStr ());
}

::std::ostream &operator<< (::std::ostream &os,
                            const oblong::plasma::detail::CompositeSlaw &cs)
{
  cs.Spew (os);
  return os;
}
}
}
}  // namespace oblong::plasma::detail
