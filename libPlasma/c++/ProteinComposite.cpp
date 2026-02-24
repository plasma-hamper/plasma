
/* (c)  oblong industries */

#include "ProteinComposite.h"

#include "SlawList.h"
#include "SlawMap.h"
#include "PlasmaStreams.h"

#include <libPlasma/c/protein.h>

#include <algorithm>
#include <ostream>


namespace oblong {
namespace plasma {
namespace detail {

namespace proteincomposite {

CompositeSlaw::Ref ExtractDescrips (SlawRef s)
{
  if (s.IsNull () || s.IsMap ())
    return CompositeSlaw::Ref ();
  SlawRef descrips (protein_descrips (s), s);
  if (descrips.IsNull ())
    return CompositeSlaw::Ref ();
  return new SlawList (descrips);
}

CompositeSlaw::Ref ExtractIngests (SlawRef s)
{
  if (s.IsNull ())
    return CompositeSlaw::Ref ();
  if (s.IsMap ())
    return new SlawMap (s);
  if (!s.IsProtein ())
    return CompositeSlaw::Ref ();
  return CompositeSlaw::Make (SlawRef (protein_ingests (s), s));
}

}  // namespace

ProteinComposite::ProteinComposite (SlawRef protein)
    : descrips_ (proteincomposite::ExtractDescrips (protein)),
      ingests_ (proteincomposite::ExtractIngests (protein))
{
}

ProteinComposite::ProteinComposite (Ref d, Ref i) : descrips_ (d), ingests_ (i)
{
}

bool ProteinComposite::IsNull () const
{
  return descrips_->IsNull () && ingests_->IsNull ();
}

bool ProteinComposite::IsProtein () const
{
  return true;
}

bool ProteinComposite::IsEqual (const CompositeSlaw *other) const
{
  if (other->IsProtein ())
    {
      if (const ProteinComposite *p =
            dynamic_cast<const ProteinComposite *> (other))
        {
          const CompositeSlaw *pd = p->descrips_;
          const CompositeSlaw *pi = p->ingests_;
          return descrips_->Equals (pd) && ingests_->Equals (pi);
        }
    }
  return descrips_->Equals (other) && ingests_->Equals (other);
}

SlawRef ProteinComposite::AsSlaw () const
{
  return SlawRef (protein_from (descrips_->AsSlaw (), ingests_->AsSlaw ()));
}

SlawRef ProteinComposite::Find (bslaw key) const
{
  return ingests_->Find (key);
}

int64 ProteinComposite::Count () const
{
  return descrips_->Count ();
}

SlawRef ProteinComposite::Nth (int64 n) const
{
  return descrips_->Nth (n);
}

SlawRefs ProteinComposite::Slice (int64 from, int64 to) const
{
  return descrips_->Slice (from, to);
}

int64 ProteinComposite::IndexOf (bslaw s, unt64 start) const
{
  return descrips_->IndexOf (s, start);
}

SlawRefs ProteinComposite::KVList () const
{
  return ingests_->KVList ();
}

unt64 ProteinComposite::KeyCount () const
{
  return ingests_->KeyCount ();
}

SlawRefs ProteinComposite::IngestList () const
{
  return ingests_->Components ();
}

SlawRefs ProteinComposite::DescripList () const
{
  return descrips_->Components ();
}

CompositeSlaw::Ref ProteinComposite::Descrips () const
{
  return descrips_;
}

CompositeSlaw::Ref ProteinComposite::Ingests () const
{
  return ingests_;
}

Str ProteinComposite::ToStr () const
{
  return "{" + descrips_->ToStr () + ", " + ingests_->ToStr () + "}";
}

void ProteinComposite::Spew (OStreamReference os) const
{
  os.os << "#PROTEIN<" << ::std::endl
        << " -> DESCRIPS: " << descrips_ << ::std::endl
        << " -> INGESTS: " << ingests_ << ::std::endl
        << ">";
}
}
}
}  // namespace oblong::plasma::detail
