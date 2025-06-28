
/* (c)  oblong industries */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ossl-common.h"
#include "libLoam/c/ob-rand.h"
#include "libLoam/c/ob-sys.h"
#include "libLoam/c/ob-time.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-string.h"
#include "libLoam/c/ob-dirs.h"
#include "libLoam/c/ob-util.h"
#include "libLoam/c/ob-vers.h"
#include "libLoam/c/ob-thread.h"
#include "libPlasma/c/plasma-retorts.h"
#include "libPlasma/c/private/pool-tls.h"
#include "libPlasma/c/private/pool_impl.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/engine.h>
#include <openssl/conf.h>

static const char dflt_authenticated_ciphers[] =
  /* 2017/11/21 We're following Mozilla's recommendation now:
   * https://wiki.mozilla.org/Security/Server_Side_TLS#Recommended_configurations
   * This includes some new ciphers, decommission of old ciphers and heavy
   * prioritization of ECC for performance. We're following the Mozilla's
   * recommendations for modern compatibility with some intermediate
   * compatibility thrown in. */
  /* 2015/09/29 We're stopping support for RC4 per RFC 7465 (because of the many
   * vulnerabilities that it faces): https://tools.ietf.org/html/rfc7465 */
  /*
   * -------------------- TLS 1.2 ciphersuites --------------------
   * Putting ECDHE cipher suites first, due to bug 10171 (basically,
   * DHE is too slow for iPhone).  Even though Bruce Schneier has
   * concerns about ECC in a post-Snowden world:
   * https://www.schneier.com/blog/archives/2013/09/the_nsa_is_brea.html#c1676105 */
  "ECDHE-ECDSA-AES256-GCM-SHA384"
  ":"
  "ECDHE-RSA-AES256-GCM-SHA384"
  ":"
  "ECDHE-ECDSA-CHACHA20-POLY1305"
  ":"
  "ECDHE-RSA-CHACHA20-POLY1305"
  ":"
  "ECDHE-ECDSA-AES128-GCM-SHA256"
  ":"
  "ECDHE-RSA-AES128-GCM-SHA256"
  ":"
  /* It looks like OpenJDK supports TLS 1.2, but not GCM.  So, for its
   * sake, let's throw in some CBC-based TLS 1.2-only ciphersuites.
   * http://icedtea.classpath.org/hg/icedtea8-forest/jdk/file/aa2c9ce06632/src/share/classes/sun/security/ssl/CipherSuite.java#l888
   */
  "ECDHE-ECDSA-AES256-SHA384"
  ":"
  "ECDHE-RSA-AES256-SHA384"
  ":"
  "ECDHE-ECDSA-AES128-SHA256"
  ":"
  "ECDHE-RSA-AES128-SHA256"
  ":"
  /* Mostly using AES-128, not AES-256.  Quoth the Bruce,
   * "That being said, the key schedule for AES-256 is very poor.
   * I would recommend that people use AES-128 and not AES-256."
   * http://www.schneier.com/blog/archives/2009/07/another_new_aes.html#c386957
   *
   * AES-128 GCM, using non-elliptic curve Diffie-Hellman, because it looks
   * like Red Hat has no plans of ever building OpenSSL with elliptic
   * curve support:
   * https://bugzilla.redhat.com/show_bug.cgi?id=319901
   */
  /* 2015/09/19 This is no longer the case, Redhat has re-enabled EC
   * https://access.redhat.com/documentation/en-US/Red_Hat_Enterprise_Linux/7/html/Security_Guide/sec-Hardening_TLS_Configuration.html
   * However, still leaving these in just in case there's concern about
   * the NSA cooking the curves */
  "DHE-RSA-AES128-GCM-SHA256"
  ":"
  /* Customer "TR" says they want AES-256, with RSA and Diffie-Hellman.
   * So, let's give it to 'em.  Here are two ciphersuites for them; one
   * with GCM and one with CBC. */
  "DHE-RSA-AES256-GCM-SHA384"
  ":"
  "DHE-RSA-AES256-SHA256"
#if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
  /* A security level too high for the certs in use generates errors like this:
   * error: <20500007> ... SSL routines:ssl_add_cert_chain:ee key too small:../ssl/ssl_cert.c:832:
   * See script bld/cmake/fixtures/tcps/generate.sh for how the unit tests certificates are generated.
   * We would like security level 4, but will use 3 while we ponder implications of raising it higher.
   * On second thought, let's use 2 for now, and advise developers to tighten up
   * their certs so we can raise it to 3 in the future.
   * See //www.openssl.org/docs/man1.1.0/ssl/SSL_CTX_set_security_level.html
   */
  /* Do it as data rather than explicit call to SSL_set_security_level */
  ":"
  "@SECLEVEL=2"
#endif
  ;

static const char dflt_anonymous_ciphers[] =
  /* Anonymous ciphersuites.  (Advantage: no certificates needed.
   * Disadvantage: susceptible to man-in-the-middle attacks.) */
  "ADH-AES128-GCM-SHA256"
  ":"
  /* -------------------- pre-1.2 ciphersuites -------------------- */
  "ADH-AES128-SHA"
  ":"
  "AECDH-AES128-SHA"
#if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
  /* The default level of 1 currently causes failure with anonymous ciphers, probably by design */
  /* Do it as data rather than explicit call to SSL_set_security_level */
  ":"
  "@SECLEVEL=0"
#endif
  ;

static ob_once_t ossl_common_once_control = OB_ONCE_INIT;

int64 ob_tls_num_cert_authorities = 0;

#define OB_CIPHER_SUITES "OB_CIPHER_SUITES"
#define OB_CIPHER_SUITES_ANON "OB_CIPHER_SUITES_ANON"

static const char *get_authenticated_ciphers (void)
{
  const char *ret = getenv (OB_CIPHER_SUITES);
  if (!ret)
    ret = dflt_authenticated_ciphers;
  return ret;
}

static const char *get_anonymous_ciphers (void)
{
  const char *ret = getenv (OB_CIPHER_SUITES_ANON);
  if (!ret)
    ret = dflt_anonymous_ciphers;
  return ret;
}

/* OpenSSL 1.1 changed the memory hook API to add caller info. */
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
#define FLARG
#define NOFLARG_PLEASE
#else
#define FLARG , const char *file, int line
#define NOFLARG_PLEASE                                                         \
  (void) file;                                                                 \
  (void) line
#endif

/* OpenSSL has a habit of using uninitialized memory.  (They turn up their
 * nose at tools like valgrind.)  To avoid spurious valgrind errors (as well
 * as to allay any concerns that the uninitialized memory is actually
 * affecting behavior), let's install a custom malloc function which is
 * actually calloc.
 */
static void *my_zeroing_malloc (size_t howmuch FLARG)
{
  NOFLARG_PLEASE;
  return calloc (1, howmuch);
}
static void *my_realloc (void *p, size_t howmuch FLARG)
{
  NOFLARG_PLEASE;
  return realloc (p, howmuch);
}
static void my_free (void *p FLARG)
{
  NOFLARG_PLEASE;
  free (p);
}

static bool threading_not_yet_initialized (void)
{
  const char *func = "CRYPTO_get_locking_callback";
  if (CRYPTO_get_locking_callback ())
    goto bad;
  func = "CRYPTO_get_add_lock_callback";
  if (CRYPTO_get_add_lock_callback ())
    goto bad;
  func = "CRYPTO_get_id_callback";
  if (CRYPTO_get_id_callback ())
    goto bad;
  func = "CRYPTO_get_dynlock_create_callback";
  if (CRYPTO_get_dynlock_create_callback ())
    goto bad;
  func = "CRYPTO_get_dynlock_lock_callback";
  if (CRYPTO_get_dynlock_lock_callback ())
    goto bad;
  func = "CRYPTO_get_dynlock_destroy_callback";
  if (CRYPTO_get_dynlock_destroy_callback ())
    goto bad;

  return true;

bad:
  OB_LOG_WARNING_CODE (0x2050001c,
                       "%s returned non-NULL, which suggests that something\n"
                       "besides libPlasma has configured threading callbacks\n"
                       "in OpenSSL.  This might work out okay, but if\n"
                       "things start getting hinky in the presence of\n"
                       "threads, this is probably why.\nhttps://lists.oblong."
                       "com/pipermail/buildtools/2013-January/000713.html\n",
                       func);
  return false;
}

static void constructor_time_init (void)
{
  if (1 != CRYPTO_set_mem_functions (my_zeroing_malloc, my_realloc, my_free))
    OB_LOG_INFO_CODE (0x20500017,
                      "CRYPTO_set_mem_functions failed.\n"
                      "This is not really anything to be worried about,\n"
                      "in itself, although it does indicate that\n"
                      "something besides libPlasma is also using OpenSSL,\n"
                      "which opens up the potential for some weird\n"
                      "interactions.\nhttps://lists.oblong.com/pipermail/"
                      "buildtools/2013-January/000713.html\n");
  if (threading_not_yet_initialized ())
    {
      if (1 != THREAD_setup ())
        OB_LOG_ERROR_CODE (0x2050001b,
                           "THREAD_setup failed (out of memory?) which is\n"
                           "probably rather bad!\n");
    }
}

/* Set up thread callbacks at constructor time (before main), rather
 * than waiting for first use (when global_init is called), in case
 * other things in this process are also using OpenSSL.  That way, we'll
 * provide a consistent set of thread callbacks for them, rather than
 * installing the callbacks after they might have already started using
 * OpenSSL.  See my rant which includes a paragraph about why OpenSSL's
 * thread safety mechanism sucks when using OpenSSL from multiple libraries:
 * https://web.archive.org/web/20121101024358/http://lists.randombit.net/pipermail/cryptography/2012-October/003380.html
 */
OB_PRE_POST (constructor_time_init (), THREAD_cleanup ());

static void transfer_entropy (void)
{
  unt64 wurdz[4];
  const unt64 start = ob_monotonic_time ();

  if (ob_truly_random (wurdz, sizeof (wurdz)) >= OB_OK)
    {
      // time is not very random, but might add a couple of bits
      wurdz[0] ^= start;
      wurdz[1] ^= ob_monotonic_time ();

      // Regardless of number of bytes, assume ob_truly_random() might
      // only have 128 bits of entropy.
      RAND_add (wurdz, sizeof (wurdz), 16.0);
    }
}

static void global_cleanup (void)
{
  ERR_free_strings ();
  RAND_cleanup ();
  EVP_cleanup ();
  // ugh; causes double frees: ENGINE_cleanup ();
  CONF_modules_free ();
  ERR_remove_state (0);
}

static void global_init (void)
{
  OB_LOG_DEBUG_CODE (0x20500000, "global_init\n");

  SSL_load_error_strings (); /* readable error messages */
  SSL_library_init ();       /* initialize library */

  /* Additional algorithms which are not needed for SSL itself, but
   * might be needed, say, to decrypt a password-protected private key.
   * For example, "pbeWithSHA1And3-KeyTripleDES-CBC". */
  OpenSSL_add_all_algorithms ();

  /* Calling this is supposed to be a good idea */
  OPENSSL_config (NULL);

  /* In particular, this will load the RdRand engine, if we are on
   * a processor with RdRand support.  That will give us random numbers
   * which are higher-quality (and perhaps faster?) */
  // seems like this is called by OPENSSL_config:
  // ENGINE_load_builtin_engines ();

  OB_LOG_DEBUG_CODE (0x20500001, "finished library init\n");

  /* If we're using a RAND engine, print it out.  (This will help
   * verify that we're using RdRand on machines where it's supported.) */
  ENGINE *eng = ENGINE_get_default_RAND ();
  if (eng)
    {
      OB_LOG_INFO_CODE (0x2050001d, "RAND engine is \"%s\"\n",
                        ENGINE_get_name (eng));
      ENGINE_free (eng); /* decrement reference count */
    }

  /* Technically, there's no real reason to free things at process
   * termination, but for some reason valgrind thinks OpenSSL's
   * error strings are "possibly lost", so let's free them for
   * valgrind's sake. */
  atexit (global_cleanup);
}

static ob_retort add_authorities (const char *file, va_list vargies)
{
  SSL_CTX *context = va_arg (vargies, SSL_CTX *);
  int ret = SSL_CTX_load_verify_locations (context, file, NULL);
  OB_LOG_INFO_CODE (0x20500018, "%s certificate authority from '%s'\n",
                    (ret ? "Loaded" : "Failed to load"), file);
  return (ret ? ob_tls_num_cert_authorities++, OB_OK : OB_UNKNOWN_ERR);
}

ob_retort ob_ossl_create_context (method_func mfun, SSL_CTX **ctx_out)
{
  const ob_retort tort = ob_once (&ossl_common_once_control, global_init);
  if (tort < OB_OK)
    return tort;

  // Prepare the context
  SSL_CTX *context = SSL_CTX_new (mfun ());
  if (context)
    {
      // clang-format off
      long options =
        SSL_OP_SINGLE_DH_USE
#if HAVE_OPENSSL_ECC
        | SSL_OP_SINGLE_ECDH_USE
#endif
#if 1
        /* Let's not use server preference anymore.
         * Here's the rationale: we'd like to prefer
         * DHE over ECDHE (due to lingering concerns
         * about NSA cooking the curve parameters),
         * but in the case of the Java client, we'd
         * like to prefer ECDHE over DHE, since old
         * versions of Java don't support DHE > 1024.
         * Therefore, since this depends on the client,
         * we should use the client's order. */
        /* 2015/09/29 Due to DHE being too slow on mobile
         * devices, we prefer ECDHE now, consistent with
         * Java 7. Best of all, "Java 8 allows
         * TLS servers to decide which is the best suite
         * to use from those supported by user agents"
         * http://blog.ivanristic.com/2014/03/ssl-tls-improvements-in-java-8.html*/
        | SSL_OP_CIPHER_SERVER_PREFERENCE
#endif
        | SSL_OP_NO_SSLv2
        | SSL_OP_NO_SSLv3
        | SSL_OP_NO_TLSv1
        | SSL_OP_NO_TLSv1_1
        ;
// clang-format on

#ifdef SSL_OP_NO_TLSv1_3
      /* (partially?) work around hang in openssl 1.1.1b.
      * https://github.com/openssl/openssl/issues/7967
      * https://gitlab.oblong.com/platform/yovo/issues/234
      *
      * Alternate approach: leave num_tickets alone, do
      *    options |= SSL_OP_NO_TLSv1_3;
      */

      options |= SSL_OP_NO_TICKET;
      SSL_CTX_set_num_tickets (context, 0);
#endif

      /* Disallow initial connection to unpatched server, we're confident that
       * our servers are patched (i.e. openssl after 0.9.8)
       * https://www.openssl.org/docs/manmaster/ssl/SSL_CTX_set_options.html */
      SSL_CTX_clear_options (context, SSL_OP_LEGACY_SERVER_CONNECT);
      SSL_CTX_set_options (context, options);

#if 0
      /* This is helpful for debugging the unhelpful error
       * "unable to get local issuer certificate", which could mean
       * almost anything.  But, unfortunately, this is a little too
       * verbose for normal use. */
      X509_VERIFY_PARAM *vp = X509_VERIFY_PARAM_new ();
      if (1 != X509_VERIFY_PARAM_set_flags (vp, X509_V_FLAG_CB_ISSUER_CHECK))
        OB_LOG_ERROR_CODE (0x20500019, "X509_VERIFY_PARAM_set_flags failed\n");
      if (1 != SSL_CTX_set1_param (context, vp))
        OB_LOG_ERROR_CODE (0x2050001a, "SSL_CTX_set1_param failed\n");
      X509_VERIFY_PARAM_free (vp);
#endif

      // Don't set cipher list here; we set it individually in ob_ossl_launch_thread
      // SSL_CTX_set_cipher_list (context, authenticated_ciphers);
      *ctx_out = context;
      OB_LOG_DEBUG_CODE (0x20500003, "created context successfully\n");
      return ob_search_standard_path (ob_etc_path,
                                      "certificate-authorities.pem", "RF",
                                      add_authorities, context);
    }

  OB_LOG_DEBUG_CODE (0x20500004, "not so successful in creating context\n");
  return OB_UNKNOWN_ERR;
}

// copies src to dst and collapses multiple spaces to a single space
static void shrink_spaces (char *dst, size_t dst_size, const char *src)
{
  ob_safe_copy_string (dst, dst_size, src);
  size_t i, j = 0;
  for (i = 0; dst[i]; i++)
    if (dst[i] == ' ')
      {
        dst[j] = ' ';
        if (j > 0 && dst[j - 1] != ' ')
          j++;
      }
    else
      dst[j++] = dst[i];
  dst[j] = 0;
}

// copies src to dst, and returns dst, and:
// converts 'v' to ' ', so "TLSv1.2" becomes "TLS 1.2"
// OpenSSL omits minor version when it is 0, so TLS 1.0 is somewhat oddly
// called "TLSv1".  Let's add the missing ".0"
static const char *mangle_version (char *dst, size_t dst_size, const char *src)
{
  ob_safe_copy_string (dst, dst_size, src);
  size_t i;
  bool dot = false;
  for (i = 0; dst[i]; i++)
    if (dst[i] == 'v')
      dst[i] = ' ';
    else if (dst[i] == '.')
      dot = true;
  if (!dot)
    ob_safe_append_string (dst, dst_size, ".0");
  return dst;
}

typedef struct
{
  int A;
  SSL *B;
  conacc_func f;
  char host[635]; /* max host name length according to RFC 1123 */
  bool anon_ok;
} thread_args;

#ifndef _MSC_VER
/* http://www.mail-archive.com/openssl-dev@openssl.org/msg24923.html
 *
 * Although OpenSSL has fixed this bug, the version of OpenSSL that comes
 * with Ubuntu 10.04 (even with all updates applied, as of mid-2012) still
 * seems to have the bug.  Therefore we need to work around it.
 *
 * The issue is that the SSL_CTX is supposed to be read-only, and therefore
 * shareable by multiple threads.  But, buggily, there is code in OpenSSL
 * that tries to sort a list inside the SSL_CTX when looking something up.
 * Once the list is sorted, the SSL_CTX is shareable as it is supposed to
 * be.  But if multiple threads race into that initial sort, Bad Things
 * happen.  (I have seen this myself with pingpong_test on Ubuntu 10.04.)
 *
 * So, my workaround is to use a mutex to make sure the call to
 * SSL_connect is single-threaded the first time it is called.  However,
 * once the first SSL_connect call has completed, we can assume the list
 * is sorted, and future SSL_connect calls can happen in parallel.
 * Thus, the two different places in the code below where the mutex
 * can be released: one before the call to SSL_connect (aka args.f)
 * and one after.
 */
static pthread_mutex_t work_around_ossl_bug_1795 = PTHREAD_MUTEX_INITIALIZER;
static bool have_connected_before = false;
#endif

static void *ossl_common_thread_main (void *v)
{
  thread_args *args_ptr = (thread_args *) v;
  thread_args args = *args_ptr;
  free (args_ptr);
  unt64 t[4];
  t[0] = ob_monotonic_time ();
  OB_LOG_DEBUG_CODE (0x20500005, "started new thread\n");
  transfer_entropy ();
  t[1] = ob_monotonic_time ();
  OB_LOG_DEBUG_CODE (0x20500006, "entropy transfer took %" OB_FMT_64 "u ns\n"
                                 "performing handshake\n",
                     t[1] - t[0]);
  long err509;
  long ssl_error = 0;
#ifndef _MSC_VER
  pthread_mutex_lock (&work_around_ossl_bug_1795);
  bool mutex_is_locked = true;
  if (have_connected_before)
    {
      // We only need to single-thread the first call.  After the connection
      // function has been called once, future calls can be concurrent.
      pthread_mutex_unlock (&work_around_ossl_bug_1795);
      mutex_is_locked = false;
    }
#endif
  int ret = args.f (args.B);
  const int erryes = errno;
#ifndef _MSC_VER
  if (mutex_is_locked)
    {
      have_connected_before = true;
      pthread_mutex_unlock (&work_around_ossl_bug_1795);
    }
#endif
  if (ret != 1)
    {
      const int interpretation = SSL_get_error (args.B, ret);
      char *errstack = ob_ossl_err_as_string ();
      char buf[160];
      OB_LOG_ERROR_CODE (0x20500007, "SSL handshake error!\n"
                                     "ret was %d, interpretation was %s\n"
                                     "%s",
                         ret,
                         ob_ossl_interpretation_as_string (buf, sizeof (buf),
                                                           interpretation,
                                                           erryes),
                         errstack);
      free (errstack);
      t[2] = erryes;
      t[3] = interpretation;
      ssl_error = interpretation;
    }
  else if (args.host[0]
           && X509_V_OK
                != (err509 = OREILLY_post_connection_check (args.B, args.host,
                                                            args.anon_ok)))
    {
      OB_LOG_ERROR_CODE (0x20500010, "Unhappy about peer certificate: '%s'\n",
                         X509_verify_cert_error_string (err509));
      ssl_error = SSL_ERROR_SSL;
    }
  else
    {
      char buf[160], fub[160], vers[32];
      shrink_spaces (fub, sizeof (fub),
                     SSL_CIPHER_description (SSL_get_current_cipher (args.B),
                                             buf, sizeof (buf)));
      OB_LOG_INFO_CODE (0x2050000f,
                        "Connected securely using %s with cipher suite:\n%s",
                        mangle_version (vers, sizeof (vers),
                                        SSL_get_version (args.B)),
                        fub);
      t[2] = ob_monotonic_time ();
      OB_LOG_DEBUG_CODE (0x20500008, "handshake took %" OB_FMT_64 "u ns\n"
                                     "starting data transfer\n",
                         t[2] - t[1]);
      OREILLY_data_transfer (args.A, args.B);
      t[3] = ob_monotonic_time ();
      OB_LOG_DEBUG_CODE (0x20500009, "data transfer took %" OB_FMT_64 "u ns\n"
                                     "cleaning up thread-local data\n",
                         t[3] - t[2]);
    }

  // free and close all the resources
  const int b_fd = SSL_get_fd (args.B);
  if (0 == SSL_shutdown (args.B))
    // yeah, need to do it twice; see SSL_shutdown man page
    SSL_shutdown (args.B);
  SSL_free (args.B);

  ob_retort ort = ob_close_socket (b_fd);
  if (ort < OB_OK)
    OB_LOG_ERROR_CODE (0x2050000d, "error closing socket in TLS thread:\n%s\n",
                       ob_error_string (ort));

  ort = ob_close_socket (args.A);
  if (ort < OB_OK)
    OB_LOG_ERROR_CODE (0x2050000e, "error closing socket in TLS thread:\n%s\n",
                       ob_error_string (ort));

  // Since we have these four nice timestamps, let's use them as entropy,
  // although don't claim they provide very much entropy.
  RAND_add (t, sizeof (t), 0.1);

  // clean up OpenSSL's thread local state (ha ha, TLS double entendre)
  // 0 means current thread
  ERR_remove_state (0);
  OB_LOG_DEBUG_CODE (0x2050000a, "exiting thread with %ld\n", ssl_error);

  if (ssl_error == SSL_ERROR_SSL)
    return (void *) ssl_error;
  return NULL;
}

#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
#define SSL_CTX_get_default_passwd_cb(ctx) ctx->default_passwd_callback
#define SSL_CTX_get_default_passwd_cb_userdata(ctx)                            \
  ctx->default_passwd_callback_userdata
#endif

static X509 *string_to_x509 (const char *s, SSL_CTX *context)
{
  BIO *bio = BIO_new_mem_buf ((void *) s, -1);
  if (!bio)
    return NULL;
  /* XXX: Anybody understand the difference between PEM_read_bio_X509()
   * and PEM_read_bio_X509_AUX()?  I do not.  Which one should we use?
   * Thanks a lot, OpenSSL (lack of) documentation! */
  X509 *x =
    PEM_read_bio_X509_AUX (bio, NULL, SSL_CTX_get_default_passwd_cb (context),
                           SSL_CTX_get_default_passwd_cb_userdata (context));
  BIO_free (bio);
  return x;
}

static EVP_PKEY *string_to_pkey (const char *s, SSL_CTX *context)
{
  BIO *bio = BIO_new_mem_buf ((void *) s, -1);
  if (!bio)
    return NULL;
  EVP_PKEY *k =
    PEM_read_bio_PrivateKey (bio, NULL, SSL_CTX_get_default_passwd_cb (context),
                             SSL_CTX_get_default_passwd_cb_userdata (context));
  BIO_free (bio);
  return k;
}

ob_retort ob_ossl_launch_thread (int clear_sock, int cipher_sock,
                                 SSL_CTX *context, conacc_func cafunc,
                                 pthread_t *thr_out, bool auth_suites,
                                 bool anon_suites, bool client_auth_required,
                                 const char *host, const char *certificate,
                                 const char *private_key)
{
  thread_args *args = (thread_args *) calloc (1, sizeof (thread_args));
  if (!args)
    return OB_NO_MEM;

  OB_LOG_DEBUG_CODE (0x2050000b, "creating a new SSL\n");
  SSL *ssl = SSL_new (context);
  if (!ssl || 0 == SSL_set_fd (ssl, cipher_sock))
    {
      char *errstack = ob_ossl_err_as_string ();
      OB_LOG_ERROR_CODE (0x20500011, "failure in %s:\n%s",
                         ssl ? "SSL_set_fd" : "SSL_new", errstack);
      free (errstack);
      if (ssl)
        SSL_free (ssl);
      free (args);
      return POOL_TLS_ERROR;
    }
  OB_LOG_DEBUG_CODE (0x2050000c, "finished setting the socket\n");

  if (certificate && private_key)
    {
      const char *what = NULL;
      X509 *cert = NULL;
      EVP_PKEY *pkey = NULL;
      if (!((what = "certificate", cert = string_to_x509 (certificate, context))
            && (what = "private key",
                pkey = string_to_pkey (private_key, context))))
        {
          char *errstack = ob_ossl_err_as_string ();
          OB_LOG_ERROR_CODE (0x20500016, "unable to parse %s:\n%s", what,
                             errstack);
          free (errstack);
          X509_free (cert);
          EVP_PKEY_free (pkey);
          SSL_free (ssl);
          free (args);
          return POOL_TLS_ERROR;
        }
      // do something with them
      if (1 != SSL_use_certificate (ssl, cert)
          || 1 != SSL_use_PrivateKey (ssl, pkey)
          || 1 != SSL_check_private_key (ssl))
        {
          char *errstack = ob_ossl_err_as_string ();
          OB_LOG_ERROR_CODE (0x20500015,
                             "certificate unhappiness of this flavor:\n%s",
                             errstack);
          free (errstack);
          X509_free (cert);
          EVP_PKEY_free (pkey);
          SSL_free (ssl);
          free (args);
          return POOL_TLS_ERROR;
        }
      // now free them
      X509_free (cert);
      EVP_PKEY_free (pkey);
    }

  if (client_auth_required)
    SSL_set_verify (ssl, (SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT),
                    OREILLY_verify_callback);

  const char *authenticated_ciphers = get_authenticated_ciphers ();
  const char *anonymous_ciphers = get_anonymous_ciphers ();

  size_t suites_sz =
    2 + strlen (authenticated_ciphers) + strlen (anonymous_ciphers);

  char *suites = (char *) calloc (suites_sz, 1);
  if (!suites)
    {
      SSL_free (ssl);
      free (args);
      return OB_NO_MEM;
    }

  if (auth_suites)
    ob_safe_append_string (suites, suites_sz, authenticated_ciphers);
  if (auth_suites && anon_suites)
    ob_safe_append_string (suites, suites_sz, ":");
  if (anon_suites)
    ob_safe_append_string (suites, suites_sz, anonymous_ciphers);

  if (0 == SSL_set_cipher_list (ssl, suites))
    {
      char *errstack = ob_ossl_err_as_string ();
      OB_LOG_ERROR_CODE (0x20500012,
                         "SSL_set_cipher_list on '%s' returned failure\n%s",
                         suites, errstack);
      free (errstack);
      free (suites);
      SSL_free (ssl);
      free (args);
      return POOL_TLS_ERROR;
    }

  free (suites);

  args->A = clear_sock;
  args->B = ssl;
  args->f = cafunc;
  args->anon_ok = anon_suites;
  if (host)
    ob_safe_copy_string (args->host, sizeof (args->host), host);

  const int ptc_ret = pthread_create (thr_out, NULL, ossl_common_thread_main, args);
  if (ptc_ret != 0)
    return ob_errno_to_retort (ptc_ret);
  return OB_OK;
}

ob_retort ob_ossl_join_thread (pthread_t thr)
{
  void *status;

  const int ptj_ret = pthread_join (thr, &status);
  if (ptj_ret != 0)
    return ob_errno_to_retort (ptj_ret);

  if (status)
    return POOL_TLS_ERROR;

  return OB_OK;
}

char *ob_ossl_err_as_string (void)
{
  BIO *bio = BIO_new (BIO_s_mem ());
  ERR_print_errors (bio);
  char *buf = NULL;
  size_t len = BIO_get_mem_data (bio, &buf);
  char *ret = (char *) calloc (1, 1 + len);
  if (ret)
    memcpy (ret, buf, len);
  BIO_free (bio);
  return ret;
}

const char *ob_ossl_interpretation_as_string (char *buf, size_t buf_len,
                                              int interpretation, int erryes)
{
  switch (interpretation)
    {
      case SSL_ERROR_NONE:
        return "SSL_ERROR_NONE";
      case SSL_ERROR_SSL:
        return "SSL_ERROR_SSL";
      case SSL_ERROR_WANT_READ:
        return "SSL_ERROR_WANT_READ";
      case SSL_ERROR_WANT_WRITE:
        return "SSL_ERROR_WANT_WRITE";
      case SSL_ERROR_WANT_X509_LOOKUP:
        return "SSL_ERROR_WANT_X509_LOOKUP";
      case SSL_ERROR_SYSCALL:
        snprintf (buf, buf_len, "SSL_ERROR_SYSCALL (errno = %d \"%s\")", erryes,
                  strerror (erryes));
        return buf;
      case SSL_ERROR_ZERO_RETURN:
        return "SSL_ERROR_ZERO_RETURN";
      case SSL_ERROR_WANT_CONNECT:
        return "SSL_ERROR_WANT_CONNECT";
      case SSL_ERROR_WANT_ACCEPT:
        return "SSL_ERROR_WANT_ACCEPT";
      default:
        snprintf (buf, buf_len, "unknown interpretation %d", interpretation);
        return buf;
    }
}

ob_retort ob_tls_banner (FILE *where)
{
  const char *build_version = OPENSSL_VERSION_TEXT;
  const char *run_version = SSLeay_version (SSLEAY_VERSION);

#if HAVE_OPENSSL_ECC
  const char *ecc = "[ECC=yes]";
#else
  const char *ecc = "[ECC=no]";
#endif

  ob_banner (where);
  fprintf (where, "TLS support provided by: ");

  if (0 == strcmp (build_version, run_version))
    fprintf (where, "%s %s\n", run_version, ecc);
  else
    fprintf (where, "%s (build-time) %s\n"
                    "                         %s (run-time)\n",
             build_version, ecc, run_version);

  fprintf (where,
           "\nCertificate-based ciphersuites can be customized "
           "with " OB_CIPHER_SUITES " environment variable.\nCurrently: %s\n",
           get_authenticated_ciphers ());
  fprintf (where, "\nAnonymous ciphersuites can be customized "
                  "with " OB_CIPHER_SUITES_ANON
                  " environment variable.\nCurrently: %s\n",
           get_anonymous_ciphers ());

  return OB_OK;
}

static void customize_prompt (const char *filename)
{
  char buf[80];
  snprintf (buf, sizeof (buf), "Enter pass phrase for '%s':", filename);
  EVP_set_pw_prompt (buf);
}

ob_retort ob_ossl_setup_certs (SSL_CTX *ctx, const char *certificate_chain,
                               const char *private_key, int *bits_out)
{
  char *errstack;
  int ret;
  BIO *bio;
  EVP_PKEY *pkey;
  const char *badfunc;
  const char *badfile;

  OB_LOG_INFO_CODE (0x20500013, "Loading certificate chain from '%s'\n"
                                "and private key from '%s'\n",
                    certificate_chain, private_key);

  badfunc = "SSL_CTX_use_certificate_chain_file";
  badfile = certificate_chain;
  if (1 != SSL_CTX_use_certificate_chain_file (ctx, certificate_chain))
    goto bad;

  badfunc = "BIO_new_file";
  badfile = private_key;
  bio = BIO_new_file (private_key, "r");
  if (!bio)
    goto bad;

  badfunc = "PEM_read_bio_PrivateKey";
  badfile = private_key;
  customize_prompt (private_key);
  pkey =
    PEM_read_bio_PrivateKey (bio, NULL, SSL_CTX_get_default_passwd_cb (ctx),
                             SSL_CTX_get_default_passwd_cb_userdata (ctx));
  EVP_set_pw_prompt (NULL);
  BIO_free (bio);
  if (!pkey)
    goto bad;

  /* for non-RSA keys, bits_out remains unchanged */
  if (EVP_PKEY_RSA == EVP_PKEY_id (pkey))
    {
      RSA *rsa = EVP_PKEY_get1_RSA (pkey);
      /* because RSA_size is in bytes! */
      *bits_out = 8 * RSA_size (rsa);
      RSA_free (rsa);
    }

  badfunc = "SSL_CTX_use_PrivateKey";
  badfile = private_key;
  ret = SSL_CTX_use_PrivateKey (ctx, pkey);
  EVP_PKEY_free (pkey);

  if (1 != ret)
    goto bad;

  badfunc = "SSL_CTX_check_private_key";
  badfile = private_key;
  if (1 != SSL_CTX_check_private_key (ctx))
    {
    bad:
      errstack = ob_ossl_err_as_string ();
      OB_LOG_ERROR_CODE (0x20500014, "certificate unhappiness in %s on %s:\n%s",
                         badfunc, badfile, errstack);
      free (errstack);
      return POOL_ANONYMOUS_ONLY;
    }

  return OB_OK;
}
