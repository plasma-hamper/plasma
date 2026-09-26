#!/bin/sh
set -e
# Tracing is off by default; the "-v" option turns it on.

usage() {
  cat <<"_EOF_"
Generate (possibly insecure) example certificates for use with pool_tcp_server and secure plasma

Usage:
  ob-plasma-cert.sh [options] cmd
e.g.
  ob-plasma-cert.sh mkca              - generate fake certificate-authorities.pem
  ob-plasma-cert.sh mkserver hostname - generate server-certificate-chain.pem, server-private-key.pem
  ob-plasma-cert.sh mkclient          - generate client-certificate-chain.pem, client-private-key.pem
  ob-plasma-cert.sh demo              - demonstrate and test secure authenticated plasma to localhost
Options:
  -s seclevel                         - openssl 1.1 security level (default 2)
  -t days                             - lifetime of certificates in days (default 2)
  -v                                  - verbose

Example:
  If server is named "myserver.oblong.com", here's how to set up an example secure relationship from scratch:
    $ ob-plasma-cert.sh mkca
    $ ob-plasma-cert.sh mkserver myserver.oblong.com
    $ scp certificate-auhorities.pem server-*.pem myserver.oblong.com:/etc/oblong/
    $ ssh myserver.oblong.com pool_tcp_server
    $ p-create tcps://myserver.oblong.com/blort      # verify that connecting is disallowed without cert
    error: no certificate
    $ ob-plasma-cert.sh mkclient
    $ cp certificate-auhorities.pem client-*.pem /etc/oblong
    $ p-create tcps://myserver.oblong.com/blort

See also bld/cmake/fixtures/tcps/generate.sh, which uses this script.

Note: this script depends on /etc/ssl/openssl.conf.  Here be dragons.  See comments.
_EOF_
}

# Parse options
lifetime=2
seclevel=2
while test "$1" != ""
do
  case "$1" in
  -s) shift; seclevel=$1;;
  -t) shift; lifetime=$1;;
  -v) set -x;;
  -*) usage; exit 1;;
  *) break;;
  esac
  shift
done

ca_lifetime=$lifetime
server_lifetime=$lifetime
client_lifetime=$lifetime

# See https://www.openssl.org/docs/man1.1.0/ssl/SSL_CTX_set_security_level.html
# Also, it seems message digest has to be sha512 or higher for security level 4
case $seclevel in
0) keylen=512;   md=sha1;;   # like the good old days?
1) keylen=1024   md=sha256;;
2) keylen=2048   md=sha256;;
3) keylen=3072   md=sha256;;
4) keylen=7680;  md=sha512;;
5) keylen=15360; md=sha512;;
*) echo "unsupported security level $seclevel"; exit 1;;
esac

#
# Gory details:
#
# doc/plasma-tls.md documents Plasma TLS support.
# pool_tcp_server's help message says:
#  If secure connections are desired, the server will look for the two
#  mandatory files 'server-certificate-chain.pem' and 'server-private-key.pem',
#  and optional file 'dh-params.pem' in OB_ETC_PATH, e.g. $HOME/.oblong/etc:/etc/oblong
#  Clients will look for the file 'certificate-authorities.pem', to
#  authenticate the server's certificate.
#  If -C is specified, then the client also needs 'client-certificate-chain.pem'
#  and 'client-private-key.pem', and the server needs 'certificate-authorities.pem'.
#
# See chapter 5 of "Network Security with OpenSSL",
# https://books.google.com/books?id=IIqwAy4qEl0C
# "Generating Files Needed by the Examples", page 102,
# for the openssl commands needed to generate server and client certificates,
# and "5.1.3 Step 2: Peer Authentication", page 107,
# for how those files are used by plasma to establish the SSL connection securely,
# and finally "5.1.3.5 Post-connection assertions", page 111
# for how the client makes sure that the server presents a certificate that
# contains the FQDN of the server's address.
#
# Timeline of mutual connection authentication:
# 1. Client sends connect request containing client-certificate-chain.pem
# 2. Server verifies that client-certificate-chain.pem can be
#    validated by server's certificate-authorities.pem, then replies with
#    server-certificate-chain.pem
# 3. Client verifies that server-certificate-chain.pem can be validated
#    by client's certificate-authorities.pem
# 4. Client verifies that server-certificate-chain.pem has a dNSName field
#    or commonName matching the hostname in the url it was connecting to.
#    (It also accepts an IP Address field matching the 'hostname' in the url.)
#
# So... if you want to be able to connect to your server equally well via
# either url tcps://localhost/poolname and tcps://foo.example.com/poolname,
# it better have a certificate with two hostnames, one containing "localhost",
# and one containing "foo.example.com" (in some combination of the
# commonName field or dnsName fields).
#
# Here's what a server certificate whose commonName contains "localhost" looks like
# when displayed with "openssl x509 -text -noout < client-certificate-chain.pem":
#  Certificate:
#   Data:
#       Version: 3 (0x2)
#       Serial Number: 3 (0x3)
#   Signature Algorithm: sha1WithRSAEncryption
#       Issuer: C=US, ST=California, L=Los Angeles, O=Oblong Industries, OU=Plasma, CN=chives.la923.oblong.net
#       Validity
#           Not Before: Jun  5 02:32:16 2012 GMT
#           Not After : Jun  5 02:32:16 2017 GMT
#       Subject: C=US, ST=CA, L=Los Angeles, O=Oblong Industries, OU=Plasma, CN=localhost
#  ...

ps_confdir=.

bs_abort() {
    echo "fatal error: $*"
    exit 1
}

do_mkca() {
    local ca_hostname=example-ca-hostname.com
    local ca_subj="/C=US/ST=California/L=Los Angeles/CN=$ca_hostname"

    if test -f $ps_confdir/ca-certificate.pem
    then
        bs_abort "$ps_confdir/ca-certificate.pem already exists"
    fi
    mkdir -p $ps_confdir

    openssl genrsa $keylen > $ps_confdir/ca-private-key.pem
    chmod 600 $ps_confdir/ca-private-key.pem

    # Generate fake CA.
    # FIXME: Don't rely on system openssl.conf at all, use -config here!
    # FIXME: add basicConstraints=CA:TRUE to config!
    openssl req -new -x509 \
       -$md \
       -days $ca_lifetime \
       -key $ps_confdir/ca-private-key.pem \
       -out $ps_confdir/ca-certificate.pem \
       -subj "${ca_subj}" \
       #

    # Show the ca cert and private key for debugging
    #openssl x509 < $ps_confdir/ca-certificate.pem -text -noout

    # Leave results in current directory
    cp $ps_confdir/ca-certificate.pem certificate-authorities.pem
}

do_mkserver() {
    local server_hostname=$1
    local server_subj="/C=US/ST=California/L=Los Angeles/CN=$server_hostname"

    openssl genrsa $keylen > $ps_confdir/server-private-key.pem
    # Generate server cert
    # FIXME: Don't rely on system openssl.conf at all, use -config here!
    # FIXME: add basicConstraints=CA:FALSE to config!
    openssl req -new \
           -$md \
           -key $ps_confdir/server-private-key.pem \
           -out $ps_confdir/server.csr \
           -subj "${server_subj}" \
           #
    # Show the request for debugging
    #openssl req < $ps_confdir/server.csr -text -noout
    # We're self signing our own server cert here.  This is a no-no in production.
    # FIXME: specify real lifetime here
    openssl x509 -req \
           -$md \
           -CA $ps_confdir/ca-certificate.pem -CAkey $ps_confdir/ca-private-key.pem \
           -days $server_lifetime \
           -in $ps_confdir/server.csr \
           -out $ps_confdir/server-certificate.pem \
           -set_serial 01 \
           #
    # Sanity-check the cert
    openssl verify -CAfile $ps_confdir/ca-certificate.pem $ps_confdir/server-certificate.pem

    # Show the server cert and private key for debugging
    #openssl x509 < $ps_confdir/server-certificate.pem -text -noout
    #openssl rsa < $ps_confdir/server-private-key.pem -text -noout

    # Leave results in current directory
    test "$ps_confdir" != . && cp $ps_confdir/server-private-key.pem .
    #cat $ps_confdir/ca-certificate.pem $ps_confdir/server-certificate.pem > server-certificate-chain.pem
    cat $ps_confdir/server-certificate.pem > server-certificate-chain.pem
}

do_mkclient() {
    local client_hostname=ignored
    local client_subj="/C=US/ST=California/L=Los Angeles/CN=$client_hostname"

    openssl genrsa $keylen > $ps_confdir/client-private-key.pem
    # Generate client cert
    # FIXME: Don't rely on system openssl.conf at all, use -config here!
    # FIXME: add basicConstraints=CA:FALSE to config!
    openssl req -new \
        -$md \
        -key $ps_confdir/client-private-key.pem \
        -out $ps_confdir/client.csr \
        -subj "${client_subj}" \
        #
    openssl x509 -req \
        -$md \
        -CAkey $ps_confdir/ca-private-key.pem \
        -CA $ps_confdir/ca-certificate.pem \
        -days $client_lifetime \
        -in $ps_confdir/client.csr \
        -out $ps_confdir/client-certificate.pem \
        -set_serial 1 \
        #
    # Sanity-check the cert
    openssl verify -CAfile $ps_confdir/ca-certificate.pem $ps_confdir/client-certificate.pem

    # Show the client cert and private key for debugging
    #openssl x509 < $ps_confdir/client-certificate.pem -text -noout
    #openssl rsa < $ps_confdir/client-private-key.pem -text -noout

    # Leave results in current directory
    test "$ps_confdir" != . && cp $ps_confdir/client-private-key.pem .
    #cat $ps_confdir/ca-certificate.pem $ps_confdir/client-certificate.pem > client-certificate-chain.pem
    cat $ps_confdir/client-certificate.pem > client-certificate-chain.pem
}

do_demo() {

    echo "do_demo: Begin sanity check"

    ps_confdir=$PWD/demo.tmp
    rm -rf demo.tmp; mkdir demo.tmp
    export OB_ETC_PATH=$PWD/etc.tmp
    rm -rf etc.tmp; mkdir etc.tmp
    export OB_POOLS_DIR=$PWD/pools.tmp
    rm -rf pools.tmp; mkdir pools.tmp
    ob-version

    echo "do_demo: Generate server certs and start server"
    do_mkca
    cp certificate-authorities.pem etc.tmp

    do_mkserver localhost
    cp server-*.pem etc.tmp

    pool_tcp_server -C -P demo.tmp/server.info
    pid=$(awk '/pid:/ {print $2}' < demo.tmp/server.info)
    port=$(awk '/port:/ {print $2}' < demo.tmp/server.info)

    echo "do_demo: Verifying that connecting is disallowed without cert"
    if p-create tcps://localhost:$port/blort
    then
        bs_abort "FAIL: p-create was able to connect without a cert"
    fi

    echo "do_demo: Verifying that connecting is allowed with cert"
    do_mkclient
    cp client*.pem etc.tmp
    p-create tcps://localhost:$port/blort

    # Kill server, remove temporary files
    kill -INT $pid
    rm -rf etc.tmp demo.tmp pools.tmp

    echo "PASS"
}

case "$1" in
mkca)     do_mkca;;
mkserver) do_mkserver $2;;
mkclient) do_mkclient;;
demo)     do_demo;;
*) usage; exit 1;;
esac
