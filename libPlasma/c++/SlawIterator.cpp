
/* (c)  oblong industries */

#include "SlawIterator.h"
#include "libPlasma/c++/PlasmaStreams.h"

#include <ostream>


namespace oblong {
namespace plasma {


void SlawIterator::SetIndex (unt64 idx)
{
  idx_ = (::std::min) (idx, unt64 (slaw_.Count ()));
}

SlawIterator::SlawIterator () : idx_ (0) {}

SlawIterator::SlawIterator (const Slaw &s) : slaw_ (s)
{
  SetIndex (0);
}

SlawIterator::SlawIterator (const SlawIterator &other)
    : slaw_ (other.slaw_), idx_ (other.idx_)
{
}

SlawIterator::SlawIterator (const Slaw &s, unt64 idx) : slaw_ (s)
{
  SetIndex (idx);
}

SlawIterator::reference SlawIterator::operator* () const
{
  return slaw_.Nth (idx_);
}

SlawIterator::pointer SlawIterator::operator->() const
{
  return pointer (slaw_.Nth (idx_));
}

SlawIterator &SlawIterator::operator++ ()
{
  SetIndex (idx_ + 1);
  return *this;
}

SlawIterator SlawIterator::operator++ (int)
{
  SetIndex (idx_ + 1);
  return SlawIterator (slaw_, idx_);
}

SlawIterator &SlawIterator::operator-- ()
{
  SetIndex (idx_ - 1);
  return *this;
}

SlawIterator SlawIterator::operator-- (int)
{
  SetIndex (idx_ - 1);
  return SlawIterator (slaw_, idx_);
}

SlawIterator SlawIterator::operator+ (difference_type n) const
{
  return SlawIterator (slaw_, idx_ + n);
}

SlawIterator &SlawIterator::operator+= (difference_type n)
{
  SetIndex (idx_ + n);
  return *this;
}

SlawIterator SlawIterator::operator- (difference_type n) const
{
  return SlawIterator (slaw_, idx_ - n);
}

SlawIterator &SlawIterator::operator-= (difference_type n)
{
  SetIndex (idx_ - n);
  return *this;
}

SlawIterator::reference SlawIterator::operator[] (difference_type n) const
{
  return slaw_.Nth (idx_ + n);
}

bool SlawIterator::operator== (const SlawIterator &o) const
{
  return (IsEnd () && o.IsEnd ()) || (idx_ == o.idx_ && slaw_ == o.slaw_);
}

bool SlawIterator::operator!= (const SlawIterator &o) const
{
  return !(*this == o);
}

bool SlawIterator::operator< (const SlawIterator &o) const
{
  return idx_ < o.idx_;
}

bool SlawIterator::operator<= (const SlawIterator &o) const
{
  return idx_ <= o.idx_;
}

bool SlawIterator::operator> (const SlawIterator &o) const
{
  return idx_ > o.idx_;
}

bool SlawIterator::operator>= (const SlawIterator &o) const
{
  return idx_ >= o.idx_;
}

bool SlawIterator::IsEnd () const
{
  return idx_ == unt64 (slaw_.Count ());
}

SlawIterator::difference_type
SlawIterator::operator- (const SlawIterator &other) const
{
  unt64 end = slaw_.IsNull () ? unt64 (other.slaw_.Count ()) : idx_;
  return SlawIterator::difference_type (end - other.idx_);
}

void SlawIterator::Spew (::std::ostream &os) const
{
  os << "<SlawIterator (" << idx_ << ")>";
}

::std::ostream &operator<< (::std::ostream &os,
                            const oblong::plasma::SlawIterator &it)
{
  it.Spew (os);
  return os;
}
}
}  // namespace oblong::plasma
