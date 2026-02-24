
/* (c)  oblong industries */

#include "SerialWorker.h"

#include <sstream>

namespace oblong {
namespace plasma {
namespace bench {

SerialWorker::~SerialWorker ()
{
  for (WSize i = 0, n = workers_.size (); i < n; ++i)
    delete workers_[i].second;
}

SerialWorker::SerialWorker (const Config &cfg) : Worker (cfg) {}

void SerialWorker::Add (Worker *w)
{
  workers_.push_back (::std::make_pair (true, w));
}

void SerialWorker::Add (const Reader::Config &c)
{
  workers_.push_back (::std::make_pair (!c.pool.empty (), new Reader (c)));
}

void SerialWorker::Add (const Writer::Config &c)
{
  workers_.push_back (::std::make_pair (!c.pool.empty (), new Writer (c)));
}

ob_retort SerialWorker::DoRun (pool_hose h, TimedBytes &tb)
{
  ob_retort r = OB_OK;
  for (unt64 b = 0; b < batches_; ++b)
    for (WSize i = 0, n = workers_.size (); i < n; ++i)
      for (unt64 j = 0; j < bsize_; ++j)
        {
          ob_retort rr = workers_[i].first ? workers_[i].second->Run ()
                                           : workers_[i].second->DoRun (h, tb);
          if (OB_OK == r)
            r = rr;
        }
  return r;
}

::std::string SerialWorker::Description () const
{
  ::std::ostringstream desc;
  desc << "Serial worker (" << batches_ << (batches_ > 1 ? " rounds" : " round")
       << " with " << bsize_ << (bsize_ > 1 ? " runs" : " run")
       << " per worker):";
  for (WSize i = 0, n = workers_.size (); i < n; ++i)
    desc << "\n#  - " << workers_[i].second->Description ();
  return desc.str ();
}
}
}
}  // namespace oblong::plasma::bench
