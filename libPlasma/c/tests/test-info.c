
/* (c)  oblong industries */

// Test the new pool_get_info() function added for Jao:
// https://bugs.oblong.com/show_bug.cgi?id=48

#include "pool_cmd.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-vers.h"
#include "libLoam/c/ob-sys.h"
#include "libPlasma/c/slaw-string.h"
#include "libPlasma/c/slaw-coerce.h"
#include "libPlasma/c/slaw-path.h"
#include "libPlasma/c/protein.h"
#include "libPlasma/c/slaw.h"

#include <stdlib.h>

static void usage (void)
{
  fprintf (stderr,
           "Usage: test-info [-t <type>] [-s <size>] [-i <toc cap>] "
           "<pool_name>\n"
           "\t<type> defaults to \"mmap\"\n"
           "\t<size> defaults to %" OB_FMT_64 "u bytes\n",
           (unt64) POOL_MMAP_DEFAULT_SIZE);
  exit (EXIT_FAILURE);
}

int mainish (int argc, char **argv)
{
  pool_cmd_info cmd;
  int c;

  memset (&cmd, 0, sizeof (cmd));
  while ((c = getopt (argc, argv, "i:s:t:v")) != -1)
    {
      switch (c)
        {
          case 'i':
            cmd.toc_capacity = strtoll (optarg, NULL, 0);
            break;
          case 's':
            cmd.size = strtoll (optarg, NULL, 0);
            break;
          case 't':
            cmd.type = optarg;
            break;
          case 'v':
            cmd.verbose = 1;
            break;
          default:
            usage ();
        }
    }

  pool_cmd_setup_options (&cmd);
  if (pool_cmd_get_poolname (&cmd, argc, argv, optind))
    usage ();

  ob_retort pret = pool_create (cmd.pool_name, cmd.type, cmd.create_options);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x20411000,
                         "Can't create %s size %" OB_FMT_64 "u: %s\n",
                         cmd.pool_name, cmd.size, ob_error_string (pret));

  pool_cmd_open_pool (&cmd);

  protein terminal_info;
  pret = pool_get_info (cmd.ph, -1, &terminal_info);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x20411001, "pool_get_info(-1) failed with: %s\n",
                         ob_error_string (pret));

  bool terminal = slaw_path_get_bool (terminal_info, "terminal", false);
  if (!terminal)
    OB_FATAL_ERROR_CODE (0x20411002, "terminal was false, but expected true\n");

  const char *typ = slaw_path_get_string (terminal_info, "type", ":(");
  if (strcmp (typ, cmd.type) != 0)
    OB_FATAL_ERROR_CODE (0x20411003, "expected type %s, but got %s\n", cmd.type,
                         typ);

  unt64 sz = slaw_path_get_unt64 (terminal_info, "size", 923);
  if (sz != cmd.size)
    OB_FATAL_ERROR_CODE (
      0x20411004, "expected size %" OB_FMT_64 "u, but got %" OB_FMT_64 "u\n",
      cmd.size, sz);

  protein transport_info;
  pret = pool_get_info (cmd.ph, 0, &transport_info);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x20411005, "pool_get_info(0) failed with: %s\n",
                         ob_error_string (pret));

  if (strstr (cmd.pool_name, "://"))
    {
      slabu *sb = slabu_of_strings_from_split (cmd.pool_name, "://");
      const char *proto = slaw_string_emit (slabu_list_nth (sb, 0));
      slabu *sb2 = slabu_of_strings_from_split (
        slaw_string_emit (slabu_list_nth (sb, 1)), "/");
      const char *host = slaw_string_emit (slabu_list_nth (sb2, 0));
      slabu *sb3 = slabu_of_strings_from_split (host, ":");
      if (slabu_count (sb3) == 2)
        {
          host = slaw_string_emit (slabu_list_nth (sb3, 0));
          unt64 port;
          OB_DIE_ON_ERROR (slaw_to_unt64 (slabu_list_nth (sb3, 1), &port));
          unt64 p = slaw_path_get_unt64 (transport_info, "port", 923);
          if (p != port)
            OB_FATAL_ERROR_CODE (0x20411006,
                                 "expected port %" OB_FMT_64
                                 "u, but got %" OB_FMT_64 "u\n",
                                 port, p);
        }

      terminal = slaw_path_get_bool (transport_info, "terminal", true);
      if (terminal)
        OB_FATAL_ERROR_CODE (0x20411007,
                             "terminal was true, but expected false\n");

      typ = slaw_path_get_string (transport_info, "type", ":(");
      proto = "tcp";
      if (strcmp (typ, proto) != 0)
        OB_FATAL_ERROR_CODE (0x20411008, "expected type %s, but got %s\n",
                             proto, typ);

      const char *h = slaw_path_get_string (transport_info, "host", ":(");
      if (strcmp (h, host) != 0)
        OB_FATAL_ERROR_CODE (0x20411009, "expected host %s, but got %s\n", host,
                             h);

      slabu_free (sb3);
      slabu_free (sb2);
      slabu_free (sb);
    }
  else
    {
      if (!slawx_equal (terminal_info, transport_info))
        OB_FATAL_ERROR_CODE (0x2041100a, "terminal_info != transport_info\n");
    }

  pret = pool_withdraw (cmd.ph);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x2041100b, "pool_withdraw failed with: %s\n",
                         ob_error_string (pret));

  pret = pool_dispose (cmd.pool_name);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x2041100c, "pool_dispose failed with: %s\n",
                         ob_error_string (pret));

  protein_free (terminal_info);
  protein_free (transport_info);
  pool_cmd_free_options (&cmd);

  return EXIT_SUCCESS;
}

int main (int argc, char **argv)
{
  OB_DIE_ON_ERROR (OB_CHECK_ABI ());

  return check_for_leaked_file_descriptors_scoped (mainish, argc, argv);
}
