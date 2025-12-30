
/* (c)  oblong industries */

#include "PingPonger.h"

#include <sstream>

#include "libPlasma/c/slaw.h"
#include "libPlasma/c/protein.h"


namespace oblong {
namespace plasma {
namespace bench {


PingPonger::Config::Config () : timeout (0), ping (false) {}


PingPonger::Config::Config (const Config &other)
    : Worker::Config (other), timeout (other.timeout), ping (other.ping)
{
  for (unt64 i = 0; i < other.proteins.size (); ++i)
    proteins.push_back (protein_dup (other.proteins[i]));
}

PingPonger::~PingPonger ()
{
  FreeProteins (proteins_);
}

PingPonger::PingPonger (const Config &cfg)
    : LoneWorker (cfg),
      proteins_ (cfg.proteins),
      timeout_ (cfg.timeout),
      ping_ (cfg.ping),
      psizes_ (ProteinSizes (proteins_))
{
}

ob_retort PingPonger::DoRun (pool_hose h, TimedBytes &tb)
{
  ClearErrors ();
  for (unt64 b = 0; b < batches_; ++b)
    {
      unt64 ec = 0;
      unt64 bs = 0;
      tb.LapseStart ();
      if (ping_)
        for (unt64 i = 0; i < bsize_; ++i)
          {
            bs += Write (h, b, i, ec);
            bs += Read (h, b, i, ec);
          }
      else
        for (unt64 i = 0; i < bsize_; ++i)
          {
            bs += Read (h, b, i, ec);
            bs += Write (h, b, i, ec);
          }
      tb.LapseEndWith (::std::make_pair (int64 (2 * bsize_ - ec), bs));
    }
  ReportErrors (ping_ ? "Ping" : "Pong");
  return OB_OK;
}

::std::string PingPonger::Description () const
{
  ::std::ostringstream os;
  os << (ping_ ? "Ping: " : "Pong: ") << batches_
     << (batches_ > 1 ? " batches" : " batch") << " of " << bsize_
     << (bsize_ > 1 ? " proteins" : " protein")
     << (proteins_.size () > 1 ? " (sizes " : " (size ")
     << slaw_len (proteins_[0]);
  for (unt64 i = 1, n = proteins_.size (); i < n; ++i)
    os << ", " << psizes_[i];
  os << ")";
  return os.str ();
}

unt64 PingPonger::Read (pool_hose h, unt64 b, unt64 i, unt64 &ec)
{
  protein p;
  ob_retort r = pool_await_next (h, timeout_, &p, NULL, NULL);
  if (OB_OK == r)
    {
      unt64 l = slaw_len (p);
      protein_free (p);
      return l;
    }
  AddError (b, i, r);
  ++ec;
  return 0;
}

unt64 PingPonger::Write (pool_hose h, unt64 b, unt64 i, unt64 &ec)
{
  static const unt64 N = proteins_.size ();
  unt64 n = N > 1 ? (b * bsize_ + i) % N : 0;
  ob_retort r = pool_deposit (h, proteins_[n], NULL);
  pool_seekby (h, 1);
  if (OB_OK == r)
    return psizes_[n];
  AddError (b, i, r);
  ++ec;
  return 0;
}
}
}
}  // namespace oblong::plasma::bench
