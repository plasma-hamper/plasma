/* (c)  oblong industries */

// Tests ob_ossl_cert_matches_host(), which decides whether the
// certificate a TLS server presented actually belongs to the host we
// thought we were connecting to.
//
// The certificates here are never signed, and have no keys, because
// nothing in the matching looks at a signature -- only at the
// commonName and the subjectAltName.  That keeps this test free of
// checked-in .pem files, which would eventually expire, and free of
// key generation, which would make it slow.

#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-vers.h"
#include "libPlasma/c/ossl/ossl-common.h"

#include <openssl/x509v3.h>

#include <stdlib.h>

// Make a certificate with the given commonName and subjectAltName;
// either may be NULL to leave it out.  "san" is in the syntax
// X509V3_EXT_conf_nid() expects, e. g. "DNS:example.com".
static X509 *make_cert (const char *cn, const char *san)
{
  X509 *cert = X509_new ();
  if (!cert)
    OB_FATAL_ERROR_CODE (0x2031c000, "X509_new() failed\n");

  // Version 3, since only a v3 certificate can carry extensions.
  X509_set_version (cert, 2);

  if (cn)
    {
      X509_NAME *subj = X509_get_subject_name (cert);
      if (1 != X509_NAME_add_entry_by_txt (subj, "CN", MBSTRING_ASC,
                                           (const unsigned char *) cn, -1, -1,
                                           0))
        OB_FATAL_ERROR_CODE (0x2031c001, "couldn't set commonName '%s'\n", cn);
    }

  if (san)
    {
      X509_EXTENSION *ext =
        X509V3_EXT_conf_nid (NULL, NULL, NID_subject_alt_name, san);
      if (!ext)
        OB_FATAL_ERROR_CODE (0x2031c002, "couldn't make subjectAltName '%s'\n",
                             san);
      if (1 != X509_add_ext (cert, ext, -1))
        OB_FATAL_ERROR_CODE (0x2031c003, "couldn't add subjectAltName '%s'\n",
                             san);
      X509_EXTENSION_free (ext);
    }

  return cert;
}

static void check (const char *cn, const char *san, const char *host,
                   bool expected)
{
  X509 *cert = make_cert (cn, san);
  const bool got = ob_ossl_cert_matches_host (cert, host);

  if (got != expected)
    OB_FATAL_ERROR_CODE (0x2031c004,
                         "certificate with commonName %s%s%s "
                         "and subjectAltName %s%s%s\n"
                         "was %s for host '%s', but should have been %s\n",
                         cn ? "'" : "", cn ? cn : "(none)", cn ? "'" : "",
                         san ? "'" : "", san ? san : "(none)", san ? "'" : "",
                         got ? "accepted" : "rejected", host,
                         expected ? "accepted" : "rejected");

  X509_free (cert);
}

int main (int argc, char **argv)
{
  OB_DIE_ON_ERROR (OB_CHECK_ABI ());

  // No subjectAltName, so the commonName is used.  This is the shape of
  // the certificates in bld/cmake/fixtures/tcps, so these cases are what
  // keep the "tcps" test fixture working.
  check ("localhost", NULL, "localhost", true);
  check ("localhost", NULL, "example.com", false);
  // Host names are not case sensitive.
  check ("LOCALHOST", NULL, "localhost", true);

  // A certificate with a subjectAltName that doesn't match, and no
  // commonName to fall back on, used to be accepted for any host at all.
  // That was the bug this test exists for.
  check (NULL, "DNS:example.com", "localhost", false);
  check (NULL, "DNS:example.com", "example.com", true);

  // A certificate that identifies nobody identifies nobody.
  check (NULL, NULL, "localhost", false);

  // RFC 6125: once a certificate has a subjectAltName, the commonName
  // is not to be consulted, even though it would have matched.
  check ("localhost", "DNS:example.com", "localhost", false);

  // Pool URIs may name a host by IPv4 address.
  check (NULL, "IP:127.0.0.1", "127.0.0.1", true);
  check (NULL, "IP:127.0.0.1", "127.0.0.2", false);
  check ("127.0.0.1", NULL, "127.0.0.1", true);

  // Ordinary wildcards match one label, and only one.
  check (NULL, "DNS:*.example.com", "a.example.com", true);
  check (NULL, "DNS:*.example.com", "example.com", false);
  check (NULL, "DNS:*.example.com", "a.b.example.com", false);

  // Partial wildcards are a misfeature, and we don't accept them.
  check (NULL, "DNS:f*.example.com", "foo.example.com", false);

  // One certificate can name several hosts.
  check (NULL, "DNS:one.example.com,DNS:two.example.com", "two.example.com",
         true);
  check (NULL, "DNS:one.example.com,DNS:two.example.com", "three.example.com",
         false);

  return EXIT_SUCCESS;
}
