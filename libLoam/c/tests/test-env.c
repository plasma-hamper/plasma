
/* (c)  oblong industries */

#include <stdio.h>

#include "libLoam/c/ob-dirs.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-util.h"

void test_ob_append_env_list (void)
{
  ob_retort err;
  char *val;
  char expected[1024];

  /* Case 1: append without existing value */
  err = ob_unsetenv ("BLARG");
  if (err != OB_OK)
    error_exit ("ob_unsetenv failed");
  err = ob_append_env_list ("BLARG", "/dir1");
  if (err != OB_OK)
    error_exit ("ob_append_env_list failed");
  val = getenv ("BLARG");
  if (!val)
    error_exit ("getenv failed");
  if (strcmp (val, "/dir1"))
    error_exit ("ob_append_env_list wrong value");

  /* Case 2: append to existing value */
  err = ob_append_env_list ("BLARG", "/dir2");
  if (err != OB_OK)
    error_exit ("ob_append_env_list failed");
  val = getenv ("BLARG");
  if (!val)
    error_exit ("getenv failed");
  snprintf (expected, sizeof (expected),
            "%s%c%s", "/dir1", OB_PATH_CHAR, "/dir2");
  if (strcmp (val, expected))
    error_exit ("ob_append_env_list wrong value");

  /* Case 3: prepend to existing value */
  err = ob_prepend_env_list ("BLARG", "/dir0");
  if (err != OB_OK)
    error_exit ("ob_prepend_env_list failed");
  val = getenv ("BLARG");
  if (!val)
    error_exit ("getenv failed");
  snprintf (expected, sizeof (expected), "%s%c%s%c%s",
            "/dir0", OB_PATH_CHAR,
            "/dir1", OB_PATH_CHAR,
            "/dir2");
  if (strcmp (val, expected))
    error_exit ("ob_prepend_env_list wrong value");
}

int main (int argc, char **argv)
{
  test_ob_append_env_list ();
  return EXIT_SUCCESS;
}
