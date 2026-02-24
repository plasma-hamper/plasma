
/* (c)  oblong industries */

///
/// Test the various pool navigation methods for correctness.
///

#include "pool_cmd.h"
#include "libLoam/c/ob-sys.h"
#include "libLoam/c/ob-vers.h"
#include "libLoam/c/ob-log.h"
#include "libPlasma/c/protein.h"
#include "libPlasma/c/slaw.h"

#define NUM_PROTEINS 3

static void usage (void)
{
  ob_banner (stderr);
  fprintf (stderr, "Usage: seek_test [-i <toc cap>] [-t <type>] [-s <size>] "
                   "<pool_name>\n");
  exit (1);
}

int mainish (int argc, char *argv[])
{
  pool_hose ph;
  protein proteins[NUM_PROTEINS];
  protein curr_prot;
  pool_timestamp ts;
  ob_retort pret;
  char protein_name[100];
  int64 idx, cidx;
  int i;
  int c;
  pool_cmd_info cmd;

  memset (&cmd, 0, sizeof (cmd));
  while ((c = getopt (argc, argv, "i:s:t:")) != -1)
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
          // Would be nice to have a verbose option
          default:
            usage ();
        }
    }

  pool_cmd_setup_options (&cmd);
  if (pool_cmd_get_poolname (&cmd, argc, argv, optind))
    usage ();

  pret = pool_create (cmd.pool_name, cmd.type, cmd.create_options);
  if (pret != OB_OK)
    {
      fprintf (stderr, "no can create %s (%" OB_FMT_64 "u): %s\n",
               cmd.pool_name, cmd.size, ob_error_string (pret));
      exit (1);
    }

  pool_cmd_open_pool (&cmd);
  ph = cmd.ph;

  // Test that pool_runout on empty pool returns success
  pret = pool_runout (ph);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x2040e000, "pool_runout failed: %s\n",
                         ob_error_string (pret));

  // Test that pool_rewind on empty pool returns success
  pret = pool_rewind (ph);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x2040e001, "pool_rewind failed: %s\n",
                         ob_error_string (pret));

  // Test that pool_tolast on empty pool returns success
  pret = pool_tolast (ph);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x2040e002, "pool_tolast failed: %s\n",
                         ob_error_string (pret));

  for (i = 0; i < NUM_PROTEINS; i++)
    {
      snprintf (protein_name, sizeof (protein_name), "%d", i);
      // Try both with and without index argument
      if (i == 0)
        pret = pool_cmd_add_test_protein (ph, protein_name, &proteins[i], &idx);
      else
        pret = pool_cmd_add_test_protein (ph, protein_name, &proteins[i], NULL);
      if (pret != OB_OK)
        OB_FATAL_ERROR_CODE (0x2040e003, "deposit protein failed: %s\n",
                             ob_error_string (pret));
      if ((i == 0) && (idx != 0))
        OB_FATAL_ERROR_CODE (
          0x2040e004,
          "deposit returned wrong index: %" OB_FMT_64 "d (should be 0)\n", idx);
    }

  // Test that pool_curr gives first protein on start
  pret = pool_curr (ph, &curr_prot, &ts);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x2040e005, "pool_curr failed: %s\n",
                         ob_error_string (pret));
  if (!pool_cmd_check_protein_match (proteins[0], curr_prot))
    OB_FATAL_ERROR_CODE (0x2040e006, "proteins don't match\n");
  Free_Protein (curr_prot);

  // Test that pool_curr doesn't move the index
  pret = pool_curr (ph, &curr_prot, &ts);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x2040e007, "pool_curr failed: %s\n",
                         ob_error_string (pret));
  if (!pool_cmd_check_protein_match (proteins[0], curr_prot))
    OB_FATAL_ERROR_CODE (0x2040e008, "proteins don't match\n");
  Free_Protein (curr_prot);

  // Test that pool_tolast moves to last protein
  pret = pool_tolast (ph);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x2040e009, "pool_tolast failed: %s\n",
                         ob_error_string (pret));
  pret = pool_curr (ph, &curr_prot, &ts);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x2040e00a, "pool_curr failed: %s\n",
                         ob_error_string (pret));
  if (!pool_cmd_check_protein_match (proteins[NUM_PROTEINS - 1], curr_prot))
    OB_FATAL_ERROR_CODE (0x2040e00b, "proteins don't match\n");
  Free_Protein (curr_prot);

  // Test that pool_runout moves past last protein
  pret = pool_runout (ph);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x2040e00c, "pool_runout failed: %s\n",
                         ob_error_string (pret));
  pret = pool_curr (ph, &curr_prot, &ts);
  if (pret != POOL_NO_SUCH_PROTEIN)
    OB_FATAL_ERROR_CODE (0x2040e00d,
                         "should have gotten POOL_NO_SUCH_PROTEIN but got %s\n",
                         ob_error_string (pret));

  // Test that pool_rewind puts us back to the beginning
  pret = pool_rewind (ph);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x2040e00e, "pool_rewind failed: %s\n",
                         ob_error_string (pret));
  pret = pool_curr (ph, &curr_prot, &ts);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x2040e00f, "pool_curr failed: %s\n",
                         ob_error_string (pret));
  if (!pool_cmd_check_protein_match (proteins[0], curr_prot))
    OB_FATAL_ERROR_CODE (0x2040e010, "proteins don't match\n");
  Free_Protein (curr_prot);

  // Test that pool_frwdby moves us the right amount
  pret = pool_frwdby (ph, 1);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x2040e011, "pool_frwdby failed: %s\n",
                         ob_error_string (pret));
  pret = pool_curr (ph, &curr_prot, &ts);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x2040e012, "pool_curr failed: %s\n",
                         ob_error_string (pret));
  if (!pool_cmd_check_protein_match (proteins[1], curr_prot))
    OB_FATAL_ERROR_CODE (0x2040e013, "proteins don't match\n");
  Free_Protein (curr_prot);

  // pool_seekby is the same as pool_frwdby

  // Test that pool_backby moves us the right amount
  pret = pool_backby (ph, 1);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x2040e014, "pool_backby failed: %s\n",
                         ob_error_string (pret));
  pret = pool_curr (ph, &curr_prot, &ts);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x2040e015, "pool_curr failed: %s\n",
                         ob_error_string (pret));
  if (!pool_cmd_check_protein_match (proteins[0], curr_prot))
    OB_FATAL_ERROR_CODE (0x2040e016, "proteins don't match\n");
  Free_Protein (curr_prot);

  // Test pool_seekto absolute index change
  pret = pool_seekto (ph, 1);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x2040e017, "pool_seekto failed: %s\n",
                         ob_error_string (pret));
  pret = pool_curr (ph, &curr_prot, &ts);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x2040e018, "pool_curr failed: %s\n",
                         ob_error_string (pret));
  if (!pool_cmd_check_protein_match (proteins[1], curr_prot))
    OB_FATAL_ERROR_CODE (0x2040e019, "proteins don't match\n");
  Free_Protein (curr_prot);

  // Reset index and test mighty combo read + seek ops
  pret = pool_rewind (ph);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x2040e01a, "pool_rewind failed: %s\n",
                         ob_error_string (pret));
  if ((pret = pool_index (ph, &idx)) != OB_OK)
    OB_FATAL_ERROR_CODE (0x2040e01b, "pool_index failed: %s\n",
                         ob_error_string (pret));
  if (idx != 0)
    OB_FATAL_ERROR_CODE (0x2040e01c, "pool_index returned wrong value: %s\n",
                         ob_error_string (pret));

  // pool_next gets current protein and advances index by 1
  // XXX is this what we want? next is ambiguous
  for (i = 0; i < NUM_PROTEINS; i++)
    {
      pret = pool_next (ph, &curr_prot, &ts, NULL);
      if (pret != OB_OK)
        OB_FATAL_ERROR_CODE (0x2040e01d, "pool_next failed: %s\n",
                             ob_error_string (pret));
      if (!pool_cmd_check_protein_match (proteins[i], curr_prot))
        OB_FATAL_ERROR_CODE (0x2040e01e, "proteins don't match\n");
      Free_Protein (curr_prot);
    }

  // Check that we really are out of proteins
  pret = pool_next (ph, &curr_prot, &ts, NULL);
  if (pret != POOL_NO_SUCH_PROTEIN)
    OB_FATAL_ERROR_CODE (0x2040e01f, "pool_next gave wrong answer: %s\n",
                         ob_error_string (pret));
  if ((pret = pool_index (ph, &idx)) != OB_OK)
    OB_FATAL_ERROR_CODE (0x2040e020, "pool_index failed: %s\n",
                         ob_error_string (pret));
  if (idx != NUM_PROTEINS)
    OB_FATAL_ERROR_CODE (0x2040e021, "pool_index returned wrong value: %s\n",
                         ob_error_string (pret));

  // XXX Missing test for pool_next on index < first protein

  // pool_prev gets previous protein and leaves index at that protein
  for (i = NUM_PROTEINS - 1; i >= 0; i--)
    {
      pret = pool_prev (ph, &curr_prot, &ts, &idx);
      if (pret != OB_OK)
        OB_FATAL_ERROR_CODE (0x2040e022, "pool_prev failed: %s\n",
                             ob_error_string (pret));
      if (!pool_cmd_check_protein_match (proteins[i], curr_prot))
        OB_FATAL_ERROR_CODE (0x2040e023, "proteins don't match\n");
      Free_Protein (curr_prot);
      OB_DIE_ON_ERROR (pool_index (ph, &cidx));
      if (cidx != idx)
        OB_FATAL_ERROR_CODE (
          0x2040e024, "Got %" OB_FMT_64 "d, but expected %" OB_FMT_64 "d\n",
          cidx, idx);
    }
  // Check that we really are out of proteins
  pret = pool_prev (ph, &curr_prot, &ts, NULL);
  if (pret != POOL_NO_SUCH_PROTEIN)
    OB_FATAL_ERROR_CODE (0x2040e025, "pool_prev gave wrong answer: %s\n",
                         ob_error_string (pret));
  if ((pret = pool_index (ph, &idx)) != OB_OK)
    OB_FATAL_ERROR_CODE (0x2040e026, "pool_index failed: %s\n",
                         ob_error_string (pret));
  if (idx != 0)
    OB_FATAL_ERROR_CODE (0x2040e027, "pool_index returned wrong value: %s\n",
                         ob_error_string (pret));

  // pool_index tells us where we are
  for (i = 0; i < NUM_PROTEINS; i++)
    {
      if ((pret = pool_index (ph, &idx)) != OB_OK)
        OB_FATAL_ERROR_CODE (0x2040e028, "pool_index failed: %s\n",
                             ob_error_string (pret));
      if (idx != i)
        OB_FATAL_ERROR_CODE (0x2040e029,
                             "pool_index returned wrong value: %s\n",
                             ob_error_string (pret));
      pret = pool_next (ph, &curr_prot, &ts, NULL);
      if (pret != OB_OK)
        OB_FATAL_ERROR_CODE (0x2040e02a, "pool_next failed: %s\n",
                             ob_error_string (pret));
      Free_Protein (curr_prot);
    }
  if ((pret = pool_index (ph, &idx)) != OB_OK)
    OB_FATAL_ERROR_CODE (0x2040e02b, "pool_index failed: %s\n",
                         ob_error_string (pret));
  if (idx != NUM_PROTEINS)
    OB_FATAL_ERROR_CODE (0x2040e02c, "pool_index returned wrong value: %s\n",
                         ob_error_string (pret));

  // Test that a cloned pool hose has the correct index and works in general

  pool_hose clone_ph;
  pret = pool_hose_clone (ph, &clone_ph);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x2040e02d, "pool_clone failed: %s\n",
                         ob_error_string (pret));
  // The index of the cloned pool should be the same as the original
  if ((pret = pool_index (ph, &idx)) != OB_OK)
    OB_FATAL_ERROR_CODE (0x2040e02e, "pool_index failed: %s\n",
                         ob_error_string (pret));
  if (idx != NUM_PROTEINS)
    OB_FATAL_ERROR_CODE (0x2040e02f, "pool_index returned wrong value: %s\n",
                         ob_error_string (pret));

  // Read from the cloned pool hose
  OB_DIE_ON_ERROR (pool_rewind (clone_ph));
  pret = pool_next (clone_ph, &curr_prot, &ts, &idx);
  if (pret != OB_OK)
    OB_FATAL_ERROR_CODE (0x2040e030, "pool_next failed: %s\n",
                         ob_error_string (pret));
  if (!pool_cmd_check_protein_match (proteins[0], curr_prot))
    OB_FATAL_ERROR_CODE (0x2040e031, "proteins don't match\n");
  Free_Protein (curr_prot);
  OB_DIE_ON_ERROR (pool_index (clone_ph, &cidx));
  if (cidx != idx + 1)
    OB_FATAL_ERROR_CODE (
      0x2040e032, "Got %" OB_FMT_64 "d, but expected %" OB_FMT_64 "d + 1\n",
      cidx, idx);

  // Search for protein in the past
  slaw search = slaw_string ("descrip_0");
  if ((pret = pool_probe_back (ph, search, &curr_prot, &ts, &idx)) != OB_OK)
    OB_FATAL_ERROR_CODE (0x2040e033,
                         "pool_probe_back did not find protein: %s\n",
                         ob_error_string (pret));
  if (!pool_cmd_check_protein_match (proteins[0], curr_prot))
    OB_FATAL_ERROR_CODE (0x2040e034, "proteins don't match\n");
  Free_Protein (curr_prot);
  Free_Slaw (search);
  OB_DIE_ON_ERROR (pool_index (ph, &cidx));
  if (cidx != idx)
    OB_FATAL_ERROR_CODE (0x2040e035,
                         "Got %" OB_FMT_64 "d, but expected %" OB_FMT_64 "d\n",
                         cidx, idx);

  // Search for protein in the future
  search = slaw_string ("descrip_2");
  if ((pret = pool_probe_frwd (ph, search, &curr_prot, &ts, &idx)) != OB_OK)
    OB_FATAL_ERROR_CODE (0x2040e036,
                         "pool_probe_back did not find protein: %s\n",
                         ob_error_string (pret));
  if (!pool_cmd_check_protein_match (proteins[2], curr_prot))
    OB_FATAL_ERROR_CODE (0x2040e037, "proteins don't match\n");
  Free_Protein (curr_prot);
  Free_Slaw (search);
  OB_DIE_ON_ERROR (pool_index (ph, &cidx));
  if (cidx != idx + 1)
    OB_FATAL_ERROR_CODE (
      0x2040e038, "Got %" OB_FMT_64 "d, but expected %" OB_FMT_64 "d + 1\n",
      cidx, idx);

  // Test that index starts at the end of the pool on participate

  OB_DIE_ON_ERROR (pool_withdraw (ph));
  pool_cmd_open_pool (&cmd);
  ph = cmd.ph;
  int64 start_index;
  if ((pret = pool_index (ph, &start_index)) != OB_OK)
    OB_FATAL_ERROR_CODE (0x2040e039, "pool_index failed: %s\n",
                         ob_error_string (pret));
  // Pool index should start after the last protein, so first protein
  // returned is one deposited after the participate.
  if (start_index != (NUM_PROTEINS))
    OB_FATAL_ERROR_CODE (0x2040e03a,
                         "pool index at participate time is %" OB_FMT_64 "d"
                         ", should be %d\n",
                         start_index, NUM_PROTEINS - 1);

  // Test that index starts at the end of the pool on participate_creatingly

  OB_DIE_ON_ERROR (pool_withdraw (ph));
  pret = pool_participate_creatingly (cmd.pool_name, "mmap", &ph, NULL);
  ob_retort expected = OB_OK;
  if (pret != expected)
    OB_FATAL_ERROR_CODE (0x2040e03b,
                         "got '%s' from pool_participate_creatingly, "
                         "but expected '%s'\n",
                         ob_error_string (pret), ob_error_string (expected));
  if ((pret = pool_index (ph, &start_index)) != OB_OK)
    OB_FATAL_ERROR_CODE (0x2040e03c, "pool_index failed: %s\n",
                         ob_error_string (pret));
  if (start_index != (NUM_PROTEINS))
    OB_FATAL_ERROR_CODE (0x2040e03d,
                         "pool index at participate_creatingly "
                         "time is %" OB_FMT_64 "d"
                         ", should be %d\n",
                         start_index, NUM_PROTEINS - 1);

  // Not necessary but good habit
  for (i = 0; i < NUM_PROTEINS; i++)
    Free_Protein (proteins[i]);

  OB_DIE_ON_ERROR (pool_withdraw (ph));
  OB_DIE_ON_ERROR (pool_withdraw (clone_ph));

  pret = pool_dispose (cmd.pool_name);
  if (pret != OB_OK)
    {
      fprintf (stderr, "no can stop %s: %s\n", cmd.pool_name,
               ob_error_string (pret));
      return 1;
    }

  pool_cmd_free_options (&cmd);

  return 0;
}

int main (int argc, char **argv)
{
  OB_DIE_ON_ERROR (OB_CHECK_ABI ());

  return check_for_leaked_file_descriptors_scoped (mainish, argc, argv);
}
