
/* (c)  oblong industries */

#include "libPlasma/c/private/pool-tls.h"
#include "libPlasma/c/plasma-retorts.h"
#include "libLoam/c/ob-vers.h"

ob_retort ob_tls_banner (FILE *where)
{
  ob_banner (where);
  fprintf (where, "This version of plasma was built without TLS support.\n");
  return POOL_NO_TLS;
}

ob_retort ob_tls_server_available (void)
{
  return POOL_NO_TLS;
}

ob_retort ob_tls_server_launch_thread (int clear_sock, int cipher_sock,
                                       pthread_t *thr_out, bool anon_ok,
                                       bool client_auth_required)
{
  return POOL_NO_TLS;
}

ob_retort ob_tls_server_join_thread (pthread_t thr)
{
  return POOL_NO_TLS;
}

ob_retort ob_tls_client_available (void)
{
  return POOL_NO_TLS;
}

ob_retort ob_tls_client_launch_thread (int clear_sock, int cipher_sock,
                                       pthread_t *thr_out, const char *host,
                                       bool anon_ok, const char *certificate,
                                       const char *private_key)
{
  return POOL_NO_TLS;
}

ob_retort ob_tls_client_join_thread (pthread_t thr)
{
  return POOL_NO_TLS;
}
