
/* (c)  oblong industries */

#include "LoneWorker.h"

namespace oblong {
namespace plasma {
namespace bench {

LoneWorker::LoneWorker (const Config &c) : Worker (c) {}

void LoneWorker::AddError (unt64 b, unt64 i, ob_retort r)
{
  errs_.push_back (::std::make_pair (b * bsize_ + i, r));
}

void LoneWorker::ClearErrors ()
{
  errs_.clear ();
}

void LoneWorker::ReportErrors (const char *msg)
{
  if (!errs_.empty ())
    {
      const unt64 N (errs_.size ());
      ::std::cerr << "# " << msg << ": " << N << " errors found:";
      for (unt64 i = 0; i < N; ++i)
        {
          unt64 l (errs_[i].first);
          ob_retort e (errs_[i].second);
          unt64 j (i);
          while (j < N - 1 && e == errs_[j + 1].second
                 && ++l == errs_[j + 1].first)
            j++;
          ::std::cerr << "\n## " << ob_error_string (e) << " (";
          if (j > i)
            {
              ::std::cerr << j - i + 1 << " times, " << errs_[i].first << " - "
                          << errs_[j].first;
              i = j;
            }
          else
            ::std::cerr << errs_[i].first;
          ::std::cerr << ")";
        }
      ::std::cerr << ::std::endl;
    }
}
}
}
}  // namespace oblong::plasma::bench
