
/* (c)  oblong industries */

#include <libPlasma/c/slaw.h>
#include <libPlasma/c/protein.h>
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-vers.h"

#include <stdio.h>

#define protein_num_ingests(p) slaw_list_count (protein_ingests (p))
#define protein_next_ingest(p, cur_ing)                                        \
  slaw_list_emit_next (protein_ingests (p), cur_ing)
#define protein_nth_ingest(p, n) slaw_list_emit_nth (protein_ingests (p), n)
#define protein_has_descrip(p, des)                                            \
  (slaw_list_find (protein_descrips (p), des) >= 0)
#define protein_ingest_from_key(p, key) slaw_map_find (protein_ingests (p), key)
#define protein_ingest_from_key_string(p, str)                                 \
  slaw_map_find_c (protein_ingests (p), str)
#define protein_has_descrip_string(p, str)                                     \
  (slaw_list_find_c (protein_descrips (p), str) >= 0)

// This is just to make valgrind happy!
static slaw to_be_freed[500];
static int free_counter = 0;

static slaw f (slaw s)
{
  to_be_freed[free_counter++] = s;
  return s;
}

static slaw empty_map (void)
{
  slabu *sb = slabu_new ();
  return slaw_map_f (sb);
}

static slaw empty_list (void)
{
  slabu *sb = slabu_new ();
  return slaw_list_f (sb);
}

int main (int ac, char **av)
{
  OB_DIE_ON_ERROR (OB_CHECK_ABI ());

  slaw s[21];
  slaw snl = slaw_nil ();

  s[0] = slaw_string ("adze");
  s[1] = slaw_string ("bilge");
  s[2] = slaw_string ("chyme");
  s[3] = slaw_string ("dirge");
  s[4] = slaw_string ("erg");
  s[5] = slaw_string ("furze");
  s[6] = slaw_string ("gorse");
  s[7] = slaw_string ("hemp");
  s[8] = slaw_string ("ilk");
  s[9] = slaw_string ("jinx");
  s[10] = slaw_string ("kurd");
  s[11] = slaw_string ("lard");
  s[12] = slaw_string ("mumps");
  s[13] = slaw_string ("nape");
  s[14] = slaw_string ("ooze");
  s[15] = slaw_string ("pule");
  s[16] = slaw_string ("quash");
  s[17] = slaw_string ("rolf");
  s[18] = slaw_string ("sump");
  s[19] = slaw_string ("torque");
  s[20] = slaw_string ("urge");

  protein pn1 = protein_from (NULL, NULL);
  protein pn2 = protein_from (NULL, NULL);
  OBSERT (pn2 != pn1);
  OBSERT (protein_is_empty (pn1));
  OBSERT (protein_is_empty (pn2));

  protein p1 = protein_from_ff (slaw_list_inline (s[0], NULL), empty_map ());

  protein p2 =
    protein_from_ff (empty_list (), slaw_map_inline (s[1], s[2], NULL));

  protein p3 =
    protein_from_ff (slaw_list_inline (s[3], s[4], s[5], NULL), empty_map ());

  protein p4 =
    protein_from_ff (empty_list (), slaw_map_inline (s[6], s[7], s[8], s[9],
                                                     s[10], s[11], NULL));

  protein p5 = protein_from_ff (
    slaw_list_inline (s[12], s[13], s[14], NULL),
    slaw_map_inline (s[15], s[16], s[17], s[18], s[19], s[20], NULL));


  OBSERT (1 == slaw_list_count (protein_descrips (p1)));
  OBSERT (0 == protein_num_ingests (p1));

  OBSERT (0 == slaw_list_count (protein_descrips (p2)));
  OBSERT (1 == protein_num_ingests (p2));

  OBSERT (3 == slaw_list_count (protein_descrips (p3)));
  OBSERT (0 == protein_num_ingests (p3));

  OBSERT (0 == slaw_list_count (protein_descrips (p4)));
  OBSERT (3 == protein_num_ingests (p4));

  OBSERT (3 == slaw_list_count (protein_descrips (p5)));
  OBSERT (3 == protein_num_ingests (p5));



  OBSERT (protein_has_descrip (p1, s[0]));
  OBSERT (!protein_has_descrip (p1, s[1]));
  OBSERT (!protein_has_descrip (p1, snl));
  OBSERT (!protein_has_descrip (p1, NULL));

  OBSERT (NULL == protein_ingest_from_key (p1, s[0]));
  OBSERT (NULL == protein_ingest_from_key (p1, snl));
  OBSERT (NULL == protein_ingest_from_key (p1, NULL));

  bslaw d1 = protein_descrips (p1);
  OBSERT (NULL != slaw_list_emit_first (d1));
  OBSERT (slawx_equal_lc (slaw_list_emit_first (d1), "adze"));
  OBSERT (slawx_equal (s[0], slaw_list_emit_first (d1)));
  OBSERT (NULL == slaw_list_emit_next (d1, slaw_list_emit_first (d1)));
  OBSERT (NULL == slaw_list_emit_first (protein_ingests (p1)));

  OBSERT (slawx_equal (s[0], slaw_list_emit_nth (d1, 0)));
  OBSERT (!slawx_equal (s[1], slaw_list_emit_nth (d1, 0)));
  OBSERT (NULL == slaw_list_emit_nth (d1, 1));
  OBSERT (NULL == protein_nth_ingest (p1, 0));



  OBSERT (!protein_has_descrip (p2, s[1]));
  OBSERT (!protein_has_descrip (p2, snl));
  OBSERT (!protein_has_descrip (p2, NULL));

  OBSERT (NULL != protein_ingest_from_key (p2, s[1]));
  OBSERT (NULL != protein_ingest_from_key_string (p2, "bilge"));
  OBSERT (0 != slawx_equal (s[2], protein_ingest_from_key (p2, s[1])));
  OBSERT (NULL == protein_ingest_from_key (p2, s[0]));
  OBSERT (NULL == protein_ingest_from_key (p2, snl));
  OBSERT (NULL == protein_ingest_from_key (p2, NULL));

  bslaw i2 = protein_ingests (p2);
  OBSERT (NULL == slaw_list_emit_first (protein_descrips (p2)));
  OBSERT (NULL != slaw_list_emit_first (i2));
  OBSERT (slawx_equal (slaw_list_emit_first (i2), f (slaw_cons (s[1], s[2]))));
  OBSERT (!slawx_equal (slaw_list_emit_first (i2), f (slaw_cons (s[2], s[1]))));

  OBSERT (NULL == slaw_list_emit_nth (protein_descrips (p2), 0));
  OBSERT (slawx_equal (protein_nth_ingest (p2, 0), f (slaw_cons (s[1], s[2]))));
  OBSERT (NULL == protein_nth_ingest (p2, 1));
  OBSERT (NULL == protein_nth_ingest (p2, -1));



  OBSERT (protein_has_descrip (p3, s[3]));
  OBSERT (protein_has_descrip (p3, s[4]));
  OBSERT (protein_has_descrip (p3, s[5]));
  OBSERT (protein_has_descrip_string (p3, "dirge"));
  OBSERT (protein_has_descrip_string (p3, "erg"));
  OBSERT (protein_has_descrip_string (p3, "furze"));
  OBSERT (!protein_has_descrip_string (p3, "gorse"));
  OBSERT (0 <= protein_search (
            p3, f (slaw_list_inline_c ("dirge", "erg", "furze", NULL))));
  OBSERT (
    0 <= protein_search (p3, f (slaw_list_inline_c ("erg", "furze", NULL))));
  OBSERT (
    0 <= protein_search (p3, f (slaw_list_inline_c ("dirge", "erg", NULL))));
  OBSERT (
    0 <= protein_search (p3, f (slaw_list_inline_c ("dirge", "furze", NULL))));
  OBSERT (0 <= protein_search (p3, f (slaw_list_inline_c ("dirge", NULL))));
  OBSERT (0 <= protein_search (p3, f (slaw_list_inline_c ("erg", NULL))));
  OBSERT (0 <= protein_search (p3, f (slaw_list_inline_c ("furze", NULL))));
  OBSERT (0
          > protein_search (p3, f (slaw_list_inline_c ("furze", "erg", NULL))));
  OBSERT (!protein_has_descrip (p3, s[1]));
  OBSERT (!protein_has_descrip (p3, snl));
  OBSERT (!protein_has_descrip (p3, NULL));

  bslaw d3 = protein_descrips (p3);
  bslaw slop = slaw_list_emit_first (d3);
  OBSERT (NULL != slop);
  OBSERT (slawx_equal_lc (slop, "dirge"));
  OBSERT (slawx_equal (s[3], slop));
  slop = slaw_list_emit_next (d3, slop);
  OBSERT (NULL != slop);
  OBSERT (slawx_equal_lc (slop, "erg"));
  OBSERT (slawx_equal (s[4], slop));
  slop = slaw_list_emit_next (d3, slop);
  OBSERT (NULL != slop);
  OBSERT (slawx_equal_lc (slop, "furze"));
  OBSERT (slawx_equal (s[5], slop));
  slop = slaw_list_emit_next (d3, slop);
  OBSERT (NULL == slop);
  OBSERT (NULL == slaw_list_emit_first (protein_ingests (p3)));

  OBSERT (slawx_equal (s[3], slaw_list_emit_nth (d3, 0)));
  OBSERT (slawx_equal (s[4], slaw_list_emit_nth (d3, 1)));
  OBSERT (slawx_equal (s[5], slaw_list_emit_nth (d3, 2)));
  OBSERT (NULL == slaw_list_emit_nth (d3, 3));
  OBSERT (NULL == protein_nth_ingest (p3, 0));



#define BLERV(pro)                                                             \
  OBSERT (NULL != protein_ingest_from_key (pro, s[6]));                        \
  OBSERT (NULL != protein_ingest_from_key (pro, s[8]));                        \
  OBSERT (NULL != protein_ingest_from_key (pro, s[10]));                       \
  OBSERT (NULL != protein_ingest_from_key_string (pro, "gorse"));              \
  OBSERT (NULL != protein_ingest_from_key_string (pro, "ilk"));                \
  OBSERT (NULL != protein_ingest_from_key_string (pro, "kurd"));               \
  OBSERT (slawx_equal (s[7], protein_ingest_from_key_string (pro, "gorse")));  \
  OBSERT (slawx_equal (s[9], protein_ingest_from_key_string (pro, "ilk")));    \
  OBSERT (slawx_equal (s[11], protein_ingest_from_key_string (pro, "kurd")));  \
  OBSERT (slawx_equal_lc (protein_ingest_from_key (pro, s[6]), "hemp"));       \
  OBSERT (slawx_equal_lc (protein_ingest_from_key (pro, s[8]), "jinx"));       \
  OBSERT (slawx_equal_lc (protein_ingest_from_key (pro, s[10]), "lard"));      \
  OBSERT (NULL == protein_ingest_from_key (pro, s[7]));                        \
  OBSERT (NULL == protein_ingest_from_key (pro, s[9]));                        \
  OBSERT (NULL == protein_ingest_from_key (pro, s[11]));                       \
  OBSERT (NULL == protein_ingest_from_key (pro, s[0]));                        \
  OBSERT (NULL == protein_ingest_from_key (pro, snl));                         \
  OBSERT (NULL == protein_ingest_from_key (pro, NULL));                        \
                                                                               \
  OBSERT (NULL == slaw_list_emit_first (protein_descrips (pro)));              \
  slop = slaw_list_emit_first (protein_ingests (pro));                         \
  OBSERT (NULL != slop);                                                       \
  OBSERT (slawx_equal (slop, f (slaw_cons (s[6], s[7]))));                     \
  OBSERT (!slawx_equal (slop, f (slaw_cons (s[7], s[6]))));                    \
  slop = protein_next_ingest (pro, slop);                                      \
  OBSERT (NULL != slop);                                                       \
  OBSERT (slawx_equal (slop, f (slaw_cons (s[8], s[9]))));                     \
  OBSERT (!slawx_equal (slop, f (slaw_cons (s[9], s[8]))));                    \
  slop = protein_next_ingest (pro, slop);                                      \
  OBSERT (NULL != slop);                                                       \
  OBSERT (slawx_equal (slop, f (slaw_cons (s[10], s[11]))));                   \
  OBSERT (!slawx_equal (slop, f (slaw_cons (s[11], s[10]))));                  \
  slop = protein_next_ingest (pro, slop);                                      \
  OBSERT (NULL == slop);                                                       \
                                                                               \
  OBSERT (NULL == slaw_list_emit_nth (protein_descrips (pro), 0));             \
  OBSERT (                                                                     \
    slawx_equal (protein_nth_ingest (pro, 0), f (slaw_cons (s[6], s[7]))));    \
  OBSERT (                                                                     \
    slawx_equal (protein_nth_ingest (pro, 1), f (slaw_cons (s[8], s[9]))));    \
  OBSERT (                                                                     \
    slawx_equal (protein_nth_ingest (pro, 2), f (slaw_cons (s[10], s[11]))));  \
  OBSERT (NULL == protein_nth_ingest (pro, 3));                                \
  OBSERT (NULL == protein_nth_ingest (pro, -1));


  BLERV (p4);
  protein pp4 = protein_dup (p4);
  BLERV (pp4);


  // used to be GROMP
  OBSERT (protein_has_descrip (p5, s[12]));
  OBSERT (protein_has_descrip (p5, s[13]));
  OBSERT (protein_has_descrip (p5, s[14]));
  OBSERT (protein_has_descrip_string (p5, "mumps"));
  OBSERT (protein_has_descrip_string (p5, "nape"));
  OBSERT (protein_has_descrip_string (p5, "ooze"));
  OBSERT (!protein_has_descrip_string (p5, "pule"));
  OBSERT (0 <= protein_search (
            p5, f (slaw_list_inline_c ("mumps", "nape", "ooze", NULL))));
  OBSERT (
    0 <= protein_search (p5, f (slaw_list_inline_c ("nape", "ooze", NULL))));
  OBSERT (
    0 <= protein_search (p5, f (slaw_list_inline_c ("mumps", "nape", NULL))));
  OBSERT (
    0 <= protein_search (p5, f (slaw_list_inline_c ("mumps", "ooze", NULL))));
  OBSERT (0 <= protein_search (p5, f (slaw_list_inline_c ("mumps", NULL))));
  OBSERT (0 <= protein_search (p5, f (slaw_list_inline_c ("nape", NULL))));
  OBSERT (0 <= protein_search (p5, f (slaw_list_inline_c ("ooze", NULL))));
  OBSERT (0
          > protein_search (p5, f (slaw_list_inline_c ("ooze", "nape", NULL))));
  OBSERT (!protein_has_descrip (p5, s[1]));
  OBSERT (!protein_has_descrip (p5, snl));
  OBSERT (!protein_has_descrip (p5, NULL));
  OBSERT (NULL != protein_ingest_from_key (p5, s[15]));
  OBSERT (NULL != protein_ingest_from_key (p5, s[17]));
  OBSERT (NULL != protein_ingest_from_key (p5, s[19]));
  OBSERT (NULL != protein_ingest_from_key_string (p5, "pule"));
  OBSERT (NULL != protein_ingest_from_key_string (p5, "rolf"));
  OBSERT (NULL != protein_ingest_from_key_string (p5, "torque"));
  OBSERT (slawx_equal (s[16], protein_ingest_from_key_string (p5, "pule")));
  OBSERT (slawx_equal (s[18], protein_ingest_from_key_string (p5, "rolf")));
  OBSERT (slawx_equal (s[20], protein_ingest_from_key_string (p5, "torque")));
  OBSERT (slawx_equal_lc (protein_ingest_from_key (p5, s[15]), "quash"));
  OBSERT (slawx_equal_lc (protein_ingest_from_key (p5, s[17]), "sump"));
  OBSERT (slawx_equal_lc (protein_ingest_from_key (p5, s[19]), "urge"));
  OBSERT (NULL == protein_ingest_from_key (p5, s[16]));
  OBSERT (NULL == protein_ingest_from_key (p5, s[18]));
  OBSERT (NULL == protein_ingest_from_key (p5, s[20]));
  OBSERT (NULL == protein_ingest_from_key (p5, s[0]));
  OBSERT (NULL == protein_ingest_from_key (p5, snl));
  OBSERT (NULL == protein_ingest_from_key (p5, NULL));

  slop = slaw_list_emit_first (protein_descrips (p5));
  OBSERT (NULL != slop);
  OBSERT (slawx_equal_lc (slop, "mumps"));
  OBSERT (slawx_equal (s[12], slop));
  slop = slaw_list_emit_next (protein_descrips (p5), slop);
  OBSERT (NULL != slop);
  OBSERT (slawx_equal_lc (slop, "nape"));
  OBSERT (slawx_equal (s[13], slop));
  slop = slaw_list_emit_next (protein_descrips (p5), slop);
  OBSERT (NULL != slop);
  OBSERT (slawx_equal_lc (slop, "ooze"));
  OBSERT (slawx_equal (s[14], slop));
  slop = slaw_list_emit_next (protein_descrips (p5), slop);
  OBSERT (NULL == slop);
  slop = slaw_list_emit_first (protein_ingests (p5));
  OBSERT (NULL != slop);
  OBSERT (slawx_equal (slop, f (slaw_cons (s[15], s[16]))));
  OBSERT (!slawx_equal (slop, f (slaw_cons (s[16], s[15]))));
  slop = protein_next_ingest (p5, slop);
  OBSERT (NULL != slop);
  OBSERT (slawx_equal (slop, f (slaw_cons (s[17], s[18]))));
  OBSERT (!slawx_equal (slop, f (slaw_cons (s[18], s[17]))));
  slop = protein_next_ingest (p5, slop);
  OBSERT (NULL != slop);
  OBSERT (slawx_equal (slop, f (slaw_cons (s[19], s[20]))));
  OBSERT (!slawx_equal (slop, f (slaw_cons (s[20], s[19]))));
  slop = protein_next_ingest (p5, slop);
  OBSERT (NULL == slop);

  OBSERT (slawx_equal (s[12], slaw_list_emit_nth (protein_descrips (p5), 0)));
  OBSERT (slawx_equal (s[13], slaw_list_emit_nth (protein_descrips (p5), 1)));
  OBSERT (slawx_equal (s[14], slaw_list_emit_nth (protein_descrips (p5), 2)));
  OBSERT (NULL == slaw_list_emit_nth (protein_descrips (p5), 3));
  OBSERT (
    slawx_equal (protein_nth_ingest (p5, 0), f (slaw_cons (s[15], s[16]))));
  OBSERT (
    slawx_equal (protein_nth_ingest (p5, 1), f (slaw_cons (s[17], s[18]))));
  OBSERT (
    slawx_equal (protein_nth_ingest (p5, 2), f (slaw_cons (s[19], s[20]))));
  OBSERT (NULL == protein_nth_ingest (p5, 3));
  OBSERT (NULL == protein_nth_ingest (p5, -1));
  // end of used to be GROMP


  // free stuff to please valgrind
  int i;
  slaw_free (snl);
  for (i = 0; i < (sizeof (s) / sizeof (s[0])); i++)
    slaw_free (s[i]);
  protein_free (pn1);
  protein_free (pn2);
  protein_free (p1);
  protein_free (p2);
  protein_free (p3);
  protein_free (p4);
  protein_free (p5);
  protein_free (pp4);
  for (i = 0; i < free_counter; i++)
    slaw_free (to_be_freed[i]);

  return 0;
}
