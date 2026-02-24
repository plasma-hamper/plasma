
/* (c)  oblong industries */

// A test for the "easy start" constructor and EasyFinish() method
// of ArgParse.  Since these deal with global state (by squirreling
// away argc and argv) and send output to stderr, it's easiest to
// test these in a separate program with a Ruby wrapper script, rather
// than with the traditional gtest infrastructure.

#include "libLoam/c++/ArgParse.h"
#include "libLoam/c/ob-log.h"

#include <iostream>

using namespace oblong::loam;

static void trove_dump (const char *name, const ArgParse::apstringvec &t)
{
  std::cout << name << ": [";
  const char *sep = "";
  for (int64 i = 0; i < t.Count (); i++)
    {
      std::cout << sep << t.Nth (i);
      sep = ",";
    }
  std::cout << "]" << std::endl;
}

#ifdef _MSC_VER
#include <io.h>
#define dup2 _dup2
#endif

int main (int argc, char **argv)
{
  // Redirect stderr to stdout.  This is because we are checking the
  // results with a Ruby script, and Ruby's backticks only capture
  // stdout, and on Windows we can't use shell redirection like "2>&1".
  // So, effectively do the "2>&1" here in C++.  (Luckily, even Windows
  // has dup2, although it looks like it might be one of those functions
  // that Microsoft has inexplicably put an underscore in front of,
  // presumably to thwart writing cross-platform programs.)
  if (-1 == dup2 (1, 2))
    {
      ob_perror ("dup2");
      return 99;
    }

  ArgParse::apstring output_pool = "dvi-notifications";
  ArgParse::apfloat compression = 0.2;
  ArgParse::apint frame_rate_num = 30000;
  ArgParse::apint frame_rate_denom = 1001;
  ArgParse::apint parallel_tiles = 2;
  ArgParse::apint kernel = 3;
  ArgParse::apstringvec slot0poolsTrove;
  ArgParse::apstringvec slot1poolsTrove;

  ArgParse ap (argc, argv,
               "\nThe wjc gstreamer plugin will select defaults for options"
               " left unspecified.",
               21);
  ap.ArgString ("pool", "\aoutput pool for notifications ", &output_pool, true);
  ap.ArgStrings ("slot-0-pools",
                 "\anames, one per wjc card, to be used for slot 0 output",
                 &slot0poolsTrove, ',');
  ap.ArgStrings ("slot-1-pools",
                 "\anames, one per wjc card, to be used for slot 1 output",
                 &slot1poolsTrove, ',');
  ap.ArgFloat ("compression", "\acompression fraction ", &compression, true);
  ap.ArgInt ("rate-num",
             "\anumerator of framerate fraction (rate_num/"
             "rate_denom)\n\a",
             &frame_rate_num, true);
  ap.ArgInt ("rate-denom",
             "\adenominator of framerate fraction (rate_num/"
             "rate_denom)\n\a",
             &frame_rate_denom, true);
  ap.ArgInt ("tiles", "\anumber of tiles to process in parallel ",
             &parallel_tiles, true);
  ap.ArgInt ("kernel",
             "\awavelet kernel to use\n\a(0 = IRR_9x7, 1 = REV_5x3"
             ", 2 = IRR_5x3, 3 = KERNEL_DEFAULT)\n\a",
             &kernel, true);

  ap.Alias ("pool", "p");
  ap.Alias ("rate-num", "N");
  ap.Alias ("rate-denom", "D");
  ap.Alias ("tiles", "t");
  ap.Alias ("kernel", "k");
  ap.Alias ("compression", "c");
  ap.Alias ("slot-0-pools", "0");
  ap.Alias ("slot-1-pools", "1");

  const char *min_noa = getenv ("OB_MIN_NOA");
  const char *max_noa = getenv ("OB_MAX_NOA");

  if (min_noa)
    {
      if (max_noa && 0 == strcmp (max_noa, "UNLIMITED"))
        ap.EasyFinish (atoi (min_noa), ArgParse::UNLIMITED);
      else if (max_noa)
        ap.EasyFinish (atoi (min_noa), atoi (max_noa));
      else
        ap.EasyFinish (atoi (min_noa));
    }
  else
    ap.EasyFinish ();

  std::cout << "output_pool: " << output_pool << std::endl;
  std::cout << "compression: " << compression << std::endl;
  std::cout << "frame_rate_num: " << frame_rate_num << std::endl;
  std::cout << "frame_rate_denom: " << frame_rate_denom << std::endl;
  std::cout << "parallel_tiles: " << parallel_tiles << std::endl;
  std::cout << "kernel: " << kernel << std::endl;
  trove_dump ("slot0poolsTrove", slot0poolsTrove);
  trove_dump ("slot1poolsTrove", slot1poolsTrove);
  trove_dump ("non_option_args", ap.Leftovers ());

  return EXIT_SUCCESS;
}
