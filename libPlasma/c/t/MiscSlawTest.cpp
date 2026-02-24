
/* (c)  oblong industries */

// This is a gtest-based (and thus C++) test that serves as a catch-all
// for a bunch of small tests of slaw, serving the same purpose that
// MiscPoolTest serves for pools.

#include <gtest/gtest.h>
#include <tests/ob-test-helpers.h>
#include <fstream>
#include <algorithm>
#include "libPlasma/c/slaw.h"
#include "libPlasma/c/protein.h"
#include "libPlasma/c/slaw-string.h"
#include "libPlasma/c/private/plasma-testing.h"
#include "libPlasma/c/tests/plasma-gtest-helpers.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-string.h"
#include "libLoam/c/ob-file.h"
#include "libLoam/c/ob-pthread.h"

static const char *ob_test_source_relative (const char *f)
{
  return ob_test_source_relative2 (ABS_TOP_SRCDIR "/libPlasma/c/t", f);
}

TEST (MiscSlawTest, SubstringNegative)
{
  slaw s1 = slaw_string_from_substring ("foo", -1);
  EXPECT_TRUE (NULL == s1);
  slaw s2 =
    slaw_string_from_substring ("foo", OB_CONST_I64 (-9223372036854775807) - 1);
  EXPECT_TRUE (NULL == s2);
}

TEST (MiscSlawTest, SubstringZero)
{
  slaw s1 = slaw_string_from_substring ("foo", 0);
  EXPECT_FALSE (NULL == s1);
  EXPECT_EQ (0, slaw_string_emit_length (s1));
  EXPECT_STREQ ("", slaw_string_emit (s1));
  slaw_free (s1);
}

TEST (MiscSlawTest, Substring)
{
  slaw s1 = slaw_string_from_substring ("foo", 2);
  EXPECT_FALSE (NULL == s1);
  EXPECT_EQ (2, slaw_string_emit_length (s1));
  EXPECT_STREQ ("fo", slaw_string_emit (s1));
  slaw_free (s1);
}

TEST (MiscSlawTest, SubstringNull)
{
  slaw s1 = slaw_string_from_substring (NULL, 2);
  EXPECT_TRUE (NULL == s1);
}

TEST (MiscSlawTest, SearchEmptyInNonEmpty)
{
  slaw h = slaw_list_inline_c ("Watch", "it,", "buddy!", NULL);
  slaw n = slaw_list_empty ();
  EXPECT_EQ (0, slaw_list_contigsearch (h, n));
  EXPECT_EQ (0, slaw_list_gapsearch (h, n));
  slaw_free (n);
  slaw_free (h);
}

TEST (MiscSlawTest, SearchEmptyInEmpty)
{
  slaw h = slaw_list_empty ();
  slaw n = slaw_list_empty ();
  EXPECT_EQ (0, slaw_list_contigsearch (h, n));
  EXPECT_EQ (0, slaw_list_gapsearch (h, n));
  slaw_free (n);
  slaw_free (h);
}

TEST (MiscSlawTest, SearchEmptyInNonList)
{
  slaw h = slaw_unt16 (45067);
  slaw n = slaw_list_empty ();
  EXPECT_EQ (0, slaw_list_contigsearch (h, n));
  EXPECT_EQ (0, slaw_list_gapsearch (h, n));
  slaw_free (n);
  slaw_free (h);
}

TEST (MiscSlawTest, SearchEmptyInNull)
{
  slaw h = NULL;
  slaw n = slaw_list_empty ();
  EXPECT_EQ (0, slaw_list_contigsearch (h, n));
  EXPECT_EQ (0, slaw_list_gapsearch (h, n));
  slaw_free (n);
}

TEST (MiscSlawTest, MergeByArray)
{
  slaw m1 = slaw_map_inline_cc ("Jeremy", "Clarkson", "Richard", "Hammond",
                                "James", "May", "The", "Stig", NULL);
  slaw m2 =
    slaw_map_inline_cc ("Captain Jack", "Harkness", "Gwen", "Cooper", NULL);
  slaw m3 = slaw_map_inline_cc ("The", "Doctor", "Rose", "Tyler", "Martha",
                                "Jones", NULL);
  slaw m4 =
    slaw_map_inline_cc ("Captain Jack", "Sparrow", "Davy", "Jones", NULL);
  bslaw maps[] = {m1, m2, m3, m4};
  slaw mx = slaw_maps_merge_byarray (maps, 4);
  slaw my = slaw_maps_merge_f (m1, m2, m3, m4, NULL);
  EXPECT_TRUE (slawx_equal_lf (mx, my));
  slabu *sb = slabu_new ();
  for (bslaw cole = slaw_list_emit_first (mx); cole != NULL;
       cole = slaw_list_emit_next (mx, cole))
    {
      bslaw first = slaw_cons_emit_car (cole);
      bslaw last = slaw_cons_emit_cdr (cole);
      slaw str = slaw_string_format ("%s %s", slaw_string_emit (first),
                                     slaw_string_emit (last));
      slabu_list_add_x (sb, str);
    }
  slaw joined = slaw_strings_join_slabu_f (sb, ", ");
  EXPECT_STREQ ("Jeremy Clarkson, Richard Hammond, James May, The Doctor, "
                "Captain Jack Sparrow, Gwen Cooper, Rose Tyler, Martha Jones, "
                "Davy Jones",
                slaw_string_emit (joined));
  slaw_free (joined);
  slaw_free (mx);
}

#ifdef _MSC_VER
static const char null_device[] = "NUL";
static const int dir_error = EACCES;
#else
static const char null_device[] = "/dev/null";
static const int dir_error = EISDIR;
#endif

TEST (MiscSlawTest, BadBinaryOpen)
{
  /* Prevent these messages from being printed out during the test,
     since we expect them:

     error: <20002000> binary slaw file (/dev/null) is less than 8 bytes long!
     error: <20002001> binary slaw file (/dev/zero) does not begin with magic n
     error: <20002002> binary slaw file (scratch/MiscSlawTest.BadBinaryOpen.bin
   */
  ob_suppress_message (OBLV_ERROR, 0x20002000);
  ob_suppress_message (OBLV_ERROR, 0x20002001);
  ob_suppress_message (OBLV_ERROR, 0x20002002);

  slaw s;
  EXPECT_TORTEQ (ob_errno_to_retort (ENOENT),
                 slaw_read_from_binary_file ("/no/such/file", &s));
  EXPECT_TORTEQ (SLAW_WRONG_FORMAT,
                 slaw_read_from_binary_file (null_device, &s));
#ifndef _MSC_VER  // no /dev/zero on Windows
  EXPECT_TORTEQ (SLAW_WRONG_FORMAT,
                 slaw_read_from_binary_file ("/dev/zero", &s));
#endif
  const char *name = "scratch/MiscSlawTest.BadBinaryOpen.bin";
  std::ofstream ofs (name, std::ios_base::binary | std::ios_base::out
                             | std::ios_base::trunc);
  ASSERT_TRUE (ofs.good ());
  for (int i = 0; i < 2; i++)
    {
      ofs.put ((char) 0xff);
      ofs.put ((char) 0xff);
      ofs.put (0x0b);
      ofs.put (0x10);
    }
  ofs.close ();
  EXPECT_TORTEQ (SLAW_WRONG_VERSION, slaw_read_from_binary_file (name, &s));
}

TEST (MiscSlawTest, BadGenericOpen)
{
  /* Prevent this message from being printed out during the test,
     since we expect it:

     error: <2000500c> Reader error: Invalid leading UTF-8 octet: #FF at 0
   */
  ob_suppress_message (OBLV_ERROR, 0x2000500c);

  slaw s;
  EXPECT_TORTEQ (ob_errno_to_retort (ENOENT),
                 slaw_read_from_file ("/no/such/file", &s));
  EXPECT_TORTEQ (SLAW_END_OF_FILE, slaw_read_from_file (null_device, &s));
  const char *name = "scratch/MiscSlawTest.BadGenericOpen.bin";
  std::ofstream ofs (name, std::ios_base::binary | std::ios_base::out
                             | std::ios_base::trunc);
  ASSERT_TRUE (ofs.good ());
  for (int i = 0; i < 8; i++)
    ofs.put ((char) 0xff);
  ofs.close ();
  EXPECT_TORTEQ (SLAW_WRONG_FORMAT, slaw_read_from_file (name, &s));
}

TEST (MiscSlawTest, BadOutputTextOpen)
{
  slaw_output f;
  EXPECT_EQ (
    ob_errno_to_retort (dir_error),
    slaw_output_open_text_options_f (
      ".", &f,
      slaw_map_inline_cf ("ordered_maps", slaw_boolean (false), NULL)));
}

struct thingy
{
  char ext[8];
  ob_retort (*ofunc) (const char *filename, slaw_output *f);
};

static const thingy things[] = {{"bin", slaw_output_open_binary},
                                {"yaml", slaw_output_open_text}};

TEST (MiscSlawTest, GenericOpen)
{
  slaw s[3];
  unt8 meaning = 42;

  s[0] = slaw_list_inline_c ("kindly fatten kittens left here", NULL);
  s[1] =
    slaw_map_inline_cf ("hardy heron", slaw_float64 (8.04), "jaunty jackalope",
                        slaw_float64 (9.04), "lucid lynx", slaw_float64 (10.04),
                        "natty narwhal", slaw_float64 (11.04), NULL);
  s[2] = protein_from_llr (s[0], s[1], &meaning, 1);

  for (int i = 0; i < 2; i++)
    {
      SCOPED_TRACE (things[i].ext);
      slaw name = slaw_string_format ("scratch/MiscSlawTest.GenericOpen.%s",
                                      things[i].ext);
      slaw_output of = NULL;
      EXPECT_TORTEQ (OB_OK, things[i].ofunc (slaw_string_emit (name), &of));
      for (int j = 0; j < 3; j++)
        EXPECT_TORTEQ (OB_OK, slaw_output_write (of, s[j]));
      EXPECT_TORTEQ (OB_OK, slaw_output_close (of));

      slaw_input f = NULL;
      slaw g = NULL;
      EXPECT_TORTEQ (OB_OK, slaw_input_open (slaw_string_emit (name), &f));
      for (int j = 0; j < 3; j++)
        {
          EXPECT_TORTEQ (OB_OK, slaw_input_read (f, &g));
          EXPECT_TRUE (slawx_equal (s[j], g));
          slaw_free (g);
        }
      EXPECT_TORTEQ (SLAW_END_OF_FILE, slaw_input_read (f, &g));
      EXPECT_TORTEQ (SLAW_END_OF_FILE, slaw_input_read (f, &g));
      EXPECT_TORTEQ (OB_OK, slaw_input_close (f));
      slaw_free (name);
    }

  for (int i = 0; i < 3; i++)
    slaw_free (s[i]);
}

// http://www.thesmokinggun.com/file/katy-perry-rider?page=4
#define KATY_PERRYS_SPELLING                                                   \
  "DO NOT STAIR AT THE BACKSEAT THRU THE REARVIEUW MIRROW"

TEST (MiscSlawTest, SlawToString)
{
  slaw s = NULL;
  for (int i = 0; i < 50; i++)
    {
      s = slaw_list_inline_f (slaw_string ("\303\234nicode"),
                              slaw_string ("DwCDUZmdct XGVLUI\312\274NDEO "
                                           "zHVPlmG\360\235\225\203wy"
                                           " HubGdlLjVN XCGLDUJ\312\274NUE "
                                           "yyUTesYmrw"),
                              slaw_unt32 (0xdeadbeef), slaw_nil (),
                              slaw_string (KATY_PERRYS_SPELLING),
                              slaw_unt64 (OB_CONST_U64 (12345678901234567890)),
                              s, NULL);
    }
  slaw str = NULL;
  EXPECT_TORTEQ (OB_OK, slaw_to_string (s, &str));
  slaw t = NULL;
  EXPECT_TORTEQ (OB_OK, slaw_from_string (slaw_string_emit (str), &t));
  EXPECT_TRUE (slawx_equal (s, t));
  slaw_free (s);
  slaw_free (t);
  slaw_free (str);
}

TEST (MiscSlawTest, IncorrectYamlStringTag)
{
  const char *bad =
    "%YAML 1.1\n"
    "%TAG ! tag:oblong.com,2009:slaw/\n"
    "--- !protein \n"
    "ingests: !!omap \n"
    "    - !!string \"observations\" : !!seq [!!omap [{!!string \"timestamp\" "
    ": !!string\n"
    "\"2011-09-08T17:18:14.209-04:00\"}, {!!string \"attrs\" : !!omap [{!!stri"
    "ng \"pid\" :\n"
    "!!string \"PID_0987654321\"}, {!!string \"value\" : !!string \"30.5\"}, {"
    "? !!string\n"
    "\"measurementType\" : !!string \"I\"}, {!!string \"measurementQuality\" :"
    " !!string\n"
    "\"GOOD\"}, {!!string \"measurementPhase\" : !!string \"AB\"}]}, {!!string"
    " \"id\" :\n"
    "!!string \"Line_0005\"}], !!omap [{!!string \"timestamp\" : !!string\n"
    "\"2011-09-08T17:18:14.209-04:00\"}, {!!string \"attrs\" : !!omap [{!!stri"
    "ng \"pid\" :\n"
    "!!string \"PID_0987654322\"}, {!!string \"value\" : !!string \"30.5499992"
    "37060547\"},\n"
    "{!!string \"measurementType\" : !!string \"P\"}, {!!string \"measurementQ"
    "uality\" :\n"
    "!!string \"GOOD\"}, {!!string \"measurementPhase\" : !!string \"AB\"}]}, "
    "{? !!string\n"
    "\"id\" : !!string \"Line_0005\"}]]\n"
    "descrips: !!seq \n"
    "  - !!string \"sluice\"\n"
    "  - !!string \"prot-spec v1.0\"\n"
    "  - !!string \"observations\"\n";

  const char *good = "%YAML 1.1\n"
                     "%TAG ! tag:oblong.com,2009:slaw/\n"
                     "--- !protein\n"
                     "descrips:\n"
                     "- sluice\n"
                     "- prot-spec v1.0\n"
                     "- observations\n"
                     "ingests: !!omap\n"
                     "- observations:\n"
                     "  - !!omap\n"
                     "    - timestamp: 2011-09-08T17:18:14.209-04:00\n"
                     "    - attrs: !!omap\n"
                     "      - pid: PID_0987654321\n"
                     "      - value: '30.5'\n"
                     "      - measurementType: I\n"
                     "      - measurementQuality: GOOD\n"
                     "      - measurementPhase: AB\n"
                     "    - id: Line_0005\n"
                     "  - !!omap\n"
                     "    - timestamp: 2011-09-08T17:18:14.209-04:00\n"
                     "    - attrs: !!omap\n"
                     "      - pid: PID_0987654322\n"
                     "      - value: '30.549999237060547'\n"
                     "      - measurementType: P\n"
                     "      - measurementQuality: GOOD\n"
                     "      - measurementPhase: AB\n"
                     "    - id: Line_0005\n"
                     "...\n";

  slaw sbad = NULL;
  slaw sgood = NULL;

  slaw roundtrip_bad = NULL;
  slaw roundtrip_good = NULL;

  EXPECT_TORTEQ (OB_OK, slaw_from_string (bad, &sbad));
  EXPECT_TORTEQ (OB_OK, slaw_from_string (good, &sgood));
  EXPECT_TORTEQ (OB_OK, slaw_to_string (sbad, &roundtrip_bad));
  EXPECT_TORTEQ (OB_OK, slaw_to_string (sgood, &roundtrip_good));
  EXPECT_TRUE (slawx_equal (sbad, sgood))
    << "We got:\n\n"
    << slaw_string_emit (roundtrip_bad) << "\n\nbut expected:\n\n"
    << slaw_string_emit (roundtrip_good) << std::endl;
  slaw_free (sbad);
  slaw_free (sgood);
  slaw_free (roundtrip_bad);
  slaw_free (roundtrip_good);
}

class MiscSlawTest1 : public ::testing::Test
{
 public:
  typedef void (MiscSlawTest1::*flushTestSetup) ();

  slaw a_slaw_string;
  slaw a_slaw_map;
  slaw a_slaw_list;
  slaw a_large_map;
  protein a_small_protein;
  protein a_large_protein;
  byte *bin500;
  unt64 len500;
  bslaw some_slawx[6];
  const unt8 meaning;
  slaw_input si;
  slaw_output so;
  int pipe1[2];
  int pipe2[2];
  flushTestSetup thread_setup;

  MiscSlawTest1 ();
  ~MiscSlawTest1 () override;

  void FlushTest (flushTestSetup setup_output, flushTestSetup setup_input);
  void FlushTestThreadMain ();

  void SetupBinaryOutput ();
  void SetupBinaryInput ();

  void SetupYamlOutput ();
  void SetupYamlInput ();
};

MiscSlawTest1::MiscSlawTest1 ()
    : meaning (42), si (NULL), so (NULL), thread_setup (NULL)
{
  const char *fname = ob_test_source_relative ("500.png");
  OB_DIE_ON_ERROR (ob_read_binary_file (fname, &bin500, &len500));

  a_slaw_string = slaw_string ("kindly fatten kittens left here");
  slaw descrips = slaw_list_inline (a_slaw_string, NULL);
  a_slaw_map =
    slaw_map_inline_cf ("hardy heron", slaw_float64 (8.04), "jaunty jackalope",
                        slaw_float64 (9.04), "lucid lynx", slaw_float64 (10.04),
                        "natty narwhal", slaw_float64 (11.04), NULL);
  a_small_protein = protein_from_llr (descrips, a_slaw_map, &meaning, 1);
  a_large_protein = protein_from_llr (descrips, a_slaw_map, bin500, len500);
  a_slaw_list = slaw_list_inline_f (
    slaw_string ("\303\234nicode"),
    slaw_string ("DwCDUZmdct XGVLUI\312\274NDEO "
                 "zHVPlmG\360\235\225\203wy"
                 " HubGdlLjVN XCGLDUJ\312\274NUE "
                 "yyUTesYmrw"),
    slaw_unt32 (0xdeadbeef), slaw_nil (), slaw_string (KATY_PERRYS_SPELLING),
    slaw_unt64 (OB_CONST_U64 (12345678901234567890)), NULL);
  byte *sorted = new byte[len500];
  memcpy (sorted, bin500, len500);
  std::sort (sorted, sorted + len500);
  byte uniq[256];  // can't be more than 256 unique bytes, by definition
  byte *uend = std::unique_copy (sorted, sorted + len500, uniq);
  a_large_map = slaw_map_inline_ff (
    slaw_string ("That's an error"), slaw_unt8_array (bin500, len500),
    slaw_string ("unique"), slaw_unt8_array (uniq, uend - uniq),
    slaw_unt8_array (sorted, len500),
    slaw_string ("that was binary data "
                 "as a map key"),
    NULL);

  some_slawx[0] = a_slaw_string;
  some_slawx[1] = a_slaw_map;
  some_slawx[2] = a_slaw_list;
  some_slawx[3] = a_large_map;
  some_slawx[4] = a_small_protein;
  some_slawx[5] = a_large_protein;
  slaw_free (descrips);
  delete[] sorted;
}

MiscSlawTest1::~MiscSlawTest1 ()
{
  slaw_free (a_slaw_string);
  slaw_free (a_slaw_map);
  slaw_free (a_slaw_list);
  slaw_free (a_large_map);
  protein_free (a_small_protein);
  protein_free (a_large_protein);
  free (bin500);
}

static void *flush_test_thread_main (void *v)
{
  MiscSlawTest1 *mst1 = (MiscSlawTest1 *) v;
  mst1->FlushTestThreadMain ();
  return NULL;
}

void MiscSlawTest1::FlushTest (flushTestSetup setup_output,
                               flushTestSetup setup_input)
{
  OB_DIE_ON_ERROR (ob_pipe_cloexec (pipe1));
  OB_DIE_ON_ERROR (ob_pipe_cloexec (pipe2));

  thread_setup = setup_input;
  (this->*setup_output) ();

  pthread_t thr;
  EXPECT_EQ (0, pthread_create (&thr, NULL, flush_test_thread_main, this));

  for (size_t i = 0; i < sizeof (some_slawx) / sizeof (some_slawx[0]); i++)
    {
      EXPECT_TORTEQ (OB_OK, slaw_output_write (so, some_slawx[i]));
      slaw foo = NULL;
      EXPECT_EQ (ssize_t (sizeof (foo)),
                 ssize_t (read (pipe2[0], &foo, sizeof (foo))));
      EXPECT_TRUE (slawx_equal (some_slawx[i], foo));
      slaw_free (foo);
    }

  EXPECT_TORTEQ (OB_OK, slaw_output_close (so));
  EXPECT_EQ (0, pthread_join (thr, NULL));
  EXPECT_TORTEQ (OB_OK, slaw_input_close (si));
  EXPECT_EQ (0, close (pipe2[0]));
  EXPECT_EQ (0, close (pipe2[1]));
}

void MiscSlawTest1::FlushTestThreadMain ()
{
  ob_retort tort;
  slaw s = NULL;
  (this->*thread_setup) ();
  while ((tort = slaw_input_read (si, &s)) != SLAW_END_OF_FILE)
    {
      OB_DIE_ON_ERROR (tort);
      EXPECT_EQ (ssize_t (sizeof (s)),
                 ssize_t (write (pipe2[1], &s, sizeof (s))));
      s = NULL;
    }
}

void MiscSlawTest1::SetupBinaryOutput ()
{
  OB_DIE_ON_ERROR (slaw_output_open_binary_x (fdopen (pipe1[1], "wb"), &so));
}

void MiscSlawTest1::SetupBinaryInput ()
{
  OB_DIE_ON_ERROR (slaw_input_open_binary_fdx (pipe1[0], &si));
}

void MiscSlawTest1::SetupYamlOutput ()
{
  OB_DIE_ON_ERROR (slaw_output_open_text_x (fdopen (pipe1[1], "wb"), &so));
}

void MiscSlawTest1::SetupYamlInput ()
{
  OB_DIE_ON_ERROR (slaw_input_open_text_fdx (pipe1[0], &si));
}

TEST_F (MiscSlawTest1, FlushBinary)
{
  FlushTest (&MiscSlawTest1::SetupBinaryOutput,
             &MiscSlawTest1::SetupBinaryInput);
}

TEST_F (MiscSlawTest1, FlushYaml)
{
  FlushTest (&MiscSlawTest1::SetupYamlOutput, &MiscSlawTest1::SetupYamlInput);
}

static const char limit_expected[] =
  "%YAML 1.1\n"
  "%TAG ! tag:oblong.com,2009:slaw/\n"
  "--- !!omap\n"
  "- That's an error: !array [137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13,\n"
  "    73, 72, 68, 82, 0, 0, 2, 123, and 24983 more (25003 total)]\n"
  "- unique: !array [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,\n"
  "    16, 17, 18, 19, and 236 more (256 total)]\n"
  "- ? !array [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\n"
  "    and 24983 more (25003 total)]\n"
  "  : that was binary data as a map key\n"
  "...\n";

TEST_F (MiscSlawTest1, ArrayLimit)
{
  slaw str = NULL;
  slaw options = slaw_map_inline_cf ("max-array-elements", slaw_int64 (20),
                                     "tag_numbers", slaw_boolean (false), NULL);
  EXPECT_TORTEQ (OB_OK, slaw_to_string_options_f (a_large_map, &str, options));
  EXPECT_STREQ (limit_expected, slaw_string_emit (str));
  slaw_free (str);
}

class ProteinSearchTest : public ::testing::Test
{
 public:
  protein haystack;
  slaw needle1, needle2, needle3, needle4, needle5, needle6, needle7;

  ProteinSearchTest ();
  ~ProteinSearchTest () override;
};

ProteinSearchTest::ProteinSearchTest ()
    : haystack (protein_from_ff (
        slaw_list_f (slabu_of_strings_from_split (KATY_PERRYS_SPELLING, " ")),
        NULL)),
      needle1 (slaw_string ("MIRROW")),
      needle2 (slaw_string ("MIRROR")),
      needle3 (slaw_list_inline_c ("STAIR", "REARVIEUW", NULL)),
      needle4 (slaw_list_inline_c ("STARE", "REARVIEW", NULL)),
      needle5 (slaw_list_inline_c ("BACKSEAT", "THRU", "THE", NULL)),
      needle6 (slaw_list_inline_c ("DO", "AT", "THRU", NULL)),
      needle7 (slaw_list_inline_c ("THE", "REARVIEUW", NULL))
{
}

ProteinSearchTest::~ProteinSearchTest ()
{
  slaw_free (haystack);
  slaw_free (needle1);
  slaw_free (needle2);
  slaw_free (needle3);
  slaw_free (needle4);
  slaw_free (needle5);
  slaw_free (needle6);
  slaw_free (needle7);
}

TEST_F (ProteinSearchTest, Gap)
{
  EXPECT_EQ (9, protein_search_ex (haystack, needle1, SEARCH_GAP));
  EXPECT_EQ (-1, protein_search_ex (haystack, needle2, SEARCH_GAP));
  EXPECT_EQ (2, protein_search_ex (haystack, needle3, SEARCH_GAP));
  EXPECT_EQ (-1, protein_search_ex (haystack, needle4, SEARCH_GAP));
  EXPECT_EQ (5, protein_search_ex (haystack, needle5, SEARCH_GAP));
  EXPECT_EQ (0, protein_search_ex (haystack, needle6, SEARCH_GAP));
  EXPECT_EQ (4, protein_search_ex (haystack, needle7, SEARCH_GAP));
}

TEST_F (ProteinSearchTest, Contig)
{
  EXPECT_EQ (9, protein_search_ex (haystack, needle1, SEARCH_CONTIG));
  EXPECT_EQ (-1, protein_search_ex (haystack, needle2, SEARCH_CONTIG));
  EXPECT_EQ (-1, protein_search_ex (haystack, needle3, SEARCH_CONTIG));
  EXPECT_EQ (-1, protein_search_ex (haystack, needle4, SEARCH_CONTIG));
  EXPECT_EQ (5, protein_search_ex (haystack, needle5, SEARCH_CONTIG));
  EXPECT_EQ (-1, protein_search_ex (haystack, needle6, SEARCH_CONTIG));
  EXPECT_EQ (7, protein_search_ex (haystack, needle7, SEARCH_CONTIG));
}
