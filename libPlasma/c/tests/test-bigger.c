
/* (c)  oblong industries */

// Test the retort POOL_PROTEIN_BIGGER_THAN_POOL.
// (It is sort of tested in wrap_test, but not very explicitly.)

#include "libPlasma/c/protein.h"
#include "libPlasma/c/slaw.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-vers.h"
#include "pool_cmd.h"

static void usage (void)
{
  fprintf (stderr,
           "Usage: test-bigger [-t <type>] [-s <size>] [-i <toc cap>] "
           "<pool_name>\n"
           "\t<type> defaults to \"mmap\"\n"
           "\t<size> defaults to %" OB_FMT_64 "u bytes\n",
           (unt64) POOL_MMAP_DEFAULT_SIZE);
  exit (EXIT_FAILURE);
}

static int mainish (int argc, char **argv)
{
  pool_cmd_info cmd;
  int c;
  int64 idx;
  protein p;
  ob_retort expected;

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
    OB_FATAL_ERROR_CODE (0x20410000,
                         "Can't create %s size %" OB_FMT_64 "u: %s\n",
                         cmd.pool_name, cmd.size, ob_error_string (pret));

  pool_cmd_open_pool (&cmd);
  unt64 sz = ((cmd.size + 4095) & ~OB_CONST_U64 (4095));

  // Let's make a protein slightly bigger than the pool
  int ret = EXIT_SUCCESS;
  protein big = protein_from_ff (
    NULL, slaw_map_inline_cf ("this is too big",
                              slaw_int8_array_filled (sz, -1), NULL));
  protein medium = protein_from_ff (
    NULL, slaw_map_inline_cf ("this should not be too big",
                              slaw_int8_array_filled (sz / 2, -1), NULL));
  protein tiny = protein_from_ff (
    NULL, slaw_map_inline_cf ("this is very small",
                              slaw_int8_array_filled (0, -1), NULL));

  // And try to deposit it
  pret = pool_deposit (cmd.ph, big, &idx);
  if (pret != (expected = POOL_PROTEIN_BIGGER_THAN_POOL))
    {
      OB_LOG_ERROR_CODE (0x20410001, "expected %s but got %s\n",
                         ob_error_string (expected), ob_error_string (pret));
      ret = EXIT_FAILURE;
      goto withdraw;
    }

  // Pool should still be empty at this point
  pret = pool_oldest_index (cmd.ph, &idx);
  if (pret != (expected = POOL_NO_SUCH_PROTEIN))
    {
      OB_LOG_ERROR_CODE (0x20410002, "expected %s but got %s\n",
                         ob_error_string (expected), ob_error_string (pret));
      ret = EXIT_FAILURE;
      goto withdraw;
    }

  pret = pool_newest_index (cmd.ph, &idx);
  if (pret != (expected = POOL_NO_SUCH_PROTEIN))
    {
      OB_LOG_ERROR_CODE (0x20410003, "expected %s but got %s\n",
                         ob_error_string (expected), ob_error_string (pret));
      ret = EXIT_FAILURE;
      goto withdraw;
    }

  // Now deposit our tiny protein
  pret = pool_deposit (cmd.ph, tiny, &idx);
  if (pret < OB_OK)
    {
      OB_LOG_ERROR_CODE (0x20410004, "expected OB_OK but got %s\n",
                         ob_error_string (pret));
      ret = EXIT_FAILURE;
      goto withdraw;
    }
  if (idx != 0)
    {
      OB_LOG_ERROR_CODE (0x20410005, "expected 0 but got %" OB_FMT_64 "d\n",
                         idx);
      ret = EXIT_FAILURE;
      goto withdraw;
    }

  // Pool should not be empty anymore
  pret = pool_oldest_index (cmd.ph, &idx);
  if (pret < OB_OK)
    {
      OB_LOG_ERROR_CODE (0x20410006, "expected OB_OK but got %s\n",
                         ob_error_string (pret));
      ret = EXIT_FAILURE;
      goto withdraw;
    }
  if (idx != 0)
    {
      OB_LOG_ERROR_CODE (0x20410007, "expected 0 but got %" OB_FMT_64 "d\n",
                         idx);
      ret = EXIT_FAILURE;
      goto withdraw;
    }

  pret = pool_newest_index (cmd.ph, &idx);
  if (pret < OB_OK)
    {
      OB_LOG_ERROR_CODE (0x20410008, "expected OB_OK but got %s\n",
                         ob_error_string (pret));
      ret = EXIT_FAILURE;
      goto withdraw;
    }
  if (idx != 0)
    {
      OB_LOG_ERROR_CODE (0x20410009, "expected 0 but got %" OB_FMT_64 "d\n",
                         idx);
      ret = EXIT_FAILURE;
      goto withdraw;
    }

  // Try depositing the too-big protein again
  pret = pool_deposit (cmd.ph, big, &idx);
  if (pret != (expected = POOL_PROTEIN_BIGGER_THAN_POOL))
    {
      OB_LOG_ERROR_CODE (0x2041000a, "expected %s but got %s\n",
                         ob_error_string (expected), ob_error_string (pret));
      ret = EXIT_FAILURE;
      goto withdraw;
    }

  // Our tiny protein should still be there
  pret = pool_oldest_index (cmd.ph, &idx);
  if (pret < OB_OK)
    {
      OB_LOG_ERROR_CODE (0x2041000b, "expected OB_OK but got %s\n",
                         ob_error_string (pret));
      ret = EXIT_FAILURE;
      goto withdraw;
    }
  if (idx != 0)
    {
      OB_LOG_ERROR_CODE (0x2041000c, "expected 0 but got %" OB_FMT_64 "d\n",
                         idx);
      ret = EXIT_FAILURE;
      goto withdraw;
    }

  pret = pool_newest_index (cmd.ph, &idx);
  if (pret < OB_OK)
    {
      OB_LOG_ERROR_CODE (0x2041000d, "expected OB_OK but got %s\n",
                         ob_error_string (pret));
      ret = EXIT_FAILURE;
      goto withdraw;
    }
  if (idx != 0)
    {
      OB_LOG_ERROR_CODE (0x2041000e, "expected 0 but got %" OB_FMT_64 "d\n",
                         idx);
      ret = EXIT_FAILURE;
      goto withdraw;
    }

  pret = pool_nth_protein (cmd.ph, 0, &p, NULL);
  if (pret < OB_OK)
    {
      OB_LOG_ERROR_CODE (0x2041000f, "expected OB_OK but got %s\n",
                         ob_error_string (pret));
      ret = EXIT_FAILURE;
      goto withdraw;
    }
  if (!slawx_equal (p, tiny))
    {
      OB_LOG_ERROR_CODE (0x20410010, "didn't get expected protein\n");
      protein_free (p);
      ret = EXIT_FAILURE;
      goto withdraw;
    }
  protein_free (p);

  // It should be okay to deposit a protein half the size of the pool
  pret = pool_deposit (cmd.ph, medium, &idx);
  if (pret < OB_OK)
    {
      OB_LOG_ERROR_CODE (0x20410011, "expected OB_OK but got %s\n",
                         ob_error_string (pret));
      ret = EXIT_FAILURE;
      goto withdraw;
    }
  if (idx != 1)
    {
      OB_LOG_ERROR_CODE (0x20410012, "expected 1 but got %" OB_FMT_64 "d\n",
                         idx);
      ret = EXIT_FAILURE;
      goto withdraw;
    }

  // Now there should be two proteins
  pret = pool_oldest_index (cmd.ph, &idx);
  if (pret < OB_OK)
    {
      OB_LOG_ERROR_CODE (0x20410013, "expected OB_OK but got %s\n",
                         ob_error_string (pret));
      ret = EXIT_FAILURE;
      goto withdraw;
    }
  if (idx != 0)
    {
      OB_LOG_ERROR_CODE (0x20410014, "expected 0 but got %" OB_FMT_64 "d\n",
                         idx);
      ret = EXIT_FAILURE;
      goto withdraw;
    }

  pret = pool_newest_index (cmd.ph, &idx);
  if (pret < OB_OK)
    {
      OB_LOG_ERROR_CODE (0x20410015, "expected OB_OK but got %s\n",
                         ob_error_string (pret));
      ret = EXIT_FAILURE;
      goto withdraw;
    }
  if (idx != 1)
    {
      OB_LOG_ERROR_CODE (0x20410016, "expected 1 but got %" OB_FMT_64 "d\n",
                         idx);
      ret = EXIT_FAILURE;
      goto withdraw;
    }

  // Let's deposit another half-sized protein
  pret = pool_deposit (cmd.ph, medium, &idx);
  if (pret < OB_OK)
    {
      OB_LOG_ERROR_CODE (0x20410017, "expected OB_OK but got %s\n",
                         ob_error_string (pret));
      ret = EXIT_FAILURE;
      goto withdraw;
    }
  if (idx != 2)
    {
      OB_LOG_ERROR_CODE (0x20410018, "expected 2 but got %" OB_FMT_64 "d\n",
                         idx);
      ret = EXIT_FAILURE;
      goto withdraw;
    }

  // Now that should have blown away the first two proteins
  pret = pool_oldest_index (cmd.ph, &idx);
  if (pret < OB_OK)
    {
      OB_LOG_ERROR_CODE (0x20410019, "expected OB_OK but got %s\n",
                         ob_error_string (pret));
      ret = EXIT_FAILURE;
      goto withdraw;
    }
  if (idx != 2)
    {
      OB_LOG_ERROR_CODE (0x2041001a, "expected 2 but got %" OB_FMT_64 "d\n",
                         idx);
      ret = EXIT_FAILURE;
      goto withdraw;
    }

  pret = pool_newest_index (cmd.ph, &idx);
  if (pret < OB_OK)
    {
      OB_LOG_ERROR_CODE (0x2041001b, "expected OB_OK but got %s\n",
                         ob_error_string (pret));
      ret = EXIT_FAILURE;
      goto withdraw;
    }
  if (idx != 2)
    {
      OB_LOG_ERROR_CODE (0x2041001c, "expected 2 but got %" OB_FMT_64 "d\n",
                         idx);
      ret = EXIT_FAILURE;
      goto withdraw;
    }

  // Still shouldn't be able to deposit a too-big protein
  pret = pool_deposit (cmd.ph, big, &idx);
  if (pret != (expected = POOL_PROTEIN_BIGGER_THAN_POOL))
    {
      OB_LOG_ERROR_CODE (0x2041001d, "expected %s but got %s\n",
                         ob_error_string (expected), ob_error_string (pret));
      ret = EXIT_FAILURE;
      goto withdraw;
    }

#if 0  // temporarily disable this part of the tests due to performance problems in compatibility testing

  // Let's do a sweep of proteins around half the size of the pool
  // and see if anything bad happens
  for (c = 1024  ;  c >= -1024  ;  c--)
    { protein goldilocks =
        protein_from_ff (NULL,
                         slaw_map_inline_cf ("this might be just right!",
                                             slaw_int8_array_filled (sz
                                                                     / 2 + c,
                                                                     -1),
                                             NULL));
      pret = pool_deposit (cmd.ph, goldilocks, &idx);
      Free_Protein (goldilocks);
      if (pret != OB_OK)
        { OB_LOG_ERROR ("with c=%d, expected OB_OK but\n"
                        "got %s\n",
                        c, ob_error_string (pret));
          ret = EXIT_FAILURE;
          goto withdraw;
        }
    }

  // Let's do a sweep of proteins around the size of the pool
  // and see if anything bad happens
  for (c = 1024  ;  c >= -1024  ;  c--)
    { protein goldilocks =
        protein_from_ff (NULL,
                         slaw_map_inline_cf ("this might be just right!",
                                             slaw_int8_array_filled (sz
                                                                     + c, -1),
                                             NULL));
      pret = pool_deposit (cmd.ph, goldilocks, &idx);
      Free_Protein (goldilocks);
      if (pret != OB_OK  &&  pret != POOL_PROTEIN_BIGGER_THAN_POOL)
        { OB_LOG_ERROR ("with c=%d, expected OB_OK or\n"
                        "POOL_PROTEIN_BIGGER_THAN_POOL, but\n"
                        "got %s\n",
                        c, ob_error_string (pret));
          ret = EXIT_FAILURE;
          goto withdraw;
        }
    }
#endif

withdraw:
  pret = pool_withdraw (cmd.ph);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x2041001e, "pool_withdraw failed with: %s\n",
                         ob_error_string (pret));

  pret = pool_dispose (cmd.pool_name);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x2041001f, "pool_dispose failed with: %s\n",
                         ob_error_string (pret));

  protein_free (big);
  protein_free (medium);
  protein_free (tiny);
  pool_cmd_free_options (&cmd);

  return ret;
}

int main (int argc, char **argv)
{
  OB_DIE_ON_ERROR (OB_CHECK_ABI ());

  return check_for_leaked_file_descriptors_scoped (mainish, argc, argv);
}
