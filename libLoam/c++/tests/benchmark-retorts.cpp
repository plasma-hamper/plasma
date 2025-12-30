
/* (c)  oblong industries */

#include <stdlib.h>
#include <libLoam/c/ob-retorts.h>
#include <libLoam/c++/FatherTime.h>
#include <libLoam/c++/Str.h>
#include <libLoam/c++/ObRetort.h>

#include <iostream>


using namespace oblong::loam;
using namespace std;


#define FLAT_ITERATIONS 500000000 /* 500 million */
#define NESTY_ITERATIONS 5000000  /* 5 million */
#define NESTY_DEPTH 200


/* these are in a separate compilation unit, to prevent inlining */
ob_retort return_ob_retort (void);
ObRetort return_ObRetort (void);

ob_retort nesty_ob_retort (void *, int64);
ObRetort nesty_ObRetort (void *, int64);


#define BENCH_BEGIN(str, iterations)                                           \
  cout << "Testing " str ": ";                                                 \
  start = FatherTime::AbsoluteTime ();                                         \
  for (unt64 i = 0; i < (iterations); i++)                                     \
    {
#define BENCH_END(iterations)                                                  \
  }                                                                            \
  end = FatherTime::AbsoluteTime ();                                           \
  cout << (end - start) << "s, " << (end - start) / (iterations) * 1e9         \
       << "ns per iteration." << endl

int main (int argc, char **argv)
{
  float64 start, end;

  BENCH_BEGIN ("discarding ob_retort return", FLAT_ITERATIONS);
  {
    return_ob_retort ();
  }
  BENCH_END (FLAT_ITERATIONS);

  BENCH_BEGIN ("discarding ObRetort return", FLAT_ITERATIONS);
  {
    return_ObRetort ();
  }
  BENCH_END (FLAT_ITERATIONS);

  BENCH_BEGIN ("checking ob_retort return", FLAT_ITERATIONS);
  {
    if (return_ob_retort () < 0)
      abort ();
  }
  BENCH_END (FLAT_ITERATIONS);

  BENCH_BEGIN ("checking ObRetort return", FLAT_ITERATIONS);
  {
    if (return_ObRetort ().IsError ())
      abort ();
  }
  BENCH_END (FLAT_ITERATIONS);

  BENCH_BEGIN ("deep stack of ob_retort returns", NESTY_ITERATIONS);
  {
    if (nesty_ob_retort ((void *) nesty_ob_retort, NESTY_DEPTH) < 0)
      abort ();
  }
  BENCH_END (NESTY_ITERATIONS * NESTY_DEPTH);

  BENCH_BEGIN ("deep stack of ObRetort returns", NESTY_ITERATIONS);
  {
    if (nesty_ObRetort ((void *) nesty_ObRetort, NESTY_DEPTH) < 0)
      abort ();
  }
  BENCH_END (NESTY_ITERATIONS * NESTY_DEPTH);

  return 0;
}
