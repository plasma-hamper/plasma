
/* (c)  oblong industries */

#include "Protein.h"
#include "libPlasma/c++/PlasmaStreams.h"

#include <ostream>


namespace oblong {
namespace plasma {


const pool_timestamp Protein::NO_TIME (-1);
const int64 Protein::NO_INDEX (-1);


Protein::~Protein () {}

Protein::Protein (const Protein &o)
    : protein_ (o.protein_),
      stamp_ (o.stamp_),
      index_ (o.index_),
      hose_ (o.hose_)
{
}

Protein &Protein::operator= (const Protein &o)
{
  if (this != &o)
    {
      protein_ = o.protein_;
      stamp_ = o.stamp_;
      index_ = o.index_;
      hose_ = o.hose_;
    }
  return *this;
}

Protein::Protein ()
    : protein_ (Slaw::Protein ()),
      stamp_ (NO_TIME),
      index_ (NO_INDEX),
      hose_ (NULL)
{
}

Protein::Protein (slaw p)
    : protein_ (p), stamp_ (NO_TIME), index_ (NO_INDEX), hose_ (NULL)
{
  if (!protein_.IsProtein ())
    protein_ = Slaw::Protein (protein_.Descrips (), protein_.Ingests ());
}

Protein::Protein (Slaw p)
    : protein_ (Slaw::Protein (p.Descrips (), p.Ingests ())),
      stamp_ (NO_TIME),
      index_ (NO_INDEX),
      hose_ (NULL)
{
}

Protein::Protein (Slaw descrips, Slaw ingests)
    : protein_ (Slaw::Protein (descrips, ingests)),
      stamp_ (NO_TIME),
      index_ (NO_INDEX),
      hose_ (NULL)
{
}

Protein::Protein (Slaw p, pool_timestamp t, int64 i, pool_hose h)
    : protein_ (p), stamp_ (t), index_ (i), hose_ (h)
{
}

bool Protein::IsNull () const
{
  return protein_.IsNull ();
}

Protein Protein::Null ()
{
  static Protein null (protein (NULL));
  return null;
}

bool Protein::IsEmpty () const
{
  return (Descrips ().IsNull () && Ingests ().IsNull ());
}

pool_timestamp Protein::Timestamp () const
{
  return stamp_;
}

int64 Protein::Index () const
{
  return index_;
}

Str Protein::Origin () const
{
  // return Pool (detail::PoolImplFactory::Find (hose_));
  if (hose_)
    return pool_name (hose_);
  return Str ();
}

bool Protein::ComesFrom (pool_hose h) const
{
  return (hose_ && hose_ == h);
}

bool Protein::operator== (const Protein &other) const
{
  return (protein_ == other.protein_);
}

bool Protein::operator!= (const Protein &other) const
{
  return !(*this == other);
}

Slaw Protein::Descrips () const
{
  return (IsNull () ? Slaw () : protein_.Descrips ());
}

Slaw Protein::Ingests () const
{
  return (IsNull () ? Slaw () : protein_.Ingests ());
}

bprotein Protein::ProteinValue () const
{
  return protein_.SlawValue ();
}

Slaw Protein::ToSlaw () const
{
  return protein_;
}

int64 Protein::Search (Slaw needle, Protein_Search_Type how) const
{
  return Search (needle.SlawValue (), how);
}

int64 Protein::Search (bslaw needle, Protein_Search_Type how) const
{
  return protein_search_ex (ProteinValue (), needle, how);
}

bool Protein::Matches (Slaw needle, Protein_Search_Type how) const
{
  return Search (needle, how) > -1;
}

bool Protein::Matches (bslaw needle, Protein_Search_Type how) const
{
  return Search (needle, how) > -1;
}

void Protein::Spew (OStreamReference os) const
{
  os.os << "<From Pool: " << hose_ << ", at index " << index_ << " and time "
        << stamp_ << ":\n";
  protein_.Spew (os);
  os.os << "\n>\n";
}

::std::ostream &operator<< (::std::ostream &os,
                            const oblong::plasma::Protein &p)
{
  p.Spew (os);
  return os;
}
}
}  // namespace oblong::plasma
