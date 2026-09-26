
/* (c)  oblong industries */

// Derived from ssl/common.c in the sample code:
//
// http://examples.oreilly.com/9780596002701/NSwO-1.3.tar.gz
//
// for the book:
//
// Network Security with OpenSSL by John Viega, Matt Messier, & Pravir Chandra
// Copyright 2002 O'Reilly Media, Inc.  ISBN 978-0-596-00270-1
//
// http://shop.oreilly.com/category/customer-service/faq-examples.do

#include "ossl-common.h"
#include "libLoam/c/ob-log.h"
#include <openssl/x509v3.h>

#if OPENSSL_VERSION_NUMBER < 0x10002000L
#error "ob_ossl_cert_matches_host() needs X509_check_host(), OpenSSL >= 1.0.2"
#endif

int OREILLY_verify_callback (int ok, X509_STORE_CTX *store)
{
  char issuer[256], subject[256];

  if (!ok)
    {
      X509 *cert = X509_STORE_CTX_get_current_cert (store);
      int depth = X509_STORE_CTX_get_error_depth (store);
      int err = X509_STORE_CTX_get_error (store);

      X509_NAME_oneline (X509_get_issuer_name (cert), issuer, sizeof (issuer));
      X509_NAME_oneline (X509_get_subject_name (cert), subject,
                         sizeof (subject));
      OB_LOG_ERROR_CODE (0x20503000, "-Error with certificate at depth: %i\n"
                                     "  issuer   = %s\n"
                                     "  subject  = %s\n"
                                     "  err %i:%s\n",
                         depth, issuer, subject, err,
                         X509_verify_cert_error_string (err));
    }

  return ok;
}

// Does "cert" actually belong to "host"?
//
// X509_check_host() implements the name matching described by RFC 6125:
// it compares "host" against the certificate's subjectAltName dNSName
// entries, and consults the commonName only when the certificate has no
// subjectAltName at all.  (Which is the case for the certificates in
// bld/cmake/fixtures/tcps, so don't be tempted by
// X509_CHECK_FLAG_NEVER_CHECK_SUBJECT.)
//
// X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS accepts an ordinary wildcard
// certificate like "*.example.com", but not a partial one like
// "f*.example.com".
//
// "host" can also be an IPv4 literal, since pool URIs permit one; that's
// what X509_check_ip_asc() is for.  A bracketed IPv6 literal never gets
// this far, because parse_pseudo_uri() in pool_tcp.c can't parse one.
//
// Both functions return 1 on a match, 0 on a mismatch, and a negative
// number if they couldn't tell (an internal error, or an argument that
// isn't a well-formed name or address); only 1 means yes.
bool ob_ossl_cert_matches_host (X509 *cert, const char *host)
{
  const unsigned int flags = X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS;

  if (1 == X509_check_host (cert, host, 0, flags, NULL))
    return true;

  return (1 == X509_check_ip_asc (cert, host, 0));
}

long OREILLY_post_connection_check (SSL *ssl, const char *host, bool anon_ok)
{
  X509 *cert = SSL_get_peer_certificate (ssl);

  if (!cert)
    {
      if (anon_ok)
        return X509_V_OK;
      OB_LOG_ERROR_CODE (0x20503001,
                         "certificate is required, but %s doesn't have one\n",
                         host);
      return X509_V_ERR_APPLICATION_VERIFICATION;
    }

  if (!ob_ossl_cert_matches_host (cert, host))
    {
      char subject[256];
      X509_NAME_oneline (X509_get_subject_name (cert), subject,
                         sizeof (subject));
      OB_LOG_ERROR_CODE (0x20503002, "certificate does not identify '%s'\n"
                                     "  subject  = %s\n",
                         host, subject);
      X509_free (cert);
      return X509_V_ERR_APPLICATION_VERIFICATION;
    }

  X509_free (cert);
  return SSL_get_verify_result (ssl);
}
