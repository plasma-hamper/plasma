
/* (c)  oblong industries */

#ifndef OSSL_COMMON_COLD
#define OSSL_COMMON_COLD

#include "libLoam/c/ob-api.h"
#include "libLoam/c/ob-attrs.h"
#include "libLoam/c/ob-retorts.h"
#include "libLoam/c/ob-pthread.h"
#include "libLoam/c/ob-file.h"
#include <openssl/ssl.h>

#if OPENSSL_VERSION_NUMBER >= 0x10000000
typedef const SSL_METHOD *(method_func) (void);
#else
typedef SSL_METHOD *(method_func) (void);
#endif
typedef int (*conacc_func) (SSL *);

typedef struct pool_tls_info pool_tls_info;

int THREAD_setup (void);
int THREAD_cleanup (void);
void OREILLY_data_transfer (ob_sock_t A, SSL *B);
int OREILLY_verify_callback (int ok, X509_STORE_CTX *store);
bool ob_ossl_cert_matches_host (X509 *cert, const char *host);
long OREILLY_post_connection_check (SSL *ssl, const char *host, bool anon_ok);

ob_retort ob_ossl_create_context (method_func mfun, SSL_CTX **ctx_out);
ob_retort ob_ossl_launch_thread (ob_sock_t clear_sock, ob_sock_t cipher_sock,
                                 SSL_CTX *context, conacc_func cafunc,
                                 pthread_t *thr_out, bool auth_suites,
                                 bool anon_suites, bool client_auth_required,
                                 const char *host, const char *certificate,
                                 const char *private_key,
                                 pool_tls_info *tls_info);
ob_retort ob_ossl_join_thread (pthread_t thr);
char *ob_ossl_err_as_string (void);
const char *ob_ossl_interpretation_as_string (char *buf, size_t buf_len,
                                              int interpretation, int erryes);
ob_retort ob_ossl_setup_certs (SSL_CTX *ctx, const char *certificate_chain,
                               const char *private_key, int *bits_out);

extern int64 ob_tls_num_cert_authorities;

#endif /* OSSL_COMMON_COLD */
