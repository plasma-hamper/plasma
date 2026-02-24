
/* (c)  oblong industries */

/**************************************************
 * static data in slaw-concat not threadsafe
 * Most of the functions in slaw-concat.c look something like this:
 * 
 * \code
 * slaw slaw_lists_concat_preservingly (slaw s1, ...)
 * { if (! s1  ||  ! slaw_is_list (s1))
 *     return NULL;
 *   va_list vargies;
 *   static slabu *sb = NULL;
 *   if (! sb)
 *     if (! (sb = slabu_new ()))
 *       return NULL;
 *   slabu_disown_contents (sb);
 * 
 *   slaw s, sl;
 *   va_start (vargies, s1);
 *   sl = s1;
 *   do
 *     { if (slaw_is_list (sl))
 *         { int q, num;
 *           s = NULL;
 *           for (num=slaw_list_count(sl), q=0  ;  q<num  ;  q++)
 *             if (s = slaw_list_emit_next (sl, s))
 *               slabu_add (sb, s);
 *         }
 *       sl = va_arg (vargies, slaw);
 *     }
 *   while (sl);
 *   va_end (vargies);
 * 
 *   return slaw_list_preservingly (sb);
 * }
 * \endcode
 * 
 * Basically, it keeps around a single slabu for use in all future invocations
 * of the function. The problem is that if multiple threads try to call
 * slaw_lists_concat_preservingly at once, they will both be using the same
 * slabu, and horrible badness will ensue.
 * 
 * This technique is also used in protein_from_monoslaw and
 * protein_from_monoslaw_preservingly in protein.c, and in the
 * slaw_##type##_arrays_concat and slaw_##type##_arrays_concat_preservingly
 * template functions macros in slaw-numerics.c.
 * 
 * This test should demonstrate this technique failing in a multithreaded
 * environment.
 **************************************************/

#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-sys.h"
#include "libLoam/c/ob-pthread.h"
#include "libLoam/c/ob-util.h"
#include "libPlasma/c/protein.h"
#include "libPlasma/c/slaw.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define ITERATIONS 4

static void *thread_main (void *ignored)
{
  int i, j;
  bslaw cole;

  const int SIZE = (ob_running_under_valgrind () ? 150 : 1500);

  for (i = 0; i < ITERATIONS; i++)
    {
      slaw lst = slaw_list_f (slabu_new ()); /* empty list */

      /* create a list of SIZE integers in sequence */
      for (j = 0; j < SIZE; j++)
        {
          slaw num = slaw_int32 (j);
          slabu *sb = slabu_new ();
          slaw nxt;

          OB_DIE_ON_ERROR (slabu_list_add_x (sb, num));
          nxt = slaw_list_f (sb);
          lst = slaw_lists_concat_f (lst, nxt, NULL);
        }

      /* now check the list */
      j = 0;
      for (cole = slaw_list_emit_first (lst); cole != NULL;
           cole = slaw_list_emit_next (lst, cole))
        {
          const int32 *p = slaw_int32_emit (cole);
          int32 val = *p;
          if (val != j)
            {
              fprintf (stderr, "On iteration %d, expected %d but got %d\n", i,
                       j, val);
              exit (EXIT_FAILURE);
            }
          // check alignment
          unt64 u = (unt64) (unsigned long) p;
          if (u % 4 != 0)
            OB_FATAL_ERROR_CODE (0x20304000,
                                 "%" OB_FMT_64 "x is not 4-byte aligned\n", u);
          j++;
        }

      if (j != SIZE)
        {
          fprintf (stderr,
                   "On iteration %d, expected list length to be %d "
                   "but it was %d\n",
                   i, SIZE, j);
          exit (EXIT_FAILURE);
        }

      slaw_free (lst);
    }

  return NULL;
}

int main (int argc, char **argv)
{
  int nthreads;

  if (argc != 2 || (nthreads = atoi (argv[1])) < 1)
    {
      fprintf (stderr, "Usage: %s nthreads\n", argv[0]);
      return EXIT_FAILURE;
    }
  else
    {
      int i;
      pthread_t *threads = (pthread_t *) malloc (nthreads * sizeof (pthread_t));

      for (i = 0; i < nthreads; i++)
        {
          int erryes = pthread_create (&threads[i], NULL, thread_main, NULL);
          if (erryes)
            error_exit ("pthread_create[%d]: %s\n", i, strerror (erryes));
        }

      for (i = 0; i < nthreads; i++)
        {
          int erryes = pthread_join (threads[i], NULL);
          if (erryes)
            error_exit ("pthread_join[%d]: %s\n", i, strerror (erryes));
        }

      free (threads);
    }
  return EXIT_SUCCESS;
}
