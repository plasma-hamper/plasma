
/* (c)  oblong industries */

#include <stdio.h>
#include <stdlib.h>

// Basic integer and floating-point types
#include "libLoam/c/ob-types.h"

// String operations
#include "libLoam/c/ob-string.h"

// File operations
#include "libLoam/c/ob-file.h"

// Directory operations
#include "libLoam/c/ob-dirs.h"

// Logging
#include "libLoam/c/ob-log.h"

// Error codes
#include "libLoam/c/ob-retorts.h"

// Random numbers
#include "libLoam/c/ob-rand.h"

// Atomic operations
#include "libLoam/c/ob-atomic.h"

// Byte swapping
#include "libLoam/c/ob-endian.h"

// Miscellaneous
#include "libLoam/c/ob-util.h"

// Version information
#include "libLoam/c/ob-vers.h"

// Time-related stuff
#include "libLoam/c/ob-time.h"

int main (int argc, char **argv)
{
  // Let's define some integers and print them out:
  int8 s1 = -100;
  int16 s2 = 10000;
  int32 s4 = 123456789;
  int64 s8 = OB_CONST_I64 (-123456789123456789);

  unt8 u1 = 200;
  unt16 u2 = 40000;
  unt32 u4 = 0xdeadbeef;
  unt64 u8 = OB_CONST_U64 (0xdeadf00dbadbeef1);

  size_t nbits = 8 * sizeof (void *);

  printf ("a signed  8-bit integer is %d\n", s1);
  printf ("a signed 16-bit integer is %d\n", s2);
  printf ("a signed 32-bit integer is %d\n", s4);
  printf ("a signed 64-bit integer is %" OB_FMT_64 "d\n", s8);

  printf ("an unsigned  8-bit integer is %u\n", u1);
  printf ("an unsigned 16-bit integer is %u\n", u2);
  printf ("an unsigned 32-bit integer is 0x%x\n", u4);
  printf ("an unsigned 64-bit integer is 0x%" OB_FMT_64 "x\n", u8);

  printf ("This is a %" OB_FMT_SIZE "d-bit machine\n", nbits);

  // How about some floating point:
  float64 bread = OB_NAN;
  float64 buzz_lightyear = OB_POSINF;
  float64 negative = OB_NEGINF;

  printf ("When we have Indian food, I love the %f\n", bread);
  printf ("To %f and beyond!\n", buzz_lightyear);
  printf ("When I think negative thoughts, %f usually comes to mind.\n",
          negative);

  // Some time stuff:
  printf ("It's been %f seconds since January 1, 1970.\n", ob_current_time ());
  ob_micro_sleep (100000);
  printf ("Now it's  %f seconds since January 1, 1970.\n", ob_current_time ());

  // Hey, would you like a uuid?
  char uu[37];
  ob_retort tort = ob_generate_uuid (uu);
  if (tort >= OB_OK)
    printf ("You are as unique as %s\n", uu);
  else
    // Many Oblong functions return a "retort" to indicate if they
    // succeeded.  Failure is indicated by a negative retort (less
    // than OB_OK, which is 0).  You can convert a retort to a
    // string with ob_error_string().
    printf ("ob_generate_uuid() failed with %s\n", ob_error_string (tort));

  // Really, though, if you want to report an error, you could
  // do it with the OB_LOG_ERROR() function:
  OB_LOG_ERROR ("Went to %f, but couldn't go beyond.\n", buzz_lightyear);

  // Or if you want to log something that's not an error:
  OB_LOG_INFO ("Did you know you're %x?\n", u4);

  // Want to look up a user id?
  int uid;
  tort = ob_uid_from_name ("nobody", &uid);
  if (tort < OB_OK)
    OB_LOG_ERROR ("couldn't find nobody because %s\n", ob_error_string (tort));
  else
    OB_LOG_INFO ("nobody is %d\n", uid);

  // Ever wanted to know what your program is called?
  printf ("The currently running program is %s\n", ob_get_prog_name ());

  // Safely copy a string into a fixed-length buffer:
  ob_safe_copy_string (uu, sizeof (uu), "Pac-Man\n");
  printf ("I like to play %s", uu);

  // Want to get rid of that pesky newline?
  ob_chomp (uu);
  printf ("I like to play %s on my Xbox 360\n", uu);

  // Match a glob pattern
  const char *str = "foo.c";
  const char *pat = "*.[ch]";
  printf ("Checking whether  \"%s\" matches the pattern \"%s\"... ", str, pat);
  if (ob_match_glob (str, pat))
    printf ("yes\n");
  else
    printf ("no\n");

  // Want to read a whole file with one function call?
  char *motd = ob_read_file ("/etc/motd");
  if (motd)
    {
      OB_LOG_INFO ("The message of the day is:\n%s", motd);
      free (motd);
    }
  else
    OB_LOG_ERROR ("There was no message today\n");

  // Oh, but what if it's a binary file?
  unt8 *data;
  unt64 len;
  tort = ob_read_binary_file ("/bin/true", &data, &len);
  if (tort < OB_OK)
    OB_LOG_ERROR ("Couldn't read /bin/true because %s\n",
                  ob_error_string (tort));
  else
    {
      OB_LOG_INFO ("/bin/true is %" OB_FMT_64 "u bytes long,\n"
                   "and the first byte is 0x%02x\n",
                   len, data[0]);
      free (data);
    }

  // Have you wondered what the opposite of deadbeef is?
  printf ("Today I ate %x\n", ob_swap32 (u4));

  // Random numbers
  printf ("It will take %d weeks to finish this project\n",
          ob_rand_int32 (1, 52));

  // Atomic operations
  int64 beer = 99;
  printf ("%" OB_FMT_64 "u bottles of beer on the wall\n"
          "%" OB_FMT_64 "u bottles of beer\n",
          beer, beer);
  ob_atomic_int64_add (&beer, -1);
  printf ("Take one down and pass it around\n"
          "%" OB_FMT_64 "u bottles of beer on the wall\n",
          beer);

  // Create a directory and any necessary parent directories
  const char *dir = "/tmp/a/very/long/directory/path/but/all/the/"
                    "components/will/be/created";
  tort = ob_mkdir_p (dir);
  printf ("When creating %s, got %s\n", dir, ob_error_string (tort));

  // Similar to C library's basename(), but const-correct
  const char *something = "/etc/apparmor.d/abstractions/freedesktop.org";
  printf ("The basename of %s is %s\n", something, ob_basename (something));

  // Find out the plasma version
  char *gsv = ob_get_version (OB_VERSION_OF_GSPEAK);
  printf ("The plasma version is \"%s\"\n", gsv);
  free (gsv);  // need to free the string that ob_get_version() allocated

  // Got a fatal error?
  OB_FATAL_ERROR ("And that's all I have to say about that\n");

  printf ("This never gets executed\n");
  return EXIT_SUCCESS;
}
