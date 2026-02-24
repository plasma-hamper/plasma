
/* (c)  oblong industries */

#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-types.h"
#include "libLoam/c/ob-sys.h"
#include <math.h>
#include "libPlasma/c/slaw-io.h"
#include "libPlasma/c/pool_cmd.h"
#include "libPlasma/c/slaw-string.h"
#include "libPlasma/c/pool.h"
#include "libPlasma/c/protein.h"
#include <stdlib.h>
#include <stdio.h>


static const byte branch[] = {0xE2, 0x91, 0x86};

static slaw_output output;
static slaw_input input;

static const char *pass;

static void something (int line, slaw s)
{
  ob_retort err;

  if (output)
    {
      err = slaw_output_write (output, s);
      OB_DIE_ON_ERROR (err);
      slaw_free (s);
    }
  else
    {
      slaw s2;
      err = slaw_input_read (input, &s2);
      OB_DIE_ON_ERROR (err);
      if (!slawx_equal (s, s2))
        {
          fprintf (stderr, "not equal at line %d (%s)\n", line, pass);
          fprintf (stderr, "original:\n");
          slaw_spew_overview (s, stderr, NULL);
          fputc ('\n', stderr);
          fprintf (stderr, "reconstructed:\n");
          slaw_spew_overview (s2, stderr, NULL);
          fputc ('\n', stderr);
          exit (EXIT_FAILURE);
        }
      slaw_free (s);
      slaw_free (s2);
    }
}

static void slaw_playground (void)
{
  slabu *sb;

  something (
    __LINE__,
    slaw_string ("http://ophelia.media.mit.edu/downloads/jamie-gspeak.m4v"));

  something (__LINE__, slaw_cons_ff (slaw_string ("QuacktimeBurper.cpp"),
                                     slaw_unt8 (97)));

  something (__LINE__, slaw_float64 (1.23456789));

  something (__LINE__, slaw_float64 (5));

  something (__LINE__, slaw_float64 (OB_NAN));

  something (__LINE__, slaw_float64 (OB_POSINF));

  something (__LINE__, slaw_float64 (OB_NEGINF));

  sb = slabu_new ();
  OB_DIE_ON_ERROR (slabu_list_add_x (sb, slaw_string ("yes")));
  OB_DIE_ON_ERROR (slabu_list_add_x (sb, slaw_string ("no")));
  OB_DIE_ON_ERROR (slabu_list_add_x (sb, slaw_string ("true")));
  OB_DIE_ON_ERROR (slabu_list_add_x (sb, slaw_string ("false")));
  OB_DIE_ON_ERROR (slabu_list_add_x (sb, slaw_string ("null")));
  OB_DIE_ON_ERROR (slabu_list_add_x (sb, slaw_string ("~")));
  OB_DIE_ON_ERROR (slabu_list_add_x (sb, slaw_nil ()));
  OB_DIE_ON_ERROR (slabu_list_add_x (sb, slaw_string (".NaN")));
  OB_DIE_ON_ERROR (slabu_list_add_x (sb, slaw_float32 (OB_NAN)));
  something (__LINE__, slaw_list_f (sb));

  const char *str = "'SetGWorld' is deprecated (declared at "
                    "/System/Library/Frameworks/ApplicationServices.framework/"
                    "Frameworks/QD.framework/Headers/QDOffscreen.h:254)";
  protein p = protein_from_ff (
    slaw_list_inline_c (str, NULL),
    slaw_map_inline_cf ("2 and beyond", slaw_float32 (OB_POSINF), NULL));
  something (__LINE__, p);

  // succeeds!
  p = protein_from_ff (NULL, NULL);
  something (__LINE__, p);

  // fails :-(
  p = protein_from_ff (NULL, slaw_string ("a_simple_protein"));
  something (__LINE__, p);

  something (__LINE__, slaw_nil ());

  something (__LINE__, slaw_string_from_substring ((const char *) branch,
                                                   sizeof (branch)));

  something (__LINE__, slaw_unt16_array_filled (5, 0xfded));

  something (__LINE__, slaw_v4unt64c_array_empty (0));

  something (__LINE__,
             slaw_map_inline_ff (slaw_string ("color"), slaw_boolean (1),
                                 slaw_string ("hope"), slaw_boolean (0), NULL));

  m5unt16 m;
  memset (&m, 0xff, sizeof (m));

  m.e = 0;
  m.e1 = 1;
  m.e2 = 2;
  m.e3 = 3;
  m.e4 = 4;
  m.e5 = 5;
  m.e12 = 12;
  m.e23 = 23;
  m.e34 = 34;
  m.e45 = 45;
  m.e51 = 51;
  m.e13 = 13;
  m.e24 = 24;
  m.e35 = 35;
  m.e41 = 41;
  m.e52 = 52;
  m.e123 = 123;
  m.e234 = 234;
  m.e345 = 345;
  m.e451 = 451;
  m.e512 = 512;
  m.e124 = 124;
  m.e235 = 235;
  m.e341 = 341;
  m.e452 = 452;
  m.e513 = 513;
  m.e1234 = 1234;
  m.e2345 = 2345;
  m.e3451 = 3451;
  m.e4512 = 4512;
  m.e5123 = 5123;
  m.e12345 = 12345;

  something (__LINE__, slaw_m5unt16 (m));
}

int mainish (int argc, char **argv)
{
  ob_retort err;

  const char *filename = "scratch/test-slaw-io.tmp";

  pass = "binary";

  err = slaw_output_open_binary (filename, &output);
  OB_DIE_ON_ERROR (err);

  slaw_playground ();

  err = slaw_output_close (output);
  OB_DIE_ON_ERROR (err);

  output = NULL;

  err = slaw_input_open_binary (filename, &input);
  OB_DIE_ON_ERROR (err);

  slaw_playground ();

  err = slaw_input_close (input);
  OB_DIE_ON_ERROR (err);

  input = NULL;

  unlink (filename);

  pass = "yaml";

  err = slaw_output_open_text (filename, &output);
  OB_DIE_ON_ERROR (err);

  slaw_playground ();

  err = slaw_output_close (output);
  OB_DIE_ON_ERROR (err);

  output = NULL;

  err = slaw_input_open_text (filename, &input);
  OB_DIE_ON_ERROR (err);

  slaw_playground ();

  err = slaw_input_close (input);
  OB_DIE_ON_ERROR (err);

  input = NULL;

  unlink (filename);

  int i;
  for (i = 0; i < 2; i++)
    {
      FILE *f;

      pass = "binary";

      f = fopen (filename, "wb");
      if (!f)
        {
          perror ("fopen wb");
          return EXIT_FAILURE;
        }

      err = ((i ? slaw_output_open_binary_z
                : slaw_output_open_binary_x) (f, &output));
      OB_DIE_ON_ERROR (err);

      slaw_playground ();

      err = slaw_output_close (output);
      OB_DIE_ON_ERROR (err);

      output = NULL;

      if (i)
        fclose (f);

      f = fopen (filename, "rb");
      if (!f)
        {
          perror ("fopen rb");
          return EXIT_FAILURE;
        }

      err =
        ((i ? slaw_input_open_binary_z : slaw_input_open_binary_x) (f, &input));
      OB_DIE_ON_ERROR (err);

      slaw_playground ();

      err = slaw_input_close (input);
      OB_DIE_ON_ERROR (err);

      input = NULL;

      if (i)
        fclose (f);

      unlink (filename);

      int j;
      for (j = 0; j < 2; j++)
        {
          slaw options = slaw_map_inline_ff (slaw_string ("directives"),
                                             slaw_boolean (j), NULL);

          pass = "yaml";

          f = fopen (filename, "w");
          if (!f)
            {
              perror ("fopen w");
              slaw_free (options);
              return EXIT_FAILURE;
            }

          err = ((i ? slaw_output_open_text_options_z
                    : slaw_output_open_text_options_x) (f, &output, options));
          OB_DIE_ON_ERROR (err);

          slaw_playground ();

          err = slaw_output_close (output);
          OB_DIE_ON_ERROR (err);

          output = NULL;

          if (i)
            fclose (f);

          f = fopen (filename, "r");
          if (!f)
            {
              perror ("fopen r");
              return EXIT_FAILURE;
            }

          err =
            ((i ? slaw_input_open_text_z : slaw_input_open_text_x) (f, &input));
          OB_DIE_ON_ERROR (err);

          slaw_playground ();

          err = slaw_input_close (input);
          OB_DIE_ON_ERROR (err);

          input = NULL;

          if (i)
            fclose (f);

          unlink (filename);

          slaw_free (options);
        }
    }

  return EXIT_SUCCESS;
}

int main (int argc, char **argv)
{
  return check_for_leaked_file_descriptors_scoped (mainish, argc, argv);
}
