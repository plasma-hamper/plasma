
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
      OB_LOG_ERROR_CODE (0x20503000,
                         "-Error with certificate at depth: %i\n"
                         "  issuer   = %s\n"
                         "  subject  = %s\n"
                         "  err %i:%s\n",
                         depth, issuer, subject, err,
                         X509_verify_cert_error_string (err));
    }

  return ok;
}

long OREILLY_post_connection_check (SSL *ssl, const char *host, bool anon_ok)
{
  X509 *cert;
  X509_NAME *subj;
  char buf[256];
  int extcount;
  int ok = 0;

  if (!(cert = SSL_get_peer_certificate (ssl)))
    {
      if (anon_ok)
        return X509_V_OK;
      OB_LOG_ERROR_CODE (
        0x20503001, "certificate is required, but %s doesn't have one\n", host);
      goto err_occurred;
    }
  if ((extcount = X509_get_ext_count (cert)) > 0)
    {
      int i;

      for (i = 0; i < extcount; i++)
        {
          char *extstr;
          X509_EXTENSION *ext;

          ext = X509_get_ext (cert, i);
          extstr =
            (char *) OBJ_nid2sn (OBJ_obj2nid (X509_EXTENSION_get_object (ext)));

          if (!strcmp (extstr, "subjectAltName"))
            {
              int j;
#if (OPENSSL_VERSION_NUMBER >= 0x00908000L)
              const
#endif
                unsigned char *data;
              STACK_OF (CONF_VALUE) * val;
              CONF_VALUE *nval;
#if (OPENSSL_VERSION_NUMBER >= 0x10000000L)
              const
#endif
                X509V3_EXT_METHOD *meth;
              void *ext_str = NULL;

              if (!(meth = X509V3_EXT_get (ext)))
                break;
              data = X509_EXTENSION_get_data (ext)->data;

#if (OPENSSL_VERSION_NUMBER > 0x00907000L)
              if (meth->it)
                ext_str = ASN1_item_d2i (NULL, &data,
                                         X509_EXTENSION_get_data (ext)->length,
                                         ASN1_ITEM_ptr (meth->it));
              else
                ext_str = meth->d2i (NULL, &data,
                                     X509_EXTENSION_get_data (ext)->length);
#else
              ext_str =
                meth->d2i (NULL, &data, X509_EXTENSION_get_data (ext)->length);
#endif
              val = meth->i2v (meth, ext_str, NULL);
              for (j = 0; j < sk_CONF_VALUE_num (val); j++)
                {
                  nval = sk_CONF_VALUE_value (val, j);
                  if (!strcmp (nval->name, "DNS")
                      || !strcmp (nval->name, "IP Address"))
                    {
                      if (!strcmp (nval->value, host))
                        {
                          ok = 1;
                          break;
                        }
                    }
                }
            }
          if (ok)
            break;
        }
    }

  if (!ok && (subj = X509_get_subject_name (cert))
      && X509_NAME_get_text_by_NID (subj, NID_commonName, buf, sizeof (buf))
           > 0)
    {
      buf[sizeof (buf) - 1] = 0;
      if (strcasecmp (buf, host) != 0)
        {
          OB_LOG_ERROR_CODE (0x20503002, "'%s' is not '%s'\n", buf, host);
          goto err_occurred;
        }
    }

  X509_free (cert);
  return SSL_get_verify_result (ssl);

err_occurred:
  if (cert)
    X509_free (cert);
  return X509_V_ERR_APPLICATION_VERIFICATION;
}
