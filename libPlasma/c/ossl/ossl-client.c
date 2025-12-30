
/* (c)  oblong industries */

#include "ossl-common.h"
#include "ossl-eintr-helper.h"
#include "libLoam/c/ob-retorts.h"
#include "libLoam/c/ob-dirs.h"
#include "libLoam/c/ob-thread.h"
#include "libPlasma/c/private/pool-tls.h"
#include "libPlasma/c/private/pool_impl.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/opensslv.h>

static SSL_CTX *ossl_client_context;
static ob_retort ossl_client_init_client_retort = OB_UNKNOWN_ERR;
static ob_once_t ossl_client_once_control = OB_ONCE_INIT;

static void init_client_context (void)
{
  ossl_client_init_client_retort =
    ob_ossl_create_context (SSLv23_client_method, &ossl_client_context);
  if (ossl_client_init_client_retort >= OB_OK)
    {
      if (ob_tls_num_cert_authorities > 0)
        SSL_CTX_set_verify (ossl_client_context, SSL_VERIFY_PEER,
                            OREILLY_verify_callback);
      else
        ossl_client_init_client_retort = POOL_ANONYMOUS_ONLY;

      char *certificate_chain = ob_resolve_standard_path (
        ob_etc_path, "client-certificate-chain.pem", "RF");
      char *private_key =
        ob_resolve_standard_path (ob_etc_path, "client-private-key.pem", "RF");
      if (certificate_chain && private_key)
        {
          int dmy; /* unused */
          ob_retort tort = ob_ossl_setup_certs (
            ossl_client_context, certificate_chain, private_key, &dmy);
          if (tort < OB_OK)
            ossl_client_init_client_retort = tort;
        }
      free (private_key);
      free (certificate_chain);
    }
}

ob_retort ob_tls_client_available (void)
{
  const ob_retort tort =
    ob_once (&ossl_client_once_control, init_client_context);
  if (tort < OB_OK)
    return tort;
  return ossl_client_init_client_retort;
}

ob_retort ob_tls_client_launch_thread (int clear_sock, int cipher_sock,
                                       pthread_t *thr_out, const char *host,
                                       bool anon_ok, const char *certificate,
                                       const char *private_key)
{
  ob_retort tort = ob_tls_client_available ();
  if (tort < OB_OK)
    return tort;

  return ob_ossl_launch_thread (clear_sock, cipher_sock, ossl_client_context,
                                EINTR_SSL_connect_harder, thr_out,
                                (ob_tls_num_cert_authorities > 0), anon_ok,
                                false, host, certificate, private_key);
}

ob_retort ob_tls_client_join_thread (pthread_t thr)
{
  return ob_ossl_join_thread (thr);
}
