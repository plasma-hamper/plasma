
/* (c)  oblong industries */

///
/// Routines implementing TCP connections to pools, using the generic
/// network pool support.
///

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include "plasma_config.h"
#include "libLoam/c/ob-sys.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-file.h"
#include "libLoam/c/ob-time.h"
#include "libLoam/c/ob-attrs.h"
#include "libPlasma/c/slaw-interop.h"
#include "libPlasma/c/slaw-path.h"
#include "libPlasma/c/pool.h"
#include "libPlasma/c/private/pool-tls.h"
#include "libPlasma/c/private/pool_impl.h"
#include "libPlasma/c/private/pool_tcp.h"
#include "libPlasma/c/protein.h"
#define EINTR_WANT_CONNECT 1
#include "libPlasma/c/eintr-helper.h"
#ifndef _MSC_VER
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

// Rate limiting for socket close warnings
static float64 last_close_warning_time = 0;
static unt64 close_warning_count = 0;
static const float64 WARNING_THROTTLE_SECS = 5.0;  // 5 seconds

#ifdef _MSC_VER


#else

#define winsock_init()
#define winsock_shutdown()

#endif

/// Helper function to get socket address info for logging
static void get_socket_info (ob_sock_t sock, char *buf, size_t buflen)
{
#ifndef _MSC_VER
  struct sockaddr_storage local_addr, peer_addr;
  socklen_t addrlen = sizeof(local_addr);
  
  buf[0] = '\0';
  
  // Get local address
  if (getsockname(sock, (struct sockaddr *)&local_addr, &addrlen) == 0)
    {
      char local_str[INET6_ADDRSTRLEN + 16];  // IP + port
      char peer_str[INET6_ADDRSTRLEN + 16];
      
      // Format local address
      if (local_addr.ss_family == AF_INET)
        {
          struct sockaddr_in *addr4 = (struct sockaddr_in *)&local_addr;
          char ip[INET_ADDRSTRLEN];
          inet_ntop(AF_INET, &addr4->sin_addr, ip, sizeof(ip));
          snprintf(local_str, sizeof(local_str), "%s:%d", ip, ntohs(addr4->sin_port));
        }
      else if (local_addr.ss_family == AF_INET6)
        {
          struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&local_addr;
          char ip[INET6_ADDRSTRLEN];
          inet_ntop(AF_INET6, &addr6->sin6_addr, ip, sizeof(ip));
          snprintf(local_str, sizeof(local_str), "[%s]:%d", ip, ntohs(addr6->sin6_port));
        }
      else
        {
          snprintf(local_str, sizeof(local_str), "<unknown family %d>", local_addr.ss_family);
        }
      
      // Get peer address
      addrlen = sizeof(peer_addr);
      if (getpeername(sock, (struct sockaddr *)&peer_addr, &addrlen) == 0)
        {
          // Format peer address
          if (peer_addr.ss_family == AF_INET)
            {
              struct sockaddr_in *addr4 = (struct sockaddr_in *)&peer_addr;
              char ip[INET_ADDRSTRLEN];
              inet_ntop(AF_INET, &addr4->sin_addr, ip, sizeof(ip));
              snprintf(peer_str, sizeof(peer_str), "%s:%d", ip, ntohs(addr4->sin_port));
            }
          else if (peer_addr.ss_family == AF_INET6)
            {
              struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&peer_addr;
              char ip[INET6_ADDRSTRLEN];
              inet_ntop(AF_INET6, &addr6->sin6_addr, ip, sizeof(ip));
              snprintf(peer_str, sizeof(peer_str), "[%s]:%d", ip, ntohs(addr6->sin6_port));
            }
          else
            {
              snprintf(peer_str, sizeof(peer_str), "<unknown family %d>", peer_addr.ss_family);
            }
          
          snprintf(buf, buflen, " (fd %d: %s -> %s)",
                   sock, local_str, peer_str);
        }
      else
        {
          // getpeername failed, socket might be disconnected
          int err = errno;
          snprintf(buf, buflen, " (fd %d: %s -> <disconnected: %s>)",
                   sock, local_str, strerror(err));
        }
    }
  else
    {
      // getsockname failed
      int err = errno;
      snprintf(buf, buflen, " (fd %d: <getsockname failed: %s>)", sock, strerror(err));
    }
#else
  snprintf(buf, buflen, " (fd %d)", sock);
#endif
}

/// Implement the send and receive functions, needed by the generic
/// network pools layer.  See pool_net.[ch] for details.

// Based on readn() and writen() from "Unix Network Programming,
// Vol. 1, 3rd ed."

ob_retort pool_tcp_send_nbytes (ob_sock_t sock, const void *buf, size_t len,
                                ob_handle_t wake_event)
{
  size_t nleft;
  ssize_t nwritten;
  const char *ptr;
  ob_select2_t sel2;
  ob_retort tort;
#ifdef MSG_NOSIGNAL
  const int send_flags = MSG_NOSIGNAL;
#else
  const int send_flags = 0;
#endif

  if (wake_event != OB_NULL_HANDLE)
    {
      tort = ob_select2_prepare (&sel2, OB_SEL2_SEND, sock, wake_event);
      if (tort < OB_OK)
        return tort;
    }

  ptr = (const char *) buf;
  nleft = len;

  ob_retort pret = OB_OK;

  while (nleft > 0)
    {
      if (wake_event != OB_NULL_HANDLE)
        {
          tort = ob_select2 (&sel2, POOL_WAIT_FOREVER, true);
          if (tort == POOL_AWAIT_WOKEN && nleft != len)
            {
              pret = POOL_AWAIT_WOKEN_DIRTY;
              break;
            }
          else if (tort < OB_OK)
            {
              pret = tort;
              break;
            }
        }

      errno = 0;
      nwritten = send (sock, ptr, nleft, send_flags);
      const int erryes = errno;

      if (nwritten > 0)
        {
          nleft -= nwritten;
          ptr += nwritten;
        }
      else
        {
          if (SHOULD_TRY_AGAIN (nwritten))
            {
              // give it another go
              nwritten = 0;
              continue;
            }

          // it's likely the other end closed the socket for some reason
          if (nwritten == -1 && erryes == EPIPE)
            {
              close_warning_count++;
              float64 now = ob_current_time();
              
              if (now - last_close_warning_time > WARNING_THROTTLE_SECS)
                {
                  char sock_info[256];
                  get_socket_info(sock, sock_info, sizeof(sock_info));
                  unt64 count = close_warning_count;
                  close_warning_count = 0;
                  
                  if (count > 1)
                    {
                      OB_LOG_WARNING_CODE (0x20108027,
                                           "socket was closed unexpectedly %s (occurred %llu times in last %.0f seconds)",
                                           sock_info, (unsigned long long)count, 
                                           WARNING_THROTTLE_SECS);
                    }
                  else
                    {
                      OB_LOG_WARNING_CODE (0x20108027,
                                           "socket was closed unexpectedly %s",
                                           sock_info);
                    }
                  last_close_warning_time = now;
                }
              pret = POOL_UNEXPECTED_CLOSE;
            }
          else
            {
              char sock_info[256];
              get_socket_info(sock, sock_info, sizeof(sock_info));
              OB_LOG_WARNING_CODE (0x20108000,
                                   "send() returned %" OB_FMT_SIZE "d with "
                                   "errno '%s' with %" OB_FMT_SIZE
                                   "d bytes left %s\n",
                                   nwritten, strerror (erryes), nleft, sock_info);
              pret = POOL_SEND_BADTH;
            }
          errno = erryes;
          break;
        }
    }

  if (wake_event != OB_NULL_HANDLE)
    {
      tort = ob_select2_finish (&sel2);
      if (tort <= OB_OK && pret == OB_OK)
        return tort;
    }

  return pret;
}

ob_retort pool_tcp_recv_nbytes (ob_sock_t sock, void *buf, size_t len,
                                ob_handle_t wake_event)
{
  size_t nleft;
  ssize_t nread;
  char *ptr;

  ob_select2_t sel2;
  ob_retort tort;

  if (wake_event != OB_NULL_HANDLE)
    {
      tort = ob_select2_prepare (&sel2, OB_SEL2_RECEIVE, sock, wake_event);
      if (tort < OB_OK)
        return tort;
    }

  ptr = (char *) buf;
  nleft = len;

  ob_retort pret = OB_OK;

  while (nleft > 0)
    {
      if (wake_event != OB_NULL_HANDLE)
        {
          tort = ob_select2 (&sel2, POOL_WAIT_FOREVER, true);
          if (tort == POOL_AWAIT_WOKEN && nleft != len)
            {
              pret = POOL_AWAIT_WOKEN_DIRTY;
              break;
            }
          else if (tort < OB_OK)
            {
              pret = tort;
              break;
            }
        }

      errno = 0;
      nread = recv (sock, ptr, nleft, 0);
      const int erryes = errno;

      if (nread > 0)
        {
          nleft -= nread;
          ptr += nread;
        }
      else
        {
          if (SHOULD_TRY_AGAIN (nread))
            {
              // give it another go
              nread = 0;
              continue;
            }

          // it's likely the other end closed the socket for some reason
          if (nread == 0)
            {
              close_warning_count++;
              float64 now = ob_current_time();
              
              if (now - last_close_warning_time > WARNING_THROTTLE_SECS)
                {
                  char sock_info[256];
                  get_socket_info(sock, sock_info, sizeof(sock_info));
                  unt64 count = close_warning_count;
                  close_warning_count = 0;
                  
                  if (count > 1)
                    {
                      OB_LOG_WARNING_CODE (0x20108028,
                                           "socket was closed unexpectedly %s (occurred %llu times in last %.0f seconds)",
                                           sock_info, (unsigned long long)count,
                                           WARNING_THROTTLE_SECS);
                    }
                  else
                    {
                      OB_LOG_WARNING_CODE (0x20108028,
                                           "socket was closed unexpectedly %s",
                                           sock_info);
                    }
                  last_close_warning_time = now;
                }
              pret = POOL_UNEXPECTED_CLOSE;
            }
          else
            {
              char sock_info[256];
              get_socket_info(sock, sock_info, sizeof(sock_info));
              OB_LOG_WARNING_CODE (0x20108001,
                                   "recv() returned %" OB_FMT_SIZE "d with "
                                   "errno '%s' with %" OB_FMT_SIZE
                                   "d bytes left %s\n",
                                   nread, strerror (erryes), nleft, sock_info);
              pret = POOL_RECV_BADTH;
            }
          errno = erryes;
          break;
        }
    }

  if (wake_event != OB_NULL_HANDLE)
    {
      tort = ob_select2_finish (&sel2);
      if (tort <= OB_OK && pret == OB_OK)
        return tort;
    }

  return pret;
}


static ob_retort pool_tcp_close (unt64 code, pool_net_data *net)
{
  ob_retort ort = ob_close_socket (net->connfd);
  if (ort < OB_OK)
    OB_LOG_ERROR_CODE (code, "failed to close socket: %s\n",
                       ob_error_string (ort));

  // XXX: assumes 0 is not a valid pthread_t (which is true on Linux
  // and OS X, but may not be true in the general case)
  if (net->tls_thread)
    {
      ob_retort tort = ob_tls_client_join_thread (net->tls_thread);
      if (tort < OB_OK)
        {
          OB_LOG_ERROR_CODE (code, "joining TLS thread: '%s'\n",
                             ob_error_string (tort));
          return tort;
        }
    }
  return OB_OK;
}


typedef enum {
  Ob_Insecure,      /* don't use TLS */
  Ob_Opportunistic, /* use TLS if available; allow anonymous */
  Ob_Secure         /* require TLS with certificate */
} Ob_Security_Level;

/// Per-pool hose data needed to create the TCP connection.

typedef struct
{
  char *hostname;
  char *poolName;
  /// The port number is kept in string format for the convenience of
  /// getaddrinfo.
  char *port_str;
  /// set by pool_tcp_list, which does not operate on one single pool
  bool empty_pool_name_ok;
  // what level of security was requested?
  Ob_Security_Level security;
} parsed_pseudo_uri;

/// Pull out TCP data from the pool hose.

static inline parsed_pseudo_uri *get_parsed_pseudo_uri (pool_hose ph)
{
  return (parsed_pseudo_uri *) ph->ext;
}

/// Allocate a fresh tcp data struct
static parsed_pseudo_uri *new_parsed_pseudo_uri (void)
{
  parsed_pseudo_uri *d = (parsed_pseudo_uri *) malloc (sizeof (*d));
  if (!d)
    return NULL;
  memset (d, 0, sizeof (*d));
  return d;
}

/// Free TCP connection data.

static void free_parsed_pseudo_uri (parsed_pseudo_uri *d)
{
  if (d)
    free (d->hostname);
  if (d)
    free (d->poolName);
  if (d)
    free (d->port_str);
  free (d);
}

/// Clean up from a failed connection attempt.

static ob_retort parse_pseudo_uri_cleanup (char *uri, ob_retort pret)
{
  free (uri);
  return pret;
}

/// Figure out what hostname, port, and pool to connect to.
/// Hostname, port, etc. are encoded in the pool name like so:
/// @code
/// <protocol>://<hostname>[:port]/<pool_name>
/// @endcode

static ob_retort parse_pseudo_uri (const char *full_pool_name,
                                   parsed_pseudo_uri *d)
{
  // Yaaaaaaaaay, string manipulation in C.
  char *uri = strdup (full_pool_name);
  if (!uri)
    return OB_NO_MEM;
  char *hostname = strstr (uri, "://");
  if (!hostname)
    return parse_pseudo_uri_cleanup (uri, POOL_POOLNAME_BADTH);
  const size_t proto_len = hostname - uri;
  if (proto_len == 3 && uri[0] == 't' && uri[1] == 'c' && uri[2] == 'p')
    d->security = Ob_Insecure;
  else if (proto_len == 4 && uri[0] == 't' && uri[1] == 'c' && uri[2] == 'p')
    {
      if (uri[3] == 'o')
        d->security = Ob_Opportunistic;
      else if (uri[3] == 's')
        d->security = Ob_Secure;
      else
        goto unknown_proto;
    }
  else
    {
    unknown_proto:
      *hostname = 0; /* NUL-terminate protocol in "uri" for printing */
      OB_LOG_ERROR_CODE (0x20108026, "Didn't expect '%s' as a protocol\n"
                                     "in '%s'\n",
                         uri, full_pool_name);
      free (uri);
      return POOL_POOLNAME_BADTH;
    }
  hostname = hostname + strlen ("://");
  // Check for optional port
  char *port = strchr (hostname, ':');
  if (port)
    *port++ = '\0';
  char *poolName;
  if (port)
    poolName = strchr (port, '/');
  else
    poolName = strchr (hostname, '/');
  if (!poolName)
    return parse_pseudo_uri_cleanup (uri, POOL_POOLNAME_BADTH);
  *poolName++ = '\0';
  // Got all the bits, now copy them over
  d->hostname = strdup (hostname);
  if (port)
    d->port_str = strdup (port);
  else
    // Must strdup so that cleanup code can free it unconditionally
    d->port_str = strdup (POOL_TCP_PORT_STR);
  d->poolName = strdup (poolName);
  // Check for malloc failures all in one go
  if (!d->hostname || !d->port_str || !d->poolName)
    return parse_pseudo_uri_cleanup (uri, OB_NO_MEM);
  ob_log (OBLV_DBUG, 0x20108002, "hostname %s\n", d->hostname);
  ob_log (OBLV_DBUG, 0x20108003, "pool name %s\n", d->poolName);
  ob_log (OBLV_DBUG, 0x20108004, "port %s\n", d->port_str);
  free (uri);
  // If any one of hostname, pool name, or port is the empty string,
  // then this is not a valid pool URI.
  if (!d->hostname[0] || !d->port_str[0]
      || (!d->empty_pool_name_ok && !d->poolName[0]))
    return POOL_POOLNAME_BADTH;

  // Validate port number
  long port_num;
  char *endptr = NULL;

  errno = 0;
  port_num = strtol (d->port_str, &endptr, 0);
  if (*endptr != 0 || errno != 0 || port_num < 0 || port_num > 0xffff)
    return POOL_POOLNAME_BADTH;

  return OB_OK;
}

static const byte greeting[] = {0x00,
                                0x00,
                                0x00,
                                0x00,
                                0x00,
                                0x00,
                                0x00,
                                0x50,
                                0x93,
                                0x93,
                                0x00,
                                0x80,
                                0x18,
                                0x00,
                                0x00,
                                0x02,
                                0x00,
                                0x00,
                                0x00,
                                0x10,
                                0x40,
                                0x00,
                                0x00,
                                0x04,
                                0x20,
                                0x00,
                                0x00,
                                0x01,
                                0x6f,
                                0x70,
                                0x00,
                                0x00,
                                0x08,
                                0x00,
                                0x00,
                                0x03,
                                0x00,
                                0x00,
                                0x00,
                                0x01,
                                0x40,
                                0x00,
                                0x00,
                                0x08,
                                0x20,
                                0x00,
                                0x00,
                                0x02,
                                0x61,
                                0x72,
                                0x67,
                                0x73,
                                0x00,
                                0x00,
                                0x00,
                                0x00,
                                0x10,
                                0x00,
                                0x00,
                                0x01,
                                0x00,
                                0x00,
                                0x00,
                                0x05,
                                0x20,
                                0x00,
                                0x00,
                                0x02,
                                0x5e,
                                0x2f,
                                0x5e,
                                0x2f,
                                0x5e,
                                0x2f,
                                0x5e,
                                0x00,
                                POOL_TCP_VERSION_CURRENT,
                                SLAW_VERSION_CURRENT,
                                0x00,
                                0x00,
                                0x00,
                                0x00,
                                0x00,
                                0x00,
                                0x00,
                                0x00,
                                0x00,
                                0x00};

#ifdef DROP_SUPPORT_FOR_SLAW_V1
static unt64 current_cmds (void)
{
  unt64 cmds = 0;

  cmds |= (OB_CONST_U64 (1) << POOL_CMD_CREATE);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_DISPOSE);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_PARTICIPATE);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_PARTICIPATE_CREATINGLY);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_WITHDRAW);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_DEPOSIT);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_NTH_PROTEIN);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_NEXT);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_PROBE_FRWD);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_NEWEST_INDEX);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_OLDEST_INDEX);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_AWAIT_NEXT_SINGLE);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_MULTI_ADD_AWAITER);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_INFO);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_LIST);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_INDEX_LOOKUP);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_PROBE_BACK);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_PREV);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_FANCY_ADD_AWAITER);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_SET_HOSE_NAME);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_SUB_FETCH);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_RENAME);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_ADVANCE_OLDEST);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_SLEEP);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_CHANGE_OPTIONS);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_LIST_EX);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_SUB_FETCH_EX);
#ifdef GREENHOUSE
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_GREENHOUSE);
#endif
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_STARTTLS);

  return cmds;
}
#endif

static ob_retort negotiate_version (pool_net_data *net,
                                    bool abbreviated_greeting,
                                    // these next two args are for error msgs
                                    const char *hostname, const char *port_str)
{
#ifdef DROP_SUPPORT_FOR_SLAW_V1
  // Don't even try to negotiate.
  // This is inflexible, but useful during creduce.
  {
    net->net_version = POOL_TCP_VERSION_CURRENT;
    net->slaw_version = SLAW_VERSION_CURRENT;
    net->supported_cmds = current_cmds ();
    if (!abbreviated_greeting)
      return OB_OK;
  }
#endif
  // send greeting, which includes which version we support
  const ob_handle_t nullWaitObject = OB_NULL_HANDLE;

  const byte *greet = greeting;
  size_t greet_size = sizeof (greeting);

  if (abbreviated_greeting)
    {
      /* Only send the version numbers.  This is for when we re-handshake
       * after initiating TLS. */
      greet += 76;
      greet_size = 2;
    }

  ob_retort pret =
    pool_tcp_send_nbytes (net->connfd, greet, greet_size, nullWaitObject);
  if (pret < OB_OK)
    return pret;

  // get back what version the server supports
  byte vers[2];
  OB_INVALIDATE (vers);
  pret =
    pool_tcp_recv_nbytes (net->connfd, vers, sizeof (vers), nullWaitObject);
  if (pret < OB_OK)
    return pret;
  net->net_version = vers[0];
  net->slaw_version = vers[1];
  if (net->net_version == 0 && net->slaw_version == 0)
    return OB_OK; /* this indicates a legacy connection */
  else if (net->net_version > POOL_TCP_VERSION_CURRENT)
    {
      if (net->net_version == 'H' && net->slaw_version == 'T')
        OB_LOG_ERROR_CODE (0x20108021, "%s:%s\n"
                                       "looks like it might be an http\n"
                                       "server, not a pool server!\n",
                           hostname, port_str);
      else
        OB_LOG_ERROR_CODE (0x20108020, "%s:%s\n"
                                       "server claims protocol %d/slaw %d,\n"
                                       "but we only know protocol %d/slaw %d\n",
                           hostname, port_str, net->net_version,
                           net->slaw_version, POOL_TCP_VERSION_CURRENT,
                           SLAW_VERSION_CURRENT);
      return POOL_WRONG_VERSION; /* some future version we don't support */
    }

  // If this is protocol version 1 or higher, we get one byte saying how
  // many bytes follow, which then make up a bitmask of supported commands.
  byte len;
  pret = pool_tcp_recv_nbytes (net->connfd, &len, sizeof (len), nullWaitObject);
  if (pret < OB_OK)
    return pret;

  unt64 cmds = 0;
  byte *cmd_bytes = (byte *) malloc (len);
  if (!cmd_bytes)
    return OB_NO_MEM;
  pret = pool_tcp_recv_nbytes (net->connfd, cmd_bytes, len, nullWaitObject);
  if (pret < OB_OK)
    {
      free (cmd_bytes);
      return pret;
    }
  byte i;
  for (i = 0; i < len; i++)
    cmds |= ((unt64) cmd_bytes[i]) << (i * 8);
  free (cmd_bytes);
  net->supported_cmds = cmds;
  return OB_OK;
}

// pool TCP protocol version 0 doesn't include negotiation of what
// commands are supported, so we have a hardcoded list of what commands
// the v0 protocol supports
static unt64 v0_cmds (void)
{
  unt64 cmds = 0;

  cmds |= (OB_CONST_U64 (1) << POOL_CMD_CREATE);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_DISPOSE);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_PARTICIPATE);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_PARTICIPATE_CREATINGLY);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_WITHDRAW);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_DEPOSIT);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_NTH_PROTEIN);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_NEXT);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_PROBE_FRWD);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_NEWEST_INDEX);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_OLDEST_INDEX);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_AWAIT_NEXT_SINGLE);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_MULTI_ADD_AWAITER);
  // XXX: these are somewhat dubious, because some v0 servers support
  // them and some don't, but there's no way to tell for sure.
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_INFO);
  cmds |= (OB_CONST_U64 (1) << POOL_CMD_LIST);
  return cmds;
}

static ob_retort socktort (void)
{
#ifdef _MSC_VER
  return ob_win32err_to_retort (WSAGetLastError ());
#else
  return ob_errno_to_retort (errno);
#endif
}

const char *ob_sockmsg (void)
{
  return ob_error_string (socktort ());
}

ob_retort ob_common_sockopts (ob_sock_t fd)
{
// Try to get better QoS by setting DSCP:
// http://mailman.mit.edu/pipermail/mosh-devel/2012-April/000101.html
// https://github.com/dtaht/mosh/commit/6dd29ec3eeb4a3634fc146576b0802bd33f9bea5
// http://tools.ietf.org/html/rfc4594
// http://www.bufferbloat.net/
//
// Unfortunately, for unknown reasons, this call returns EINVAL on OS X
// Looks like the mosh people are having the same problem on OS X:
// http://mailman.mit.edu/pipermail/mosh-devel/2012-June/000214.html
#ifndef __APPLE__
  char dscp = 32;  // CS4 - "Real-Time Interactive"
  if (setsockopt (fd, IPPROTO_IP, IP_TOS, EVIL_SOCKOPT_CAST (&dscp),
                  sizeof (dscp))
      != 0)
    {
      OB_LOG_ERROR_CODE (0x20108024, "setsockopt/IP_TOS: '%s'\n",
                         ob_sockmsg ());
      return POOL_SOCK_BADTH;
    }
#endif

  // We want looooow latency, turn off delayed acks and Nagle
  int on = 1;
  if (setsockopt (fd, IPPROTO_TCP, TCP_NODELAY, EVIL_SOCKOPT_CAST (&on),
                  sizeof (on))
      != 0)
    {
      OB_LOG_ERROR_CODE (0x20108023, "setsockopt/TCP_NODELAY: '%s'\n",
                         ob_sockmsg ());
      return POOL_SOCK_BADTH;
    }

#ifdef PLASMA_ENABLE_TCP_OPTIMIZATIONS
#ifndef _MSC_VER
  // Increase socket buffer sizes for large transfers
  int bufsize = PLASMA_TCP_BUFFER_SIZE;
  if (setsockopt (fd, SOL_SOCKET, SO_RCVBUF, EVIL_SOCKOPT_CAST (&bufsize),
                  sizeof (bufsize))
      != 0)
    {
      OB_LOG_WARNING_CODE (0x20108026, "setsockopt/SO_RCVBUF: '%s' (continuing anyway)\n",
                           ob_sockmsg ());
    }
  
  if (setsockopt (fd, SOL_SOCKET, SO_SNDBUF, EVIL_SOCKOPT_CAST (&bufsize),
                  sizeof (bufsize))
      != 0)
    {
      OB_LOG_WARNING_CODE (0x20108027, "setsockopt/SO_SNDBUF: '%s' (continuing anyway)\n",
                           ob_sockmsg ());
    }

#ifdef HAVE_TCP_USER_TIMEOUT
  // Set TCP timeout for large transfers
  int timeout_ms = PLASMA_TCP_TIMEOUT_MS;
  if (setsockopt (fd, IPPROTO_TCP, TCP_USER_TIMEOUT, EVIL_SOCKOPT_CAST (&timeout_ms),
                  sizeof (timeout_ms))
      != 0)
    {
      OB_LOG_WARNING_CODE (0x20108028, "setsockopt/TCP_USER_TIMEOUT: '%s' (continuing anyway)\n",
                           ob_sockmsg ());
    }
#endif

#ifdef HAVE_TCP_KEEPINTVL
  // Reduce keepalive interval to 30 seconds
  int keepintvl = 30;
  if (setsockopt (fd, IPPROTO_TCP, TCP_KEEPINTVL, EVIL_SOCKOPT_CAST (&keepintvl),
                  sizeof (keepintvl))
      != 0)
    {
      OB_LOG_WARNING_CODE (0x20108029, "setsockopt/TCP_KEEPINTVL: '%s' (continuing anyway)\n",
                           ob_sockmsg ());
    }
#endif

#ifdef HAVE_TCP_KEEPIDLE
  // Start keepalive after 60 seconds of idle
  int keepidle = 60;
  if (setsockopt (fd, IPPROTO_TCP, TCP_KEEPIDLE, EVIL_SOCKOPT_CAST (&keepidle),
                  sizeof (keepidle))
      != 0)
    {
      OB_LOG_WARNING_CODE (0x2010802a, "setsockopt/TCP_KEEPIDLE: '%s' (continuing anyway)\n",
                           ob_sockmsg ());
    }
#endif
#endif // !_MSC_VER
#endif // PLASMA_ENABLE_TCP_OPTIMIZATIONS

  return ob_nosigpipe_sockopt (fd);
}

ob_retort ob_nosigpipe_sockopt (OB_UNUSED ob_sock_t fd)
{
#ifdef SO_NOSIGPIPE
  int on = 1;
  /* Linux only has MSG_NOSIGNAL (a flag for send), while OS X only has
   * SO_NOSIGPIPE (a socket option).  We support both to cover both OSes. */
  if (setsockopt (fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof (on)) != 0)
    {
      OB_LOG_ERROR_CODE (0x20108025, "setsockopt/SO_NOSIGPIPE: '%s'\n",
                         ob_sockmsg ());
      return POOL_SOCK_BADTH;
    }
#endif

  return OB_OK;
}

ob_retort ob_nosigpipe_sockopt_x2 (ob_sock_t socks[2])
{
  int i;
  ob_retort err = OB_OK;
  for (i = 0; i < 2; i++)
    ob_err_accum (&err, ob_nosigpipe_sockopt (socks[i]));
  return err;
}

static ob_retort my_connect1 (const struct addrinfo *ai, ob_sock_t *fd_out)
{
  // Create a socket
  ob_sock_t fd =
    ob_socket_cloexec (ai->ai_family, ai->ai_socktype, ai->ai_protocol);
  if (OB_IS_BAD_SOCK (fd))
    return POOL_SOCK_BADTH;

  // Set Nagle and DSCP
  ob_retort tort = ob_common_sockopts (fd);
  if (tort < OB_OK)
    {
      ob_retort ort = ob_close_socket (fd);
      if (ort < OB_OK)
        OB_LOG_ERROR_CODE (0x20108007, "ob_close_socket on fail path "
                                       "for ob_common_socketopts:\n%s\n",
                           ob_error_string (ort));
      return tort;
    }

  // Connect to server
  int err = EINTR_connect_harder (fd, ai->ai_addr, ai->ai_addrlen);
  if (err != 0)
    {
      ob_retort conntort = ob_errno_to_retort (err);
      OB_LOG_ERROR_CODE (0x20108008, "connect: '%s' [family %d]\n",
                         ob_error_string (conntort), ai->ai_family);
      ob_retort ort = ob_close_socket (fd);
      if (ort < OB_OK)
        OB_LOG_ERROR_CODE (0x20108009, "ob_close_socket on fail path "
                                       "for connect:\n%s\n",
                           ob_error_string (ort));
      return POOL_SERVER_UNREACH;
    }
  *fd_out = fd;
  return OB_OK;
}

static int cmp_ai (const void *v1, const void *v2)
{
  const struct addrinfo *a1 = *(const struct addrinfo **) v1;
  const struct addrinfo *a2 = *(const struct addrinfo **) v2;

  if (a1->ai_family < a2->ai_family)
    return -1;
  if (a1->ai_family > a2->ai_family)
    return 1;
  else
    return 0;
}

static ob_retort my_connect (struct addrinfo *ai, ob_sock_t *fd_out)
{
  size_t count = 0, pos = 0;
  struct addrinfo *al;

  // count number of addresses
  for (al = ai; al; al = al->ai_next)
    count++;

  // convert linked list to array
  struct addrinfo **aa =
    (struct addrinfo **) calloc (count, sizeof (struct addrinfo *));
  if (!aa)
    return OB_NO_MEM;
  for (al = ai; al; al = al->ai_next)
    aa[pos++] = al;

  /* We would like to try IPv4 before IPv6, no matter what the OS said.
   * For one thing, this is because old pool servers only listened on IPv4,
   * so we'll have much better luck trying IPv4 first, especially if the
   * server is old.
   *
   * In order to accomplish this goal, we sort the array of addresses by
   * numeric order of family, because it just so happens that even though
   * different OSes use different family numbers, the family
   * number for AF_INET is always less than AF_INET6. */
  qsort (aa, count, sizeof (struct addrinfo *), cmp_ai);

  // now try connecting to the addresses until one succeeds, or all fail
  ob_retort tort = OB_UNKNOWN_ERR;
  for (pos = 0; pos < count && tort < OB_OK; pos++)
    tort = my_connect1 (aa[pos], fd_out);

  free (aa);
  return tort;
}

static ob_retort start_tls (pool_net_data *net, const char *hostname,
                            const char *port_str, Ob_Security_Level security,
                            bslaw options)
{
  ob_retort pret, remote_pret;

  slaw empty_map = slaw_map_f (slabu_new ());

  pret = pool_net_send_op (net, POOL_CMD_STARTTLS, "x", empty_map);
  slaw_free (empty_map);
  if (pret < OB_OK)
    return pret;

  slaw ignore_slaw = NULL;
  pret = pool_net_recv_result (net, "rx", &remote_pret, &ignore_slaw);
  if (pret < OB_OK)
    return pret;

  slaw_free (ignore_slaw);
  if (remote_pret < OB_OK)
    return remote_pret;

  const char *certificate = slaw_path_get_string (options, "certificate", NULL);
  const char *private_key = slaw_path_get_string (options, "private-key", NULL);

  ob_sock_t pair[2];
  if (pret >= OB_OK)
    pret = ob_socketpair_cloexec (OB_SP_DOMAIN, SOCK_STREAM, 0, pair);
  if (pret >= OB_OK)
    pret = ob_nosigpipe_sockopt_x2 (pair);
  if (pret >= OB_OK)
    pret = ob_tls_client_launch_thread (pair[0], net->connfd, &net->tls_thread,
                                        hostname, security != Ob_Secure,
                                        certificate, private_key);
  if (pret < OB_OK)
    return pret;

  net->connfd = pair[1];
  return negotiate_version (net, true, hostname, port_str);
}

/// Initiate a connection to the remote pool server.

static ob_retort pool_tcp_make_connection1 (const char *poolName,
                                            pool_net_data *net,
                                            ob_handle_t *wakeup_handle_loc,
                                            parsed_pseudo_uri *d,
                                            pool_context ctx)
{
  ob_retort pret, tort;

  pret = parse_pseudo_uri (poolName, d);
  if (pret != OB_OK)
    return pret;

  // Convert hostname to address
  struct addrinfo *ai;

  // Below constrains us to useful results
  struct addrinfo hint;
  memset (&hint, 0, sizeof (hint));
  hint.ai_family = AF_UNSPEC;
  hint.ai_socktype = SOCK_STREAM;
  hint.ai_protocol = IPPROTO_TCP;

  if (getaddrinfo (d->hostname, d->port_str, &hint, &ai) != 0)
    return POOL_NO_SUCH_POOL;

  int fd = -1;
  bool server_is_old_version = false;
  int tries = 0;
  const int maxtries = 3;

reconnect_to_server:
  pret = my_connect (ai, &fd);
  if (pret < OB_OK)
    {
      freeaddrinfo (ai);
      return pret;
    }

  // Tell the network subsystem how to send data over this connection
  net->connfd = fd;

  net->wakeup_handle_loc = wakeup_handle_loc;

  net->send_nbytes = pool_tcp_send_nbytes;
  net->recv_nbytes = pool_tcp_recv_nbytes;
  OB_CLEAR (net->outstanding);
  net->outstanding.status = OB_NO_AWAIT;
  net->supported_cmds = v0_cmds ();

  // XXX: assumes 0 is not a valid pthread_t (which is true on Linux
  // and OS X, but may not be true in the general case)
  net->tls_thread = 0;

  if (!server_is_old_version)
    {
      pret = negotiate_version (net, false, d->hostname, d->port_str);
      if (pret >= OB_OK && net->net_version == 0 && net->slaw_version == 0)
        {
          net->slaw_version = 1;
          server_is_old_version = true;
          tort = pool_tcp_close (0x2010800a, net);
          if (tort < OB_OK)
            {
              freeaddrinfo (ai);
              return tort;
            }
          goto reconnect_to_server;
        }
      else if (pret < OB_OK)
        {
          tort = pool_tcp_close (0x2010800b, net);
          if (tort < OB_OK)
            {
              freeaddrinfo (ai);
              return tort;
            }
          if (pret == POOL_SEND_BADTH || pret == POOL_RECV_BADTH)
            {
              tries++;
              ob_log (OBLV_INFO, 0x20108005,
                      "When connecting to '%s', got %s on try %d of %d\n",
                      poolName, ob_error_string (pret), tries, maxtries);
              if (tries < maxtries)
                {
                  ob_micro_sleep (tries * 100000);
                  goto reconnect_to_server;
                }
            }
        }
    }

  freeaddrinfo (ai);

  if (pret >= OB_OK)
    {
      if (pool_net_supports_cmd (net, POOL_CMD_STARTTLS)
          && (d->security == Ob_Opportunistic || d->security == Ob_Secure))
        {
          ob_retort t = start_tls (net, d->hostname, d->port_str, d->security,
                                   pool_ctx_get_options (ctx));
          if (t < OB_OK)
            {
              tort = pool_tcp_close (0x2010802c, net);
              return (tort < OB_OK ? tort : t);
            }
        }
      else if (d->security == Ob_Secure)
        {
          tort = pool_tcp_close (0x20108029, net);
          if (tort < OB_OK)
            return tort;
          // client said TLS is required, but server does not support it
          return POOL_NO_TLS;
        }
      else if (d->security == Ob_Insecure
               && pool_net_supports_cmd (net, POOL_CMD_STARTTLS)
               && !pool_net_supports_cmd (net, POOL_CMD_PARTICIPATE))
        {
          tort = pool_tcp_close (0x2010802a, net);
          if (tort < OB_OK)
            return tort;
          // If server supports STARTTLS but doesn't support PARTICIPATE,
          // take that as a clue that the server requires TLS (i. e. we
          // must do STARTTLS before we can do anything else).
          // If the client say don't allow TLS, this is a problem.
          return POOL_TLS_REQUIRED;
        }
    }

  return pret;
}

static ob_retort pool_tcp_make_connection (const char *name_of_pool,
                                           pool_net_data *net,
                                           ob_handle_t *wakeup_handle_loc,
                                           parsed_pseudo_uri *d,
                                           pool_context ctx)
{
  ob_retort tort =
    pool_tcp_make_connection1 (name_of_pool, net, wakeup_handle_loc, d, ctx);
#ifdef GREENHOUSE
#ifdef OB_HAS_PRAGMA_MESSAGE
#pragma message("GREENHOUSE enabled, hobbling protocol")
#endif
  if (tort >= OB_OK)
    {
      if (!pool_net_supports_cmd (net, POOL_CMD_GREENHOUSE))
        {
          ob_retort t = pool_tcp_close (0x2010802b, net);
          if (t < OB_OK)
            tort = t;
          else
            tort = POOL_NOT_A_GREENHOUSE_SERVER;
        }
    }
#else
#ifdef OB_HAS_PRAGMA_MESSAGE
#pragma message("GREENHOUSE disabled, full-flavored protocol engaged")
#endif
#endif
  return tort;
}

/// TCP implementation of type-specific functions, as defined in
/// pool_impl.h.  Nothing fancy going on here, just wrapping up
/// operations and sending them off to the remote server.

// The generic network support could be extended to implement much of
// the below functions with the addition of create_connection() and
// close_connection() members in the pool_net_data struct.

static ob_retort pool_tcp_create (pool_hose ph, const char *type,
                                  bprotein options)
{
  ob_retort pret, remote_pret;
  parsed_pseudo_uri *d = new_parsed_pseudo_uri ();
  if (!d)
    return OB_NO_MEM;

  winsock_init ();

  pret = pool_tcp_make_connection (ph->name, ph->net,
#ifdef _MSC_VER
                                   &ph->w.wakeup_event_handle,
#else
                                   &ph->w.wakeup_read_fd,
#endif
                                   d, ph->ctx);

  if (pret != OB_OK)
    {
      free_parsed_pseudo_uri (d);
      winsock_shutdown ();
      return pret;
    }

  pret = pool_net_send_op (ph->net, POOL_CMD_CREATE, "ssp", d->poolName, type,
                           options);
  if (pret == OB_OK)
    pret = pool_net_recv_result (ph->net, "r", &remote_pret);
  free_parsed_pseudo_uri (d);
  ob_retort tort = pool_tcp_close (0x2010800c, ph->net);

  winsock_shutdown ();

  if (tort < OB_OK)
    return tort;
  if (pret != OB_OK)
    return pret;
  return remote_pret;
}

static ob_retort pool_tcp_participate (pool_hose ph)
{
  ob_retort pret, remote_pret;
  parsed_pseudo_uri *d = new_parsed_pseudo_uri ();
  if (!d)
    return OB_NO_MEM;

  winsock_init ();

  pret = pool_tcp_make_connection (ph->name, ph->net,
#ifdef _MSC_VER
                                   &ph->w.wakeup_event_handle,
#else
                                   &ph->w.wakeup_read_fd,
#endif
                                   d, ph->ctx);

  if (pret != OB_OK)
    {
      free_parsed_pseudo_uri (d);
      winsock_shutdown ();
      return pret;
    }

  pret =
    pool_net_send_op (ph->net, POOL_CMD_PARTICIPATE, "sp", d->poolName, NULL);
  if (pret != OB_OK)
    {
      free_parsed_pseudo_uri (d);
      ob_retort tort = pool_tcp_close (0x2010800d, ph->net);
      winsock_shutdown ();
      return (tort < OB_OK ? tort : pret);
    }
  pret = pool_net_recv_result (ph->net, "r", &remote_pret);
  if (pret != OB_OK)
    {
      free_parsed_pseudo_uri (d);
      ob_retort tort = pool_tcp_close (0x2010800e, ph->net);
      winsock_shutdown ();
      return (tort < OB_OK ? tort : pret);
    }
  if (remote_pret != OB_OK)
    {
      free_parsed_pseudo_uri (d);
      ob_retort tort = pool_tcp_close (0x2010800f, ph->net);
      winsock_shutdown ();
      return (tort < OB_OK ? tort : remote_pret);
    }

  ph->ext = d;
  return pool_net_set_hose_name (ph, ph->hose_name);
}

static ob_retort pool_tcp_participate_creatingly (pool_hose ph,
                                                  const char *type,
                                                  bprotein create_options)
{
  ob_retort pret, remote_pret;
  parsed_pseudo_uri *d = new_parsed_pseudo_uri ();
  if (!d)
    return OB_NO_MEM;

  winsock_init ();

  pret = pool_tcp_make_connection (ph->name, ph->net,
#ifdef _MSC_VER
                                   &ph->w.wakeup_event_handle,
#else
                                   &ph->w.wakeup_read_fd,
#endif
                                   d, ph->ctx);

  if (pret != OB_OK)
    {
      free_parsed_pseudo_uri (d);
      winsock_shutdown ();
      return pret;
    }

  // XXX when we add participate options, replace NULL below
  pret = pool_net_send_op (ph->net, POOL_CMD_PARTICIPATE_CREATINGLY, "sspp",
                           d->poolName, type, create_options, NULL);
  if (pret != OB_OK)
    {
      free_parsed_pseudo_uri (d);
      ob_retort tort = pool_tcp_close (0x20108010, ph->net);
      winsock_shutdown ();
      return (tort < OB_OK ? tort : pret);
    }

  pret = pool_net_recv_result (ph->net, "r", &remote_pret);
  if (pret != OB_OK)
    {
      free_parsed_pseudo_uri (d);
      ob_retort tort = pool_tcp_close (0x20108011, ph->net);
      winsock_shutdown ();
      return (tort < OB_OK ? tort : pret);
    }

  if (ph->net->net_version < POOL_TCP_VERSION_WITH_NEW_PCREATINGLY_CODES)
    {
      if (POOL_EXISTS == remote_pret)
        remote_pret = OB_OK;
      else if (OB_OK == remote_pret)
        remote_pret = POOL_CREATED;
    }

  if ((remote_pret != OB_OK) && (remote_pret != POOL_CREATED))
    {
      OB_LOG_DEBUG_CODE (0x20108006,
                         "creatingly remote_pret %" OB_FMT_RETORT "d\n",
                         remote_pret);
      free_parsed_pseudo_uri (d);
      ob_retort tort = pool_tcp_close (0x20108012, ph->net);
      winsock_shutdown ();
      return (tort < OB_OK ? tort : remote_pret);
    }

  ph->ext = d;
  pret = pool_net_set_hose_name (ph, ph->hose_name);
  if (pret < OB_OK)
    return pret;

  return remote_pret;
}

static ob_retort pool_tcp_withdraw (pool_hose ph)
{
  ob_retort pret;
  parsed_pseudo_uri *d = get_parsed_pseudo_uri (ph);

  if (ph->dirty)
    {
      // If we have a dirty ph, the protocol isn't in a consistent state, so we
      // can't send or receive messages.
      pret = OB_OK;
    }
  else
    {
      ob_retort remote_pret;
      pret = pool_net_send_op (ph->net, POOL_CMD_WITHDRAW, NULL);
      if (pret == OB_OK)
        pret = pool_net_recv_result (ph->net, "r", &remote_pret);
      if (pret == OB_OK)
        pret = remote_pret;
    }

  ob_retort tort = pool_tcp_close (0x20108013, ph->net);
  free_parsed_pseudo_uri (d);
  ph->ext = NULL;
  winsock_shutdown ();
  return (tort < OB_OK ? tort : pret);
}

static ob_retort pool_tcp_hiatus (pool_hose ph)
{
  parsed_pseudo_uri *d = get_parsed_pseudo_uri (ph);

  /* In the child after a fork, there are no threads, so attempting
   * to join the thread here would be erroneous.  So null out the
   * thread to prevent pool_tcp_close from joining it.
   * (Once again, we are somewhat nonportably assuming that 0 is not
   * a valid pthread_t.) */
  ph->net->tls_thread = 0;

  ob_retort tort = pool_tcp_close (0x20108022, ph->net);
  free_parsed_pseudo_uri (d);
  ph->ext = NULL;
  winsock_shutdown (); /* admittedly pointless, since Windows can't fork */
  return tort;
}

static ob_retort pool_tcp_dispose (pool_hose ph)
{
  ob_retort pret, remote_pret;
  pool_net_data net;
  OB_INVALIDATE (net);
  OB_INVALIDATE (remote_pret);
  parsed_pseudo_uri *d = new_parsed_pseudo_uri ();
  if (!d)
    return OB_NO_MEM;

  winsock_init ();

  ob_handle_t invalid_wait_obj = OB_NULL_HANDLE;

  pret =
    pool_tcp_make_connection (ph->name, &net, &invalid_wait_obj, d, ph->ctx);
  if (pret != OB_OK)
    {
      free_parsed_pseudo_uri (d);
      winsock_shutdown ();
      return pret;
    }
  pret = pool_net_send_op (&net, POOL_CMD_DISPOSE, "s", d->poolName);
  if (pret != OB_OK)
    {
      ob_retort tort = pool_tcp_close (0x20108014, &net);
      free_parsed_pseudo_uri (d);
      winsock_shutdown ();
      return (tort < OB_OK ? tort : pret);
    }
  pret = pool_net_recv_result (&net, "r", &remote_pret);
  ob_retort tort = pool_tcp_close (0x20108015, &net);
  free_parsed_pseudo_uri (d);
  winsock_shutdown ();
  if (tort < OB_OK)
    return tort;
  if (pret != OB_OK)
    return pret;
  return remote_pret;
}

static ob_retort pool_tcp_sleep (pool_hose ph)
{
  ob_retort pret, remote_pret;
  pool_net_data net;
  OB_INVALIDATE (net);
  OB_INVALIDATE (remote_pret);
  parsed_pseudo_uri *d = new_parsed_pseudo_uri ();
  if (!d)
    return OB_NO_MEM;

  winsock_init ();

  ob_handle_t invalid_wait_obj = OB_NULL_HANDLE;

  pret =
    pool_tcp_make_connection (ph->name, &net, &invalid_wait_obj, d, ph->ctx);
  if (pret != OB_OK)
    {
      free_parsed_pseudo_uri (d);
      winsock_shutdown ();
      return pret;
    }
  if (pool_net_supports_cmd (&net, POOL_CMD_SLEEP))
    pret = pool_net_send_op (&net, POOL_CMD_SLEEP, "s", d->poolName);
  else
    pret = POOL_UNSUPPORTED_OPERATION;
  if (pret != OB_OK)
    {
      ob_retort tort = pool_tcp_close (0x2010801c, &net);
      free_parsed_pseudo_uri (d);
      winsock_shutdown ();
      return (tort < OB_OK ? tort : pret);
    }
  pret = pool_net_recv_result (&net, "r", &remote_pret);
  ob_retort tort = pool_tcp_close (0x2010801d, &net);
  free_parsed_pseudo_uri (d);
  winsock_shutdown ();
  if (tort < OB_OK)
    return tort;
  if (pret != OB_OK)
    return pret;
  return remote_pret;
}

static ob_retort pool_tcp_rename (pool_hose ph, const char *old_name,
                                  const char *new_name)
{
  ob_retort pret, remote_pret;
  pool_net_data net;
  OB_INVALIDATE (net);
  OB_INVALIDATE (remote_pret);
  parsed_pseudo_uri *d = new_parsed_pseudo_uri ();
  parsed_pseudo_uri *dn = new_parsed_pseudo_uri ();
  if (!d || !dn)
    {
      pret = OB_NO_MEM;
    early_exit:
      free_parsed_pseudo_uri (d);
      free_parsed_pseudo_uri (dn);
      return pret;
    }

  pret = parse_pseudo_uri (new_name, dn);
  if (pret < OB_OK)
    goto early_exit;

  char *new_remote_name = (char *) alloca (1 + strlen (dn->poolName));
  strcpy (new_remote_name, dn->poolName);

  winsock_init ();

  ob_handle_t invalid_wait_obj = OB_NULL_HANDLE;
  ob_retort tort = OB_OK;

  pret =
    pool_tcp_make_connection (old_name, &net, &invalid_wait_obj, d, ph->ctx);
  if (pret == OB_OK && (0 != strcmp (d->hostname, dn->hostname)
                        || 0 != strcmp (d->port_str, dn->port_str)))
    {
      OB_LOG_ERROR_CODE (0x2010801a, "Can't rename from %s:%s to %s:%s\n",
                         d->hostname, d->port_str, dn->hostname, dn->port_str);
      tort = pool_tcp_close (0x2010801f, &net);
      pret = POOL_IMPOSSIBLE_RENAME;
    }
  free_parsed_pseudo_uri (dn);
  if (pret == OB_OK && !pool_net_supports_cmd (&net, POOL_CMD_RENAME))
    {
      tort = pool_tcp_close (0x2010801e, &net);
      pret = POOL_UNSUPPORTED_OPERATION;
    }
  if (tort < OB_OK)
    pret = tort;
  if (pret != OB_OK)
    {
      free_parsed_pseudo_uri (d);
      winsock_shutdown ();
      return pret;
    }
  pret = pool_net_send_op (&net, POOL_CMD_RENAME, "ss", d->poolName,
                           new_remote_name);
  if (pret != OB_OK)
    {
      tort = pool_tcp_close (0x20108018, &net);
      free_parsed_pseudo_uri (d);
      winsock_shutdown ();
      return (tort < OB_OK ? tort : pret);
    }
  pret = pool_net_recv_result (&net, "r", &remote_pret);
  tort = pool_tcp_close (0x20108019, &net);
  free_parsed_pseudo_uri (d);
  winsock_shutdown ();
  if (tort < OB_OK)
    return tort;
  if (pret != OB_OK)
    return pret;
  return remote_pret;
}

static ob_retort pool_tcp_list (pool_hose ph, slaw *ret_list)
{
  ob_retort pret, remote_pret;
  pool_net_data net;
  OB_INVALIDATE (net);
  OB_INVALIDATE (remote_pret);
  parsed_pseudo_uri *d = new_parsed_pseudo_uri ();
  if (!d)
    return OB_NO_MEM;

  d->empty_pool_name_ok = true;

  winsock_init ();

  ob_handle_t invalid_wait_obj = OB_NULL_HANDLE;

  pret =
    pool_tcp_make_connection (ph->name, &net, &invalid_wait_obj, d, ph->ctx);
  if (pret != OB_OK)
    {
      free_parsed_pseudo_uri (d);
      winsock_shutdown ();
      return pret;
    }

  if (d->poolName[0] == 0)
    pret = pool_net_send_op (&net, POOL_CMD_LIST, NULL);
  else if (pool_net_supports_cmd (&net, POOL_CMD_LIST_EX))
    pret = pool_net_send_op (&net, POOL_CMD_LIST_EX, "s", d->poolName);
  else
    pret = POOL_UNSUPPORTED_OPERATION;

  if (pret != OB_OK)
    {
      ob_retort tort = pool_tcp_close (0x20108016, &net);
      free_parsed_pseudo_uri (d);
      winsock_shutdown ();
      return (tort < OB_OK ? tort : pret);
    }

  pret = pool_net_recv_result (&net, "rx", &remote_pret, ret_list);
  ob_retort tort = pool_tcp_close (0x20108017, &net);
  free_parsed_pseudo_uri (d);
  winsock_shutdown ();
  if (tort < OB_OK)
    return tort;
  if (pret != OB_OK)
    return pret;
  return remote_pret;
}

static ob_retort pool_tcp_info (pool_hose ph, int64 hops, protein *return_prot)
{
  if (hops == 0)
    {
      parsed_pseudo_uri *d = get_parsed_pseudo_uri (ph);
      int32 port = atoi (d->port_str);
      slaw ingests =
        slaw_map_inline_cf ("type",             slaw_string ("tcp"),
                            "terminal",         slaw_boolean (false),
                            "host",             slaw_string (d->hostname),
                            "port",             slaw_int32 (port),
                            "net-pool-version", slaw_unt32 (ph->net->net_version),
                            "slaw-version",     slaw_unt32 (ph->net->slaw_version),
                            NULL);
      if (!ingests)
        return OB_NO_MEM;
      *return_prot = protein_from_ff (NULL, ingests);
      if (!*return_prot)
        return OB_NO_MEM;
      return OB_OK;
    }

  // If positive, decrement hops (but -1 stays -1)
  if (hops > 0)
    hops--;

  ob_retort pret = pool_net_send_op (ph->net, POOL_CMD_INFO, "i", hops);
  ob_retort remote_pret;
  if (pret == OB_OK)
    pret = pool_net_recv_result (ph->net, "rp", &remote_pret, return_prot);
  if (pret == OB_OK)
    pret = remote_pret;
  return pret;
}

/// Set up the pool implementation operation vector for TCP pools.  We
/// use generic network operation support where possible.

ob_retort pool_tcp_load_methods (pool_hose ph)
{
  ph->create = pool_tcp_create;
  ph->participate = pool_tcp_participate;
  ph->deposit = pool_net_deposit;
  ph->nth_protein = pool_net_nth_protein;
  ph->newest_index = pool_net_newest_index;
  ph->oldest_index = pool_net_oldest_index;
  ph->index_lookup = pool_net_index_lookup;
  ph->withdraw = pool_tcp_withdraw;
  ph->dispose = pool_tcp_dispose;
  ph->await_next_single = pool_net_await_next_single;
  ph->multi_add_awaiter = pool_net_multi_add_awaiter;
  ph->multi_remove_awaiter = pool_net_multi_remove_awaiter;
  ph->info = pool_tcp_info;
  ph->list_pools = pool_tcp_list;
  ph->rename = pool_tcp_rename;
  ph->advance_oldest = pool_net_advance_oldest;
  ph->psleep = pool_tcp_sleep;
  ph->change_options = pool_net_change_options;
  ph->hiatus = pool_tcp_hiatus;

  // Only network pools need below, for efficiency reasons
  ph->participate_creatingly = pool_tcp_participate_creatingly;
  ph->next = pool_net_next;
  ph->prev = pool_net_prev;
  ph->probe_frwd = pool_net_probe_frwd;
  ph->probe_back = pool_net_probe_back;
  ph->opportunistic_next = pool_net_opportunistic_next;
  ph->set_hose_name = pool_net_set_hose_name;
  ph->await_probe_frwd = pool_net_await_probe_frwd;
  ph->fetch = pool_net_fetch;
  return OB_OK;
}
