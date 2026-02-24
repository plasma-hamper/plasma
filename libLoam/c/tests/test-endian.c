
/* (c)  oblong industries */

// test byte-swapping functions from ob-endian.h

#include "libLoam/c/ob-endian.h"
#include <stdlib.h>
#include <stdio.h>
#include "libLoam/c/ob-sys.h"

// Takes numbers to swap from the command line, rather than hardcoding
// them in this file, to make absolutely sure the compiler isn't doing
// any sort of compile-time optimization of constant expressions.

// Takes triples of the form: [16|32|64] value-to-swap expected-result

int main (int argc, char **argv)
{
  if (argc < 2 || 0 != ((argc - 1) % 3))
    {
      fprintf (stderr, "Usage: test-endian "
                       "[16|32|64] value-to-swap expected-result ...\n");
      return EXIT_FAILURE;
    }

  int i;
  for (i = 1; i < argc; i += 3)
    {
      unt64 sz = (unt64) strtoull (argv[i], NULL, 0);
      unt64 value = (unt64) strtoull (argv[i + 1], NULL, 0);
      unt64 expected = (unt64) strtoull (argv[i + 2], NULL, 0);
      unt64 actual;

      switch (sz)
        {
          case 16:
            actual = ob_swap16 ((unt16) value);
            break;
          case 32:
            actual = ob_swap32 ((unt32) value);
            break;
          case 64:
            actual = ob_swap64 (value);
            break;
          default:
            fprintf (stderr, "Bad size %" OB_FMT_64 "u\n", sz);
            return EXIT_FAILURE;
        }

      if (actual != expected)
        {
          fprintf (stderr,
                   "For 0x%" OB_FMT_64 "x, expected 0x%" OB_FMT_64 "x, "
                   "but got 0x%" OB_FMT_64 "x\n",
                   value, expected, actual);
          return EXIT_FAILURE;
        }
    }

  return EXIT_SUCCESS;
}
